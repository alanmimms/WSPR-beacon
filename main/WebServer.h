#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "esp_http_server.h"

class WebServer {
public:
  void start(bool isProvisioning);
  void stop();

protected:
  httpd_handle_t server;
};

#endif // WEBSERVER_H
