// File:  mediaHTMLExtractor.cpp
// Date:  7/18/2019
// Auth:  K. Loux
// Desc:  Object for interfacing with ebird.org web site, navigating to
//        user's media profile and extracting the relevant portion of the HTML.

// Local headers
#include "mediaHTMLExtractor.h"
#include "robotsParser.h"
#include "webSocketWrapper.h"

// OS headers
#ifdef _WIN32
#define _WINSOCKAPI_
#include <Windows.h>
#else// Assume *nix
#include <unistd.h>
#include <termios.h>
#endif

// Standard C++ headers
#include <iostream>
#include <cassert>
#include <stack>
#include <numeric>

const bool MediaHTMLExtractor::verbose(false);
const UString::String MediaHTMLExtractor::userAgent(_T("eBirdDataProcessor"));
const std::string MediaHTMLExtractor::eBirdLoginURL("https://secure.birds.cornell.edu/cassso/login?service=https://ebird.org/ebird/login/cas?portal=ebird&locale=en_US");
const std::string MediaHTMLExtractor::eBirdProfileURL("https://ebird.org/profile");
const std::string MediaHTMLExtractor::baseURL("https://ebird.org/");
const unsigned short remoteDebuggingPort(9222);

MediaHTMLExtractor::MediaHTMLExtractor() : JSONInterface(userAgent), rateLimiter(GetCrawlDelay())
{
	LaunchBrowser();
}

bool MediaHTMLExtractor::LaunchBrowser()
{
	// TODO:  Do something better with this (don't hardcode path)
	std::ostringstream ss;
	ss << remoteDebuggingPort;
	const std::string command("\"\"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe\"\" --headless --disable-gpu --remote-debugging-port=" + ss.str());
	return browserPipe.Launch(command);
}

bool MediaHTMLExtractor::ExtractMediaHTML(const UString::String& htmlFileName)
{
	std::string mediaListHTML;
	if (!GetMediaListHTML(mediaListHTML))
		return false;

	const std::string resultsListTag("<div class=\"ResultsList js-ResultsContainer\">");
	std::string resultsList;
	if (!ExtractTextContainedInTag(mediaListHTML, resultsListTag, resultsList))
		return false;

	std::ofstream htmlFile(htmlFileName);
	if (!htmlFile.is_open() || !htmlFile.good())
	{
		Cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}

	htmlFile << resultsList;

	return true;
}

bool MediaHTMLExtractor::ExtractTextContainedInTag(const std::string& htmlData,
	const std::string& startTag, std::string& text)
{
	text.clear();
	const auto startLocation(htmlData.find(startTag));
	if (startLocation == std::string::npos)
	{
		std::cerr << "Failed to find tag '" << startTag << "' in page\n";
		return false;
	}

	const auto endOfPureTag(htmlData.find(" ", startLocation));
	if (endOfPureTag == std::string::npos)
	{
		std::cerr << "Failed to determine tag string\n";
		return false;
	}

	const std::string trimmedStartTag(htmlData.substr(startLocation, endOfPureTag - startLocation));
	const std::string endTag(trimmedStartTag.front() + "/" + trimmedStartTag.substr(1));
	std::stack<std::string::size_type> tagStartLocations;
	std::string::size_type position(startLocation + 1);
	while (position < htmlData.length())
	{
		const auto nextStartPosition(htmlData.find(trimmedStartTag, position));
		const auto nextEndPosition(htmlData.find(trimmedStartTag, position));

		if (nextStartPosition == std::string::npos ||
			nextEndPosition == std::string::npos)
		{
			std::cerr << "Failed to find next tag set\n";
			return false;
		}

		if (nextStartPosition < nextEndPosition)
			tagStartLocations.push(nextStartPosition);
		else if (tagStartLocations.empty())
		{
			text = htmlData.substr(startLocation, nextEndPosition + endTag.length() - startLocation);
			break;
		}
		else
			tagStartLocations.pop();
	}

	if (text.empty())
	{
		std::cerr << "Failed to find matching tag\n";
		return false;
	}

	return true;
}

