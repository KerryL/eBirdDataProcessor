// File:  frequencyDataHarvester.cpp
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

// Local headers
#include "frequencyDataHarvester.h"
#include "eBirdInterface.h"
#include "email/curlUtilities.h"

// OS headers
#ifdef _WIN32
#define _WINSOCKAPI_
#include <Windows.h>
#else// Assume *nix
#include <unistd.h>
#include <termios.h>
#endif

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <sstream>
#include <cassert>
#include <iostream>
#include <fstream>

const std::string FrequencyDataHarvester::targetSpeciesURLBase("http://ebird.org/ebird/targets");
const std::string FrequencyDataHarvester::userAgent("eBirdDataProcessor");
const std::string FrequencyDataHarvester::eBirdLoginURL("https://secure.birds.cornell.edu/cassso/login?service=https://ebird.org/ebird/login/cas?portal=ebird&locale=en");
const bool FrequencyDataHarvester::verbose(false);
const std::string FrequencyDataHarvester::cookieFile("ebdp.cookies");

FrequencyDataHarvester::FrequencyDataHarvester()
{
	DoGeneralCurlConfiguration();
}

FrequencyDataHarvester::~FrequencyDataHarvester()
{
	if (headerList)
		curl_slist_free_all(headerList);

	if (curl)
		curl_easy_cleanup(curl);
}

bool FrequencyDataHarvester::GenerateFrequencyFile(const std::string &country,
	const std::string &state, const std::string &county, const std::string &frequencyFileName)
{
	std::string loginPage;
	if (!DoCURLGet(eBirdLoginURL, loginPage))
		return false;

	while (!EBirdLoginSuccessful(loginPage))
	{
		/*if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL"), "Failed to clear existing cookies"))// erase all existing cookie data
			return false;*/

		std::string eBirdUserName, eBirdPassword;
		GetUserNameAndPassword(eBirdUserName, eBirdPassword);
		char* urlEncodedPassword(curl_easy_escape(curl, eBirdPassword.c_str(), eBirdPassword.length()));
		if (!urlEncodedPassword)
			std::cerr << "Failed to URL-encode password\n";
		eBirdPassword = urlEncodedPassword;
		curl_free(urlEncodedPassword);

		if (!PostEBirdLoginInfo(eBirdUserName, eBirdPassword, loginPage))
		{
			std::cerr << "Failed to login to eBird\n";
			return false;
		}
	}

	const std::string regionString(BuildRegionString(country, state, county));
	std::array<FrequencyData, 12> frequencyData;

	unsigned int month;
	for (month = 1; month <= 12; ++month)
	{
		std::string response;
		if (!DoCURLGet(BuildTargetSpeciesURL(regionString, month, month, ListTimeFrame::Day), response))
		{
			std::cerr << "Failed to read target species web page\n";
			return false;
		}

		if (!ExtractFrequencyData(response, frequencyData[month - 1]))
		{
			std::cerr << "Failed to parse HTML to extract frequency data\n";
			return false;
		}
	}

	if (!WriteFrequencyDataToFile(frequencyFileName, frequencyData))
		return false;

	return true;
}

