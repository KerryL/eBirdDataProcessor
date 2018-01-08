// File:  usCensusInterface.h
// Date:  1/3/2018
// Auth:  K. Loux
// Desc:  Interface to US Census Data API.  Very stripped down - designed only
//        for retrieving state and county FIPS codes.

// Local headers
#include "usCensusInterface.h"

// Standard C+ headers
#include <sstream>
#include <iostream>
#include <cassert>

const std::string USCensusInterface::apiRoot("https://api.census.gov/");
const std::string USCensusInterface::housing2016URL("data/2016/pep/housing");

const std::array<const USCensusInterface::StateFIPSEntry, 51> USCensusInterface::stateFIPSCodes =
{
	StateFIPSEntry("Alabama", 1, "AL"),
	StateFIPSEntry("Alaska", 2, "AK"),
	StateFIPSEntry("Arizona", 4, "AZ"),
	StateFIPSEntry("Arkansas", 5, "AR"),
	StateFIPSEntry("California", 6, "CA"),
	StateFIPSEntry("Colorado", 8, "CO"),
	StateFIPSEntry("Connecticut", 9, "CT"),
	StateFIPSEntry("Delaware", 10, "DE"),
	StateFIPSEntry("District of Columbia", 11, "DC"),
	StateFIPSEntry("Florida", 12, "FL"),
	StateFIPSEntry("Georgia", 13, "GA"),
	StateFIPSEntry("Hawaii", 15, "HI"),
	StateFIPSEntry("Idaho", 16, "ID"),
	StateFIPSEntry("Illinois", 17, "IL"),
	StateFIPSEntry("Indiana", 18, "IN"),
	StateFIPSEntry("Iowa", 19, "IA"),
	StateFIPSEntry("Kansas", 20, "KS"),
	StateFIPSEntry("Kentucky", 21, "KY"),
	StateFIPSEntry("Louisiana", 22, "LA"),
	StateFIPSEntry("Maine", 23, "ME"),
	StateFIPSEntry("Maryland", 24, "MD"),
	StateFIPSEntry("Massachusetts", 25, "MA"),
	StateFIPSEntry("Michigan", 26, "MI"),
	StateFIPSEntry("Minnesota", 27, "MN"),
	StateFIPSEntry("Mississippi", 28, "MS"),
	StateFIPSEntry("Missouri", 29, "MO"),
	StateFIPSEntry("Montana", 30, "MT"),
	StateFIPSEntry("Nebraska", 31, "NE"),
	StateFIPSEntry("Nevada", 32, "NV"),
	StateFIPSEntry("New Hampshire", 33, "NH"),
	StateFIPSEntry("New Jersey", 34, "NJ"),
	StateFIPSEntry("New Mexico", 35, "NM"),
	StateFIPSEntry("New York", 36, "NY"),
	StateFIPSEntry("North Carolina", 37, "NC"),
	StateFIPSEntry("North Dakota", 38, "ND"),
	StateFIPSEntry("Ohio", 39, "OH"),
	StateFIPSEntry("Oklahoma", 40, "OK"),
	StateFIPSEntry("Oregon", 41, "OR"),
	StateFIPSEntry("Pennsylvania", 42, "PA"),
	StateFIPSEntry("Rhode Island", 44, "RI"),
	StateFIPSEntry("South Carolina", 45, "SC"),
	StateFIPSEntry("South Dakota", 46, "SD"),
	StateFIPSEntry("Tennessee", 47, "TN"),
	StateFIPSEntry("Texas", 48, "TX"),
	StateFIPSEntry("Utah", 49, "UT"),
	StateFIPSEntry("Vermont", 50, "VT"),
	StateFIPSEntry("Virginia", 51, "VA"),
	StateFIPSEntry("Washington", 53, "WA"),
	StateFIPSEntry("West Virginia", 54, "WV"),
	StateFIPSEntry("Wisconsin", 55, "WI"),
	StateFIPSEntry("Wyoming", 56, "WY")
};

