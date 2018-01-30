// File:  frequencyDataHarvester.cpp
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

// Local headers
#include "frequencyDataHarvester.h"
#include "eBirdInterface.h"
#include "usCensusInterface.h"
#include "email/curlUtilities.h"
#include "mapPageGenerator.h"

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
#include <iomanip>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <thread>

const std::string FrequencyDataHarvester::targetSpeciesURLBase("http://ebird.org/ebird/targets");
const std::string FrequencyDataHarvester::userAgent("eBirdDataProcessor");
const std::string FrequencyDataHarvester::eBirdLoginURL("https://secure.birds.cornell.edu/cassso/login?service=https://ebird.org/ebird/login/cas?portal=ebird&locale=en_US");
const bool FrequencyDataHarvester::verbose(false);
const std::string FrequencyDataHarvester::cookieFile("ebdp.cookies");
const std::string FrequencyDataHarvester::endOfName("FrequencyData.csv");

using namespace std::chrono_literals;
// crawl delay determined by manually visiting www.ebird.org/robots.txt - should periodically
// check this to make sure we comply, or we should include a robots.txt parser here to automatically update
const std::chrono::steady_clock::duration FrequencyDataHarvester::eBirdCrawlDelay(std::chrono::steady_clock::duration(30s));

FrequencyDataHarvester::FrequencyDataHarvester() : rateLimiter(eBirdCrawlDelay)
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
	if (!DoEBirdLogin())
		return false;

	std::array<FrequencyData, 12> frequencyData;
	if (!PullFrequencyData(BuildRegionString(country, state, county), frequencyData))
		return false;

	return WriteFrequencyDataToFile(frequencyFileName, frequencyData);
}

// fipsStart argument can be used to resume a failed bulk harvest without needing
// to re-harvest the data for the specified state which was successfully harvested
bool FrequencyDataHarvester::DoBulkFrequencyHarvest(const std::string &country,
	const std::string &state, const std::string& targetPath, const std::string& censusKey,
	const unsigned int& fipsStart)
{
	std::cout << "Harvesting frequency data for " << state << ", " << country << std::endl;
	std::cout << "Frequency files will be stored in " << targetPath << std::endl;

	if (!DoEBirdLogin())
		return false;

	assert(country.compare("US") == 0 && "Cannot perform bulk frequency harvesting outside of US");

	unsigned int stateFIPSCode;
	if (!USCensusInterface::GetStateFIPSCode(state, stateFIPSCode))
		return false;

	USCensusInterface censusInterface(censusKey);
	std::vector<USCensusInterface::FIPSNamePair> countyList(censusInterface.GetCountyCodesInState(stateFIPSCode));

	std::cout << "Beginning harvest for " << countyList.size() << " counties";
	if (fipsStart > 0)
		std::cout << " (skipping counties with FIPS < " << fipsStart << ')';
	std::cout << std::endl;

	for (const auto& county : countyList)
	{
		if (county.fipsCode < fipsStart)
			continue;

		std::cout << county.name << " (FIPS = " << county.fipsCode << ")..." << std::endl;
		std::array<FrequencyData, 12> data;
		if (!PullFrequencyData(BuildRegionString(country, state, county.fipsCode), data))
			break;

		// Some independent cities (i.e. "Baltimore city") are recognized in census data as "county equivalents,"
		// but are apparently combined with a neighboring county by eBird.  When this happens, data will be empty,
		// but we return true to allow processing to continue.
		if (DataIsEmpty(data))
			continue;

		if (!WriteFrequencyDataToFile(targetPath + GenerateFrequencyFileName(state, county.name), data))
			break;
	}

	return true;
}

std::string FrequencyDataHarvester::GenerateFrequencyFileName(
	const std::string& state, const std::string& county)
{
	return Clean(county) + state + "FrequencyData.csv";
}

