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
const String GlobalKMLFetcher::gadmCountryURL(_T("http://www.gadm.org/country"));
const String GlobalKMLFetcher::gadmDownloadPostURL(_T("http://www.gadm.org/download"));

GlobalKMLFetcher::GlobalKMLFetcher()
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
		Cerr << "Failed to find match for '" << country << "' in available KML library\n";
		return false;
	}

	std::string response;
	if (!DoCURLPost(gadmDownloadPostURL, UString::ToNarrowString(BuildRequestString(it->second)), response))
		return false;

	const String downloadURL(ExtractDownloadURL(UString::ToStringType(response), level));
	if (!DoCURLGet(downloadURL, zippedFileContents))
		return false;

	// TODO:  Consider adding KMLManager class to be able to find KML files in already downloaded file (i.e. lookup certain county, etc.) and download as needed

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
	const String listStartTag(_T("<select name=\"cnt\">"));
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
			countryCodeMap[countryName] = countryCode;
		}

		currentPosition = entryEnd;
	}

	return countryCodeMap;
}

String GlobalKMLFetcher::ExtractDownloadURL(const String& html, const DetailLevel& level)
{
	const String urlStartTag(_T("<a href="));
	const String urlEndTag([level]()
	{
		if (level == DetailLevel::Country)
			return _T(">level 0</a>");
		else if (level == DetailLevel::SubNational1)
			return _T(">level 1</a>");
		else// if (level == DetailLevel::SubNational2)
			return _T(">level 2</a>");
	}());

	const auto urlEnd(html.find(urlEndTag));
	if (urlEnd == std::string::npos)
		return String();

	const auto urlStart(html.rfind(urlStartTag, urlEnd));
	if (urlStart == std::string::npos)
		return String();

	return html.substr(urlStart + urlStartTag.length(), urlEnd - urlStart - urlStartTag.length());
}

bool GlobalKMLFetcher::DoGeneralCurlConfiguration()
{
	if (!curl)
		curl = curl_easy_init();

	if (!curl)
	{
		Cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	/*if (verbose)
		CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L), _T("Failed to set verbose output"));// Don't fail for this one*/

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
		Cerr << "Failed to append keep alive to header\n";
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

	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), _T("Failed issuing https GET")))
		return false;
	return true;
}

bool GlobalKMLFetcher::DoCURLPost(const String &url, const std::string &data, std::string &response) const
{
	assert(curl);

	response.clear();
	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response), _T("Failed to set write data")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 1L), _T("Failed to set action to POST")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str()), _T("Failed to assign post data")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data.length()), _T("Failed to assign data size")))
		return false;

	curl_easy_setopt(curl, CURLOPT_URL, UString::ToNarrowString(url).c_str());
	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), _T("Failed issuing https POST")))
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
