#include <base64.hpp>

#include "globals.h"
#include "ws.h"

#define WS_KEY_LENGTH 16
#define WS_KEY_MAX_LENGTH ((WS_KEY_LENGTH + 2) / 3 * 4)
#define WS_PING_INTERVAL 20000

WsClient::WsClient() : _status(Status::DISCONNECTED) {}

WsClient::Status WsClient::status() { return _status; }

bool WsClient::connected()
{
	switch (_status)
	{
	case DISCONNECTED:
	case CONNECTING:
		return false;
	case IDLE:
	case RECEIVING:
		return true;
	}
}

WsClient::ConnectResponse WsClient::connect(const char *host, uint16_t port, const char *path)
{
	_host = host;
	_port = port;
	_path = path;

	_wifiClient.stop();
	_status = Status::DISCONNECTED;

	if (!_wifiClient.connect(_host, _port))
	{
		return ConnectResponse::FAILURE;
	}

	unsigned char wsKey[WS_KEY_MAX_LENGTH + 1];
	{
		byte randomBytes[WS_KEY_LENGTH];
		for (size_t i = 0; i < WS_KEY_LENGTH; ++i)
		{
			randomBytes[i] = random(0, 256);
		}

		encode_base64(randomBytes, WS_KEY_LENGTH, wsKey);
	}

	_wifiClient.print("GET ");
	_wifiClient.print(_path);
	_wifiClient.println(" HTTP/1.1");

	_wifiClient.print("Host: ");
	_wifiClient.println(_host);

	_wifiClient.println("Upgrade: websocket");
	_wifiClient.println("Connection: Upgrade");
	_wifiClient.print("Sec-WebSocket-Key: ");
	_wifiClient.println((char *)wsKey);
	_wifiClient.println("Sec-WebSocket-Version: 13");
	_wifiClient.println();

	_status = Status::CONNECTING;

	return ConnectResponse::STARTED_CONNECTING;
}

enum Opcode
{
	CONTINUATION = 0x0,
	TEXT = 0x1,
	BINARY = 0x2,
	CLOSE = 0x8,
	PING = 0x9,
	PONG = 0xA
};

// TODO: WS protocol can handle length > 125 (and the max value for 8 bits, but not handling)
// Thus, for now do NOT pass length > 125 (126, 127 are no-no's)
WsClient::SendResponse WsClient::_sendRaw(byte opcode, const byte *bytes, uint8_t length)
{
	if (!connected())
	{
		return SendResponse::NOT_CONNECTED;
	}

	// 2 byte header + 4 byte masking key + payload (max payload size wo/ extension)
	//
	// TODO: If the length is > 125, we kinda just uh-oh. But not expecting anything w/
	// length > 125 to be sent, so ignoring this case for now.
	byte frame[2 + 4 + 125];

	// FIN + Opcode
	frame[0] = (byte)0x80 | opcode;

	// Mask = 1
	frame[1] = (byte)0x80 | (byte)length;

	// Masking key
	frame[2] = random(0, 256);
	frame[3] = random(0, 256);
	frame[4] = random(0, 256);
	frame[5] = random(0, 256);

	//  Mask the payload
	for (uint8_t i = 0; i < length; ++i)
	{
		frame[6 + i] = bytes[i] ^ frame[2 + (i % 4)];
	}

	// 6 bc/ 1 byte FIN + OPCODE, 1 byte mask and length, 4 bytes masking key
	_wifiClient.write(frame, 6 + length);

	return SendResponse::SENT;
}

WsClient::SendResponse WsClient::send(const char *text)
{

	uint8_t length = strlen(text) + 1;
	return _sendRaw((byte)Opcode::TEXT, (const byte *)text, length);
}

WsClient::SendResponse WsClient::ping()
{
	return _sendRaw((byte)Opcode::PING, nullptr, 0);
}

// TODO: Add PING logic
WsClient::Payload WsClient::loop()
{
	Payload payload = {
		type : PayloadType::NONE,
		bytes : nullptr,
		length : 0
	};

	if (!_wifiClient.connected())
	{
		my_debug("[WS] Disconnected, attempting reconnect");
		connect(_host, _port, _path);
	}

	switch (_status)
	{
	case Status::CONNECTING:
	{
		size_t read = _wifiClient.readBytesUntil('\n', _payload, sizeof(_payload));
		_payload[read] = '\0';
		my_debug("[WS] upgrade payload line: ");
		my_debugln((char *)_payload);
		if (strcmp((char *)_payload, "\r") != 0)
			break;

		my_debugln("[WS] Connected! Changing status to IDLE");
		_status = Status::IDLE;
	}
	case Status::IDLE:
	{
		unsigned long currentMillis = millis();
		if (currentMillis - _lastPing > WS_PING_INTERVAL)
		{
			ping();
			my_debug("[WS] Sent ping @ ");
			my_debugln(currentMillis);
			_lastPing = currentMillis;
		}

		if (_wifiClient.available() < 2)
			break;

		{
			byte header[2];
			_wifiClient.read(header, 2);

			byte finRsvOpcode = header[0];
			_fin = (finRsvOpcode & 0x80) != 0;
			_opcode = finRsvOpcode & 0x0F;

			byte maskAndLength = header[1];
			_mask = (maskAndLength & 0x80) != 0;
			_payloadLength = maskAndLength & 0x7F;
		}

		_maskingKeyLoaded = false;

		my_debugln();
		my_debugln("[WS] -- Received Payload Header --");

		my_debug("[WS] Header[0]: fin=");
		my_debug(_fin);
		my_debug(" opcode=");
		my_debugln(_opcode);

		my_debug("[WS] Header[1]: mask=");
		my_debug(_mask);
		my_debug(", payloadLength=");
		my_debugln(_payloadLength);
		my_debugln();

		_status = Status::RECEIVING;
	}
	case Status::RECEIVING:
		// Handle extended length
		//
		// TODO: This should really only be done depending on the specific opcode,
		// as specified by https://datatracker.ietf.org/doc/html/rfc6455#section-5.5:
		//
		// """
		// All control frames MUST have a payload length of 125 bytes or less
		// and MUST NOT be fragmented.
		// """
		//
		if (_payloadLength == 126)
		{
			if (_wifiClient.available() < 2)
				break;
			{
				byte extendedLength[2];
				_wifiClient.read(extendedLength, sizeof(extendedLength));
				_payloadLength = (extendedLength[0] << 8) | extendedLength[1];
			}

			my_debug("[WS] extendedPayloadLength=");
			my_debugln(_payloadLength);
		}

		// Read masking key
		if (_mask && !_maskingKeyLoaded)
		{
			if (_wifiClient.available() < 4)
				break;

			_wifiClient.read(_maskingKey, 4);
			_maskingKeyLoaded = true;

			my_debug("[WS] read masking key");
		}

		// Read payload
		if (_wifiClient.available() < _payloadLength)
		{
			my_debug(_wifiClient.available());
			break;
		}

		_wifiClient.read(_payload, _payloadLength);

		if (_mask)
		{
			for (int i = 0; i < _payloadLength; i++)
			{
				_payload[i] ^= _maskingKey[i % 4];
			}
		}

		_status = Status::IDLE;

		switch ((Opcode)_opcode)
		{
		case Opcode::BINARY:
			payload.type = PayloadType::BINARY;
			break;
		case Opcode::PONG:
			payload.type = PayloadType::PONG;
			break;
		default:
			payload.type = PayloadType::UNKOWN;
		}

		payload.bytes = _payload;
		payload.length = _payloadLength;

		break;

	case Status::DISCONNECTED:
		break;
	}

	return payload;
}
