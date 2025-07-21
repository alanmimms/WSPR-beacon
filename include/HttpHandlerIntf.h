#ifndef HTTP_HANDLER_INTF_H
#define HTTP_HANDLER_INTF_H

#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include "cJSON.h"

// Forward declarations
class SettingsIntf;
class TimeIntf;
class Beacon;
class Scheduler;
class Si5351Intf;

/**
 * Abstract HTTP request/response interface to abstract platform-specific HTTP handling
 */
class HttpRequestIntf {
public:
    virtual ~HttpRequestIntf() = default;
    
    // Request data access
    virtual std::string getBody() const = 0;
    virtual std::string getUri() const = 0;
    virtual std::string getMethod() const = 0;
    virtual std::string getHeader(const std::string& name) const = 0;
    virtual size_t getContentLength() const = 0;
    
    // Request body reading
    virtual int receiveData(char* buffer, size_t maxSize) = 0;
};

/**
 * Abstract HTTP response interface
 */
class HttpResponseIntf {
public:
    virtual ~HttpResponseIntf() = default;
    
    // Response control
    virtual void setStatus(int code) = 0;
    virtual void setStatus(const std::string& statusLine) = 0;
    virtual void setContentType(const std::string& contentType) = 0;
    virtual void setHeader(const std::string& name, const std::string& value) = 0;
    
    // Response body
    virtual void send(const std::string& content) = 0;
    virtual void send(const char* data, size_t length) = 0;
    virtual void sendError(int code, const std::string& message = "") = 0;
    virtual void sendChunk(const char* data, size_t length) = 0;
    virtual void endChunked() = 0;
};

/**
 * Result of an HTTP handler operation
 */
enum class HttpHandlerResult {
    OK,
    ERROR,
    NOT_FOUND,
    BAD_REQUEST,
    INTERNAL_ERROR
};

/**
 * Shared HTTP endpoint logic for both ESP32 and host-mock platforms
 * This class contains all the common business logic for HTTP endpoints,
 * abstracting away platform-specific HTTP server implementations.
 */
class HttpEndpointHandler {
public:
    HttpEndpointHandler(SettingsIntf* settings, TimeIntf* time);
    ~HttpEndpointHandler() = default;
    
    // Dependency injection
    void setScheduler(Scheduler* scheduler);
    void setBeacon(Beacon* beacon);
    void setSettingsChangedCallback(std::function<void()> callback);
    
    // State updates (for beacon state tracking)
    void updateBeaconState(const char* netState, const char* txState, const char* band, uint32_t frequency);
    
    // Common endpoint handlers
    HttpHandlerResult handleApiSettings(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiStatus(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiTime(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiTimeSync(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiWifiScan(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiCalibrationStart(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiCalibrationStop(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiCalibrationAdjust(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiCalibrationCorrection(HttpRequestIntf* request, HttpResponseIntf* response);
    HttpHandlerResult handleApiWSPREncode(HttpRequestIntf* request, HttpResponseIntf* response);
    
    // Utility methods
    static std::string formatTimeISO(int64_t unixTime);
    
protected:
    // Platform-specific operations (to be implemented by subclasses)
    virtual HttpHandlerResult getPlatformWifiStatus(HttpResponseIntf* response) = 0;
    virtual HttpHandlerResult performWifiScan(HttpResponseIntf* response) = 0;
    virtual void addPlatformSpecificStatus(cJSON* status) = 0;
    
    // Helper methods (available to subclasses)
    std::string getSettingsJson();
    std::string getStatusJson();
    std::string getTimeJson();
    bool parseJsonSettings(const std::string& jsonStr);
    HttpHandlerResult sendJsonResponse(HttpResponseIntf* response, const std::string& json);
    HttpHandlerResult sendError(HttpResponseIntf* response, int code, const std::string& message);
    
private:
    SettingsIntf* settings;
    TimeIntf* time;
    Scheduler* scheduler;
    Beacon* beacon;
    std::function<void()> settingsChangedCallback;
    
    // Beacon state tracking
    struct BeaconState {
        std::string networkState = "BOOTING";
        std::string transmissionState = "IDLE";
        std::string currentBand = "20m";
        uint32_t currentFrequency = 14097100;
        bool isTransmitting = false;
    } beaconState;
};

/**
 * ESP32-specific implementation
 */
class ESP32HttpEndpointHandler : public HttpEndpointHandler {
public:
    ESP32HttpEndpointHandler(SettingsIntf* settings, TimeIntf* time);
    
protected:
    HttpHandlerResult getPlatformWifiStatus(HttpResponseIntf* response) override;
    HttpHandlerResult performWifiScan(HttpResponseIntf* response) override;
    void addPlatformSpecificStatus(cJSON* status) override;
};

/**
 * Host-mock specific implementation  
 */
class HostMockHttpEndpointHandler : public HttpEndpointHandler {
public:
    HostMockHttpEndpointHandler(SettingsIntf* settings, TimeIntf* time, double timeScale = 1.0);
    
    void setTimeScale(double scale) { timeScale_ = scale; }
    
protected:
    HttpHandlerResult getPlatformWifiStatus(HttpResponseIntf* response) override;
    HttpHandlerResult performWifiScan(HttpResponseIntf* response) override;
    void addPlatformSpecificStatus(cJSON* status) override;
    
private:
    double timeScale_;
    std::chrono::steady_clock::time_point serverStartTime_;
    int64_t mockStartTime_;
    
    int64_t getMockTime() const;
};

#endif // HTTP_HANDLER_INTF_H