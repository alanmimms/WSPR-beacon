#include "HttpHandlerIntf.h"
#include "SettingsIntf.h"
#include "TimeIntf.h"
#include "Beacon.h"
#include "Scheduler.h"
#include "Si5351Intf.h"
#include "JTEncode.h"
#include "cJSON.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <cstdlib>

// Base HttpEndpointHandler implementation

HttpEndpointHandler::HttpEndpointHandler(SettingsIntf* settings, TimeIntf* time)
    : settings(settings), time(time), scheduler(nullptr), beacon(nullptr) {
}

void HttpEndpointHandler::setScheduler(Scheduler* sched) {
    scheduler = sched;
}

void HttpEndpointHandler::setBeacon(Beacon* beaconInstance) {
    beacon = beaconInstance;
}

void HttpEndpointHandler::setSettingsChangedCallback(std::function<void()> callback) {
    settingsChangedCallback = callback;
}

void HttpEndpointHandler::updateBeaconState(const char* netState, const char* txState, const char* band, uint32_t frequency) {
    beaconState.networkState = netState;
    beaconState.transmissionState = txState;
    beaconState.currentBand = band;
    beaconState.currentFrequency = frequency;
    beaconState.isTransmitting = (strcmp(txState, "TRANSMITTING") == 0);
}