void FrequencyDataHarvester::GetUserNameAndPassword(std::string& userName, std::string& password)
{
	std::cout << "NOTE:  In order for this routine to work properly, you must not have submitted any checklists for the current day in the specified region." << std::endl;

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

bool FrequencyDataHarvester::DoGeneralCurlConfiguration()
{
	if (!curl)
		curl = curl_easy_init();

	if (!curl)
	{
		std::cerr << "Failed to initialize CURL" << std::endl;
		return false;
	}

	if (verbose)
		CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L), "Failed to set verbose output");// Don't fail for this one

	/*if (!caCertificatePath.empty())
		curl_easy_setopt(curl, CURLOPT_CAPATH, caCertificatePath.c_str());*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL), "Failed to enable SSL"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str()), "Failed to set user agent"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L), "Failed to enable location following"))
		return false;

	headerList = curl_slist_append(headerList, "Connection: Keep-Alive");
	if (!headerList)
	{
		std::cerr << "Failed to append keep alive to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookieFile.c_str()), "Failed to load the cookie file"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookieFile.c_str()), "Failed to enable saving cookies"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FrequencyDataHarvester::CURLWriteCallback), "Failed to set the write callback"))
		return false;

	return true;
}

bool FrequencyDataHarvester::PostEBirdLoginInfo(const std::string& userName, const std::string& password, std::string& resultPage)
{
	assert(curl);

/*struct curl_slist *cookies = NULL;// TODO:  Remove
curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
std::cout << "=============COOKIES=========" << std::endl;
while(cookies) {
std::cout << cookies->data << std::endl;
        cookies = cookies->next;
      }
curl_slist_free_all(cookies);
std::cout << "=============END COOKIES=========" << std::endl;// TODO:  Remove*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resultPage), "Failed to set write data"))
		return false;

	std::string token(ExtractTokenFromLoginPage(resultPage));
	if (token.empty())
	{
		std::cerr << "Failed to get session token\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 1L), "Failed to set action to POST"))
		return false;

	const std::string loginInfo(BuildEBirdLoginInfo(userName, password, token));
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, loginInfo.c_str());

	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), "Failed issuding https POST (login)"))
		return false;

	return true;
}

bool FrequencyDataHarvester::EBirdLoginSuccessful(const std::string& htmlData)
{
	const std::string startTag1("<li ><a href=\"/ebird/myebird\">");
	const std::string startTag2("<li class=\"selected\"><a href=\"/ebird/myebird\" title=\"My eBird\">");
	const std::string endTag("</a>");
	std::string dummy;
	std::string::size_type offset1(0), offset2(0);
	return ExtractTextBetweenTags(htmlData, startTag1, endTag, dummy, offset1) ||
		ExtractTextBetweenTags(htmlData, startTag2, endTag, dummy, offset2);
}

std::string FrequencyDataHarvester::ExtractTokenFromLoginPage(const std::string& htmlData)
{
	const std::string tokenTagStart("<input type=\"hidden\" name=\"lt\" value=\"");
	const std::string tokenTagEnd("\" />");

	std::string::size_type offset(0);
	std::string token;
	if (!ExtractTextBetweenTags(htmlData, tokenTagStart, tokenTagEnd, token, offset))
		return std::string();

	return token;
}

std::string FrequencyDataHarvester::BuildEBirdLoginInfo(const std::string&userName, const std::string& password, const std::string& token)
{
	return "username=" + userName
		+ "&password=" + password
		+ "&rememberMe=on"
		+ "&lt=" + token
		+ "&execution=e1s1"
		+ "&_eventId=submit";
}

std::string FrequencyDataHarvester::BuildRegionString(const std::string &country, const std::string &state,
	const std::string &county)
{
	EBirdInterface ebi;
	const std::string countryCode(ebi.GetCountryCode(country));
	if (countryCode.empty())
		return std::string();

	if (state.empty())
		return countryCode;

	const std::string stateCode(ebi.GetStateCode(countryCode, state));
	if (stateCode.empty())
		return std::string();

	if (county.empty())
		return stateCode;

	const std::string countyCode(ebi.GetCountyCode(stateCode, county));
	if (countyCode.empty())
		return std::string();

	return countyCode;
}

std::string FrequencyDataHarvester::BuildTargetSpeciesURL(const std::string& regionString,
	const unsigned int& beginMonth, const unsigned int& endMonth, const ListTimeFrame& timeFrame)
{
	std::ostringstream ss;
	// r1 is "show speicies observed in"
	// r2 is "that I need for my list"
	// We'll always keep them the same for now
	ss << targetSpeciesURLBase << "?r1=" << regionString << "&bmo=" << beginMonth << "&emo="
		<< endMonth << "&r2=" << regionString << "&t2=" << GetTimeFrameString(timeFrame);
	return ss.str();
	// NOTE:  Web site appends "&_mediaType=on&_mediaType=on" to URL, but it doesn't seem to make any difference (maybe has to do with selecting "with photos" or "with audio"?)
}

std::string FrequencyDataHarvester::GetTimeFrameString(const ListTimeFrame& timeFrame)
{
	switch (timeFrame)
	{
	case ListTimeFrame::Life:
		return "life";

	case ListTimeFrame::Year:
		return "year";

	case ListTimeFrame::Month:
		return "month";

	case ListTimeFrame::Day:
		return "day";

	default:
		break;
	}

	assert(false);
	return std::string();
}

bool FrequencyDataHarvester::DoCURLGet(const std::string& url, std::string &response)
{
	assert(curl);

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response), "Failed to set write data"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 0L), "Failed to set action to GET"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()), "Failed to set URL"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), "Failed issuing https GET"))
		return false;
	return true;
}

//==========================================================================
// Class:			FrequencyDataHarvester
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
size_t FrequencyDataHarvester::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	const size_t totalSize(size * nmemb);
	std::string& s(*static_cast<std::string*>(userData));
	s.append(ptr, totalSize);

	return totalSize;
}

bool FrequencyDataHarvester::ExtractFrequencyData(const std::string& htmlData, FrequencyData& data)
{
	const std::string checklistCountTagStart("<div class=\"last-updated\">Based on ");
	const std::string checklistCountTagEnd(" complete checklists</div>");

	std::string::size_type currentOffset(0);
	std::string checklistCountString;
	if (!ExtractTextBetweenTags(htmlData, checklistCountTagStart, checklistCountTagEnd, checklistCountString, currentOffset))
	{
		std::cerr << "Failed to extract checklist count from HTML\n";
		return false;
	}

	std::istringstream ss(checklistCountString);
	ss.imbue(std::locale(""));
	if ((ss >> data.checklistCount).fail())
	{
		std::cerr << "Failed to parse checklist count\n";
		return false;
	}

	const std::string speciesTagStart("<td headers=\"species\" class=\"species-name\">");
	const std::string speciesTagEnd("</td>");
	const std::string frequencyTagStart("<td headers=\"freq\" class=\"num\">");
	const std::string frequencyTagEnd("</td>");

	FrequencyData::SpeciesFrequency entry;
	while (ExtractTextBetweenTags(htmlData, speciesTagStart, speciesTagEnd, entry.species, currentOffset))
	{
		std::string frequencyString;
		if (!ExtractTextBetweenTags(htmlData, frequencyTagStart, frequencyTagEnd, frequencyString, currentOffset))
		{
			std::cerr << "Failed to extract frequency for species '" << entry.species << "'\n";
			return false;
		}

		std::istringstream freqSS(frequencyString);
		if (!(freqSS >> entry.frequency).good())
		{
			std::cerr << "Failed to parse frequency for species '" << entry.species << "'\n";
			return false;
		}

		data.frequencies.push_back(entry);
	}

	return data.frequencies.size() > 0;
}

bool FrequencyDataHarvester::ExtractTextBetweenTags(const std::string& htmlData, const std::string& startTag,
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

bool FrequencyDataHarvester::WriteFrequencyDataToFile(const std::string& fileName, const std::array<FrequencyData, 12>& data)
{
	const unsigned int maxSpecies([&data]()
	{
		unsigned int s(0);
		for (const auto& d : data)
		{
			if (d.frequencies.size() > s)
				s = d.frequencies.size();
		}
		return s;
	}());

	std::ofstream file(fileName.c_str());
	if (!file.good() || !file.is_open())
	{
		std::cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}

	file << "January," << data[0].checklistCount <<
		",February," << data[1].checklistCount <<
		",March," << data[2].checklistCount <<
		",April," << data[3].checklistCount <<
		",May," << data[4].checklistCount <<
		",June," << data[5].checklistCount <<
		",July," << data[6].checklistCount <<
		",August," << data[7].checklistCount <<
		",September," << data[8].checklistCount <<
		",October," << data[9].checklistCount <<
		",November," << data[10].checklistCount <<
		",December," << data[11].checklistCount << ",\n";

	unsigned int i;
	for (i = 0; i < data.size(); ++i)
		file << "Species,Frequency,";
	file << '\n';

	for (i = 0; i < maxSpecies; ++i)
	{
		for (const auto& m : data)
		{
			if (i < m.frequencies.size())
			{
				file << m.frequencies[i].species << ',' << m.frequencies[i].frequency << ',';
			}
			else
				file << ",,";
		}

		file << '\n';
	}

	return true;
}
