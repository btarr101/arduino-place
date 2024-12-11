#include <Arduino.h>
#include <WiFiS3.h>
#include <FastLED.h>

#include "globals.h"
#include "ws.h"

CRGB leds[LED_COUNT];
bool showLeds = false;

WsClient wsClient;

void safeBoot();
void setupWifi();
void setupLeds();
void setupWs();
// void wsEvent(WStype_t type, uint8_t *payload, size_t length);
void wsHandleSyncOne(uint8_t payload[4]);
void wsHandleSyncAll(uint8_t payload[3 * LED_COUNT]);

void setup()
{
  safeBoot();
  setupWifi();
  setupWs();
  setupLeds();
}

void loop()
{
  static bool sentConnected = false;

  WsClient::Payload payload = wsClient.loop();
  if (payload.type == WsClient::PayloadType::BINARY)
  {
    my_debug("[WSc] Payload:");
    for (int i = 0; i < payload.length; i++)
    {
      my_debug(' ');
      my_debug_byte(payload.bytes[i]);
    }
    my_debugln();

    if (payload.length == 3 * LED_COUNT)
    {
      wsHandleSyncAll(payload.bytes);
    }
    else if (payload.length == 4)
    {
      wsHandleSyncOne(payload.bytes);
    }
  }
  else if (!sentConnected && wsClient.connected())
  {
    wsClient.send("Connected");
    sentConnected = true;
  }

  if (showLeds)
  {
    FastLED.show();
    showLeds = false;
  }
}

void safeBoot()
{
  my_debug_begin(115200);
  my_debugln();
  my_debugln();
  my_debugln();

  for (uint8_t t = 4; t > 0; t--)
  {
    my_debug("[SETUP] BOOT WAIT (");
    my_debug(t);
    my_debugln(")");
    delay(1000);
  }
}

void setupWifi()
{
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    my_debugln("[Wifi]: Communication with WiFi module failed!");
    // don't continue
    for (;;)
      ;
  }

  String firmware_version = WiFi.firmwareVersion();
  if (firmware_version < WIFI_FIRMWARE_LATEST_VERSION)
  {
    my_debugln("[Wifi]: Please upgrade the firmware");
  }

  my_debugln("[Wifi]: Connecting");

  int status = WL_IDLE_STATUS;

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    my_debug("[Wifi]: Attempting to connect to SSID: ");
    my_debugln(WIFI_SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);

    delay(1000);
  }

  my_debugln("[Wifi]: Connected!");

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  my_debug("[Wifi]: IP Address: ");
  my_debugln(ip);
}

void setupWs()
{
  WsClient::ConnectResponse response = wsClient.connect(WS_HOST, WS_PORT, WS_URL);
  if (response == WsClient::ConnectResponse::FAILURE)
  {
    my_debugln("[WSc] Failed to initiate connecting!");
    for (;;)
      ;
  }
  my_debugln("[WSc] Started connecting...");
}

void setupLeds()
{
  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, LED_COUNT)
      .setCorrection(TypicalLEDStrip)
      .setRgbw();
  FastLED.setBrightness(LED_BRIGHTNESS);
}

void wsHandleSyncOne(uint8_t payload[4])
{
  size_t index = payload[0];
  uint8_t r = payload[1];
  uint8_t g = payload[2];
  uint8_t b = payload[3];

  my_debug("[Sync one]: ");
  my_debug("index:");
  my_debug(index);
  my_debug(", r:");
  my_debug(r);
  my_debug(", g:");
  my_debug(g);
  my_debug(", b:");
  my_debugln(b);

  leds[index] = CRGB(r, g, b);
  showLeds = true;
}

void wsHandleSyncAll(uint8_t payload[3 * LED_COUNT])
{
  for (size_t i = 0; i < LED_COUNT; ++i)
  {
    size_t red_offset = i * 3;
    leds[i] = CRGB(
        payload[red_offset],
        payload[red_offset + 1],
        payload[red_offset + 2]);
  }
  showLeds = true;
}
