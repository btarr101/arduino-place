#include <Arduino.h>
#include <Arduino_LED_Matrix.h>
#include <WiFiS3.h>
#include <WebSocketsClient.h>
#include <FastLED.h>

#define WS_HOST "arduino-place-server-lrpx.shuttle.app"
#define WS_PORT 443
#define WS_URL "/ws"

#define LED_TYPE WS2811
#define LED_PIN 5
#define LED_COLOR_ORDER RGB
#define LED_COUNT 100
#define LED_BRIGHTNESS 128

WebSocketsClient wsClient;
CRGB leds[LED_COUNT];
bool showLeds = false;
ArduinoLEDMatrix matrix;

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;
int status = WL_IDLE_STATUS;
char server[] = "arduino-place-server-lrpx.shuttle.app";
WiFiSSLClient client;

void safeBoot();
void setupWifi();
void setupLeds();
void setupWs();
void wsEvent(WStype_t type, uint8_t *payload, size_t length);
void wsHandleSyncOne(uint8_t payload[4]);
void wsHandleSyncAll(uint8_t payload[3 * LED_COUNT]);

void wsLoop()
{
  if (client.available() < 2)
  {
    return;
  }

  uint8_t header[2];
  client.read(header, sizeof(header));

  uint8_t finRsvOpcode = header[0];
  bool fin = (finRsvOpcode & 0x80) != 0;
  uint8_t opcode = finRsvOpcode & 0x0F;

  Serial.println("---------------");
  Serial.print("[WS] Header[0]: fin=");
  Serial.print(fin);
  Serial.print(" opcode=");
  Serial.println(opcode);

  uint8_t maskLength = header[1];
  bool mask = (maskLength & 0x80) != 0;
  uint16_t payloadLength = maskLength & 0x7F;

  Serial.print("[WS] Header[1]: mask=");
  Serial.print(mask);
  Serial.print(", payloadLength=");
  Serial.println(payloadLength);

  if (payloadLength == 126)
  {
    uint8_t extendedLength[2];
    while (client.available() < 2)
    {
      ; // Wait for enough
    }
    client.read(extendedLength, sizeof(extendedLength));
    payloadLength = (extendedLength[0] << 8) | extendedLength[1];
    Serial.print("[WS] extendedPayloadLength=");
    Serial.println(payloadLength);
  }

  uint8_t maskingKey[4];
  if (mask)
  {
    client.read(maskingKey, sizeof(maskingKey));
  }

  uint8_t payload[LED_COUNT * 3];
  client.read(payload, payloadLength);

  if (mask)
  {
    for (int i = 0; i < payloadLength; i++)
    {
      payload[i] ^= maskingKey[i % 4]; // XOR with the masking key
    }
  }

  // Step 9: Process the payload (for example, print it)
  Serial.print("[WS] Payload: ");
  for (int i = 0; i < payloadLength; i++)
  {
    Serial.print(payload[i], HEX);
  }
  Serial.println();
  Serial.println("---------------");
}

void setup()
{
  safeBoot();
  setupWifi();

  // TODO: organize this and make useable!
  if (client.connect(server, 443))
  {
    client.println("GET /ws HTTP/1.1");
    client.println("Host: arduino-place-server-lrpx.shuttle.app");
    client.println("Upgrade: websocket");
    client.println("Connection: Upgrade");
    client.println("Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==");
    client.println("Sec-WebSocket-Version: 13");
    client.println();
  }

  while (!client.available())
  {
    ; // wait for available data
  }

  for (;;)
  {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r")
      break;
  }

  const char *message = "Connected";
  size_t length = strlen(message);

  uint8_t frame[2 + 4 + 10];

  // FIN = 0b1
  // Opcode = 0b0001 (text frame)
  frame[0] = 0x81;

  // Mask = 1
  // Payload length <= 125 (not 2^7, > 125 signals extended length)
  //
  // Thus note: max length of payload is 125 bytes!
  frame[1] = 0x80;

  // Masking key
  frame[2] = random(0, 256);
  frame[3] = random(0, 256);
  frame[4] = random(0, 256);
  frame[5] = random(0, 256);

  // Payload
  for (size_t i = 0; i < length; ++i)
  {
    frame[6 + i] = message[i] ^ frame[2 + (i % 4)];
  }
  client.write(frame, 6 + length);

  setupLeds();
  matrix.begin();
}

void loop()
{
  wsLoop();
  // wsClient.loop();
  if (showLeds)
  {
    FastLED.show();
    showLeds = false;
  }
}

void safeBoot()
{
  Serial.begin(115200);
  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--)
  {
    Serial.print("[SETUP] BOOT WAIT (");
    Serial.print(t);
    Serial.println(")");
    Serial.flush();
    delay(1000);
  }
}

void setupWifi()
{
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("[Wifi]: Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  String firmware_version = WiFi.firmwareVersion();
  if (firmware_version < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("[Wifi]: Please upgrade the firmware");
  }

  Serial.println("[Wifi]: Connecting");

  int status = WL_IDLE_STATUS;

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("[Wifi]: Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);

    delay(1000);
  }

  Serial.println("[Wifi]: Connected!");

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("[Wifi]: IP Address: ");
  Serial.println(ip);
}

void setupWs()
{
  wsClient.onEvent(wsEvent);
  wsClient.setReconnectInterval(5000);
  wsClient.begin(WS_HOST, WS_PORT, WS_URL);

  delay(1000);
}

void setupLeds()
{
  FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, LED_COUNT)
      .setCorrection(TypicalLEDStrip)
      .setRgbw();
  FastLED.setBrightness(LED_BRIGHTNESS);
}

void wsEvent(WStype_t type, uint8_t *payload, size_t length)
{
  Serial.print("[WSc] recieved message type ");
  Serial.println(type);
  Serial.print("[WSc] message size: ");
  Serial.println(length);

  switch (type)
  {
  case WStype_CONNECTED:
    wsClient.sendTXT("Connected");
    break;
  case WStype_BIN:
    if (length == 3 * LED_COUNT)
    {
      wsHandleSyncAll(payload);
    }
    else if (length == 4)
    {
      wsHandleSyncOne(payload);
    }
    break;
  case WStype_DISCONNECTED:
  case WStype_TEXT:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

void wsHandleSyncOne(uint8_t payload[4])
{
  size_t index = payload[0];
  uint8_t r = payload[1];
  uint8_t g = payload[2];
  uint8_t b = payload[3];

  Serial.print("[Sync one]: ");
  Serial.print("index:");
  Serial.print(index);
  Serial.print(", r:");
  Serial.print(r);
  Serial.print(", g:");
  Serial.print(g);
  Serial.print(", b:");
  Serial.println(b);

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