std::string HttpEndpointHandler::formatTimeISO(int64_t unixTime) {
    std::time_t timeVal = static_cast<std::time_t>(unixTime);
    struct tm utc_tm;
    
    if (gmtime_r(&timeVal, &utc_tm) == nullptr) {
        return "1970-01-01T00:00:00Z";
    }

    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

HttpHandlerResult HttpEndpointHandler::sendJsonResponse(HttpResponseIntf* response, const std::string& json) {
    response->setContentType("application/json");
    response->send(json);
    return HttpHandlerResult::OK;
}

HttpHandlerResult HttpEndpointHandler::sendError(HttpResponseIntf* response, int code, const std::string& message) {
    response->sendError(code, message);
    return HttpHandlerResult::ERROR;
}

std::string HttpEndpointHandler::getSettingsJson() {
    if (!settings) {
        return "{\"error\":\"Settings not available\"}";
    }
    
    char* jsonStr = settings->toJsonString();
    if (!jsonStr) {
        return "{\"error\":\"Failed to serialize settings\"}";
    }
    
    std::string result(jsonStr);
    free(jsonStr);
    return result;
}

bool HttpEndpointHandler::parseJsonSettings(const std::string& jsonStr) {
    if (!settings) {
        return false;
    }
    
    return settings->fromJsonString(jsonStr.c_str()) && settings->store();
}

std::string HttpEndpointHandler::getStatusJson() {
    // Get base settings JSON
    std::string settingsJson = getSettingsJson();
    if (settingsJson.find("error") != std::string::npos) {
        return settingsJson;
    }
    
    // Parse settings to add live status
    cJSON* status = cJSON_Parse(settingsJson.c_str());
    if (!status) {
        return "{\"error\":\"Failed to parse settings\"}";
    }
    
    // Add current time and status information
    if (time) {
        int64_t currentTime = time->getTime();
        bool synced = time->isTimeSynced();
        cJSON_AddNumberToObject(status, "time", currentTime);
        cJSON_AddBoolToObject(status, "synced", synced);
    }
    
    // Add beacon transmission state
    cJSON_AddStringToObject(status, "txState", beaconState.transmissionState.c_str());
    cJSON_AddStringToObject(status, "curBand", beaconState.currentBand.c_str());
    cJSON_AddNumberToObject(status, "freq", beaconState.currentFrequency);
    
    // Get next transmission info from Beacon if available
    if (beacon) {
        Beacon::NextTransmissionInfo nextTxInfo = beacon->getNextTransmissionInfo();
        cJSON_AddNumberToObject(status, "nextTx", nextTxInfo.secondsUntil);
        cJSON_AddStringToObject(status, "nextTxBand", nextTxInfo.band);
        cJSON_AddNumberToObject(status, "nextTxFreq", nextTxInfo.frequency);
        cJSON_AddBoolToObject(status, "nextTxValid", nextTxInfo.valid);
    } else if (scheduler) {
        // Fallback to scheduler only
        int nextTxSeconds = scheduler->getSecondsUntilNextTransmission();
        cJSON_AddNumberToObject(status, "nextTx", nextTxSeconds);
        cJSON_AddStringToObject(status, "nextTxBand", beaconState.currentBand.c_str());
        cJSON_AddNumberToObject(status, "nextTxFreq", beaconState.currentFrequency);
        cJSON_AddBoolToObject(status, "nextTxValid", false);
    } else {
        cJSON_AddNumberToObject(status, "nextTx", 120); // Fallback
        cJSON_AddStringToObject(status, "nextTxBand", "20m");
        cJSON_AddNumberToObject(status, "nextTxFreq", 14095600);
        cJSON_AddBoolToObject(status, "nextTxValid", false);
    }
    
    // Add transmission statistics from settings
    cJSON* stats = cJSON_CreateObject();
    if (stats) {
        int totalTxCnt = settings ? settings->getInt("totalTxCnt", 0) : 0;
        int totalTxMin = settings ? settings->getInt("totalTxMin", 0) : 0;
        cJSON_AddNumberToObject(stats, "txCnt", totalTxCnt);
        cJSON_AddNumberToObject(stats, "txMin", totalTxMin);
        
        // Add band-specific stats
        cJSON* bands = cJSON_CreateObject();
        if (bands) {
            const char* bandNames[] = {"160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
            const int numBands = sizeof(bandNames) / sizeof(bandNames[0]);
            
            for (int i = 0; i < numBands; i++) {
                cJSON* band = cJSON_CreateObject();
                if (band) {
                    char bandTxCntKey[32], bandTxMinKey[32];
                    snprintf(bandTxCntKey, sizeof(bandTxCntKey), "%sTxCnt", bandNames[i]);
                    snprintf(bandTxMinKey, sizeof(bandTxMinKey), "%sTxMin", bandNames[i]);
                    
                    int bandTxCnt = settings ? settings->getInt(bandTxCntKey, 0) : 0;
                    int bandTxMin = settings ? settings->getInt(bandTxMinKey, 0) : 0;
                    
                    cJSON_AddNumberToObject(band, "txCnt", bandTxCnt);
                    cJSON_AddNumberToObject(band, "txMin", bandTxMin);
                    cJSON_AddItemToObject(bands, bandNames[i], band);
                }
            }
            cJSON_AddItemToObject(stats, "bands", bands);
        }
        cJSON_AddItemToObject(status, "stats", stats);
    }
    
    // Add platform-specific status information
    addPlatformSpecificStatus(status);
    
    // Convert back to JSON string
    char* responseStr = cJSON_PrintUnformatted(status);
    cJSON_Delete(status);
    
    if (!responseStr) {
        return "{\"error\":\"Failed to generate response\"}";
    }
    
    std::string result(responseStr);
    free(responseStr);
    return result;
}

std::string HttpEndpointHandler::getTimeJson() {
    if (!time) {
        return "{\"error\":\"Time interface not available\"}";
    }
    
    int64_t currentTime = time->getTime();
    std::string isoTime = formatTimeISO(currentTime);
    bool synced = time->isTimeSynced();
    int64_t lastSync = time->getLastSyncTime();
    
    cJSON* timeResponse = cJSON_CreateObject();
    cJSON_AddNumberToObject(timeResponse, "unixTime", currentTime);
    cJSON_AddStringToObject(timeResponse, "isoTime", isoTime.c_str());
    cJSON_AddBoolToObject(timeResponse, "synced", synced);
    cJSON_AddNumberToObject(timeResponse, "lastSyncTime", lastSync > 0 ? lastSync * 1000 : 0LL);
    
    char* responseStr = cJSON_Print(timeResponse);
    cJSON_Delete(timeResponse);
    
    if (!responseStr) {
        return "{\"error\":\"Failed to generate time response\"}";
    }
    
    std::string result(responseStr);
    free(responseStr);
    return result;
}

// Endpoint handlers

HttpHandlerResult HttpEndpointHandler::handleApiSettings(HttpRequestIntf* request, HttpResponseIntf* response) {
    if (request->getMethod() == "GET") {
        std::string json = getSettingsJson();
        return sendJsonResponse(response, json);
    } 
    else if (request->getMethod() == "POST") {
        std::string body = request->getBody();
        if (body.empty()) {
            // Try to read from request using heap allocation to avoid stack overflow
            const size_t bufferSize = 4096;
            char* buffer = (char*)malloc(bufferSize);
            if (!buffer) {
                return sendError(response, 500, "Out of memory");
            }
            
            int bytesRead = request->receiveData(buffer, bufferSize - 1);
            if (bytesRead <= 0) {
                free(buffer);
                return sendError(response, 400, "Failed to receive request body");
            }
            buffer[bytesRead] = '\0';
            body = std::string(buffer);
            free(buffer);
        }
        
        if (parseJsonSettings(body)) {
            if (settingsChangedCallback) {
                settingsChangedCallback();
            }
            response->setStatus(204);
            response->send("");
            return HttpHandlerResult::OK;
        } else {
            return sendError(response, 400, "Invalid JSON format or save failed");
        }
    }
    
    return sendError(response, 405, "Method not allowed");
}

HttpHandlerResult HttpEndpointHandler::handleApiStatus(HttpRequestIntf* request, HttpResponseIntf* response) {
    // Add platform-specific WiFi status first
    HttpHandlerResult wifiResult = getPlatformWifiStatus(response);
    if (wifiResult != HttpHandlerResult::OK) {
        return wifiResult;
    }
    
    std::string json = getStatusJson();
    return sendJsonResponse(response, json);
}

HttpHandlerResult HttpEndpointHandler::handleApiTime(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string json = getTimeJson();
    return sendJsonResponse(response, json);
}

HttpHandlerResult HttpEndpointHandler::handleApiTimeSync(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string body = request->getBody();
    if (body.empty()) {
        const size_t bufferSize = 256;  // Time sync requests are small
        char* buffer = (char*)malloc(bufferSize);
        if (!buffer) {
            return sendError(response, 500, "Out of memory");
        }
        
        int bytesRead = request->receiveData(buffer, bufferSize - 1);
        if (bytesRead <= 0) {
            free(buffer);
            return sendError(response, 400, "Invalid request body");
        }
        buffer[bytesRead] = '\0';
        body = std::string(buffer);
        free(buffer);
    }
    
    // Parse JSON to get time
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        return sendError(response, 400, "Invalid JSON format");
    }
    
    cJSON* timeItem = cJSON_GetObjectItem(json, "time");
    if (!timeItem || !cJSON_IsNumber(timeItem)) {
        cJSON_Delete(json);
        return sendError(response, 400, "Missing or invalid time field");
    }
    
    int64_t browserTime = (int64_t)cJSON_GetNumberValue(timeItem);
    cJSON_Delete(json);
    
    // Set system time
    if (time && time->setTime(browserTime)) {
        response->setStatus(200);
        response->send("");
        return HttpHandlerResult::OK;
    } else {
        return sendError(response, 500, "Failed to set system time");
    }
}

HttpHandlerResult HttpEndpointHandler::handleApiWifiScan(HttpRequestIntf* request, HttpResponseIntf* response) {
    return performWifiScan(response);
}

HttpHandlerResult HttpEndpointHandler::handleApiCalibrationStart(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string body = request->getBody();
    if (body.empty()) {
        const size_t bufferSize = 256;
        char* buffer = (char*)malloc(bufferSize);
        if (!buffer) {
            return sendError(response, 500, "Out of memory");
        }
        
        int bytesRead = request->receiveData(buffer, bufferSize - 1);
        if (bytesRead <= 0) {
            free(buffer);
            return sendError(response, 400, "Empty request body");
        }
        buffer[bytesRead] = '\0';
        body = std::string(buffer);
        free(buffer);
    }
    
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        return sendError(response, 400, "Invalid JSON");
    }
    
    cJSON* frequency = cJSON_GetObjectItem(json, "frequency");
    if (!cJSON_IsNumber(frequency)) {
        cJSON_Delete(json);
        return sendError(response, 400, "Missing frequency");
    }
    
    uint32_t freq = (uint32_t)frequency->valueint;
    cJSON_Delete(json);
    
    // Set calibration mode to prevent TX interference
    if (beacon) {
        beacon->setCalibrationMode(true);
        
        // Use Si5351 from the beacon
        Si5351Intf* si5351 = beacon->getSi5351();
        if (si5351) {
            si5351->enableOutput(0, false);
            si5351->setFrequency(0, freq);
            si5351->enableOutput(0, true);
        }
    }
    
    return sendJsonResponse(response, "{\"status\":\"started\"}");
}

