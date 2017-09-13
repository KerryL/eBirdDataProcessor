// File:  eBirdInterface.cpp
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.  See https://confluence.cornell.edu/display/CLOISAPI/eBird+API+1.1.

// Local headers
#include "eBirdInterface.h"
#include "cJSON.h"

// Standard C++ headers
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <map>
#include <cassert>

const std::string EBirdInterface::apiRoot("http://ebird.org/ws1.1/");
const std::string EBirdInterface::recentObservationsOfSpeciesInRegionURL("data/obs/region_spp/recent");
const std::string EBirdInterface::taxonomyLookupURL("ref/taxa/ebird");
const std::string EBirdInterface::locationFindURL("ref/location/find");
const std::string EBirdInterface::locationListURL("ref/location/list");

const std::string EBirdInterface::commonNameTag("comName");
const std::string EBirdInterface::scientificNameTag("sciName");
const std::string EBirdInterface::locationNameTag("locName");

const std::string EBirdInterface::countryInfoListHeading("COUNTRY_CODE,COUNTRY_NAME,COUNTRY_NAME_LONG,LOCAL_ABBREVIATION");
const std::string EBirdInterface::stateInfoListHeading("COUNTRY_CODE,SUBNATIONAL1_CODE,SUBNATIONAL1_NAME,LOCAL_ABBREVIATION");
const std::string EBirdInterface::countyInfoListHeading("COUNTRY_CODE,SUBNATIONAL1_CODE,SUBNATIONAL2_CODE,SUBNATIONAL2_NAME");

std::unordered_map<std::string, std::string> EBirdInterface::commonToScientificMap;
std::unordered_map<std::string, std::string> EBirdInterface::scientificToCommonMap;

std::vector<std::string> EBirdInterface::GetHotspotsWithRecentObservationsOf(
	const std::string& scientificName, const std::string& region)
{
	const int daysBack(30);
	const bool allowProvisional(true);
	const bool hotspotsOnly(true);

	std::ostringstream request;
	request << apiRoot << recentObservationsOfSpeciesInRegionURL << "?r="
		<< region
		<< "&sci=" << scientificName
		<< "&back=" << daysBack
		<< "&fmt=json&includeProvisional=";

	if (allowProvisional)
		request << "true";
	else
		request << "false";

	request << "&hotspot=";

	if (hotspotsOnly)
		request << "true";
	else
		request << "false";

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::vector<std::string>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetHotspotsWithRecentObservationsOf())" << std::endl;
		std::cerr << response << std::endl;
		return std::vector<std::string>();
	}

	std::vector<std::string> hotspots;
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* hotspotInfo(cJSON_GetArrayItem(root, i));

		std::string locationName;
		if (!ReadJSON(hotspotInfo, locationNameTag, locationName))
		{
			std::cerr << "Failed to read location name tag\n";
			continue;
		}

		hotspots.push_back(locationName);
	}

	cJSON_Delete(root);
	return hotspots;
}

std::string EBirdInterface::GetScientificNameFromCommonName(const std::string& commonName)
{
	if (commonToScientificMap.empty())
	{
		std::ostringstream request;
		request << apiRoot << taxonomyLookupURL << "?cat=species"
			<< "&locale=en_US"
			<< "&fmt=json";

		std::string response;
		if (!DoCURLGet(request.str(), response))
			return std::string();

		cJSON *root(cJSON_Parse(response.c_str()));
		if (!root)
		{
			std::cerr << "Failed to parse returned string (GetScientificNameFromCommonName())" << std::endl;
			std::cerr << response << std::endl;
			return std::string();
		}

		BuildNameMaps(root);
		cJSON_Delete(root);
	}

	return commonToScientificMap[commonName];
}

void EBirdInterface::BuildNameMaps(cJSON* root)
{
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* nameInfo(cJSON_GetArrayItem(root, i));

		std::string commonName;
		if (!ReadJSON(nameInfo, commonNameTag, commonName))
		{
			std::cerr << "Failed to read common name tag\n";
			continue;
		}

		std::string scientificName;
		if (!ReadJSON(nameInfo, scientificNameTag, scientificName))
		{
			std::cerr << "Failed to read scientific name tag\n";
			continue;
		}

		commonToScientificMap[commonName] = scientificName;
		scientificToCommonMap[scientificName] = commonName;
	}
}

std::string EBirdInterface::UrlEncode(const std::string& s)
{
	std::string encoded;
	for (const auto& c : s)
	{
		if (c == ' ')
			encoded.append("%20");
		else
			encoded.append(std::string(&c, 1));
	}

	return encoded;
}

std::string EBirdInterface::GetRegionCode(const std::string& country,
	const std::string& state, const std::string& county)
{
	std::string countryCode(GetCountryCode(country));
	if (countryCode.empty())
		return std::string();
	
	if (!state.empty())
	{
		std::string stateCode(GetStateCode(countryCode, state));
		if (stateCode.empty())
			return std::string();

		if (!county.empty())
			return GetCountyCode(stateCode, county);

		return stateCode;
	}

	return countryCode;
}

