// File:  webSocketWrapper.h
// Date:  7/29/2019
// Auth:  K. Loux
// Desc:  Wrapper around 3rd party web socket class.

#ifndef WEB_SOCKET_WRAPPER_H_
#define WEB_SOCKET_WRAPPER_H_

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4477)
#pragma warning(disable:4267)
#pragma warning(disable:4100)
#pragma warning(disable:4244)
#pragma warning(disable:4456)
#endif// _WIN32

// easywsclient headers
#include "../easywsclient/easywsclient.hpp"

#ifdef _WIN32
#pragma warning(pop)
#endif// _WIN32

// Standard C++ headers
#include <memory>
#include <queue>

class WebSocketWrapper
{
public:
	~WebSocketWrapper();

	bool Connect(const std::string& url);
	bool Send(const std::string& message);
	bool Send(const std::string& message, std::string& response);

private:
	std::unique_ptr<easywsclient::WebSocket> ws;

	static int GetID(const std::string& json);
	static bool GotResponse(std::queue<std::string>& responses, const int& targetID);
};

#endif// WEB_SOCKET_WRAPPER_H_