HttpHandlerResult HttpEndpointHandler::handleApiCalibrationStop(HttpRequestIntf* request, HttpResponseIntf* response) {
    if (beacon) {
        beacon->setCalibrationMode(false);
        
        Si5351Intf* si5351 = beacon->getSi5351();
        if (si5351) {
            si5351->enableOutput(0, false);
        }
    }
    
    return sendJsonResponse(response, "{\"status\":\"stopped\"}");
}

HttpHandlerResult HttpEndpointHandler::handleApiCalibrationAdjust(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string body = request->getBody();
    if (body.empty()) {
        const size_t bufferSize = 256;
        char* buffer = (char*)malloc(bufferSize);
        if (!buffer) {
            return sendError(response, 500, "Out of memory");
        }
        
        int bytesRead = request->receiveData(buffer, bufferSize - 1);
        if (bytesRead <= 0) {
            free(buffer);
            return sendError(response, 400, "Empty request body");
        }
        buffer[bytesRead] = '\0';
        body = std::string(buffer);
        free(buffer);
    }
    
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        return sendError(response, 400, "Invalid JSON");
    }
    
    cJSON* frequency = cJSON_GetObjectItem(json, "frequency");
    if (!cJSON_IsNumber(frequency)) {
        cJSON_Delete(json);
        return sendError(response, 400, "Missing frequency");
    }
    
    uint32_t freq = (uint32_t)frequency->valueint;
    cJSON_Delete(json);
    
    if (beacon) {
        Si5351Intf* si5351 = beacon->getSi5351();
        if (si5351) {
            si5351->setFrequency(0, freq);
        }
    }
    
    return sendJsonResponse(response, "{\"status\":\"adjusted\"}");
}

