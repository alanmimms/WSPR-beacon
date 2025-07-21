#include "HttpHandlerIntf.h"
#include "TimeIntf.h"
#include "SettingsIntf.h"
#include "cJSON.h"
#include <chrono>
#include <sstream>

HostMockHttpEndpointHandler::HostMockHttpEndpointHandler(SettingsIntf* settings, TimeIntf* time, double timeScale)
    : HttpEndpointHandler(settings, time), timeScale_(timeScale) {
    serverStartTime_ = std::chrono::steady_clock::now();
    mockStartTime_ = time ? time->getTime() : std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t HostMockHttpEndpointHandler::getMockTime() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - serverStartTime_).count();
    int64_t scaledElapsed = static_cast<int64_t>(elapsed * timeScale_);
    return mockStartTime_ + (scaledElapsed / 1000);  // Convert to seconds
}

HttpHandlerResult HostMockHttpEndpointHandler::getPlatformWifiStatus(HttpResponseIntf* response) {
    // For host-mock, we simulate WiFi status based on settings
    return HttpHandlerResult::OK;
}

void HostMockHttpEndpointHandler::addPlatformSpecificStatus(cJSON* status) {
    // Simulate WiFi status for host-mock
    // TODO: Get settings to determine WiFi mode simulation
    // For now, simulate STA mode with good connection
    cJSON_AddStringToObject(status, "wifiMode", "sta");
    cJSON_AddStringToObject(status, "netState", "READY");
    cJSON_AddStringToObject(status, "ssid", "MockWiFi");
    cJSON_AddNumberToObject(status, "rssi", -45);
    
    // Add mock uptime based on time since server start
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - serverStartTime_).count();
    cJSON_AddNumberToObject(status, "uptime", elapsed);
}

HttpHandlerResult HostMockHttpEndpointHandler::performWifiScan(HttpResponseIntf* response) {
    // Mock WiFi scan results with time-varying signal strengths
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - serverStartTime_).count();
    
    // Simulate signal strength variations over time
    int timeOffset = elapsed / 5; // Changes every 5 seconds
    
    cJSON* scanResults = cJSON_CreateArray();
    
    // Network 1
    cJSON* net1 = cJSON_CreateObject();
    cJSON_AddStringToObject(net1, "ssid", "MyHomeWiFi");
    cJSON_AddNumberToObject(net1, "rssi", -45 + (timeOffset % 10) - 5); // -50 to -40
    cJSON_AddStringToObject(net1, "encryption", "WPA2");
    cJSON_AddNumberToObject(net1, "channel", 6);
    cJSON_AddItemToArray(scanResults, net1);
    
    // Network 2
    cJSON* net2 = cJSON_CreateObject();
    cJSON_AddStringToObject(net2, "ssid", "Neighbor_2.4G");
    cJSON_AddNumberToObject(net2, "rssi", -67 + (timeOffset % 8) - 4); // -71 to -63
    cJSON_AddStringToObject(net2, "encryption", "WPA2");
    cJSON_AddNumberToObject(net2, "channel", 11);
    cJSON_AddItemToArray(scanResults, net2);
    
    // Network 3
    cJSON* net3 = cJSON_CreateObject();
    cJSON_AddStringToObject(net3, "ssid", "CoffeeShop_Guest");
    cJSON_AddNumberToObject(net3, "rssi", -72 + (timeOffset % 6) - 3); // -75 to -69
    cJSON_AddStringToObject(net3, "encryption", "Open");
    cJSON_AddNumberToObject(net3, "channel", 1);
    cJSON_AddItemToArray(scanResults, net3);
    
    // Network 4
    cJSON* net4 = cJSON_CreateObject();
    cJSON_AddStringToObject(net4, "ssid", "TestNetwork_5G");
    cJSON_AddNumberToObject(net4, "rssi", -58 + (timeOffset % 12) - 6); // -64 to -52
    cJSON_AddStringToObject(net4, "encryption", "WPA3");
    cJSON_AddNumberToObject(net4, "channel", 36);
    cJSON_AddItemToArray(scanResults, net4);
    
    // Network 5
    cJSON* net5 = cJSON_CreateObject();
    cJSON_AddStringToObject(net5, "ssid", "Enterprise_Corp");
    cJSON_AddNumberToObject(net5, "rssi", -81 + (timeOffset % 4) - 2); // -83 to -79
    cJSON_AddStringToObject(net5, "encryption", "WPA2-Enterprise");
    cJSON_AddNumberToObject(net5, "channel", 44);
    cJSON_AddItemToArray(scanResults, net5);
    
    // Network 6
    cJSON* net6 = cJSON_CreateObject();
    cJSON_AddStringToObject(net6, "ssid", "WeakSignal_Test");
    cJSON_AddNumberToObject(net6, "rssi", -85 + (timeOffset % 6) - 3); // -88 to -82
    cJSON_AddStringToObject(net6, "encryption", "WPA2");
    cJSON_AddNumberToObject(net6, "channel", 13);
    cJSON_AddItemToArray(scanResults, net6);
    
    char* responseStr = cJSON_Print(scanResults);
    cJSON_Delete(scanResults);
    
    if (responseStr) {
        std::string result(responseStr);
        free(responseStr);
        return sendJsonResponse(response, result);
    } else {
        return sendJsonResponse(response, "{\"error\":\"Failed to generate scan results\"}");
    }
}