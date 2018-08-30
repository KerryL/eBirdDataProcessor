// File:  globalKMLFetcher.h
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to Global Administrative Areas web site, specifically
//        .kmz file download section.

#ifndef GLOBAL_KML_FETCHER_H_

// Local headers
#include "throttledSection.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <map>
#include <ostream>

// for cURL
typedef void CURL;

class GlobalKMLFetcher
{
public:
	GlobalKMLFetcher(std::basic_ostream<UString::String::value_type>& log);
	~GlobalKMLFetcher();

	enum class DetailLevel
	{
		Country = 0,// Least detail (i.e. country outline)
		SubNational1,
		SubNational2// Most detail (i.e. county equivalent)
	};

	bool FetchKML(const UString::String& country, const DetailLevel& level, std::string& zippedFileContents);

private:
	static const UString::String userAgent;
	static const UString::String gadmCountryURL;
	static const UString::String gadmDownloadBaseURL;
	static const bool verbose;

	std::basic_ostream<UString::String::value_type>& log;

	static const ThrottledSection::Clock::duration gadmCrawlDelay;
	ThrottledSection rateLimiter;

	bool GetCountryListPage(UString::String& html);
	UString::String BuildRequestString(const UString::String& countryCode) const;

	static std::map<UString::String, UString::String> ExtractCountryCodeMap(const UString::String& html);// Key is country name, value is county code
	UString::String BuildDownloadURL(const UString::String& countryFile, const DetailLevel& level);

	CURL* curl = nullptr;
	struct curl_slist* headerList = nullptr;
	bool DoGeneralCurlConfiguration();
	bool DoCURLGet(const UString::String& url, std::string &response);
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);
};

#endif// GLOBAL_KML_FETCHER_H_