std::string EBirdInterface::GetUserInputOnResponse(const std::string& s, const std::string& field)
{
	std::cout << "Multiple matches.  Please specify desired " << field << ":\n\n";

	std::istringstream ss(s);
	std::string line;
	std::getline(ss, line);
	std::cout << line << '\n';// heading row

	std::map<unsigned int, std::string> selectionMap;
	unsigned int i(0);
	while (std::getline(ss, line))
	{
		std::cout << ++i << ": " << line << '\n';
		selectionMap[i] = line;
	}

	std::cout << std::endl;

	unsigned int selection;
	std::cin >> selection;

	if (selection == 0 || selection > i)
		return std::string();

	return selectionMap[selection];
}

std::string EBirdInterface::GetCountryCode(const std::string& country)
{
	if (countryInfo.empty())
		BuildCountryInfo();

	std::string lowerCaseCountry(country);
	std::transform(lowerCaseCountry.begin(), lowerCaseCountry.end(), lowerCaseCountry.begin(), std::tolower);
	for (const auto& c : countryInfo)
	{
		CountryInfo lowerCaseInfo(c);
		std::transform(lowerCaseInfo.code.begin(), lowerCaseInfo.code.end(), lowerCaseInfo.code.begin(), std::tolower);
		std::transform(lowerCaseInfo.name.begin(), lowerCaseInfo.name.end(), lowerCaseInfo.name.begin(), std::tolower);
		std::transform(lowerCaseInfo.longName.begin(), lowerCaseInfo.longName.end(), lowerCaseInfo.longName.begin(), std::tolower);
		std::transform(lowerCaseInfo.localAbbreviation.begin(), lowerCaseInfo.localAbbreviation.end(), lowerCaseInfo.localAbbreviation.begin(), std::tolower);

		if (lowerCaseInfo.code.compare(lowerCaseCountry) == 0 ||
			lowerCaseInfo.name.compare(lowerCaseCountry) == 0 ||
			lowerCaseInfo.longName.compare(lowerCaseCountry) == 0 ||
			lowerCaseInfo.localAbbreviation.compare(lowerCaseCountry) == 0)
			return c.code;
	}

	std::ostringstream request;
	request << apiRoot << locationFindURL << "?rtype=country&fmt=csv&match=" << country;

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::string();

	if (response.length() == 2)
		return response;

	return ParseCountryInfoLine(GetUserInputOnResponse(response, "country")).code;
}

std::string EBirdInterface::GetStateCode(const std::string& countryCode,
	const std::string& state)
{
	std::vector<StateInfo> stateInfo(BuildStateInfo(countryCode));

	std::string lowerCaseState(state);
	std::transform(lowerCaseState.begin(), lowerCaseState.end(), lowerCaseState.begin(), std::tolower);
	for (const auto& s : stateInfo)
	{
		StateInfo lowerCaseInfo(s);
		std::transform(lowerCaseInfo.code.begin(), lowerCaseInfo.code.end(), lowerCaseInfo.code.begin(), std::tolower);
		std::transform(lowerCaseInfo.name.begin(), lowerCaseInfo.name.end(), lowerCaseInfo.name.begin(), std::tolower);
		std::transform(lowerCaseInfo.localAbbreviation.begin(), lowerCaseInfo.localAbbreviation.end(), lowerCaseInfo.localAbbreviation.begin(), std::tolower);

		if (lowerCaseInfo.code.compare(lowerCaseState) == 0 ||
			lowerCaseInfo.name.compare(lowerCaseState) == 0 ||
			lowerCaseInfo.localAbbreviation.compare(lowerCaseState) == 0)
			return s.code;
	}

	std::ostringstream request;
	request << apiRoot << locationFindURL << "?rtype=subnational1&fmt=csv&match=" << state;

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::string();

	if (response.length() == 2)
		return response;

	std::istringstream ss(response);
	std::string line;
	std::getline(ss, line);
	assert(stateInfoListHeading.compare(line) == 0);
	
	std::vector<StateInfo> matchStateInfo;
	while (std::getline(ss, line))
		matchStateInfo.push_back(ParseStateInfoLine(line));

	stateInfo.erase(std::remove_if(stateInfo.begin(), stateInfo.end(), [&matchStateInfo](const StateInfo& s)
	{
		return std::find(matchStateInfo.begin(), matchStateInfo.end(), s) == matchStateInfo.end();
	}), stateInfo.end());

	std::string reducedResponse(stateInfoListHeading);
	for (const auto& s : stateInfo)
		reducedResponse.append('\n' + s.countryCode + ',' + s.code + ',' + s.name + ',' + s.localAbbreviation);

	return ParseStateInfoLine(GetUserInputOnResponse(reducedResponse, "state")).code;
}

