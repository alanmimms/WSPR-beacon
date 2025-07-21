#include "HttpHandlerIntf.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <cstring>
#include <string>

static const char *TAG = "ESP32HttpHandler";

ESP32HttpEndpointHandler::ESP32HttpEndpointHandler(SettingsIntf* settings, TimeIntf* time)
    : HttpEndpointHandler(settings, time) {
}

HttpHandlerResult ESP32HttpEndpointHandler::getPlatformWifiStatus(HttpResponseIntf* response) {
    // This will be called during status generation to add platform-specific WiFi info
    // The actual WiFi status will be added by addPlatformSpecificStatus method
    return HttpHandlerResult::OK;
}

void ESP32HttpEndpointHandler::addPlatformSpecificStatus(cJSON* status) {
    // Add WiFi status information specific to ESP32
    wifi_mode_t wifi_mode;
    esp_err_t err = esp_wifi_get_mode(&wifi_mode);
    
    if (err == ESP_OK) {
        // Add WiFi mode to JSON response
        if (wifi_mode == WIFI_MODE_AP) {
            cJSON_AddStringToObject(status, "wifiMode", "ap");
        } else if (wifi_mode == WIFI_MODE_STA) {
            cJSON_AddStringToObject(status, "wifiMode", "sta");
        } else if (wifi_mode == WIFI_MODE_APSTA) {
            cJSON_AddStringToObject(status, "wifiMode", "apsta");
        }
        
        if (wifi_mode == WIFI_MODE_AP || wifi_mode == WIFI_MODE_APSTA) {
            cJSON_AddStringToObject(status, "netState", "AP_MODE");
            
            // Get connected client count for AP mode
            wifi_sta_list_t sta_list;
            esp_err_t sta_err = esp_wifi_ap_get_sta_list(&sta_list);
            if (sta_err == ESP_OK) {
                cJSON_AddNumberToObject(status, "clientCount", sta_list.num);
            } else {
                cJSON_AddNumberToObject(status, "clientCount", 0);
            }
        }
        
        if (wifi_mode == WIFI_MODE_STA || wifi_mode == WIFI_MODE_APSTA) {
            // Check if STA is connected - retry up to 3 times if it fails
            wifi_ap_record_t ap_info;
            err = esp_wifi_sta_get_ap_info(&ap_info);
            
            // Retry if failed (common after mode switches)
            for (int retry = 0; retry < 2 && err != ESP_OK; retry++) {
                vTaskDelay(pdMS_TO_TICKS(100));
                err = esp_wifi_sta_get_ap_info(&ap_info);
            }
            
            if (err == ESP_OK) {
                cJSON_AddStringToObject(status, "netState", "READY");
                cJSON_AddStringToObject(status, "ssid", (char*)ap_info.ssid);
                cJSON_AddNumberToObject(status, "rssi", ap_info.rssi);
            } else {
                ESP_LOGW(TAG, "esp_wifi_sta_get_ap_info failed after retries: %s", esp_err_to_name(err));
                
                // Fallback: check if we're connected but can't get AP info
                esp_netif_ip_info_t ip_info;
                esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                    // We have an IP, so we're connected, but can't get AP details
                    cJSON_AddStringToObject(status, "netState", "READY");
                    
                    // Try to get SSID from settings first (assuming we have access to settings)
                    cJSON_AddStringToObject(status, "ssid", "Connected");
                    // Don't set RSSI if we can't get it - let UI handle missing value
                } else {
                    cJSON_AddStringToObject(status, "netState", "STA_CONNECTING");
                }
            }
        }
    }
    
    // Add uptime in seconds since boot using FreeRTOS ticks
    TickType_t uptimeTicks = xTaskGetTickCount();
    int64_t uptimeSeconds = uptimeTicks / configTICK_RATE_HZ;
    cJSON_AddNumberToObject(status, "uptime", uptimeSeconds);
}

HttpHandlerResult ESP32HttpEndpointHandler::performWifiScan(HttpResponseIntf* response) {
    ESP_LOGI(TAG, "WiFi scan requested");
    
    // Switch to APSTA mode to allow scanning
    wifi_mode_t original_mode;
    esp_wifi_get_mode(&original_mode);
    if (original_mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Switching to APSTA mode for scanning");
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch to APSTA mode: %s", esp_err_to_name(err));
            return sendJsonResponse(response, "{\"error\":\"Mode switch failed\"}");
        }
    } else if (original_mode == WIFI_MODE_STA) {
        ESP_LOGI(TAG, "Switching from STA to APSTA mode for scanning");
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch to APSTA mode: %s", esp_err_to_name(err));
            return sendJsonResponse(response, "{\"error\":\"Mode switch failed\"}");
        }
    }
    
    // Perform WiFi scan
    wifi_scan_config_t scan_config = {};
    scan_config.ssid = NULL;
    scan_config.bssid = NULL;
    scan_config.channel = 0;
    scan_config.show_hidden = true;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        // Restore original mode if scan failed
        if (original_mode != WIFI_MODE_APSTA) {
            esp_wifi_set_mode(original_mode);
        }
        return sendJsonResponse(response, "{\"error\":\"Scan failed\"}");
    }
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        return sendJsonResponse(response, "{\"error\":\"Memory allocation failed\"}");
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    // Build JSON response
    cJSON* scanResults = cJSON_CreateArray();
    
    for (int i = 0; i < ap_count && i < 20; i++) {  // Limit to 20 APs
        if (strlen((char*)ap_list[i].ssid) == 0) {
            continue; // Skip hidden SSIDs
        }
        
        cJSON* ap = cJSON_CreateObject();
        
        // Escape quotes in SSID
        char escaped_ssid[64];
        int j = 0, k = 0;
        while (ap_list[i].ssid[j] && k < 62) {
            if (ap_list[i].ssid[j] == '"' || ap_list[i].ssid[j] == '\\') {
                escaped_ssid[k++] = '\\';
            }
            escaped_ssid[k++] = ap_list[i].ssid[j++];
        }
        escaped_ssid[k] = '\0';
        
        cJSON_AddStringToObject(ap, "ssid", escaped_ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_list[i].rssi);
        cJSON_AddNumberToObject(ap, "authmode", ap_list[i].authmode);
        cJSON_AddItemToArray(scanResults, ap);
    }
    
    char* responseStr = cJSON_Print(scanResults);
    cJSON_Delete(scanResults);
    
    if (responseStr) {
        std::string result(responseStr);
        free(responseStr);
        free(ap_list);
        
        // Restore original WiFi mode
        if (original_mode != WIFI_MODE_APSTA) {
            ESP_LOGI(TAG, "Restoring original WiFi mode after scan");
            esp_wifi_set_mode(original_mode);
            // Give the WiFi subsystem time to re-establish connection
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        ESP_LOGI(TAG, "WiFi scan completed, found %d APs", ap_count);
        return sendJsonResponse(response, result);
    } else {
        free(ap_list);
        return sendJsonResponse(response, "{\"error\":\"Failed to generate response\"}");
    }
}