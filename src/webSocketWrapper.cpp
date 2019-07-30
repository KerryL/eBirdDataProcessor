// File:  webSocketWrapper.cpp
// Date:  7/29/2019
// Auth:  K. Loux
// Desc:  Wrapper around 3rd party web socket class.

// Local headers
#include "webSocketWrapper.h"
#include "email/cJSON/cJSON.h"

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4477)
#pragma warning(disable:4267)
#pragma warning(disable:4100)
#pragma warning(disable:4244)
#pragma warning(disable:4456)
#endif// _WIN32

// easywsclient source
#include "../easywsclient/easywsclient.cpp"

#ifdef _WIN32
#pragma warning(pop)
#endif// _WIN32

// Standard C++ headers
#include <cassert>
#include <iostream>
#include <chrono>

WebSocketWrapper::~WebSocketWrapper()
{
	if (ws)
		ws->close();
}

bool WebSocketWrapper::Connect(const std::string& url)
{
	ws = std::unique_ptr<easywsclient::WebSocket>(easywsclient::WebSocket::from_url(url));
	if (!ws.get())
	{
		std::cerr << "Failed to connect web socket to '" << url << "'\n";
		return false;
	}

	return ws->getReadyState() == easywsclient::WebSocket::CONNECTING ||
		ws->getReadyState() == easywsclient::WebSocket::OPEN;
}

bool WebSocketWrapper::Send(const std::string& message)
{
	assert(ws);
	assert(ws->getReadyState() == easywsclient::WebSocket::OPEN);
	ws->send(message);
	ws->poll();
	return true;
}

bool WebSocketWrapper::GotResponse(std::queue<std::string>& responses, const int& targetID)
{
	while (!responses.empty())
	{
		if (GetID(responses.front()) == targetID)
			return true;
		responses.pop();
	}

	return false;
}

bool WebSocketWrapper::Send(const std::string& message, std::string& response)
{
	std::queue<std::string> responses;
	auto callback([&responses](const std::string& message)
	{
		responses.push(message);
	});

	ws->poll();
	ws->send(message);

	while (!GotResponse(responses, GetID(message)))// TODO:  Is this OK?  No risk of infinite loop?  Calling poll with a timeout of -1 doesn't seem to be a candidate - need to call poll several times to apparently flush some buffer?
	{
		ws->dispatch(callback);
		ws->poll();
		Sleep(100);// prevent busy wait
	}

	response = responses.front();
	return true;
}

int WebSocketWrapper::GetID(const std::string& json)
{
	cJSON* root(cJSON_Parse(json.c_str()));
	if (!root)
		return -1;

	cJSON* idItem(cJSON_GetObjectItem(root, "id"));
	if (!idItem)
		return -1;

	return idItem->valueint;
}

bool WebSocketWrapper::ListenFor(const unsigned int& timeoutMS, ContinueMethod checkMethod)
{
	const auto startTime(std::chrono::steady_clock::now());

	std::queue<std::string> responses;
	auto callback([&responses](const std::string& message)
	{
		responses.push(message);
	});

	while (std::chrono::steady_clock::now() < startTime + std::chrono::milliseconds(timeoutMS))
	{
		ws->dispatch(callback);
		ws->poll();

		while (!responses.empty())
		{
			if (checkMethod(responses.front()))
				return true;
			responses.pop();
		}

		Sleep(100);// prevent busy wait
	}

	return false;
}