USCensusInterface::USCensusInterface(const std::string& apiKey) : apiKey(apiKey)
{
}

std::string USCensusInterface::BuildRequestForStateFIPSCodes(
	const unsigned int& dateCode, const std::string& key)
{
	std::ostringstream ss;
	ss << housing2016URL << "?get=STATE,GEONAME&DATE=" << dateCode
		<< "&for=state:*";
	
	if (!key.empty())
		ss << "&key=" << key;

	return ss.str();
}

std::string USCensusInterface::BuildRequestForCountyFIPSCodes(const unsigned int& stateCode,
	const unsigned int& dateCode, const std::string& key)
{
	std::ostringstream ss;
	ss << housing2016URL << "?get=COUNTY,GEONAME&DATE=" << dateCode
		<< "&for=county:*&in=state:" << stateCode;

	if (!key.empty())
		ss << "&key=" << key;

	return ss.str();
}

std::vector<USCensusInterface::FIPSNamePair> USCensusInterface::GetStateCodes() const
{
	const unsigned int dateCode(9);// TODO:  Any better way than hardcoding this?  9 corresponds to 7/2016 census, apparently
	const std::string requestURL(apiRoot + BuildRequestForStateFIPSCodes(dateCode, apiKey));
	return DoRequest(requestURL);
}

std::vector<USCensusInterface::FIPSNamePair> USCensusInterface::GetCountyCodesInState(const unsigned int& stateCode) const
{
	const unsigned int dateCode(9);// TODO:  Any better way than hardcoding this?  9 corresponds to 7/2016 census, apparently
	const std::string requestURL(apiRoot + BuildRequestForCountyFIPSCodes(stateCode, dateCode, apiKey));
	return DoRequest(requestURL);
}

std::vector<USCensusInterface::FIPSNamePair> USCensusInterface::DoRequest(const std::string& request) const
{
	std::string response;
	if (!DoCURLGet(request, response))
	{
		std::cerr << "Failed to process GET request\n";
		return std::vector<FIPSNamePair>();
	}

	std::vector<FIPSNamePair> codes;
	if (!ParseResponse(response, codes))
		return std::vector<FIPSNamePair>();

	return codes;
}

bool USCensusInterface::ParseResponse(const std::string& response, std::vector<FIPSNamePair>& codes)
{
	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse response (USCensusInterface::ProcessResponse)\n";
		std::cerr << response << std::endl;
		return false;
	}

	const unsigned int responseSize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 1; i < responseSize; ++i)
	{
		cJSON* item(cJSON_GetArrayItem(root, i));
		if (!item)
		{
			std::cerr << "Failed to access item " << i << " in response:\n" << response << '\n';
			cJSON_Delete(root);
			return false;
		}

		assert(cJSON_GetArraySize(item) > 2);

		// Requests are created so the response is always [FIPS code,Region Name, ...], so we only care about the first two fields
		cJSON* fipsCode(cJSON_GetArrayItem(item, 0));
		cJSON* name(cJSON_GetArrayItem(item, 1));
		if (!fipsCode || !name)
		{
			std::cerr << "Failed to find FIPS code and/or region name\n";
			cJSON_Delete(root);
			return false;
		}

		unsigned int codeAsInt;
		std::istringstream ss(fipsCode->valuestring);
		if ((ss >> codeAsInt).fail())
		{
			std::cerr << "Failed to convert FIPS code to number\n";
			cJSON_Delete(root);
			return false;
		}

		codes.push_back(FIPSNamePair(codeAsInt, name->valuestring));
	}

	cJSON_Delete(root);
	return true;
}

bool USCensusInterface::GetStateFIPSCode(const std::string& state, unsigned int& fipsCode)
{
	for (const auto& entry : stateFIPSCodes)
	{
		if (entry.name.compare(state) == 0 ||
			entry.abbreviation.compare(state) == 0)
		{
			fipsCode = entry.fipsCode;
			return true;
		}
	}

	std::cerr << "Failed to find a match for state '" << state << "'\n";
	return false;
}
