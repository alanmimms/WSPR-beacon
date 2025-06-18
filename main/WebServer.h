#pragma once

#include "esp_http_server.h"

class WebServer {
public:
    WebServer();
    void start();
    void stop();

    // Add these declarations:
    static esp_err_t rootGetHandler(httpd_req_t *req);
    static esp_err_t fileGetHandler(httpd_req_t *req);
    static esp_err_t apiSettingsGetHandler(httpd_req_t *req);
    static esp_err_t apiSettingsPostHandler(httpd_req_t *req);
    static esp_err_t apiStatusGetHandler(httpd_req_t *req);  // <-- Add this line

private:
    httpd_handle_t server;
};

// Also declare getStatusJson if not already declared elsewhere:
extern "C" esp_err_t getStatusJson(char* buf, size_t buflen);