// File:  globalKMLFetcher.h
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to Global Administrative Areas web site, specifically
//        .kmz file download section.

#ifndef GLOBAL_KML_FETCHER_H_

// Local headers
#include "utilities/uString.h"

// Standard C++ headers
#include <map>

// for cURL
typedef void CURL;

class GlobalKMLFetcher
{
public:
	GlobalKMLFetcher();
	~GlobalKMLFetcher();

	enum class DetailLevel
	{
		Country,// Least detail (i.e. country outline)
		SubNational1,
		SubNational2// Most detail (i.e. county equivalent)
	};

	bool FetchKML(const String& country, const DetailLevel& level, std::string& zippedFileContents);

private:
	static const String userAgent;
	static const String gadmCountryURL;
	static const String gadmDownloadPostURL;
	static const bool verbose;

	bool GetCountryListPage(String& html);
	String BuildRequestString(const String& countryCode) const;

	static std::map<String, String> ExtractCountryCodeMap(const String& html);// Key is country name, value is county code
	String ExtractDownloadURL(const String& html, const DetailLevel& level);

	CURL* curl = nullptr;
	struct curl_slist* headerList = nullptr;
	bool DoGeneralCurlConfiguration();
	bool DoCURLGet(const String& url, std::string &response);
	bool DoCURLPost(const String &url, const std::string &data, std::string &response) const;
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);
};

#endif// GLOBAL_KML_FETCHER_H_
