// File:  mediaHTMLExtractor.h
// Date:  7/18/2019
// Auth:  K. Loux
// Desc:  Object for interfacing with ebird.org web site, navigating to
//        user's media profile and extracting the relevant portion of the HTML.

#ifndef MEDIA_HTML_EXTRACTOR_H_
#define MEDIA_HTML_EXTRACTOR_H_

// Local headers
#include "email/jsonInterface.h"
#include "throttledSection.h"
#include "processPipe.h"

// Local forward declarations
class WebSocketWrapper;

class MediaHTMLExtractor : JSONInterface
{
public:
	MediaHTMLExtractor();
	~MediaHTMLExtractor() = default;

	bool ExtractMediaHTML(const UString::String& htmlFileName);

private:
	static const bool verbose;
	static const UString::String userAgent;
	static const std::string eBirdLoginURL;
	static const std::string eBirdProfileURL;
	static const std::string baseURL;

	ThrottledSection rateLimiter;

	ProcessPipe browserPipe;
	unsigned int commandId = 0;

	std::string BuildNavigateCommand(const std::string& url);
	std::string BuildEnableDOMCommand();
	std::string BuildGetDocumentNodeCommand();
	std::string BuildGetFullDocumentNodeCommand();
	std::string BuildGetHTMLCommand();
	std::string BuildKeyInputCommand(const char& c, const std::string& type, const std::string& payloadFieldName);
	std::string BuildSetFocusCommand(const int& nodeID);
	std::string BuildGetBoxCommand(const int& nodeID);
	std::string BuildScrollIntoViewCommand(const std::string& criteria);
	std::string BuildMouseCommand(const int& x, const int& y, const std::string& action);

	bool GetCurrentHTML(WebSocketWrapper& ws, std::string& html);
	bool SimulateTextEntry(WebSocketWrapper& ws, const std::string& s);
	bool SimulateRawKey(WebSocketWrapper& ws, const std::string& s);
	bool SendKeyEvents(WebSocketWrapper& ws, const std::string& s, const std::string& type, const std::string& payloadFieldName);
	typedef std::vector<std::pair<std::string, std::string>> AttributeVector;
	bool FocusOnElement(WebSocketWrapper& ws, cJSON* nodesArray, const std::string& nodeName, const AttributeVector& attributes);
	bool GetCenterOfBox(WebSocketWrapper& ws, const int& nodeID, int& x, int& y);
	bool SimulateClick(WebSocketWrapper& ws, const int& x, const int& y);
	bool WaitForPageLoaded(WebSocketWrapper& ws);

	bool GetElementNodeID(cJSON* nodesArray, const std::string& nodeName, const AttributeVector& attributes, int& nodeID);

	cJSON* GetDOMNodesArray(WebSocketWrapper& ws, cJSON*& root);

	bool LaunchBrowser();
	bool DoEBirdLogin(WebSocketWrapper& ws);
	bool ClickViewMediaAsList(WebSocketWrapper& ws);
	bool ShowAllMediaEntries(WebSocketWrapper& ws);
	bool ScrollIntoView(WebSocketWrapper& ws, const std::string& criteria);

	static bool EBirdLoginSuccessful(const std::string& htmlData);
	static void GetUserNameAndPassword(std::string& userName, std::string& password);

	static bool ExtractTextBetweenTags(const std::string& htmlData, const std::string& startTag,
		const std::string& endTag, std::string& text, std::string::size_type& offset);
	static std::string ExtractTokenFromLoginPage(const std::string& htmlData);

	static std::string CleanUpAmpersands(const std::string& link);
	static std::string ModifyMediaListLink(const std::string& link);
	static std::string RemoveParameter(const std::string& link, const std::string& parameter);
	static bool ExtractTextContainedInTag(const std::string& htmlData,
		const std::string& startTag, std::string& text);

	bool GetMediaListHTML(std::string& mediaListHTML);
	static std::string FindMediaListURL(const std::string& profileHTML);

	static bool ExtractWebSocketURL(const std::string& json, UString::String& webSocketURL);
	static bool ExtractNodeID(const std::string& json, int& nodeID);
	static bool ExtractResult(const std::string& json, std::string& result);
	static unsigned int CountMediaEntries(const std::string& html);

	static std::chrono::steady_clock::duration GetCrawlDelay();

	static bool ResponseHasError(const std::string& response);
};

#endif// MEDIA_HTML_EXTRACTOR_H_
