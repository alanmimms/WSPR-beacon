#ifndef ESP32_HTTP_WRAPPERS_H
#define ESP32_HTTP_WRAPPERS_H

#include "HttpHandlerIntf.h"
#include "esp_http_server.h"
#include <string>

/**
 * ESP32 HTTP request wrapper
 */
class ESP32HttpRequest : public HttpRequestIntf {
public:
    ESP32HttpRequest(httpd_req_t* req) : req_(req) {}
    
    std::string getBody() const override {
        // For ESP32, body needs to be read via receiveData
        return "";
    }
    
    std::string getUri() const override {
        return std::string(req_->uri);
    }
    
    std::string getMethod() const override {
        switch (req_->method) {
            case HTTP_GET: return "GET";
            case HTTP_POST: return "POST";
            case HTTP_PUT: return "PUT";
            case HTTP_DELETE: return "DELETE";
            default: return "UNKNOWN";
        }
    }
    
    std::string getHeader(const std::string& name) const override {
        size_t buf_len = httpd_req_get_hdr_value_len(req_, name.c_str());
        if (buf_len == 0) {
            return "";
        }
        
        char* buf = (char*)malloc(buf_len + 1);
        if (!buf) {
            return "";
        }
        
        if (httpd_req_get_hdr_value_str(req_, name.c_str(), buf, buf_len + 1) == ESP_OK) {
            std::string result(buf);
            free(buf);
            return result;
        }
        
        free(buf);
        return "";
    }
    
    size_t getContentLength() const override {
        return req_->content_len;
    }
    
    int receiveData(char* buffer, size_t maxSize) override {
        return httpd_req_recv(req_, buffer, maxSize);
    }
    
private:
    httpd_req_t* req_;
};

/**
 * ESP32 HTTP response wrapper
 */
class ESP32HttpResponse : public HttpResponseIntf {
public:
    ESP32HttpResponse(httpd_req_t* req) : req_(req) {}
    
    void setStatus(int code) override {
        switch (code) {
            case 200: httpd_resp_set_status(req_, "200 OK"); break;
            case 204: httpd_resp_set_status(req_, "204 No Content"); break;
            case 400: httpd_resp_set_status(req_, "400 Bad Request"); break;
            case 404: httpd_resp_set_status(req_, "404 Not Found"); break;
            case 405: httpd_resp_set_status(req_, "405 Method Not Allowed"); break;
            case 500: httpd_resp_set_status(req_, "500 Internal Server Error"); break;
            default: {
                char status[32];
                snprintf(status, sizeof(status), "%d", code);
                httpd_resp_set_status(req_, status);
            }
        }
    }
    
    void setStatus(const std::string& statusLine) override {
        httpd_resp_set_status(req_, statusLine.c_str());
    }
    
    void setContentType(const std::string& contentType) override {
        httpd_resp_set_type(req_, contentType.c_str());
    }
    
    void setHeader(const std::string& name, const std::string& value) override {
        httpd_resp_set_hdr(req_, name.c_str(), value.c_str());
    }
    
    void send(const std::string& content) override {
        httpd_resp_send(req_, content.c_str(), content.length());
    }
    
    void send(const char* data, size_t length) override {
        httpd_resp_send(req_, data, length);
    }
    
    void sendError(int code, const std::string& message) override {
        if (message.empty()) {
            switch (code) {
                case 400: httpd_resp_send_err(req_, HTTPD_400_BAD_REQUEST, "Bad Request"); break;
                case 404: httpd_resp_send_err(req_, HTTPD_404_NOT_FOUND, "Not Found"); break;
                case 405: httpd_resp_send_err(req_, HTTPD_405_METHOD_NOT_ALLOWED, "Method Not Allowed"); break;
                case 500: httpd_resp_send_err(req_, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Server Error"); break;
                default: httpd_resp_send_500(req_); break;
            }
        } else {
            httpd_resp_send_err(req_, static_cast<httpd_err_code_t>(code), message.c_str());
        }
    }
    
    void sendChunk(const char* data, size_t length) override {
        httpd_resp_send_chunk(req_, data, length);
    }
    
    void endChunked() override {
        httpd_resp_send_chunk(req_, NULL, 0);
    }
    
private:
    httpd_req_t* req_;
};

#endif // ESP32_HTTP_WRAPPERS_H