HttpHandlerResult HttpEndpointHandler::handleApiCalibrationCorrection(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string body = request->getBody();
    if (body.empty()) {
        const size_t bufferSize = 256;
        char* buffer = (char*)malloc(bufferSize);
        if (!buffer) {
            return sendError(response, 500, "Out of memory");
        }
        
        int bytesRead = request->receiveData(buffer, bufferSize - 1);
        if (bytesRead <= 0) {
            free(buffer);
            return sendError(response, 400, "Empty request body");
        }
        buffer[bytesRead] = '\0';
        body = std::string(buffer);
        free(buffer);
    }
    
    cJSON* json = cJSON_Parse(body.c_str());
    if (!json) {
        return sendError(response, 400, "Invalid JSON");
    }
    
    cJSON* correction = cJSON_GetObjectItem(json, "correction");
    if (!cJSON_IsNumber(correction)) {
        cJSON_Delete(json);
        return sendError(response, 400, "Missing correction");
    }
    
    int32_t correctionPPM = (int32_t)(correction->valuedouble * 1000); // Convert to milli-PPM
    cJSON_Delete(json);
    
    if (beacon) {
        Si5351Intf* si5351 = beacon->getSi5351();
        if (si5351) {
            si5351->setCalibration(correctionPPM);
        }
    }
    
    return sendJsonResponse(response, "{\"status\":\"applied\"}");
}

