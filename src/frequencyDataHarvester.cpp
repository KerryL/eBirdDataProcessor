// File:  frequencyDataHarvester.cpp
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

// Local headers
#include "frequencyDataHarvester.h"
#include "eBirdInterface.h"
#include "mapPageGenerator.h"
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
#include <cassert>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <thread>

const String FrequencyDataHarvester::targetSpeciesURLBase(_T("http://ebird.org/ebird/targets"));
const String FrequencyDataHarvester::userAgent(_T("eBirdDataProcessor"));
const String FrequencyDataHarvester::eBirdLoginURL(_T("https://secure.birds.cornell.edu/cassso/login?service=https://ebird.org/ebird/login/cas?portal=ebird&locale=en_US"));
const bool FrequencyDataHarvester::verbose(false);
const String FrequencyDataHarvester::cookieFile(_T("ebdp.cookies"));

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

bool FrequencyDataHarvester::GenerateFrequencyFile(const String &country,
	const String &state, const String &county, const String &frequencyFilePath, const String& eBirdApiKey)
{
	if (!DoEBirdLogin())
		return false;

	EBirdInterface ebi(eBirdApiKey);
	std::array<FrequencyData, 12> frequencyData;
	const String regionCode(ebi.GetRegionCode(country, state, county));
	if (!PullFrequencyData(regionCode, frequencyData))
		return false;

	return WriteFrequencyDataToFile(frequencyFilePath + regionCode + _T(".csv"), frequencyData);
}

// fipsStart argument can be used to resume a failed bulk harvest without needing
// to re-harvest the data for the specified state which was successfully harvested
bool FrequencyDataHarvester::DoBulkFrequencyHarvest(const String &country,
	const String &state, const String& targetPath,
	const String& firstSubRegion, const String& eBirdApiKey)
{
	Cout << "Harvesting frequency data for " << state << ", " << country << std::endl;
	Cout << "Frequency files will be stored in " << targetPath << std::endl;

	if (!DoEBirdLogin())
		return false;

	EBirdInterface ebi(eBirdApiKey);
	const String countryRegionCode(ebi.GetCountryCode(country));

	// We want to be able to handle two things here:  Places which do not have sub-regions
	// beyond level 1 and pulling state-level data by specifying only the country abbreviation.
	auto subRegionList([&state, &countryRegionCode, &ebi]()
	{
		if (state.empty())
			return ebi.GetSubRegions(countryRegionCode, EBirdInterface::RegionType::SubNational1);

		const String stateRegionCode(ebi.GetStateCode(countryRegionCode, state));
		auto subRegionList(ebi.GetSubRegions(stateRegionCode, EBirdInterface::RegionType::SubNational2));
		if (subRegionList.empty())
		{
			EBirdInterface::RegionInfo regionInfo;
			regionInfo.code = stateRegionCode;
			subRegionList.push_back(regionInfo);
		}

		return subRegionList;
	}());

	Cout << "Beginning harvest for " << subRegionList.size() << " counties";
	std::sort(subRegionList.begin(), subRegionList.end(), [](const EBirdInterface::RegionInfo& a, const EBirdInterface::RegionInfo& b)
	{
		return a.code.compare(b.code) < 0;
	});

	if (!firstSubRegion.empty())
		Cout << " (skipping regions that occur before " << firstSubRegion << ')';
	Cout << std::endl;

	for (const auto& r : subRegionList)
	{
		if (!firstSubRegion.empty())
		{
			auto lastDash(r.code.find_last_of(Char('-')));
			if (lastDash != std::string::npos)
			{
				const String currentRegionCode(r.code.substr(lastDash + 1));
				if (currentRegionCode.compare(firstSubRegion) < 0)
					continue;
			}
			else
				Cerr << "Failed to extract code to determine if we should ignore (so we will include it)\n";
		}

		Cout << r.name << " (" << r.code << ")..." << std::endl;
		std::array<FrequencyData, 12> data;
		if (!PullFrequencyData(r.code, data))
			break;

		// Used to be true:
		// Some independent cities (i.e. "Baltimore city") are recognized in census data as "county equivalents,"
		// but are apparently combined with a neighboring county by eBird.  When this happens, data will be empty,
		// but we return true to allow processing to continue.
		//
		// Now:  We're pulling list of sub regions directly from eBird, so an empty data set only means that there
		// are no observations yet for that area (YES - this IS possible!).
		/*if (DataIsEmpty(data))
			continue;*/

		if (!WriteFrequencyDataToFile(targetPath + r.code + _T(".csv"), data))
			break;
	}

	return true;
}

