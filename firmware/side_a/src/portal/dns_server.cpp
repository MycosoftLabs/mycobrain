#include "dns_server.h"

DNSServer DNSServerManager::dnsServer;
bool DNSServerManager::started = false;

bool DNSServerManager::begin(IPAddress apIP) {
  if (started) {
    dnsServer.stop();
  }
  
  // DNS server will redirect all queries to the AP IP
  if (dnsServer.start(53, "*", apIP)) {
    started = true;
    return true;
  }
  return false;
}

void DNSServerManager::loop() {
  if (started) {
    dnsServer.processNextRequest();
  }
}

void DNSServerManager::stop() {
  if (started) {
    dnsServer.stop();
    started = false;
  }
}

