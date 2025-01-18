#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <SPIFFS.h>

uint32_t cameraClientId = 0;

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket ws("/ws");

void Control_Motors(char direction)
{
  Serial.println(direction);
};

void sendCameraPicture()
{
  if (cameraClientId == 0)
  {
    return;
  }
  unsigned long startTime1 = millis();
  // capture a frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Frame buffer could not be acquired");
    return;
  }

  unsigned long startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);

  while (true)
  {
    AsyncWebSocketClient *clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull()))
    {
      break;
    }
    delay(1);
  }

  unsigned long startTime3 = millis();
}

void handleJoystickData(uint8_t *data, size_t len)
{
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, data, len);
  if (error)
  {
    return;
  }
  int xValue = doc["x"] | 0;
  int yValue = doc["y"] | 0;
  bool clockwise = doc["clockwise"] | false;
  bool counterclockwise = doc["counterClockwise"] | false;
  bool diagonalRightForward = doc["diagonalRightForward"] | false;
  bool diagonalLeftForward = doc["diagonalLeftForward"] | false;
  bool diagonalRightBackward = doc["diagonalRightBackward"] | false;
  bool diagonalLeftBackward = doc["diagonalLeftBackward"] | false;
  char direction = 's'; // Default to stop
  if (clockwise)
  {
    direction = 'c'; // Circular motion clockwise
  }
  else if (counterclockwise)
  {
    direction = 'w'; // Circular motion counterclockwise
  }
  else if (yValue > 50 && abs(xValue) < 30)
  {
    direction = 'f'; // Forward
  }
  else if (yValue < -50 && abs(xValue) < 30)
  {
    direction = 'b'; // Backward
  }
  else if (xValue > 50 && abs(yValue) < 30)
  {
    direction = 'h'; // Strafe Right (horizontal)
  }
  else if (xValue < -50 && abs(yValue) < 30)
  {
    direction = 'g'; // Strafe Left (horizontal)
  }
  else if (yValue > 50 && xValue > 50)
  {
    direction = 'r'; // Turn Right Forward
  }
  else if (yValue > 50 && xValue < -50)
  {
    direction = 'l'; // Turn Left Forward
  }
  else if (yValue < -50 && xValue > 50)
  {
    direction = 'u'; // Turn Right Backward
  }
  else if (yValue < -50 && xValue < -50)
  {
    direction = 'v'; // Turn Left Backward
  }
  else if (diagonalRightForward)
  {
    direction = 'i'; // Diagonal Right Forward
  }
  else if (diagonalLeftForward)
  {
    direction = 'j'; // Diagonal Left Forward
  }
  else if (diagonalRightBackward)
  {
    direction = 'k'; // Diagonal Right Backward
  }
  else if (diagonalLeftBackward)
  {
    direction = 'm'; // Diagonal Left Backward
  }
  Control_Motors(direction);
}

void onCameraWebSocketEvent(AsyncWebSocket *server,
                            AsyncWebSocketClient *client,
                            AwsEventType type,
                            void *arg,
                            uint8_t *data,
                            size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    cameraClientId = client->id();
    break;
  case WS_EVT_DISCONNECT:
    cameraClientId = 0;
    break;
  case WS_EVT_DATA:
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  default:
    break;
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    if (server == &wsCamera)
      cameraClientId = client->id();
    break;
  case WS_EVT_DISCONNECT:
    if (server == &wsCamera)
      cameraClientId = 0;
    break;
  case WS_EVT_DATA:
    if (server == &ws)
      handleJoystickData(data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
  default:
    break;
  }
}

void setupServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html"); });

  ws.onEvent(onEvent);
  server.addHandler(&ws);
  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  server.begin();
}