bool FrequencyDataHarvester::PullFrequencyData(const String& regionString,
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

bool FrequencyDataHarvester::HarvestMonthData(const String& regionString,
	const unsigned int& month, FrequencyData& frequencyData)
{
	assert(month > 0 && month <= 12);
	String response;
	if (!DoCURLGet(BuildTargetSpeciesURL(regionString, month, month, ListTimeFrame::Day), response))
	{
		Cerr << "Failed to read target species web page\n";
		return false;
	}

	if (ExtractCountyNameFromPage(regionString, response).compare(_T("null")) == 0)
	{
		Cerr << "Warning:  Found null county data for region string '" << regionString << "'\n";
		return true;
	}

	if (!ExtractFrequencyData(response, frequencyData))
	{
		Cerr << "Failed to parse HTML to extract frequency data\n";
		return false;
	}

	return true;
}

bool FrequencyDataHarvester::DoEBirdLogin()
{
	String loginPage;
	if (!DoCURLGet(eBirdLoginURL, loginPage))
		return false;

	while (!EBirdLoginSuccessful(loginPage))
	{
		/*if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL"), "Failed to clear existing cookies"))// erase all existing cookie data
			return false;*/

		String eBirdUserName, eBirdPassword;
		GetUserNameAndPassword(eBirdUserName, eBirdPassword);
		char* urlEncodedPassword(curl_easy_escape(curl, UString::ToNarrowString(eBirdPassword).c_str(), eBirdPassword.length()));
		if (!urlEncodedPassword)
			Cerr << "Failed to URL-encode password\n";
		eBirdPassword = UString::ToStringType(urlEncodedPassword);
		curl_free(urlEncodedPassword);

		if (!PostEBirdLoginInfo(eBirdUserName, eBirdPassword, loginPage))
		{
			Cerr << "Failed to login to eBird\n";
			return false;
		}
	}

	return true;
}

void FrequencyDataHarvester::GetUserNameAndPassword(String& userName, String& password)
{
	Cout << "NOTE:  In order for this routine to work properly, you must not have submitted any checklists for the current day in the specified region." << std::endl;

	Cout << "Specify your eBird user name:  ";
	Cin >> userName;
	Cout << "Password:  ";

#ifdef _WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(hStdin, &mode);
	SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));

	Cin.ignore();
	std::getline(Cin, password);

	SetConsoleMode(hStdin, mode);
#else
	termios oldt;
	tcgetattr(STDIN_FILENO, &oldt);
	termios newt = oldt;
	newt.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	Cin.ignore();
	std::getline(Cin, password);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

	Cout << std::endl;
}

bool FrequencyDataHarvester::DoGeneralCurlConfiguration()
{
	if (!curl)
		curl = curl_easy_init();

	if (!curl)
	{
		Cerr << "Failed to initialize CURL" << std::endl;
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
		Cerr << "Failed to append keep alive to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEFILE, UString::ToNarrowString(cookieFile).c_str()), _T("Failed to load the cookie file")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_COOKIEJAR, UString::ToNarrowString(cookieFile).c_str()), _T("Failed to enable saving cookies")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FrequencyDataHarvester::CURLWriteCallback), _T("Failed to set the write callback")))
		return false;

	return true;
}

bool FrequencyDataHarvester::PostEBirdLoginInfo(const String& userName, const String& password, String& resultPage)
{
	assert(curl);

/*struct curl_slist *cookies = NULL;// TODO:  Remove
curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
Cout << "=============COOKIES=========" << std::endl;
while(cookies) {
Cout << cookies->data << std::endl;
        cookies = cookies->next;
      }
curl_slist_free_all(cookies);
Cout << "=============END COOKIES=========" << std::endl;// TODO:  Remove*/

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resultPage), _T("Failed to set write data")))
		return false;

	String token(ExtractTokenFromLoginPage(resultPage));
	if (token.empty())
	{
		Cerr << "Failed to get session token\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_POST, 1L), _T("Failed to set action to POST")))
		return false;

	const String loginInfo(BuildEBirdLoginInfo(userName, password, token));
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, UString::ToNarrowString(loginInfo).c_str());

	if (CURLUtilities::CURLCallHasError(curl_easy_perform(curl), _T("Failed issuding https POST (login)")))
		return false;

	return true;
}