bool FrequencyDataHarvester::PullFrequencyData(const std::string& regionString,
	std::array<FrequencyData, 12>& frequencyData)
{

	unsigned int month;
	for (month = 0; month < 12; ++month)
	{
		if (!HarvestMonthData(regionString, month + 1, frequencyData[month]))
			return false;
	}

	return true;
}

bool FrequencyDataHarvester::HarvestMonthData(const std::string& regionString,
	const unsigned int& month, FrequencyData& frequencyData)
{
	assert(month > 0 && month <= 12);
	std::string response;
	if (!DoCURLGet(BuildTargetSpeciesURL(regionString, month, month, ListTimeFrame::Day), response))
	{
		std::cerr << "Failed to read target species web page\n";
		return false;
	}

	if (ExtractCountyNameFromPage(regionString, response).compare("null") == 0)
	{
		std::cerr << "Warning:  Found null county data for region string '" << regionString << "'\n";
		return true;
	}

	if (!ExtractFrequencyData(response, frequencyData))
	{
		std::cerr << "Failed to parse HTML to extract frequency data\n";
		return false;
	}

	return true;
}

bool FrequencyDataHarvester::DoEBirdLogin()
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
	const std::string startTag3("<a href=\"https://secure.birds.cornell.edu/cassso/account/edit?service=https://ebird.org/MyEBird");
	const std::string endTag("</a>");
	std::string dummy;
	std::string::size_type offset1(0), offset2(0), offset3(0);
	return ExtractTextBetweenTags(htmlData, startTag1, endTag, dummy, offset1) ||
		ExtractTextBetweenTags(htmlData, startTag2, endTag, dummy, offset2) ||
		ExtractTextBetweenTags(htmlData, startTag3, endTag, dummy, offset3);
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

std::string FrequencyDataHarvester::BuildRegionString(const std::string &country, const std::string &state,
	const unsigned int &county)
{
	const std::string countryAndStateCode(BuildRegionString(country, state, std::string()));
	if (county == 0)
		return countryAndStateCode;

	std::ostringstream ss;
	ss << countryAndStateCode << '-' << std::setfill('0') << std::setw(3) << county;
	return ss.str();
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

	rateLimiter.Wait();
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
		data.checklistCount = 0;
		data.frequencies.clear();
		std::cerr << "Failed to extract checklist count from HTML; assuming no data available for this county and month\n";
		//return false;
		return true;// Allow execution to continue - assume we reach this point due to lack of data for this county-month combo
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

	EBirdDataProcessor::FrequencyInfo entry;
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
	if (CurrentDataMissingSpecies(fileName, data))
	{
		std::cerr << "New frequency data is missing species that were previously included.  This function cannot be executed if you have submitted observations for this area to eBird today." << std::endl;
		return false;
	}

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
				file << m.frequencies[i].species << ',' << m.frequencies[i].frequency << ',';
			else
				file << ",,";
		}

		file << '\n';
	}

	return true;
}

