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
const std::string taxonomyLookupURL("ref/taxa/ebird");

const std::string commonNameTag("com-name");
const std::string scientificNameTag("sci-name");

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
	std::vector<std::string> hotspotList;
	if (!DoCURLGet(request.str(), response))
		return hotspotList;

	cJSON *root = cJSON_Parse(response.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetHotspotsWithRecentObservationsOf())" << std::endl;
		std::cerr << response << std::endl;
		return hotspotList;
	}

	/*if (ReadJSON(root, userURLTag, userURL))
	{
		const std::string userCode("{user}");
		size_t begin(userURL.find(userCode));
		if (begin == std::string::npos)
			userURL.clear();
		else
			userURL.replace(begin, userCode.length(), user);
	}

	if (ReadJSON(root, userReposURLTag, reposURLRoot))
	{
		const std::string ownerCode("{owner}");
		size_t begin(reposURLRoot.find(ownerCode));
		if (begin == std::string::npos)
			reposURLRoot.clear();
		else
		{
			reposURLRoot.replace(begin, ownerCode.length(), user);
			const std::string repoCode("{repo}");
			size_t end(reposURLRoot.find(repoCode));
			if (end == std::string::npos)
				reposURLRoot.clear();
			else
				reposURLRoot.resize(end);
		}
	}*/
	std::cout << response << std::endl;

	cJSON_Delete(root);
	return hotspotList;
}

std::string EBirdInterface::GetScientificNameFromCommonName(const std::string& commonName)
{
	std::ostringstream request;
	request << apiRoot << recentObservationsOfSpeciesInRegionURL << "?cat=species"
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

	/*if (ReadJSON(root, userURLTag, userURL))
	{
		const std::string userCode("{user}");
		size_t begin(userURL.find(userCode));
		if (begin == std::string::npos)
			userURL.clear();
		else
			userURL.replace(begin, userCode.length(), user);
	}

	if (ReadJSON(root, userReposURLTag, reposURLRoot))
	{
		const std::string ownerCode("{owner}");
		size_t begin(reposURLRoot.find(ownerCode));
		if (begin == std::string::npos)
			reposURLRoot.clear();
		else
		{
			reposURLRoot.replace(begin, ownerCode.length(), user);
			const std::string repoCode("{repo}");
			size_t end(reposURLRoot.find(repoCode));
			if (end == std::string::npos)
				reposURLRoot.clear();
			else
				reposURLRoot.resize(end);
		}
	}*/
	std::cout << response << std::endl;

	cJSON_Delete(root);

	return std::string();
}