bool FrequencyDataHarvester::EBirdLoginSuccessful(const String& htmlData)
{
	const String startTag1(_T("<li ><a href=\"/ebird/myebird\">"));
	const String startTag2(_T("<li class=\"selected\"><a href=\"/ebird/myebird\" title=\"My eBird\">"));
	const String startTag3(_T("<a href=\"https://secure.birds.cornell.edu/cassso/account/edit?service=https://ebird.org/MyEBird"));
	const String endTag(_T("</a>"));
	String dummy;
	std::string::size_type offset1(0), offset2(0), offset3(0);
	return ExtractTextBetweenTags(htmlData, startTag1, endTag, dummy, offset1) ||
		ExtractTextBetweenTags(htmlData, startTag2, endTag, dummy, offset2) ||
		ExtractTextBetweenTags(htmlData, startTag3, endTag, dummy, offset3);
}

String FrequencyDataHarvester::ExtractTokenFromLoginPage(const String& htmlData)
{
	const String tokenTagStart(_T("<input type=\"hidden\" name=\"lt\" value=\""));
	const String tokenTagEnd(_T("\" />"));

	std::string::size_type offset(0);
	String token;
	if (!ExtractTextBetweenTags(htmlData, tokenTagStart, tokenTagEnd, token, offset))
		return String();

	return token;
}

String FrequencyDataHarvester::BuildEBirdLoginInfo(const String&userName, const String& password, const String& token)
{
	return _T("username=") + userName
		+ _T("&password=") + password
		+ _T("&rememberMe=on")
		+ _T("&lt=") + token
		+ _T("&execution=e1s1")
		+ _T("&_eventId=submit");
}

String FrequencyDataHarvester::BuildTargetSpeciesURL(const String& regionString,
	const unsigned int& beginMonth, const unsigned int& endMonth, const ListTimeFrame& timeFrame)
{
	OStringStream ss;
	// r1 is "show speicies observed in"
	// r2 is "that I need for my list"
	// We'll always keep them the same for now
	ss << targetSpeciesURLBase << "?r1=" << regionString << "&bmo=" << beginMonth << "&emo="
		<< endMonth << "&r2=" << regionString << "&t2=" << GetTimeFrameString(timeFrame);
	return ss.str();
	// NOTE:  Web site appends "&_mediaType=on&_mediaType=on" to URL, but it doesn't seem to make any difference (maybe has to do with selecting "with photos" or "with audio"?)
}

String FrequencyDataHarvester::GetTimeFrameString(const ListTimeFrame& timeFrame)
{
	switch (timeFrame)
	{
	case ListTimeFrame::Life:
		return _T("life");

	case ListTimeFrame::Year:
		return _T("year");

	case ListTimeFrame::Month:
		return _T("month");

	case ListTimeFrame::Day:
		return _T("day");

	default:
		break;
	}

	assert(false);
	return String();
}

bool FrequencyDataHarvester::DoCURLGet(const String& url, String &response)
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
// Class:			FrequencyDataHarvester
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
size_t FrequencyDataHarvester::CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData)
{
	const size_t totalSize(size * nmemb);
	String& s(*static_cast<String*>(userData));
	s.append(UString::ToStringType(std::string(ptr, totalSize)));

	return totalSize;
}

