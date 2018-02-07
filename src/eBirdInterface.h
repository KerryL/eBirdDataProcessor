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
	EBirdInterface(const std::string& apiKey) : tokenData(apiKey) {}

	struct LocationInfo
	{
		std::string name;
		std::string id;
		double latitude;
		double longitude;
		std::string countryCode;
		std::string subnational1Code;
		std::string subnational2Code;
	};

	std::vector<LocationInfo> GetHotspotsInRegion(const std::string& region);

	std::vector<LocationInfo> GetHotspotsWithRecentObservationsOf(
		const std::string& speciesCode, const std::string& region, const unsigned int& recentPeriod);

	struct ObservationInfo
	{
		std::string speciesCode;
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
		bool locationPrivate;

		bool dateIncludesTimeInfo = true;
	};

	std::vector<ObservationInfo> GetRecentObservationsOfSpeciesInRegion(const std::string& speciesCode,
		const std::string& region, const unsigned int& recentPeriod, const bool& includeProvisional, const bool& hotspotsOnly);

	std::string GetScientificNameFromCommonName(const std::string& commonName);
	std::string GetSpeciesCodeFromCommonName(const std::string& commonName);

	struct RegionInfo
	{
		std::string name;
		std::string code;
	};

	enum class RegionType
	{
		Country,
		SubNational1,
		SubNational2
	};

	std::vector<RegionInfo> GetSubRegions(const std::string& regionCode, const RegionType& type);

	std::string GetRegionCode(const std::string& country, const std::string& state = "", const std::string& county = "");

	// Returned codes are fully descriptive, i.e. state codes include country info, etc.
	std::string GetCountryCode(const std::string& country);
	std::string GetStateCode(const std::string& countryCode, const std::string& state);
	std::string GetCountyCode(const std::string& stateCode, const std::string& county);

private:
	static const std::string apiRoot;
	static const std::string observationDataPath;
	static const std::string recentPath;
	static const std::string taxonomyLookupEndpoint;
	static const std::string regionReferenceEndpoint;
	static const std::string hotspotReferenceEndpoint;

	static const std::string speciesCodeTag;
	static const std::string commonNameTag;
	static const std::string scientificNameTag;
	static const std::string locationNameTag;
	static const std::string locationIDTag;
	static const std::string latitudeTag;
	static const std::string longitudeTag;
	static const std::string countryCodeTag;
	static const std::string subnational1CodeTag;
	static const std::string subnational2CodeTag;
	static const std::string observationDateTag;
	static const std::string isNotHotspotTag;
	static const std::string isReviewedTag;
	static const std::string isValidTag;
	static const std::string locationPrivateTag;

	static const std::string countryTypeName;
	static const std::string subNational1TypeName;
	static const std::string subNational2TypeName;

	static const std::string nameTag;
	static const std::string codeTag;

	static const std::string eBirdTokenHeader;

	bool FetchEBirdNameData();
	void BuildNameMaps(cJSON* root);
	struct NameInfo
	{
		NameInfo() = default;
		NameInfo(const std::string& scientificName, const std::string& code)
			: scientificName(scientificName), code(code) {}
		std::string scientificName;
		std::string code;
	};
	static std::unordered_map<std::string, NameInfo> commonToScientificMap;
	static std::unordered_map<std::string, std::string> scientificToCommonMap;

	static bool ReadJSONObservationData(cJSON* item, ObservationInfo& info);

	static std::string GetUserInputOnResponse(const std::string& s, const std::string& field);

	struct SubNational1Info : public RegionInfo
	{
		std::vector<RegionInfo> subnational2Info;
	};

	struct CountryInfo : public RegionInfo
	{
		std::vector<SubNational1Info> subnational1Info;
	};

	std::vector<CountryInfo> storedRegionInfo;
	void BuildCountryInfo();
	std::vector<SubNational1Info> BuildSubNational1Info(const std::string& countryCode);

	static bool NameMatchesRegion(const std::string& name, const RegionInfo& region);

	struct TokenData : public ModificationData
	{
		TokenData(const std::string& token) : token(token) {}
		std::string token;
	};

	const TokenData tokenData;

	static bool AddTokenToCurlHeader(CURL* curl, const ModificationData* data);// Expects TokenData
};

#endif// EBIRD_INTERFACE_H_
