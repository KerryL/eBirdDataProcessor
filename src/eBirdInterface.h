// File:  eBirdInterface.h
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.

#ifndef EBIRD_INTERFACE_H_

// Local headers
#include "jsonInterface.h"

// Standard C++ headers
#include <string>
#include <vector>
#include <unordered_map>

class EBirdInterface : public JSONInterface
{
public:
	std::vector<std::string> GetHotspotsWithRecentObservationsOf(
		const std::string& scientificName, const std::string& region);

	std::string GetScientificNameFromCommonName(const std::string& commonName);

	std::string GetRegionCode(const std::string& country, const std::string& state = "", const std::string& county = "");
	// http://ebird.org/ws1.1/ref/location/find?rtype=subnational2&match=bucks&fmt=csv&countryCode=US&subnational1Code=US-PA

private:
	static const std::string apiRoot;
	static const std::string recentObservationsOfSpeciesInRegionURL;
	static const std::string taxonomyLookupURL;

	static const std::string commonNameTag;
	static const std::string scientificNameTag;
	static const std::string locationNameTag;

	void BuildNameMaps(cJSON* root);
	static std::unordered_map<std::string, std::string> commonToScientificMap;
	static std::unordered_map<std::string, std::string> scientificToCommonMap;

	static std::string UrlEncode(const std::string& s);
};

#endif// EBIRD_INTERFACE_H_
