#pragma once

#define WS_HOST "arduino-place-server-lrpx.shuttle.app"
#define WS_PORT 443
#define WS_URL "/ws"

#define LED_TYPE WS2811
#define LED_PIN 4
#define LED_COLOR_ORDER RGB
#define LED_COUNT 250
#define LED_BRIGHTNESS 64

#define DEBUG
#ifdef DEBUG
#define my_debug_begin(x) \
	Serial.begin(x);      \
	while (!Serial)       \
		;
#define my_debug(x) Serial.print(x)
#define my_debugln(x) Serial.println(x)
#define my_debug_byte(x)   \
	if (x < 0x10)          \
		Serial.print('0'); \
	Serial.print(x, HEX)
#else
#define my_debug_begin(x)
#define my_debug(x)
#define my_debugln(x)
#define my_debug_byte(x)
#endif