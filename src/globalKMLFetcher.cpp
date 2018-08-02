// File:  globalKMLFetcher.cpp
// Date:  3/8/2018
// Auth:  K. Loux
// Desc:  Interface to Global Administrative Areas web site, specifically
//        .kmz file download section.

// Local headers
#include "globalKMLFetcher.h"
#include "email/curlUtilities.h"

// Standard C++ headers
#include <cassert>

const String GlobalKMLFetcher::userAgent(_T("eBirdDataProcessor"));
const String GlobalKMLFetcher::gadmCountryURL(_T("https://gadm.org/download_country_v3.html"));
const String GlobalKMLFetcher::gadmDownloadBaseURL(_T("https://biogeo.ucdavis.edu/data/gadm3.6/"));
const bool GlobalKMLFetcher::verbose(false);

using namespace std::chrono_literals;
// crawl delay determined by manually visiting www.gadm.org/robots.txt - should periodically
// check this to make sure we comply, or we should include a robots.txt parser here to automatically update
const ThrottledSection::Clock::duration GlobalKMLFetcher::gadmCrawlDelay(std::chrono::steady_clock::duration(10s));

GlobalKMLFetcher::GlobalKMLFetcher(std::basic_ostream<String::value_type>& log) : log(log), rateLimiter(gadmCrawlDelay)
{
	DoGeneralCurlConfiguration();
}

GlobalKMLFetcher::~GlobalKMLFetcher()
{
	if (headerList)
		curl_slist_free_all(headerList);

	if (curl)
		curl_easy_cleanup(curl);
}

bool GlobalKMLFetcher::FetchKML(const String& country, const DetailLevel& level, std::string& zippedFileContents)
{
	String html;
	if (!GetCountryListPage(html))
		return false;

	auto countryCodeMap(ExtractCountryCodeMap(html));
	auto it(countryCodeMap.find(country));
	if (it == countryCodeMap.end())
	{
		log << "Failed to find match for '" << country << "' in available KML library" << std::endl;
		return false;
	}

	const String downloadURL(BuildDownloadURL(it->second, level));
	if (!DoCURLGet(downloadURL, zippedFileContents))
		return false;

	return true;
}

bool GlobalKMLFetcher::GetCountryListPage(String& html)
{
	std::string response;
	if (!DoCURLGet(gadmCountryURL, response))
		return false;

	html = UString::ToStringType(response);
	return true;
}

String GlobalKMLFetcher::BuildRequestString(const String& countryCode) const
{
	OStringStream ss;
	ss << "cnt=" << countryCode
		<< "&thm=kmz%23Google+Earth+kmz&OK=OK&_submit_check=1";
	return ss.str();
}

std::map<String, String> GlobalKMLFetcher::ExtractCountryCodeMap(const String& html)
{
	const String listStartTag(_T("<select class=\"form-control\" id=\"countrySelect\", name=\"country\""));
	const String listEndTag(_T("</select>"));
	auto listStart(html.find(listStartTag));
	auto listEnd(html.find(listEndTag));
	if (listStart == std::string::npos || listEnd == std::string::npos)
		return std::map<String, String>();

	std::map<String, String> countryCodeMap;
	auto currentPosition(listStart);
	while (currentPosition < listEnd)
	{
		const String entryTagStart(_T("<option value=\""));
		const String entryTagMiddle(_T("\">"));
		const String entryTagEnd(_T("</option>"));

		const auto entryStart(html.find(entryTagStart, currentPosition));
		const auto entryMiddle(html.find(entryTagMiddle, entryStart));
		const auto entryEnd(html.find(entryTagEnd, entryMiddle));

		if (entryStart != std::string::npos &&
			entryMiddle != std::string::npos &&
			entryEnd != std::string::npos)
		{
			const String countryCode(html.substr(entryStart + entryTagStart.length(), entryMiddle - entryStart - entryTagStart.length()));
			const String countryName(html.substr(entryMiddle + entryTagMiddle.length(), entryEnd - entryMiddle - entryTagMiddle.length()));
			if (!countryCode.empty() && !countryName.empty())
				countryCodeMap[countryName] = countryCode;
		}

		currentPosition = entryEnd;
	}

	return countryCodeMap;
}

String GlobalKMLFetcher::BuildDownloadURL(const String& countryFile, const DetailLevel& level)
{
	OStringStream levelIndex;
	levelIndex << static_cast<unsigned int>(level);
	return gadmDownloadBaseURL + _T("kmz/gadm36_") + countryFile.substr(0, 3) + _T("_") + levelIndex.str() + _T(".kmz");
}

bool GlobalKMLFetcher::DoGeneralCurlConfiguration()
{
	if (!curl)
		curl = curl_easy_init();

	if (!curl)
	{
		log << "Failed to initialize CURL" << std::endl;
		return false;
	}

	if (verbose)
		CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L), _T("Failed to set verbose output"));// Don't fail for this one

	/*if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, UString::ToNarrowString(caCertificatePath).c_str());*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL), _T("Failed to enable SSL")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USERAGENT, UString::ToNarrowString(userAgent).c_str()), _T("Failed to set user agent")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L), _T("Failed to enable location following")))
		return false;

	headerList = curl_slist_append(headerList, "Connection: Keep-Alive");
	if (!headerList)
	{
		log << "Failed to append keep alive to header" << std::endl;
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	/*if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEFILE, UString::ToNarrowString(cookieFile).c_str()), _T("Failed to load the cookie file")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEJAR, UString::ToNarrowString(cookieFile).c_str()), _T("Failed to enable saving cookies")))
		return false;*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, GlobalKMLFetcher::CURLWriteCallback), _T("Failed to set the write callback")))
		return false;

	return true;
}

bool GlobalKMLFetcher::DoCURLGet(const String& url, std::string &response)
{
	assert(curl);

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response), _T("Failed to set write data")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 0L), _T("Failed to set action to GET")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_URL, UString::ToNarrowString(url).c_str()), _T("Failed to set URL")))
		return false;

	rateLimiter.Wait();
	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), _T("Failed issuing https GET")))
		return false;
	return true;
}

//==========================================================================
// Class:			GlobalKMLFetcher
// Function:		CURLWriteCallback
//
// Description:		Static member function for receiving returned data from cURL.
//
// Input Arguments:
//		ptr			= char*
//		size		= size_t indicating number of elements of size nmemb
//		nmemb		= size_t indicating size of each element
//		userData	= void* (must be pointer to String)
//
// Output Arguments:
//		None
//
// Return Value:
//		size_t indicating number of bytes read
//
//==========================================================================
size_t GlobalKMLFetcher::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	const size_t totalSize(size * nmemb);
	std::string& s(*static_cast<std::string*>(userData));
	s.append(ptr, totalSize);

	return totalSize;
}
