// File:  eBirdInterface.h
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.  See https://confluence.cornell.edu/display/CLOISAPI/eBird+API+1.1.

#ifndef EBIRD_INTERFACE_H_
#define EBIRD_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"

// Standard C++ headers
#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>

class EBirdInterface : public JSONInterface
{
public:
	struct HotspotInfo
	{
		std::string hotspotName;
		std::string hotspotID;
		double latitude;
		double longitude;
	};

	std::vector<HotspotInfo> GetHotspotsWithRecentObservationsOf(
		const std::string& scientificName, const std::string& region, const unsigned int& recentPeriod);

	struct ObservationInfo
	{
		std::string commonName;
		std::string scientificName;
		std::tm observationDate;
		unsigned int count;
		std::string locationID;
		bool isNotHotspot;
		std::string locationName;
		double latitude;
		double longitude;
		bool observationReviewed;
		bool observationValid;
	};

	std::vector<ObservationInfo> GetRecentObservationsOfSpeciesAtHotspot(const std::string& scientificName,
		const std::string& hotspotID, const unsigned int& recentPeriod, const bool& includeProvisional);
	std::vector<ObservationInfo> GetRecentObservationsOfSpeciesInRegion(const std::string& scientificName,
		const std::string& region, const unsigned int& recentPeriod, const bool& includeProvisional, const bool& hotspotsOnly);

	std::string GetScientificNameFromCommonName(const std::string& commonName);

	std::string GetRegionCode(const std::string& country, const std::string& state = "", const std::string& county = "");

	std::string GetCountryCode(const std::string& country);
	std::string GetStateCode(const std::string& countryCode, const std::string& state);
	std::string GetCountyCode(const std::string& stateCode, const std::string& county);

private:
	static const std::string apiRoot;
	static const std::string recentObservationsOfSpeciesInRegionURL;
	static const std::string recentObservationsOfSpeciesAtHotspotsURL;
	static const std::string taxonomyLookupURL;
	static const std::string locationFindURL;
	static const std::string locationListURL;

	static const std::string commonNameTag;
	static const std::string scientificNameTag;
	static const std::string locationNameTag;
	static const std::string locationIDTag;
	static const std::string latitudeTag;
	static const std::string longitudeTag;
	static const std::string observationDateTag;
	static const std::string isNotHotspotTag;
	static const std::string isReviewedTag;
	static const std::string isValidTag;

	static const std::string countryInfoListHeading;
	static const std::string stateInfoListHeading;
	static const std::string countyInfoListHeading;

	void BuildNameMaps(cJSON* root);
	static std::unordered_map<std::string, std::string> commonToScientificMap;
	static std::unordered_map<std::string, std::string> scientificToCommonMap;

	static bool ReadJSONObservationData(cJSON* item, ObservationInfo& info);

	static std::string GetUserInputOnResponse(const std::string& s, const std::string& field);

	struct CountryInfo
	{
		std::string code;
		std::string name;
		std::string longName;
		std::string localAbbreviation;
	};
	std::vector<CountryInfo> countryInfo;
	void BuildCountryInfo();
	static CountryInfo ParseCountryInfoLine(const std::string& line);

	struct StateInfo
	{
		std::string countryCode;
		std::string code;
		std::string name;
		std::string localAbbreviation;

		bool operator==(const StateInfo& s) const
		{
			return countryCode.compare(s.countryCode) == 0 &&
				code.compare(s.code) == 0 &&
				name.compare(s.name) == 0 &&
				localAbbreviation.compare(s.localAbbreviation) == 0;
		}

		bool operator!=(const StateInfo& s) const { return !(*this == s); }
	};

	std::vector<StateInfo> BuildStateInfo(const std::string& countryCode);
	static StateInfo ParseStateInfoLine(const std::string& line);

	struct CountyInfo
	{
		std::string countryCode;
		std::string stateCode;
		std::string code;
		std::string name;

		bool operator==(const CountyInfo& s) const
		{
			return countryCode.compare(s.countryCode) == 0 &&
				stateCode.compare(s.stateCode) == 0 &&
				code.compare(s.code) == 0 &&
				name.compare(s.name) == 0;
		}

		bool operator!=(const CountyInfo& s) const { return !(*this == s); }
	};

	std::vector<CountyInfo> BuildCountyInfo(const std::string& stateCode);
	static CountyInfo ParseCountyInfoLine(const std::string& line);
};

#endif// EBIRD_INTERFACE_H_
