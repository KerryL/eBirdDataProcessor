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
	EBirdInterface(const UString::String& apiKey) : tokenData(apiKey) {}

	struct LocationInfo
	{
		UString::String name;
		UString::String id;
		double latitude;
		double longitude;
		UString::String countryCode;
		UString::String subnational1Code;
		UString::String subnational2Code;
	};

	std::vector<LocationInfo> GetHotspotsInRegion(const UString::String& region);

	std::vector<LocationInfo> GetHotspotsWithRecentObservationsOf(
		const UString::String& speciesCode, const UString::String& region, const unsigned int& recentPeriod);
		
	enum class Protocol
	{
		Incidental,// P20
		Stationary,// P21
		Traveling,// P22
		Area,// P23
		Banding,// P33
		RustyBlackbirdSMB,// P41
		CWCPointCount,// P46
		CWCAreaSearch, //P47
		Random,// P48
		OiledBirds,// P52
		NocturnalFlightCall,// P54
		AudobonCoastalBirdSurvey,// P58
		TNCCaliforniaWaterbirdCount,// P59
		Paleagic,// P60
		Historical,// P62
		CaliforniaBrownPelicanSurvey,// P69
		PROALAS,// P73
		InternationalShorebirdSurvey,// P74
		TricoloredBlackbirdWinterSurvey,//P75
		Other// Another dozen or so protocols which are no longer active for new observation data
	};
	
	static Protocol MapProtocolCodeToProtocol(const std::string& code);

	struct ObservationInfo
	{
		UString::String speciesCode;
		UString::String commonName;
		UString::String scientificName;
		std::tm observationDate;
		unsigned int count;
		UString::String locationID;
		bool isNotHotspot;
		UString::String locationName;
		double latitude;
		double longitude;
		bool observationReviewed;
		bool observationValid;
		bool locationPrivate;
		double distance = 0.0;// [km]
		unsigned int duration = 0;// [min]
		Protocol protocol = Protocol::Other;

		bool dateIncludesTimeInfo = true;
	};

	std::vector<ObservationInfo> GetRecentObservationsOfSpeciesInRegion(const UString::String& speciesCode,
		const UString::String& region, const unsigned int& recentPeriod, const bool& includeProvisional, const bool& hotspotsOnly);

	UString::String GetScientificNameFromCommonName(const UString::String& commonName);
	UString::String GetSpeciesCodeFromCommonName(const UString::String& commonName);

	struct RegionInfo
	{
		UString::String name;
		UString::String code;
	};

	enum class RegionType
	{
		Country,
		SubNational1,
		SubNational2,
		MostDetailAvailable
	};

	std::vector<RegionInfo> GetSubRegions(const UString::String& regionCode, const RegionType& type);

	UString::String GetRegionCode(const UString::String& country, const UString::String& state = UString::String(), const UString::String& county = UString::String());
	std::vector<UString::String> GetRegionCodes(const std::vector<UString::String>& countries,
		const std::vector<UString::String>& states, const std::vector<UString::String>& counties);

	// Returned codes are fully descriptive, i.e. state codes include country info, etc.
	UString::String GetCountryCode(const UString::String& country);
	UString::String GetStateCode(const UString::String& countryCode, const UString::String& state);
	UString::String GetCountyCode(const UString::String& stateCode, const UString::String& county);

private:
	static const UString::String apiRoot;
	static const UString::String observationDataPath;
	static const UString::String recentPath;
	static const UString::String taxonomyLookupEndpoint;
	static const UString::String regionReferenceEndpoint;
	static const UString::String hotspotReferenceEndpoint;

	static const UString::String speciesCodeTag;
	static const UString::String commonNameTag;
	static const UString::String scientificNameTag;
	static const UString::String locationNameTag;
	static const UString::String locationIDTag;
	static const UString::String latitudeTag;
	static const UString::String longitudeTag;
	static const UString::String countryCodeTag;
	static const UString::String subnational1CodeTag;
	static const UString::String subnational2CodeTag;
	static const UString::String observationDateTag;
	static const UString::String isNotHotspotTag;
	static const UString::String isReviewedTag;
	static const UString::String isValidTag;
	static const UString::String locationPrivateTag;

	static const UString::String countryTypeName;
	static const UString::String subNational1TypeName;
	static const UString::String subNational2TypeName;

	static const UString::String nameTag;
	static const UString::String codeTag;

	static const UString::String errorTag;
	static const UString::String titleTag;
	static const UString::String statusTag;

	static const UString::String eBirdTokenHeader;

	bool FetchEBirdNameData();
	void BuildNameMaps(cJSON* root);
	struct NameInfo
	{
		NameInfo() = default;
		NameInfo(const UString::String& scientificName, const UString::String& code)
			: scientificName(scientificName), code(code) {}
		UString::String scientificName;
		UString::String code;
	};
	static std::unordered_map<UString::String, NameInfo> commonToScientificMap;
	static std::unordered_map<UString::String, UString::String> scientificToCommonMap;

	static bool ReadJSONObservationData(cJSON* item, ObservationInfo& info);

	static UString::String GetUserInputOnResponse(const UString::String& s, const UString::String& field);

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
	std::vector<SubNational1Info> BuildSubNational1Info(const UString::String& countryCode);

	static bool NameMatchesRegion(const UString::String& name, const RegionInfo& region);

	struct TokenData : public ModificationData
	{
		TokenData(const UString::String& token) : token(token) {}
		UString::String token;
	};

	const TokenData tokenData;

	static bool AddTokenToCurlHeader(CURL* curl, const ModificationData* data);// Expects TokenData

	struct ErrorInfo
	{
		UString::String title;
		UString::String code;
		UString::String status;
	};

	static bool ResponseHasErrors(cJSON *root, std::vector<ErrorInfo>& errors);
};

#endif// EBIRD_INTERFACE_H_
