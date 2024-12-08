#include <Arduino.h>
#include <Arduino_LED_Matrix.h>
#include <WiFiS3.h>
#include <WebSocketsClient.h>

WebSocketsClient webSocketsClient;
WiFiClient client;

#define WIFI_SSID "Wouldn't you"
#define WIFI_PASS "like to know"

void setupWifi();
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.println("[WSc] Disconnected!");
    break;
  case WStype_CONNECTED:
    Serial.println("[WSc] Connected!");

    // send message to server when Connected
    webSocketsClient.sendTXT("Connected");
    break;
  case WStype_TEXT:
    Serial.print("[WSc] get text:");
    Serial.println((char *)payload);
    break;
  case WStype_BIN:
    Serial.println("[WSc] get bin:");
    Serial.print("[");
    for (size_t i = 0; i < length; i++)
    {
      if (i > 0)
      {
        Serial.print(" ");
      }
      Serial.print(payload[i], HEX);
    }
    Serial.println("]");
    break;
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

ArduinoLEDMatrix matrix;

void setup()
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

  setupWifi();

  // event handler
  webSocketsClient.onEvent(webSocketEvent);
  // try ever 5000 again if connection has failed
  webSocketsClient.setReconnectInterval(5000);
  webSocketsClient.begin("192.168.1.47", 8010, "/ws");
  // webSocketsClient.begin("ws.ifelse.io", 80);
  // webSocketsClient.begin("demo.piesocket.com", 80, "/v3/channel_123?api_key=VCXCEuvhGcBDP7XhiJJUDvR1e1D3eiVjgZ9VRiaV&notify_self");
  delay(1000);

  // webSocketsClient.enableHeartbeat(10000, 1000, 5);

  pinMode(LED_BUILTIN, OUTPUT);

  matrix.begin();
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

/* just wrap the received data up to 80 columns in the serial print*/
/* -------------------------------------------------------------------------- */
void read_response()
{
  /* -------------------------------------------------------------------------- */
  uint32_t received_data_num = 0;
  while (client.available())
  {
    /* actual data reception */
    char c = client.read();
    /* print data to serial port */
    Serial.print(c);
    /* wrap data to 80 columns*/
    received_data_num++;
    if (received_data_num % 80 == 0)
    {
      Serial.println();
    }
  }
}

const uint32_t happy[] = {
    0x19819,
    0x80000001,
    0x81f8000};

const uint32_t heart[] = {
    0x3184a444,
    0x44042081,
    0x100a0040};

void loop()
{
  static unsigned long last_ping = 0;
  // put your main code here, to run repeatedly:
  // matrix.loadFrame(happy);
  // delay(500);
  // matrix.loadFrame(heart);
  // delay(500);

  // read_response();

  // // if the server's disconnected, stop the client:
  // if (!client.connected())
  // {
  //   Serial.println();
  //   Serial.println("disconnecting from server.");
  //   client.stop();

  //   // do nothing forevermore:
  //   while (true)
  //     ;
  // }

  webSocketsClient.loop();
  unsigned long current = millis();
  if (webSocketsClient.isConnected() && (current - last_ping) > 5000)
  {
    webSocketsClient.sendTXT("Hello there kenobi!!!");
    last_ping = current;
  }
  // Serial.print("[WSc] Connected: ");
  // Serial.println(webSocketsClient.isConnected());
  // ;
}