bool FrequencyDataHarvester::ExtractFrequencyData(const String& htmlData, FrequencyData& data)
{
	const String checklistCountTagStart(_T("<div class=\"last-updated\">Based on "));
	const String checklistCountTagEnd(_T(" complete checklists</div>"));

	std::string::size_type currentOffset(0);
	String checklistCountString;
	if (!ExtractTextBetweenTags(htmlData, checklistCountTagStart, checklistCountTagEnd, checklistCountString, currentOffset))
	{
		data.checklistCount = 0;
		data.frequencies.clear();
		Cerr << "Failed to extract checklist count from HTML; assuming no data available for this county and month\n";
		//return false;
		return true;// Allow execution to continue - assume we reach this point due to lack of data for this county-month combo
	}

	IStringStream ss(checklistCountString);
	ss.imbue(std::locale(""));
	if ((ss >> data.checklistCount).fail())
	{
		Cerr << "Failed to parse checklist count\n";
		return false;
	}

	const String speciesTagStart(_T("<td headers=\"species\" class=\"species-name\">"));
	const String speciesTagEnd(_T("</td>"));
	const String frequencyTagStart(_T("<td headers=\"freq\" class=\"num\">"));
	const String frequencyTagEnd(_T("</td>"));

	EBirdDataProcessor::FrequencyInfo entry;
	while (ExtractTextBetweenTags(htmlData, speciesTagStart, speciesTagEnd, entry.species, currentOffset))
	{
		String frequencyString;
		if (!ExtractTextBetweenTags(htmlData, frequencyTagStart, frequencyTagEnd, frequencyString, currentOffset))
		{
			Cerr << "Failed to extract frequency for species '" << entry.species << "'\n";
			return false;
		}

		IStringStream freqSS(frequencyString);
		if (!(freqSS >> entry.frequency).good())
		{
			Cerr << "Failed to parse frequency for species '" << entry.species << "'\n";
			return false;
		}

		data.frequencies.push_back(entry);
	}

	return data.frequencies.size() > 0;
}

bool FrequencyDataHarvester::ExtractTextBetweenTags(const String& htmlData, const String& startTag,
	const String& endTag, String& text, std::string::size_type& offset)
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