HttpHandlerResult HttpEndpointHandler::handleApiWSPREncode(HttpRequestIntf* request, HttpResponseIntf* response) {
    std::string body = request->getBody();
    if (body.empty()) {
        return sendError(response, 400, "Empty request body");
    }
    
    cJSON* requestData = cJSON_Parse(body.c_str());
    if (!requestData) {
        return sendError(response, 400, "Invalid JSON format");
    }
    
    // Extract parameters with defaults
    cJSON* callsignItem = cJSON_GetObjectItem(requestData, "callsign");
    cJSON* locatorItem = cJSON_GetObjectItem(requestData, "locator");
    cJSON* powerItem = cJSON_GetObjectItem(requestData, "powerDbm");
    cJSON* frequencyItem = cJSON_GetObjectItem(requestData, "frequency");
    
    std::string callsign = (callsignItem && cJSON_IsString(callsignItem)) ? callsignItem->valuestring : "N0CALL";
    std::string locator = (locatorItem && cJSON_IsString(locatorItem)) ? locatorItem->valuestring : "AA00aa";
    int powerDbm = (powerItem && cJSON_IsNumber(powerItem)) ? powerItem->valueint : 10;
    uint32_t frequency = (frequencyItem && cJSON_IsNumber(frequencyItem)) ? (uint32_t)frequencyItem->valueint : 14097100;
    
    cJSON_Delete(requestData);
    
    try {
        // Create WSPR encoder
        WSPREncoder encoder(frequency);
        
        // Encode the message
        encoder.encode(callsign.c_str(), locator.c_str(), static_cast<int8_t>(powerDbm));
        
        // Build response with symbols
        cJSON* response_json = cJSON_CreateObject();
        cJSON_AddBoolToObject(response_json, "success", true);
        cJSON_AddStringToObject(response_json, "callsign", callsign.c_str());
        cJSON_AddStringToObject(response_json, "locator", locator.c_str());
        cJSON_AddNumberToObject(response_json, "powerDbm", powerDbm);
        cJSON_AddNumberToObject(response_json, "frequency", frequency);
        cJSON_AddNumberToObject(response_json, "symbolCount", WSPREncoder::TxBufferSize);
        cJSON_AddNumberToObject(response_json, "toneSpacing", WSPREncoder::ToneSpacing);
        cJSON_AddNumberToObject(response_json, "symbolPeriod", WSPREncoder::SymbolPeriod);
        
        // Add symbols array
        cJSON* symbolsArray = cJSON_CreateArray();
        for (int i = 0; i < WSPREncoder::TxBufferSize; i++) {
            cJSON_AddItemToArray(symbolsArray, cJSON_CreateNumber(encoder.symbols[i]));
        }
        cJSON_AddItemToObject(response_json, "symbols", symbolsArray);
        
        // Calculate transmission duration
        uint32_t transmissionDurationMs = WSPREncoder::TxBufferSize * WSPREncoder::SymbolPeriod;
        cJSON_AddNumberToObject(response_json, "transmissionDurationMs", transmissionDurationMs);
        cJSON_AddNumberToObject(response_json, "transmissionDurationSeconds", transmissionDurationMs / 1000.0);
        
        char* responseStr = cJSON_Print(response_json);
        cJSON_Delete(response_json);
        
        if (responseStr) {
            std::string result(responseStr);
            free(responseStr);
            return sendJsonResponse(response, result);
        } else {
            return sendError(response, 500, "Failed to generate response");
        }
        
    } catch (const std::exception& e) {
        cJSON* errorResponse = cJSON_CreateObject();
        cJSON_AddBoolToObject(errorResponse, "success", false);
        cJSON_AddStringToObject(errorResponse, "error", e.what());
        
        char* errorStr = cJSON_Print(errorResponse);
        cJSON_Delete(errorResponse);
        
        if (errorStr) {
            std::string result(errorStr);
            free(errorStr);
            response->setStatus(400);
            return sendJsonResponse(response, result);
        } else {
            return sendError(response, 400, "Unknown error");
        }
    }
}