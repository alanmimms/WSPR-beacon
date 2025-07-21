#ifndef HOST_MOCK_HTTP_WRAPPERS_H
#define HOST_MOCK_HTTP_WRAPPERS_H

#include "HttpHandlerIntf.h"
#include "httplib.h"
#include <string>
#include <cstring>

/**
 * Host-mock HTTP request wrapper for httplib
 */
class HostMockHttpRequest : public HttpRequestIntf {
public:
    HostMockHttpRequest(const httplib::Request& req) : req_(req) {}
    
    std::string getBody() const override {
        return req_.body;
    }
    
    std::string getUri() const override {
        return req_.path;
    }
    
    std::string getMethod() const override {
        return req_.method;
    }
    
    std::string getHeader(const std::string& name) const override {
        auto it = req_.headers.find(name);
        return (it != req_.headers.end()) ? it->second : "";
    }
    
    size_t getContentLength() const override {
        return req_.body.size();
    }
    
    int receiveData(char* buffer, size_t maxSize) override {
        // For httplib, the body is already available
        size_t copySize = std::min(req_.body.size(), maxSize);
        if (copySize > 0) {
            memcpy(buffer, req_.body.data(), copySize);
            return static_cast<int>(copySize);
        }
        return 0;
    }
    
private:
    const httplib::Request& req_;
};

/**
 * Host-mock HTTP response wrapper for httplib
 */
class HostMockHttpResponse : public HttpResponseIntf {
public:
    HostMockHttpResponse(httplib::Response& res) : res_(res) {}
    
    void setStatus(int code) override {
        res_.status = code;
    }
    
    void setStatus(const std::string& statusLine) override {
        // Parse status line like "200 OK" or "404 Not Found"
        size_t space = statusLine.find(' ');
        if (space != std::string::npos) {
            res_.status = std::stoi(statusLine.substr(0, space));
        } else {
            res_.status = std::stoi(statusLine);
        }
    }
    
    void setContentType(const std::string& contentType) override {
        res_.set_header("Content-Type", contentType);
    }
    
    void setHeader(const std::string& name, const std::string& value) override {
        res_.set_header(name, value);
    }
    
    void send(const std::string& content) override {
        res_.set_content(content, getContentType());
    }
    
    void send(const char* data, size_t length) override {
        res_.set_content(std::string(data, length), getContentType());
    }
    
    void sendError(int code, const std::string& message) override {
        res_.status = code;
        if (!message.empty()) {
            res_.set_content("{\"error\":\"" + message + "\"}", "application/json");
        }
    }
    
    void sendChunk(const char* data, size_t length) override {
        // httplib doesn't support chunked responses in the same way
        // Just append to content for now
        if (data && length > 0) {
            res_.body.append(data, length);
        }
    }
    
    void endChunked() override {
        // No special action needed for httplib
    }
    
private:
    httplib::Response& res_;
    
    std::string getContentType() const {
        auto it = res_.headers.find("Content-Type");
        return (it != res_.headers.end()) ? it->second : "text/plain";
    }
};

#endif // HOST_MOCK_HTTP_WRAPPERS_H