std::string MediaHTMLExtractor::BuildNavigateCommand(const std::string& url)
{
	std::ostringstream ss;
	ss << "{\"method\":\"Page.navigate\",\"id\":" << commandId++ << ",\"params\":{\"url\":\"" << url << "\"}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildGetHTMLCommand()
{
	/*std::ostringstream ss;
	ss << "{\"method\":\"DOM.getOuterHTML\",\"id\":" << commandId++ << ",\"params\":{\"nodeId\":" << nodeID << "}}";
	return ss.str();*/// This method only returns the same HTML we can get with libcURL
	std::ostringstream ss;
	ss << "{\"method\":\"Runtime.evaluate\",\"id\":" << commandId++ << ",\"params\":{\"expression\":\"document.documentElement.outerHTML\"}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildGetDocumentNodeCommand()
{
	std::ostringstream ss;
	ss << "{\"method\":\"DOM.getDocument\",\"id\":" << commandId++ << ",\"params\":{}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildEnableDOMCommand()
{
	std::ostringstream ss;
	ss << "{\"method\":\"DOM.enable\",\"id\":" << commandId++ << ",\"params\":{}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildGetFullDocumentNodeCommand()
{
	std::ostringstream ss;
	ss << "{\"method\":\"DOM.getFlattenedDocument\",\"id\":" << commandId++ << ",\"params\":{\"depth\":-1}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildKeyInputCommand(const char& c, const std::string& type, const std::string& payloadFieldName)
{
	std::ostringstream ss;
	ss << "{\"method\":\"Input.dispatchKeyEvent\",\"id\":" << commandId++ << ",\"params\":{\"type\":\""
		<< type << "\",\"" << payloadFieldName << "\":\"" << c << "\"}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildSetFocusCommand(const int& nodeID)
{
	std::ostringstream ss;
	ss << "{\"method\":\"DOM.focus\",\"id\":" << commandId++ << ",\"params\":{\"nodeId\":" << nodeID << "}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildGetBoxCommand(const int& nodeID)
{
	std::ostringstream ss;
	ss << "{\"method\":\"DOM.getBoxModel\",\"id\":" << commandId++ << ",\"params\":{\"nodeId\":" << nodeID << "}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildMouseCommand(const int& x, const int& y, const std::string& action)
{
	std::ostringstream ss;
	ss << "{\"method\":\"Input.dispatchMouseEvent\",\"id\":" << commandId++
		<< ",\"params\":{\"x\":" << x << ",\"y\":" << y << ",\"type\":\"" << action << "\",\"button\":\"left\",\"clickCount\":1}}";
	return ss.str();
}

std::string MediaHTMLExtractor::BuildScrollIntoViewCommand(const std::string& criteria)
{
	std::ostringstream ss;
	ss << "{\"method\":\"Runtime.evaluate\",\"id\":" << commandId++ << ",\"params\":{\"expression\":\"document.querySelector('" << criteria << "').scrollIntoView()\"}}";
	return ss.str();
}

bool MediaHTMLExtractor::GetCurrentHTML(WebSocketWrapper& ws, std::string& html)
{
	if (!WaitForPageLoaded(ws))
		return false;

	html.clear();
	std::string jsonResponse;
	if (!ws.Send(BuildGetHTMLCommand(), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	std::string temp;
	if (!ExtractResult(jsonResponse, temp))
		return false;
	jsonResponse = temp;
	if (!ExtractResult(jsonResponse, temp))
		return false;
	
	cJSON* resultNode(cJSON_Parse(temp.c_str()));
	if (!resultNode)
	{
		std::cerr << "Failed to parse result node while getting current HTML\n";
		return false;
	}

	cJSON* valueNode(cJSON_GetObjectItem(resultNode, "value"));
	if (!valueNode)
	{
		std::cerr << "Failed to get value while getting current HTML\n";
		cJSON_Delete(resultNode);
		return false;
	}

	html = valueNode->valuestring;
	cJSON_Delete(resultNode);
	return true;
}

bool MediaHTMLExtractor::GetMediaListHTML(std::string& mediaListHTML)
{
	std::cout << "Beginning automatic retrieval of media list" << std::endl;

	std::string jsonResponse;
	UString::OStringStream ss;
	ss << remoteDebuggingPort;
	if (!DoCURLGet(_T("127.0.0.1:") + ss.str() + _T("/json"), jsonResponse))
		return false;

	UString::String webSocketDebuggerURL;
	if (!ExtractWebSocketURL(jsonResponse, webSocketDebuggerURL))
		return false;

	Cout << "Connecting web socket to " << webSocketDebuggerURL << std::endl;
	WebSocketWrapper ws;
	if (!ws.Connect(UString::ToNarrowString(webSocketDebuggerURL)))
	{
		std::cerr << "WebSocket failed to connect\n";
		return false;
	}

	Cout << "Logging in to eBird" << std::endl;
	if (!DoEBirdLogin(ws))
		return false;

	Cout << "Navigating to user profile" << std::endl;
	if (!ws.Send(BuildNavigateCommand(eBirdProfileURL), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	std::string mediaListURL;
	while (mediaListURL.empty())// Loop to allow the page to load
	{
		std::string profileHTML;
		if (!GetCurrentHTML(ws, profileHTML))
			return false;

		mediaListURL = FindMediaListURL(profileHTML);
		Sleep(100);
	}

	Cout << "Navigating to user media content" << std::endl;
	if (!ws.Send(BuildNavigateCommand(ModifyMediaListLink(mediaListURL)), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	if (!ClickViewMediaAsList(ws))
	{
		std::cerr << "Failed to view media items as list\n";
		return false;
	}

	if (!ShowAllMediaEntries(ws))
	{
		std::cerr << "Failed to show all media entires\n";
		return false;
	}

	Cout << "Retrieving full media page HTML" << std::endl;
	if (!GetCurrentHTML(ws, mediaListHTML))
		return false;

	return true;
}

bool MediaHTMLExtractor::ShowAllMediaEntries(WebSocketWrapper& ws)
{
	// Assume we have more media items than will be display on first render
	// This lets us wait for the page to load by looking for the "Show More" button,
	// then continue clicking the button until it disappears (so we know all media items are shown)
	int nodeID(-1);
	while (true)
	{
		Sleep(100);

		if (nodeID < 0)
		{
			cJSON* root(nullptr);
			cJSON* nodesArray(GetDOMNodesArray(ws, root));
			if (!nodesArray)
				return false;

			if (!GetElementNodeID(nodesArray, "BUTTON", AttributeVector(1, std::make_pair("id", "show_more")), nodeID))
			{
				cJSON_Delete(root);
				continue;
			}
			cJSON_Delete(root);
		}

		if (!ScrollIntoView(ws, "[id=show_more]"))
			break;

		int x, y;
		if (!GetCenterOfBox(ws, nodeID, x, y))
			break;

		if (!SimulateClick(ws, x, y))
			return false;

		if (!WaitForPageLoaded(ws))
			return false;

		std::string html;
		if (GetCurrentHTML(ws, html))
			Cout << "Current media count = " << CountMediaEntries(html) << std::endl;
	}

	return true;
}

bool MediaHTMLExtractor::ScrollIntoView(WebSocketWrapper& ws, const std::string& criteria)
{
	std::string jsonResponse;
	if (!ws.Send(BuildScrollIntoViewCommand(criteria), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	return true;
}

bool MediaHTMLExtractor::ClickViewMediaAsList(WebSocketWrapper& ws)
{
	int x, y;
	while (true)// Loop because apparently sometimes we fail to find the object after one pass?
	{
		cJSON* root(nullptr);
		cJSON* nodesArray(GetDOMNodesArray(ws, root));
		if (!nodesArray)
			return false;

		int nodeID;
		if (!GetElementNodeID(nodesArray, "use", AttributeVector(1, std::make_pair("xlink:href", "#Icon--list")), nodeID))
		{
			cJSON_Delete(root);
			continue;// This can happen if we execute too fast and the page isn't loaded yet
		}
		cJSON_Delete(root);

		if (GetCenterOfBox(ws, nodeID, x, y))
			break;
	}

	if (!SimulateClick(ws, x, y))
		return false;
	
	return true;
}

bool MediaHTMLExtractor::ResponseHasError(const std::string& response)
{
	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse response\n";
		return false;
	}

	cJSON* errorNode(cJSON_GetObjectItem(root, "error"));
	if (!errorNode)
	{
		cJSON_Delete(root);
		return false;// If there's no error node, then there's no error (so no message)
	}

	int errorNumber;
	if (!ReadJSON(errorNode, _T("code"), errorNumber))
	{
		std::cerr << "Failed to read error number\n";
		cJSON_Delete(root);
		return false;
	}

	UString::String errorMessage;
	if (!ReadJSON(errorNode, _T("message"), errorMessage))
	{
		std::cerr << "Failed to read error message\n";
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	Cout << "WS Response:  Error " << errorNumber << "; " << errorMessage << std::endl;

	return true;
}

bool MediaHTMLExtractor::ExtractWebSocketURL(const std::string& json, UString::String& webSocketURL)
{
	cJSON *root(cJSON_Parse(json.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (ExtractWebSocketURL())\n";
		Cerr << json.c_str() << '\n';
		return false;
	}

	cJSON* item(cJSON_GetArrayItem(root, 0));
	if (!item)
	{
		Cerr << "Failed to get array item from JSON array\n";
		Cerr << json.c_str() << '\n';
		cJSON_Delete(root);
		return false;
	}

	if (!ReadJSON(item, _T("webSocketDebuggerUrl"), webSocketURL))
	{
		Cerr << "Faield to read webSocketDebuggerUrl from JSON message\n";
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool MediaHTMLExtractor::ExtractNodeID(const std::string& json, int& nodeID)
{
	std::string result;
	if (!ExtractResult(json, result))
		return false;

	cJSON* root(cJSON_Parse(result.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse result\n";
		return false;
	}

	cJSON* rootResult(cJSON_GetObjectItem(root, "root"));
	if (!rootResult)
	{
		std::cerr << "Failed to get root result\n";
		cJSON_Delete(root);
		return false;
	}

	if (!ReadJSON(rootResult, _T("nodeId"), nodeID))
	{
		std::cerr << "Failed to parse node ID\n";
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool MediaHTMLExtractor::ExtractResult(const std::string& json, std::string& result)
{
	cJSON* root(cJSON_Parse(json.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse JSON response\n";
		return false;
	}

	cJSON* resultNode(cJSON_GetObjectItem(root, "result"));
	if (!resultNode)
	{
		std::cerr << "Failed to get result node\n";
		cJSON_Delete(root);
		return false;
	}

	result = cJSON_PrintUnformatted(resultNode);
	cJSON_Delete(root);
	return true;
}

std::string MediaHTMLExtractor::FindMediaListURL(const std::string& profileHTML)
{
	const std::string photoFeedDivisionString("UserProfile-photoFeed");
	const auto photoFeedDivisionStart(profileHTML.find(photoFeedDivisionString));
	if (photoFeedDivisionStart == std::string::npos)
	{
		std::cerr << "Failed to find media list link\n";
		return std::string();
	}

	{
		std::ofstream f("page.html");
		f << profileHTML;
	}

	const std::string beginingOfTargetLink("https://ebird.org/media/catalog?");
	const auto targetLinkStart(profileHTML.substr(photoFeedDivisionStart).find(beginingOfTargetLink));
	if (targetLinkStart == std::string::npos)
	{
		std::cerr << "Failed to extract media list link\n";
		return std::string();
	}

	const auto endOfLink(profileHTML.substr(photoFeedDivisionStart + targetLinkStart).find("\""));
	if (endOfLink == std::string::npos)
	{
		std::cerr << "Failed to find end of media list link\n";
		return std::string();
	}

	return profileHTML.substr(photoFeedDivisionStart + targetLinkStart, endOfLink/* - targetLinkStart*/);
}

std::string MediaHTMLExtractor::ModifyMediaListLink(const std::string& link)
{
	// Remove mediaType (so we get both photo and audio data)
	// Remove regionCode (so we get data for all regions)
	const std::string mediaTypeParameter("mediaType=");
	const std::string regionCodeParameter("regionCode=");
	std::string modifiedLink(CleanUpAmpersands(link));
	modifiedLink = RemoveParameter(modifiedLink, mediaTypeParameter);
	modifiedLink = RemoveParameter(modifiedLink, regionCodeParameter);

	return modifiedLink;
}

std::string MediaHTMLExtractor::CleanUpAmpersands(const std::string& link)
{
	std::string::size_type previousStart(0), start;
	std::string modifiedLink;
	const std::string ampersand("&amp;");
	while (start = link.find(ampersand, previousStart), start != std::string::npos)
	{
		modifiedLink += link.substr(previousStart, start - previousStart + 1);
		previousStart = start + ampersand.length();
	}

	modifiedLink += link.substr(previousStart);

	return modifiedLink;
}

std::string MediaHTMLExtractor::RemoveParameter(const std::string& link, const std::string& parameter)
{
	const auto start(link.find(parameter));
	std::string modifiedLink;

	if (start != std::string::npos)
	{
		const auto parameterEnd(link.find("&", start));
		modifiedLink = link.substr(0, start);
		if (parameterEnd != std::string::npos)
			modifiedLink += link.substr(parameterEnd + 1);
	}

	return modifiedLink;
}

bool MediaHTMLExtractor::SendKeyEvents(WebSocketWrapper& ws, const std::string& s,
	const std::string& type, const std::string& payloadFieldName)
{
	for (const auto& c : s)
	{
		std::string jsonResponse;
		if (!ws.Send(BuildKeyInputCommand(c, type, payloadFieldName), jsonResponse))
			return false;

		if (ResponseHasError(jsonResponse))
			return false;
	}

	return true;
}

bool MediaHTMLExtractor::SimulateRawKey(WebSocketWrapper& ws, const std::string& s)
{
	return SendKeyEvents(ws, s, "rawKeyDown", "keyIdentifier");
}

bool MediaHTMLExtractor::SimulateTextEntry(WebSocketWrapper& ws, const std::string& s)
{
	return SendKeyEvents(ws, s, "char", "text");
}

// root argument provided so caller can free associated memory
cJSON* MediaHTMLExtractor::GetDOMNodesArray(WebSocketWrapper& ws, cJSON*& root)
{
	std::string jsonResponse;
	if (!ws.Send(BuildGetFullDocumentNodeCommand(), jsonResponse))
		return nullptr;

	if (ResponseHasError(jsonResponse))
		return nullptr;

	std::string resultString;
	if (!ExtractResult(jsonResponse, resultString))
		return nullptr;

	root = cJSON_Parse(resultString.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse node array root\n";
		return nullptr;
	}

	cJSON* nodesArray(cJSON_GetObjectItem(root, "nodes"));
	if (!nodesArray)
	{
		std::cerr << "Failed to get nodes array\n";
		cJSON_Delete(root);
		return nullptr;
	}

	return nodesArray;
}

bool MediaHTMLExtractor::FocusOnElement(WebSocketWrapper& ws, cJSON* nodesArray, const std::string& nodeName, const AttributeVector& attributes)
{
	assert(attributes.size() > 0);// Otherwise we'll match every node

	int nodeID;
	if (!GetElementNodeID(nodesArray, nodeName, attributes, nodeID))
	{
		std::cerr << "Failed to find node ID\n";
		return false;
	}

	std::string jsonResponse;
	if (!ws.Send(BuildSetFocusCommand(nodeID), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	return true;
}

bool MediaHTMLExtractor::GetElementNodeID(cJSON* nodesArray, const std::string& nodeName, const AttributeVector& attributes, int& nodeID)
{
	nodeID = -1;
	const int nodeCount(cJSON_GetArraySize(nodesArray));
	for (int i = 0; i < nodeCount; ++i)
	{
		cJSON* node(cJSON_GetArrayItem(nodesArray, i));
		if (!node)
		{
			std::cerr << "Failed to get array item\n";
			return false;
		}

		UString::String currentNodeName;
		if (!ReadJSON(node, _T("nodeName"), currentNodeName))
			continue;

		if (UString::ToNarrowString(currentNodeName) == nodeName)
		{
			cJSON* attributeArray(cJSON_GetObjectItem(node, "attributes"));
			if (!attributeArray)
				continue;

			const int attributeCount(cJSON_GetArraySize(attributeArray));
			unsigned int matchCount(0);
			for (int j = 0; j < attributeCount; j += 2)
			{
				cJSON* attrNameNode(cJSON_GetArrayItem(attributeArray, j));
				cJSON* attrValueNode(cJSON_GetArrayItem(attributeArray, j + 1));
				if (!attrNameNode || !attrValueNode)
					continue;

				for (const auto& attr : attributes)
				{
					if (attr.first == std::string(attrNameNode->valuestring) &&
						attr.second == std::string(attrValueNode->valuestring))
						++matchCount;
				}

				if (matchCount == attributes.size())
				{
					if (!ReadJSON(node, _T("nodeId"), nodeID))
					{
						std::cerr << "Failed to read node ID\n";
						return false;
					}

					break;
				}
			}

			if (nodeID > 0)
				break;
		}
	}

	if (nodeID < 0)
	{
		std::cerr << "No matching node while getting node ID\n";
		return false;
	}

	return true;
}

bool MediaHTMLExtractor::GetCenterOfBox(WebSocketWrapper& ws, const int& nodeID, int& x, int& y)
{
	std::string jsonResponse;
	if (!ws.Send(BuildGetBoxCommand(nodeID), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	std::string result;
	if (!ExtractResult(jsonResponse, result))
		return false;

	cJSON* root(cJSON_Parse(result.c_str()));
	if (!root)
		return false;

	cJSON* model(cJSON_GetObjectItem(root, "model"));
	if (!model)
	{
		std::cerr << "Failed to get model object\n";
		cJSON_Delete(root);
		return false;
	}

	cJSON* contentQuad(cJSON_GetObjectItem(model, "content"));
	if (!contentQuad)
	{
		std::cerr << "Failed to get content object\n";
		cJSON_Delete(root);
		return false;
	}

	int arraySize(cJSON_GetArraySize(contentQuad));
	std::vector<int> xValues(arraySize / 2);
	std::vector<int> yValues(arraySize / 2);
	for (int i = 0; i < xValues.size(); ++i)
	{
		cJSON* xNode(cJSON_GetArrayItem(contentQuad, i * 2));
		cJSON* yNode(cJSON_GetArrayItem(contentQuad, i * 2 + 1));
		if (!xNode || !yNode)
		{
			std::cerr << "Failed to get x and y values\n";
			cJSON_Delete(root);
			return false;
		}

		xValues[i] = xNode->valueint;
		yValues[i] = yNode->valueint;
	}

	cJSON_Delete(root);

	x = std::accumulate(xValues.begin(), xValues.end(), 0) / static_cast<int>(xValues.size());
	y = std::accumulate(yValues.begin(), yValues.end(), 0) / static_cast<int>(yValues.size());

	return true;
}

bool MediaHTMLExtractor::SimulateClick(WebSocketWrapper& ws, const int& x, const int& y)
{
	if (!ws.Send(BuildMouseCommand(x, y, "mousePressed")))
		return false;

	if (!ws.Send(BuildMouseCommand(x, y, "mouseReleased")))
		return false;

	return true;
}

bool MediaHTMLExtractor::WaitForPageLoaded(WebSocketWrapper& ws)
{
	const int timeout(10000);// [ms]
	ws.ListenFor(timeout, [](const std::string& s)
	{
		cJSON* root(cJSON_Parse(s.c_str()));
		if (!root)
			return false;

		UString::String method;
		if (!ReadJSON(root, _T("method"), method))
			return false;

		//Cout << method << std::endl;
		return method == _T("DOM.documentUpdate") ||
			method == _T("DOM.pseudoElementAdded");
	});// Never return false?

	Sleep(1000);// Even though the above condition says we're loaded, add a bit of extra time here
	return true;
}

bool MediaHTMLExtractor::DoEBirdLogin(WebSocketWrapper& ws)
{
	std::string jsonResponse;
	if (!ws.Send(BuildNavigateCommand(eBirdLoginURL), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	if (!ws.Send(BuildEnableDOMCommand(), jsonResponse))
		return false;

	if (ResponseHasError(jsonResponse))
		return false;

	std::string loginPage;
	if (!GetCurrentHTML(ws, loginPage))
		return false;

	while (!EBirdLoginSuccessful(loginPage))
	{
		std::string eBirdUserName, eBirdPassword;
		GetUserNameAndPassword(eBirdUserName, eBirdPassword);
		if (eBirdUserName.empty() || eBirdPassword.empty())
			continue;

		cJSON* root(nullptr);
		cJSON* nodesArray(GetDOMNodesArray(ws, root));
		if (!nodesArray)
			return false;

		// When we go to the input page, user name box has focus, and inputs can be navigates with the tab key.
		// So until we figure out a better way, we can input:  <user name> <tab> <password> <tab> <tab> <enter>
		if (!FocusOnElement(ws, nodesArray, "INPUT", AttributeVector(1, std::make_pair("name", "username"))) ||
			!SimulateTextEntry(ws, eBirdUserName) ||
			!FocusOnElement(ws, nodesArray, "INPUT", AttributeVector(1, std::make_pair("name", "password"))) ||
			!SimulateTextEntry(ws, eBirdPassword))
		{
			cJSON_Delete(root);
			return false;
		}

		int nodeID;
		if (!GetElementNodeID(nodesArray, "INPUT", AttributeVector(1, std::make_pair("value", "Sign in")), nodeID))
		{
			cJSON_Delete(root);
			return false;
		}
		cJSON_Delete(root);

		int x, y;
		if (!GetCenterOfBox(ws, nodeID, x, y))
			return false;

		if (!SimulateClick(ws, x, y))
			return false;
		
		if (!GetCurrentHTML(ws, loginPage))
			return false;
	}

	return true;
}

void MediaHTMLExtractor::GetUserNameAndPassword(std::string& userName, std::string& password)
{
	std::cout << "Specify your eBird user name:  ";
	std::cin >> userName;
	std::cout << "Password:  ";

#ifdef _WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(hStdin, &mode);
	SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

	std::cin.ignore();
	std::getline(std::cin, password);

	SetConsoleMode(hStdin, mode);
#else
	termios oldt;
	tcgetattr(STDIN_FILENO, &oldt);
	termios newt = oldt;
	newt.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	std::cin.ignore();
	std::getline(std::cin, password);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

	std::cout << std::endl;
}

bool MediaHTMLExtractor::EBirdLoginSuccessful(const std::string& htmlData)
{
	const std::string startTag1("<li ><a href=\"/ebird/myebird\">");
	const std::string startTag2("<li class=\"selected\"><a href=\"/ebird/myebird\" title=\"My eBird\">");
	const std::string startTag3("<a href=\"https://secure.birds.cornell.edu/cassso/account/edit?service=https://ebird.org/MyEBird");
	const std::string endTag("</a>");
	std::string dummy;
	std::string::size_type offset1(0), offset2(0), offset3(0);
	return ExtractTextBetweenTags(htmlData, startTag1, endTag, dummy, offset1) ||
		ExtractTextBetweenTags(htmlData, startTag2, endTag, dummy, offset2) ||
		ExtractTextBetweenTags(htmlData, startTag3, endTag, dummy, offset3);
}

std::string MediaHTMLExtractor::ExtractTokenFromLoginPage(const std::string& htmlData)
{
	const std::string tokenTagStart("<input type=\"hidden\" name=\"lt\" value=\"");
	const std::string tokenTagEnd("\" />");

	std::string::size_type offset(0);
	std::string token;
	if (!ExtractTextBetweenTags(htmlData, tokenTagStart, tokenTagEnd, token, offset))
		return std::string();

	return token;
}

bool MediaHTMLExtractor::ExtractTextBetweenTags(const std::string& htmlData, const std::string& startTag,
	const std::string& endTag, std::string& text, std::string::size_type& offset)
{
	std::string::size_type startPosition(htmlData.find(startTag, offset));
	if (startPosition == std::string::npos)
		return false;

	std::string::size_type endPosition(htmlData.find(endTag, startPosition + startTag.length()));
	if (endPosition == std::string::npos)
		return false;

	offset = endPosition + endTag.length();
	text = htmlData.substr(startPosition + startTag.length(), endPosition - startPosition - startTag.length());
	return true;
}

std::chrono::steady_clock::duration MediaHTMLExtractor::GetCrawlDelay()
{
	RobotsParser parser(UString::ToNarrowString(userAgent), baseURL);
	if (!parser.RetrieveRobotsTxt())
		return std::chrono::steady_clock::duration();
	return parser.GetCrawlDelay();
}

unsigned int MediaHTMLExtractor::CountMediaEntries(const std::string& html)
{
	unsigned int count(0);
	const std::string searchKey("<div class=\"ResultsList-cell\">");
	std::string::size_type position(0);
	while (position = html.find(searchKey, position + 1), position != std::string::npos)
		++count;
	return count;
}