bool FrequencyDataHarvester::WriteFrequencyDataToFile(const String& fileName, const std::array<FrequencyData, 12>& data)
{
	if (CurrentDataMissingSpecies(fileName, data))
	{
		Cerr << "New frequency data is missing species that were previously included.  This function cannot be executed if you have submitted observations for this area to eBird today." << std::endl;
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

	OFStream file(fileName.c_str());
	if (!file.good() || !file.is_open())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
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

bool FrequencyDataHarvester::CurrentDataMissingSpecies(const String& fileName, const std::array<FrequencyData, 12>& data)
{
	IFStream file(fileName.c_str());
	if (!file.is_open() || !file.good())
		return false;// Not an error - file may not exist

	std::array<std::vector<String>, 12> oldData;

	String line;
	std::getline(file, line);// skip header rows
	std::getline(file, line);// skip header rows
	while (std::getline(file, line))
	{
		IStringStream ss(line);
		auto it(oldData.begin());
		String token;
		while (std::getline(ss, token, Char(',')))
		{
			if (!token.empty())
				it->push_back(token);

			std::getline(ss, token, Char(','));// skip frequency data
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

String FrequencyDataHarvester::ExtractCountyNameFromPage(const String& regionString, const String& htmlData)
{
	const String matchStart(_T("<option value=\"") + regionString + _T("\" selected=\"selected\">"));
	const String matchEnd(_T(" County, "));

	String countyName;
	std::string::size_type offset(0);
	if (!ExtractTextBetweenTags(htmlData, matchStart, matchEnd, countyName, offset))
		return String();

	return countyName;
}

// This removes "County", everything after the comma (results are in format "Whatever County, State Name") as well as apostrophes, periods and spaces
// Two separate checks for "County" and comma because some counties are actually city names (i.e. "Baltimore city")
String FrequencyDataHarvester::Clean(const String& s)
{
	String cleanString(s);

	const auto lastComma(cleanString.find_last_of(','));
	if (lastComma != std::string::npos)
		cleanString.erase(cleanString.begin() + lastComma, cleanString.end());

	const auto countyString(cleanString.find(_T(" County")));
	if (countyString != std::string::npos)
		cleanString.erase(cleanString.begin() + countyString, cleanString.end());

	cleanString.erase(std::remove_if(cleanString.begin(), cleanString.end(), [](const Char& c)
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

bool FrequencyDataHarvester::AuditFrequencyData(const String& frequencyFilePath,
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, const String& eBirdApiKey)
{
	if (!DoEBirdLogin())
		return false;

	// TODO:  Should extend this to handle states where country has no further subdivisions

	EBirdInterface ebi(eBirdApiKey);

	for (const auto& f : freqInfo)
	{
		unsigned int i;
		bool updated(false);
		String regionString;
		std::array<FrequencyData, 12> frequencyData;
		for (i = 0; i < 12; ++i)
		{
			if (f.probabilities[i] > 0.0 && f.frequencyInfo[i].size() == 0)// "probabilities" is actually checklist count for the month
			{
				Cout << "Suspect missing data in " << f.locationCode << " for month " << i + 1 << "; Updating..." << std::endl;

				if (regionString.empty())
				{
					const String countryCode(ExtractCountryFromFileName(f.locationCode));
					const String state(ExtractStateFromFileName(f.locationCode));
					const String stateCode(ebi.GetStateCode(countryCode, state));
					const auto countyList(ebi.GetSubRegions(stateCode, EBirdInterface::RegionType::SubNational2));
					for (const auto& county : countyList)
					{
						if (f.locationCode.compare(county.code) == 0)
						{
							regionString = county.code;
							break;
						}
					}

					if (regionString.empty())
					{
						Cerr << "Failed to find region string for '" << f.locationCode << "'\n";
						break;
					}

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
			if (!WriteFrequencyDataToFile(frequencyFilePath + f.locationCode + _T(".csv"), frequencyData))
				continue;
		}
	}

	auto statesAndCountries(GetCountriesAndStates(freqInfo));
	for (const auto& sc : statesAndCountries)
	{
		const String stateCode(ebi.GetStateCode(sc.country, sc.state));

		auto missingCounties(FindMissingCounties(stateCode, freqInfo, ebi));
		for (const auto& county : missingCounties)
		{
			Cout << "Missing county " << county.name << " for state " << sc.state << ", " << sc.country << "; Updating..." << std::endl;

			std::array<FrequencyData, 12> data;
			if (!PullFrequencyData(county.code, data))
				continue;

			// See comment in DoBulkFrequencyHarvest()
			/*if (DataIsEmpty(data))
				continue;*/

			if (!WriteFrequencyDataToFile(frequencyFilePath + county.code + _T(".csv"), data))
				continue;
		}
	}

	return true;
}

std::vector<FrequencyDataHarvester::StateCountryCode> FrequencyDataHarvester::GetCountriesAndStates(
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo)
{
	std::vector<StateCountryCode> statesCountries(freqInfo.size());
	unsigned int i;
	for (i = 0; i < freqInfo.size(); ++i)
	{
		statesCountries[i].country = ExtractCountryFromFileName(freqInfo[i].locationCode);
		statesCountries[i].state = ExtractStateFromFileName(freqInfo[i].locationCode);
	}

	std::sort(statesCountries.begin(), statesCountries.end());
	statesCountries.erase(std::unique(statesCountries.begin(), statesCountries.end()), statesCountries.end());
	return statesCountries;
}

std::vector<EBirdInterface::RegionInfo> FrequencyDataHarvester::FindMissingCounties(const String& stateCode,
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, EBirdInterface& ebi)
{
	std::vector<String> countiesInDataset;
	for (const auto& f : freqInfo)
	{
		if (ExtractStateFromFileName(f.locationCode).compare(stateCode.substr(stateCode.length() - 2)) == 0)
			countiesInDataset.push_back(f.locationCode);
	}

	auto countyList(ebi.GetSubRegions(stateCode, EBirdInterface::RegionType::SubNational2));

	std::vector<EBirdInterface::RegionInfo> missingCounties(countyList.size() - countiesInDataset.size());
	if (missingCounties.size() == 0)
		return missingCounties;

	auto mIt(missingCounties.begin());
	for (const auto& c : countyList)
	{
		bool found(false);
		for (const auto& cid : countiesInDataset)
		{
			if (c.code.compare(cid) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			*mIt = c;
			++mIt;
			if (mIt == missingCounties.end())
				break;
		}
	}

	assert(mIt == missingCounties.end());

	return missingCounties;
}

String FrequencyDataHarvester::ExtractCountryFromFileName(const String& fileName)
{
	return fileName.substr(0, 2);
}

String FrequencyDataHarvester::ExtractStateFromFileName(const String& fileName)
{
	// For US, states abbreviations are all 2 characters, but this isn't universal.  Need to find the hyphen.
	// eBird does guarantee that country abbreviation are two characters, however.
	const std::string::size_type start(3);
	const std::string::size_type length(fileName.find('-', start) - start);
	assert(length != std::string::npos);
	return fileName.substr(start, length);
}
