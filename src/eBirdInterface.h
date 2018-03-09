// File:  eBirdInterface.h
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.  See https://confluence.cornell.edu/display/CLOISAPI/eBird+API+1.1.

#ifndef EBIRD_INTERFACE_H_
#define EBIRD_INTERFACE_H_

// Local headers
#include "utilities/uString.h"
#include "email/jsonInterface.h"

// Standard C++ headers
#include <vector>
#include <unordered_map>
#include <ctime>

class EBirdInterface : public JSONInterface
{
public:
	EBirdInterface(const String& apiKey) : tokenData(apiKey) {}

	struct LocationInfo
	{
		String name;
		String id;
		double latitude;
		double longitude;
		String countryCode;
		String subnational1Code;
		String subnational2Code;
	};

	std::vector<LocationInfo> GetHotspotsInRegion(const String& region);

	std::vector<LocationInfo> GetHotspotsWithRecentObservationsOf(
		const String& speciesCode, const String& region, const unsigned int& recentPeriod);

	struct ObservationInfo
	{
		String speciesCode;
		String commonName;
		String scientificName;
		std::tm observationDate;
		unsigned int count;
		String locationID;
		bool isNotHotspot;
		String locationName;
		double latitude;
		double longitude;
		bool observationReviewed;
		bool observationValid;
		bool locationPrivate;

		bool dateIncludesTimeInfo = true;
	};

	std::vector<ObservationInfo> GetRecentObservationsOfSpeciesInRegion(const String& speciesCode,
		const String& region, const unsigned int& recentPeriod, const bool& includeProvisional, const bool& hotspotsOnly);

	String GetScientificNameFromCommonName(const String& commonName);
	String GetSpeciesCodeFromCommonName(const String& commonName);

	struct RegionInfo
	{
		String name;
		String code;
	};

	enum class RegionType
	{
		Country,
		SubNational1,
		SubNational2,
		MostDetailAvailable
	};

	std::vector<RegionInfo> GetSubRegions(const String& regionCode, const RegionType& type);

	String GetRegionCode(const String& country, const String& state = String(), const String& county = String());

	// Returned codes are fully descriptive, i.e. state codes include country info, etc.
	String GetCountryCode(const String& country);
	String GetStateCode(const String& countryCode, const String& state);
	String GetCountyCode(const String& stateCode, const String& county);

private:
	static const String apiRoot;
	static const String observationDataPath;
	static const String recentPath;
	static const String taxonomyLookupEndpoint;
	static const String regionReferenceEndpoint;
	static const String hotspotReferenceEndpoint;

	static const String speciesCodeTag;
	static const String commonNameTag;
	static const String scientificNameTag;
	static const String locationNameTag;
	static const String locationIDTag;
	static const String latitudeTag;
	static const String longitudeTag;
	static const String countryCodeTag;
	static const String subnational1CodeTag;
	static const String subnational2CodeTag;
	static const String observationDateTag;
	static const String isNotHotspotTag;
	static const String isReviewedTag;
	static const String isValidTag;
	static const String locationPrivateTag;

	static const String countryTypeName;
	static const String subNational1TypeName;
	static const String subNational2TypeName;

	static const String nameTag;
	static const String codeTag;

	static const String eBirdTokenHeader;

	bool FetchEBirdNameData();
	void BuildNameMaps(cJSON* root);
	struct NameInfo
	{
		NameInfo() = default;
		NameInfo(const String& scientificName, const String& code)
			: scientificName(scientificName), code(code) {}
		String scientificName;
		String code;
	};
	static std::unordered_map<String, NameInfo> commonToScientificMap;
	static std::unordered_map<String, String> scientificToCommonMap;

	static bool ReadJSONObservationData(cJSON* item, ObservationInfo& info);

	static String GetUserInputOnResponse(const String& s, const String& field);

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
	std::vector<SubNational1Info> BuildSubNational1Info(const String& countryCode);

	static bool NameMatchesRegion(const String& name, const RegionInfo& region);

	struct TokenData : public ModificationData
	{
		TokenData(const String& token) : token(token) {}
		String token;
	};

	const TokenData tokenData;

	static bool AddTokenToCurlHeader(CURL* curl, const ModificationData* data);// Expects TokenData
};

#endif// EBIRD_INTERFACE_H_