bool FrequencyDataHarvester::CurrentDataMissingSpecies(const std::string& fileName, const std::array<FrequencyData, 12>& data)
{
	std::ifstream file(fileName.c_str());
	if (!file.is_open() || !file.good())
		return false;// Not an error - file may not exist

	std::array<std::vector<std::string>, 12> oldData;

	std::string line;
	std::getline(file, line);// skip header rows
	std::getline(file, line);// skip header rows
	while (std::getline(file, line))
	{
		std::istringstream ss(line);
		auto it(oldData.begin());
		std::string token;
		while (std::getline(ss, token, ','))
		{
			if (!token.empty())
				it->push_back(token);

			std::getline(ss, token, ',');// skip frequency data
			++it;
		}
	}

	unsigned int i;
	for (i = 0; i < 12; ++i)
	{
		for (const auto& species : oldData[i])
		{
			bool found(false);
			for (const auto& s : data[i].frequencies)
			{
				if (species.compare(s.species) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
				return true;
		}
	}

	return false;
}

std::string FrequencyDataHarvester::ExtractCountyNameFromPage(const std::string& regionString, const std::string& htmlData)
{
	const std::string matchStart("<option value=\"" + regionString + "\" selected=\"selected\">");
	const std::string matchEnd(" County, ");

	std::string countyName;
	std::string::size_type offset(0);
	if (!ExtractTextBetweenTags(htmlData, matchStart, matchEnd, countyName, offset))
		return std::string();

	return countyName;
}

// This removes "County", everything after the comma (results are in format "Whatever County, State Name") as well as apostrophes, periods and spaces
// Two separate checks for "County" and comma because some counties are actually city names (i.e. "Baltimore city")
std::string FrequencyDataHarvester::Clean(const std::string& s)
{
	std::string cleanString(s);

	const auto lastComma(cleanString.find_last_of(','));
	if (lastComma != std::string::npos)
		cleanString.erase(cleanString.begin() + lastComma, cleanString.end());

	const auto countyString(cleanString.find(" County"));
	if (countyString != std::string::npos)
		cleanString.erase(cleanString.begin() + countyString, cleanString.end());

	cleanString.erase(std::remove_if(cleanString.begin(), cleanString.end(), [](const char& c)
	{
		if (std::isspace(c) ||
			c == '\'' ||
			c == '.')
			return true;

		return false;
	}), cleanString.end());

	return cleanString;
}

bool FrequencyDataHarvester::DataIsEmpty(const std::array<FrequencyData, 12>& frequencyData)
{
	for (const auto& month : frequencyData)
	{
		if (month.frequencies.size() > 0)
			return false;
	}

	return true;
}

bool FrequencyDataHarvester::AuditFrequencyData(
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, const std::string& censusKey)
{
	if (!DoEBirdLogin())
		return false;

	const std::string countyCode("US");// TODO:  Don't hardcode

	std::string targetPath(freqInfo.front().locationHint);
	auto lastForwardSlash(targetPath.find_last_of('/'));
	auto lastBackSlash(targetPath.find_last_of('\\'));
	if (lastForwardSlash != std::string::npos && lastBackSlash != std::string::npos)
		targetPath = targetPath.substr(0, std::max(lastForwardSlash, lastBackSlash) + 1);
	else if (lastForwardSlash != std::string::npos)
		targetPath = targetPath.substr(0, lastForwardSlash + 1);
	else if (lastBackSlash != std::string::npos)
		targetPath = targetPath.substr(0, lastBackSlash + 1);

	for (const auto& f : freqInfo)
	{
		unsigned int i;
		bool updated(false);
		std::string regionString;
		std::array<FrequencyData, 12> frequencyData;
		for (i = 0; i < 12; ++i)
		{
			if (f.probabilities[i] > 0.0 && f.frequencyInfo[i].size() == 0)// "probabilities" is actually checklist count for the month
			{
				std::cout << "Suspect missing data in " << f.locationHint << " for month " << i + 1 << "; Updating..." << std::endl;

				if (regionString.empty())
				{
					const std::string state(ExtractStateFromFileName(f.locationHint));
					unsigned int stateFIPSCode;
					if (!USCensusInterface::GetStateFIPSCode(state, stateFIPSCode))
						return false;

					USCensusInterface censusInterface(censusKey);
					std::vector<USCensusInterface::FIPSNamePair> countyList(censusInterface.GetCountyCodesInState(stateFIPSCode));
					for (const auto& county : countyList)
					{
						if (MapPageGenerator::CountyNamesMatch(StripDirectory(
							f.locationHint.substr(0, f.locationHint.length() - endOfName.size() - 2)), county.name))
						{
							regionString = BuildRegionString(countyCode, state, county.fipsCode);
							break;
						}
					}

					assert(!regionString.empty());

					unsigned int j;
					for (j = 0; j < frequencyData.size(); ++j)
					{
						frequencyData[j].checklistCount = static_cast<unsigned int>(f.probabilities[j]);
						frequencyData[j].frequencies = f.frequencyInfo[j];
					}
				}

				if (!HarvestMonthData(regionString, i + 1, frequencyData[i]))
					break;

				updated = true;
			}
		}

		if (updated)
		{
			if (!WriteFrequencyDataToFile(f.locationHint, frequencyData))
				continue;
		}
	}

	auto states(GetStates(freqInfo));
	for (const auto& s : states)
	{
		unsigned int stateFIPSCode;
		if (!USCensusInterface::GetStateFIPSCode(s, stateFIPSCode))
			return false;

		auto missingCounties(FindMissingCounties(s, freqInfo, stateFIPSCode, censusKey));
		for (const auto& fips : missingCounties)
		{
			std::cout << "Missing county with FIPS = " << fips.first << " in " << s << "; Updating..." << std::endl;

			std::array<FrequencyData, 12> data;
			if (!PullFrequencyData(BuildRegionString(countyCode, s, fips.first), data))
				continue;

			if (DataIsEmpty(data))
				continue;

			if (!WriteFrequencyDataToFile(targetPath + GenerateFrequencyFileName(s, fips.second), data))
				continue;
		}
	}

	return true;
}

std::vector<std::string> FrequencyDataHarvester::GetStates(const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo)
{
	std::vector<std::string> states(freqInfo.size());
	unsigned int i;
	for (i = 0; i < freqInfo.size(); ++i)
		states[i] = ExtractStateFromFileName(freqInfo[i].locationHint);

	std::sort(states.begin(), states.end());
	states.erase(std::unique(states.begin(), states.end()), states.end());
	return states;
}

std::vector<std::pair<unsigned int, std::string>> FrequencyDataHarvester::FindMissingCounties(const std::string& state,
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo,
	const unsigned int& stateFIPSCode, const std::string& censusKey)
{
	std::vector<std::string> countiesInDataset;
	for (const auto& f : freqInfo)
	{
		if (ExtractStateFromFileName(f.locationHint).compare(state) == 0)
			countiesInDataset.push_back(StripDirectory(f.locationHint.substr(0, f.locationHint.length() - endOfName.size() - 2)));
	}

	USCensusInterface censusInterface(censusKey);
	std::vector<USCensusInterface::FIPSNamePair> countyList(censusInterface.GetCountyCodesInState(stateFIPSCode));

	std::vector<std::pair<unsigned int, std::string>> missingCounties(countyList.size() - countiesInDataset.size());
	if (missingCounties.size() == 0)
		return missingCounties;

	auto mIt(missingCounties.begin());
	for (const auto& c : countyList)
	{
		bool found(false);
		for (const auto& cid : countiesInDataset)
		{
			if (MapPageGenerator::CountyNamesMatch(c.name, cid))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			mIt->first = c.fipsCode;
			mIt->second = c.name;
			++mIt;
			if (mIt == missingCounties.end())
				break;
		}
	}

	assert(mIt == missingCounties.end());

	return missingCounties;
}

std::string FrequencyDataHarvester::ExtractStateFromFileName(const std::string& fileName)
{
	auto endStart(fileName.find(endOfName));
	return fileName.substr(endStart - 2, 2);
}

std::string FrequencyDataHarvester::StripDirectory(const std::string& s)
{
	auto lastForwardSlash(s.find_last_of('/'));
	auto lastBackSlash(s.find_last_of('\\'));
	if (lastForwardSlash != std::string::npos && lastBackSlash != std::string::npos)
		return s.substr(std::max(lastForwardSlash, lastBackSlash) + 1);
	else if (lastForwardSlash != std::string::npos)
		return s.substr(lastForwardSlash + 1);
	else if (lastBackSlash != std::string::npos)
		return s.substr(lastBackSlash + 1);

	return s;
}
