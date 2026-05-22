#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <WiFi.h>
#include <DNSServer.h>

class DNSServerManager {
public:
  static bool begin(IPAddress apIP);
  static void loop();
  static void stop();
  
private:
  static DNSServer dnsServer;
  static bool started;
};

#endif // DNS_SERVER_H

