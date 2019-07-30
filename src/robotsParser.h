// File:  robotsParser.h
// Date:  7/19/2019
// Auth:  K. Loux
// Desc:  Object for parsing robots.txt files.

#ifndef ROBOTS_PARSER_H_
#define ROBOTS_PARSER_H_

// Standard C++ headers
#include <chrono>
#include <string>

// for cURL
typedef void CURL;

class RobotsParser
{
public:
	RobotsParser(const std::string& userAgent, const std::string& baseURL);
	bool RetrieveRobotsTxt();
	std::chrono::steady_clock::duration GetCrawlDelay() const;

private:
	const std::string& userAgent;
	const std::string& baseURL;

	std::string robotsTxt;

	static const bool verbose;
	static const std::string robotsFileName;

	CURL* curl = nullptr;
	struct curl_slist* headerList = nullptr;

	bool DoGeneralCurlConfiguration();
	bool DoCURLGet(const std::string& url, std::string &response);
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static std::chrono::steady_clock::duration ExtractDelayValue(const std::string& line);
};

#endif// ROBOTS_PARSER_H_
