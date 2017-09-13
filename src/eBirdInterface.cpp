// File:  eBirdInterface.cpp
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.

// Local headers
#include "eBirdInterface.h"
#include "cJSON.h"

// Standard C++ headers
#include <sstream>
#include <iostream>

const std::string EBirdInterface::apiRoot("http://ebird.org/ws1.1/");
const std::string EBirdInterface::recentObservationsOfSpeciesInRegionURL("data/obs/region_spp/recent");
const std::string EBirdInterface::taxonomyLookupURL("ref/taxa/ebird");

const std::string EBirdInterface::commonNameTag("comName");
const std::string EBirdInterface::scientificNameTag("sciName");
const std::string EBirdInterface::locationNameTag("locName");

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

	cJSON *root = cJSON_Parse(response.c_str());
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

		cJSON *root = cJSON_Parse(response.c_str());
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