std::string EBirdInterface::GetCountyCode(const std::string& stateCode, const std::string& county)
{
	std::vector<CountyInfo> countyInfo(BuildCountyInfo(stateCode));

	std::string lowerCaseCounty(county);
	std::transform(lowerCaseCounty.begin(), lowerCaseCounty.end(), lowerCaseCounty.begin(), std::tolower);
	for (const auto& c : countyInfo)
	{
		CountyInfo lowerCaseInfo(c);
		std::transform(lowerCaseInfo.code.begin(), lowerCaseInfo.code.end(), lowerCaseInfo.code.begin(), std::tolower);
		std::transform(lowerCaseInfo.name.begin(), lowerCaseInfo.name.end(), lowerCaseInfo.name.begin(), std::tolower);

		if (lowerCaseInfo.code.compare(lowerCaseCounty) == 0 ||
			lowerCaseInfo.name.compare(lowerCaseCounty) == 0)
			return c.code;
	}

	std::ostringstream request;
	request << apiRoot << locationFindURL << "?rtype=subnational2&fmt=csv&match=" << county;

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::string();

	if (response.length() == 2)
		return response;

	std::istringstream ss(response);
	std::string line;
	std::getline(ss, line);
	assert(countyInfoListHeading.compare(line) == 0);
	
	std::vector<CountyInfo> matchCountyInfo;
	while (std::getline(ss, line))
		matchCountyInfo.push_back(ParseCountyInfoLine(line));

	countyInfo.erase(std::remove_if(countyInfo.begin(), countyInfo.end(), [&matchCountyInfo](const CountyInfo& s)
	{
		return std::find(matchCountyInfo.begin(), matchCountyInfo.end(), s) == matchCountyInfo.end();
	}), countyInfo.end());

	std::string reducedResponse(countyInfoListHeading);
	for (const auto& s : countyInfo)
		reducedResponse.append('\n' + s.countryCode + ',' + s.stateCode + ',' + s.code + ',' + s.name);

	return ParseCountyInfoLine(GetUserInputOnResponse(reducedResponse, "county")).code;
}

void EBirdInterface::BuildCountryInfo()
{
	std::ostringstream request;
	request << apiRoot << locationListURL << "?rtype=country&fmt=csv";

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return;

	std::istringstream ss(response);
	std::string line;
	std::getline(ss, line);
	assert(countryInfoListHeading.compare(line) == 0);
	
	while (std::getline(ss, line))
		countryInfo.push_back(ParseCountryInfoLine(line));
}

std::vector<EBirdInterface::StateInfo> EBirdInterface::BuildStateInfo(const std::string& countryCode)
{
	std::ostringstream request;
	request << apiRoot << locationListURL << "?rtype=subnational1&fmt=csv&countryCode=" << countryCode;

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::vector<StateInfo>();

	std::istringstream ss(response);
	std::string line;
	std::getline(ss, line);
	assert(stateInfoListHeading.compare(line) == 0);
	
	std::vector<StateInfo> stateInfo;
	while (std::getline(ss, line))
		stateInfo.push_back(ParseStateInfoLine(line));

	return stateInfo;
}

std::vector<EBirdInterface::CountyInfo> EBirdInterface::BuildCountyInfo(const std::string& stateCode)
{
	std::ostringstream request;
	request << apiRoot << locationListURL << "?rtype=subnational2&fmt=csv&subnational1Code=" << stateCode;

	std::string response;
	if (!DoCURLGet(UrlEncode(request.str()), response))
		return std::vector<CountyInfo>();

	std::istringstream ss(response);
	std::string line;
	std::getline(ss, line);
	assert(countyInfoListHeading.compare(line) == 0);
	
	std::vector<CountyInfo> countyInfo;
	while (std::getline(ss, line))
		countyInfo.push_back(ParseCountyInfoLine(line));

	return countyInfo;
}

EBirdInterface::CountryInfo EBirdInterface::ParseCountryInfoLine(const std::string& line)
{
	CountryInfo info;

	std::istringstream ss(line);
	std::getline(ss, info.code, ',');
	std::getline(ss, info.name, ',');
	std::getline(ss, info.longName, ',');
	std::getline(ss, info.localAbbreviation, ',');

	return info;
}

EBirdInterface::StateInfo EBirdInterface::ParseStateInfoLine(const std::string& line)
{
	StateInfo info;

	std::istringstream ss(line);
	std::getline(ss, info.countryCode, ',');
	std::getline(ss, info.code, ',');
	std::getline(ss, info.name, ',');
	std::getline(ss, info.localAbbreviation, ',');

	return info;
}

EBirdInterface::CountyInfo EBirdInterface::ParseCountyInfoLine(const std::string& line)
{
	CountyInfo info;

	std::istringstream ss(line);
	std::getline(ss, info.countryCode, ',');
	std::getline(ss, info.stateCode, ',');
	std::getline(ss, info.code, ',');
	std::getline(ss, info.name, ',');

	return info;
}
