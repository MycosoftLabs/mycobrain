#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <Arduino.h>

class WebSocketServer {
public:
  static void begin(void* server);
  static void loop();
  static void broadcast(const String& message);
  static int getClientCount();
  
private:
  static void* asyncWebSocket;
};

#endif // WEBSOCKET_SERVER_H

