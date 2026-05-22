#include "websocket_server.h"
#include <ESPAsyncWebServer.h>

void* WebSocketServer::asyncWebSocket = nullptr;

void WebSocketServer::begin(void* server) {
  asyncWebSocket = server;
  
  if (asyncWebSocket) {
    AsyncWebSocket* ws = (AsyncWebSocket*)asyncWebSocket;
    ws->onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                   void* arg, uint8_t* data, size_t len) {
      if (type == WS_EVT_CONNECT) {
        // Client connected
      } else if (type == WS_EVT_DISCONNECT) {
        // Client disconnected
      }
    });
  }
}

void WebSocketServer::loop() {
  // WebSocket server handles events asynchronously
  // No blocking operations needed here
}

void WebSocketServer::broadcast(const String& message) {
  if (asyncWebSocket) {
    AsyncWebSocket* ws = (AsyncWebSocket*)asyncWebSocket;
    ws->textAll(message);
  }
}

int WebSocketServer::getClientCount() {
  if (asyncWebSocket) {
    AsyncWebSocket* ws = (AsyncWebSocket*)asyncWebSocket;
    return ws->count();
  }
  return 0;
}

