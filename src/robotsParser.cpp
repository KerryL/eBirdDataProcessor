// File:  robotsParser.cpp
// Date:  7/19/2019
// Auth:  K. Loux
// Desc:  Object for parsing robots.txt files.

// Local headers
#include "robotsParser.h"
#include "email/curlUtilities.h"

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <iostream>
#include <cassert>
#include <sstream>

const bool RobotsParser::verbose(false);
const std::string RobotsParser::robotsFileName("robots.txt");

// TODO:  This is realy basic - it assumes we're only interested in the crawl delay.  We should fix this...
RobotsParser::RobotsParser(const std::string& userAgent, const std::string& baseURL)
	: userAgent(userAgent), baseURL(baseURL)
{
	DoGeneralCurlConfiguration();
}

bool RobotsParser::RetrieveRobotsTxt()
{
	std::string fullURL(baseURL);
	if (fullURL.back() != '/')
		fullURL.append("/");

	if (!DoCURLGet(baseURL + robotsFileName, robotsTxt))
		return false;
	return true;
}

std::chrono::steady_clock::duration RobotsParser::GetCrawlDelay() const
{
	using namespace std::chrono_literals;
	std::chrono::steady_clock::duration crawlDelay = 0s;

	std::string line;
	std::istringstream ss(robotsTxt);
	bool theseRulesApply(false);
	while (std::getline(ss, line))
	{
		const std::string userAgentTag("User-agent:");
		const std::string crawlDelayTag("Crawl-delay:");

		if (line.empty())
			continue;
		else if (line.find(userAgentTag) != std::string::npos)
		{
			if (line.find(userAgent) != std::string::npos ||
				line.find("*") != std::string::npos)
				theseRulesApply = true;
			else
				theseRulesApply = false;
		}
		else if (theseRulesApply && line.find(crawlDelayTag) != std::string::npos)
		{
			const auto delay(ExtractDelayValue(line));
			if (delay > crawlDelay)
				crawlDelay = delay;
		}
	}

	return crawlDelay;
}

std::chrono::steady_clock::duration RobotsParser::ExtractDelayValue(const std::string& line)
{
	const auto colon(line.find(":"));
	if (colon == std::string::npos)
		return std::chrono::steady_clock::duration();

	std::istringstream ss(line.substr(colon + 1));
	unsigned int seconds;
	if ((ss >> seconds).fail())
		return std::chrono::steady_clock::duration();

	return std::chrono::seconds(seconds);
}

bool RobotsParser::DoGeneralCurlConfiguration()
{
	if (!curl)
		curl = curl_easy_init();

	if (!curl)
	{
		std::cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	if (verbose)
		CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L), _T("Failed to set verbose output"));// Don't fail for this one

	/*if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, caCertificatePath.c_str());*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL), _T("Failed to enable SSL")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str()), _T("Failed to set user agent")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L), _T("Failed to enable location following")))
		return false;

	headerList = curl_slist_append(headerList, "Connection: Keep-Alive");
	if (!headerList)
	{
		std::cerr << "Failed to append keep alive to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	/*if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str()), _T("Failed to load the cookie file")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookieFile.c_str()), _T("Failed to enable saving cookies")))
		return false;*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, RobotsParser::CURLWriteCallback), _T("Failed to set the write callback")))
		return false;

	return true;
}

bool RobotsParser::DoCURLGet(const std::string& url, std::string &response)
{
	assert(curl);

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response), _T("Failed to set write data")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 0L), _T("Failed to set action to GET")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()), _T("Failed to set URL")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), _T("Failed issuing https GET")))
		return false;
	return true;
}

//==========================================================================
// Class:			RobotsParser
// Function:		CURLWriteCallback
//
// Description:		Static member function for receiving returned data from cURL.
//
// Input Arguments:
//		ptr			= char*
//		size		= size_t indicating number of elements of size nmemb
//		nmemb		= size_t indicating size of each element
//		userData	= void* (must be pointer to std::string)
//
// Output Arguments:
//		None
//
// Return Value:
//		size_t indicating number of bytes read
//
//==========================================================================
size_t RobotsParser::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	const size_t totalSize(size * nmemb);
	std::string& s(*static_cast<std::string*>(userData));
	s.append(ptr, totalSize);

	return totalSize;
}
