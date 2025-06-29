#pragma once

#include <functional>
#include <stddef.h>

/**
 * Abstract interface for HTTP server.
 * Allows registering URI handlers, starting/stopping server, and sending responses.
 * Host impl: can use cpp-httplib or similar.
 * Target impl: wraps ESP-IDF HTTP server.
 */
class HttpServerIntf {
public:
  virtual ~HttpServerIntf() {}

  enum Method {
    GET,
    POST,
    PUT,
    DELETE_,
    PATCH,
    HEAD
  };

  // Handler signature: (uri, method, body, bodyLen, responseSender, userCtx)
  using HandlerFn = std::function<void(const char *uri, Method method, const char *body, size_t bodyLen,
                                       std::function<void(int status, const char *contentType, const char *body, size_t bodyLen)> responseSender,
                                       void *userCtx)>;

  // Register a URI handler for a given path and HTTP method.
  // userCtx is passed to handler when invoked.
  virtual bool registerHandler(const char *uri, Method method, HandlerFn handler, void *userCtx = nullptr) = 0;

  // Start the server on the given port (default 80/443).
  virtual bool start(int port = 80) = 0;

  // Stop the server.
  virtual void stop() = 0;

  // Returns true if server is running.
  virtual bool isRunning() const = 0;
};