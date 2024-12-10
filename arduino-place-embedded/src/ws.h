#pragma once
#include <WiFiS3.h>

class WsClient
{
public:
	WsClient();

	enum Status
	{
		DISCONNECTED,
		CONNECTING,
		IDLE,
		RECEIVING
	};
	Status status();

	bool connected();

	enum ConnectResponse
	{
		STARTED_CONNECTING,
		FAILURE
	};
	ConnectResponse connect(const char *host, uint16_t port, const char *path = "/");

	enum SendResponse
	{
		SENT,
		NOT_CONNECTED
	};
	SendResponse send(const char *text);

	void loop();

private:
	static const size_t MAX_PAYLOAD_LENGTH = 300;

	bool _fin;
	byte _opcode;
	bool _mask;
	uint16_t _payloadLength;
	bool _maskingKeyLoaded;
	byte _maskingKey[4];
	byte _payload[MAX_PAYLOAD_LENGTH];

	Status _status;
	WiFiSSLClient _wifiClient;
};
