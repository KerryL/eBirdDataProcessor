// File:  mapPageGenerator.h
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating a web page that overlays observation information on an interactive map.

#ifndef MAP_PAGE_GENERATOR_H_
#define MAP_PAGE_GENERATOR_H_

// Local headers
#include "eBirdDataProcessor.h"
#include "threadPool.h"
#include "throttledSection.h"
#include "kmlLibraryManager.h"
#include "utilities/uString.h"
#include "logging/combinedLogger.h"

// Standard C++ headers
#include <map>
#include <unordered_map>
#include <shared_mutex>

class MapPageGenerator
{
public:
	MapPageGenerator(const LocationFindingParameters& locationFindingParameters,
		const std::vector<UString::String>& highDetailCountries, const UString::String& eBirdApiKey, const UString::String& kmlLibraryPath);
	typedef EBirdDataProcessor::YearFrequencyInfo ObservationInfo;

	bool WriteBestLocationsViewerPage(const UString::String& baseOutputFileName,
		const std::vector<ObservationInfo>& observationInfo);

private:
	static const UString::String htmlExtension;
	static const UString::String dataExtension;

	CombinedLogger<typename std::basic_ostream<UString::String::value_type>> log;

	const std::vector<UString::String> highDetailCountries;// TODO:  Still needed?

	struct NamePair
	{
		NamePair() = default;
		NamePair(const UString::String& shortName, const UString::String& longName)
			: shortName(shortName), longName(longName) {}

		UString::String shortName;
		UString::String longName;
	};

	static const std::array<NamePair, 48> weekNames;

	struct Keys
	{
		Keys(const UString::String& googleMapsKey, const UString::String& eBirdAPIKey, const UString::String& clientId,
			const UString::String& clientSecret) : googleMapsKey(googleMapsKey), eBirdAPIKey(eBirdAPIKey),
			clientId(clientId), clientSecret(clientSecret) {}

		const UString::String googleMapsKey;
		const UString::String eBirdAPIKey;
		const UString::String clientId;
		const UString::String clientSecret;
	};

	mutable EBirdInterface ebi;
	std::shared_timed_mutex codeToNameMapMutex;
	std::unordered_map<UString::String, UString::String> eBirdRegionCodeToNameMap;
	void AddRegionCodesToMap(const UString::String& parentCode, const EBirdInterface::RegionType& regionType);

	bool WriteHTML(const UString::String& fileName, const UString::String& dataFileName) const;
	bool WriteGeoJSONData(const UString::String& fileName, std::vector<ObservationInfo> observationProbabilities);

	static void WriteHeadSection(UString::OStream& f);
	static void WriteBody(UString::OStream& f, const UString::String& dataFileName);
	static void WriteScripts(UString::OStream& f);

	struct CountyInfo
	{
		UString::String name;
		UString::String state;
		UString::String county;
		UString::String country;
		UString::String code;

		UString::String geometryKML;

		struct WeekInfo
		{
			double probability;
			std::vector<EBirdDataProcessor::FrequencyInfo> frequencyInfo;
		};

		std::array<WeekInfo, 48> weekInfo;
	};

	static bool CreateJSONData(const std::vector<CountyInfo>& observationData, const double& kmlReductionLimit, cJSON*& geoJSON);
	static bool BuildObservationRecord(const CountyInfo& observation, const double& kmlReductionLimit, cJSON* json);
	static bool BuildWeekInfo(CountyInfo::WeekInfo weekInfo, cJSON* json);

	static UString::String ForceTrailingSlash(const UString::String& path);

	struct RegionGeometry
	{
		UString::String code;
		UString::String kml;
	};

	KMLLibraryManager kmlLibrary;
	const double kmlReductionLimit;

	void LookupAndAssignKML(CountyInfo& data);

	template<typename T>
	static bool Read(cJSON* item, T& value);

	static UString::String BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info);

	struct MapJobInfo : public ThreadPool::JobInfoBase
	{
		MapJobInfo() = default;
		MapJobInfo(CountyInfo& info, const ObservationInfo& frequencyInfo,
			const std::vector<EBirdInterface::RegionInfo>& regionNames, MapPageGenerator& mpg)
			: info(info), frequencyInfo(frequencyInfo), regionNames(regionNames), mpg(mpg){}

		CountyInfo& info;
		const ObservationInfo& frequencyInfo;
		const std::vector<EBirdInterface::RegionInfo>& regionNames;
		MapPageGenerator& mpg;

		void DoJob() override;
	};

	std::map<UString::String, std::vector<EBirdInterface::RegionInfo>> countryRegionInfoMap;
	std::map<UString::String, EBirdInterface::RegionInfo> countryLevelRegionInfoMap;

	static bool GetConfirmationFromUser();
	static std::vector<UString::String> GetCountryCodeList(const std::vector<ObservationInfo>& observationProbabilities);
	static UString::String AssembleCountyName(const UString::String& country, const UString::String& state, const UString::String& county);

	std::vector<EBirdInterface::RegionInfo> GetFullCountrySubRegionList(const UString::String& countryCode) const;
	void LookupEBirdRegionNames(const UString::String& countryCode, const UString::String& subRegion1Code, UString::String& country, UString::String& subRegion1);

	static UString::String WrapEmptyString(const UString::String& s);
};

template<typename T>
bool MapPageGenerator::Read(cJSON* item, T& value)
{
	UString::IStringStream ss;
	ss.str(UString::ToStringType(item->valuestring));
	return !(ss >> value).fail();
}

#endif// MAP_PAGE_GENERATOR_H_
