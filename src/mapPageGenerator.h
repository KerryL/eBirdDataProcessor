// File:  mapPageGenerator.h
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

#ifndef MAP_PAGE_GENERATOR_H_
#define MAP_PAGE_GENERATOR_H_

// Local headers
#include "eBirdDataProcessor.h"
#include "googleFusionTablesInterface.h"
#include "threadPool.h"
#include "throttledSection.h"
#include "kmlLibraryManager.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <map>

class MapPageGenerator
{
public:
	MapPageGenerator(const String& kmlLibraryPath, const String& eBirdAPIKey);
	typedef EBirdDataProcessor::YearFrequencyInfo ObservationInfo;

	bool WriteBestLocationsViewerPage(const String& htmlFileName,
		const String& googleMapsKey, const String& eBirdAPIKey,
		const std::vector<ObservationInfo>& observationInfo,
		const String& clientId, const String& clientSecret);

private:
	static const String birdProbabilityTableName;

	struct NamePair
	{
		NamePair() = default;
		NamePair(const String& shortName, const String& longName)
			: shortName(shortName), longName(longName) {}

		String shortName;
		String longName;
	};

	static const std::array<NamePair, 12> monthNames;

	struct Keys
	{
		Keys(const String& googleMapsKey, const String& eBirdAPIKey, const String& clientId,
			const String& clientSecret) : googleMapsKey(googleMapsKey), eBirdAPIKey(eBirdAPIKey),
			clientId(clientId), clientSecret(clientSecret) {}

		const String googleMapsKey;
		const String eBirdAPIKey;
		const String clientId;
		const String clientSecret;
	};

	typedef GoogleFusionTablesInterface GFTI;

	static const unsigned int mapsAPIRateLimit;
	static const ThrottledSection::Clock::duration mapsAPIMinDuration;
	static const ThrottledSection::Clock::duration fusionTablesAPIMinDuration;
	ThrottledSection mapsAPIRateLimiter;
	ThrottledSection fusionTablesAPIRateLimiter;

	static const unsigned int columnCount;
	static const unsigned int importCellCountLimit;
	static const unsigned int importSizeLimit;// [bytes]

	bool UploadBuffer(GFTI& fusionTables, const String& tableId, const String& buffer);

	void WriteHeadSection(OStream& f, const Keys& keys,
		const std::vector<ObservationInfo>& observationProbabilities);
	static void WriteBody(OStream& f);
	void WriteScripts(OStream& f, const Keys& keys,
		const std::vector<ObservationInfo>& observationProbabilities);
	bool CreateFusionTable(
		std::vector<ObservationInfo> observationProbabilities,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		String& tableId, const Keys& keys, std::vector<unsigned int>& styleIds,
		std::vector<unsigned int>& templateIds);

	static String CleanQueryString(const String& s);
	static String ComputeColor(const double& frequency);

	struct Color
	{
		Color() = default;
		Color(const double& red, const double& green, const double& blue)
			: red(red), green(green), blue(blue) {}
		double red = 0;
		double green = 0;
		double blue = 0;
	};

	struct CountyInfo
	{
		String name;
		String state;
		String county;
		String country;
		String code;

		String geometryKML;

		std::array<double, 12> probabilities;
		std::array<std::vector<EBirdDataProcessor::FrequencyInfo>, 12> frequencyInfo;

		unsigned int rowId = 0;
	};

	struct RegionGeometry
	{
		String code;
		String kml;
	};

	KMLLibraryManager kmlLibrary;

	static bool GetExistingCountyData(std::vector<CountyInfo>& data,
		GFTI& fusionTables, const String& tableId);
	static bool ProcessJSONQueryResponse(cJSON* root, std::vector<CountyInfo>& data);
	static bool ProcessCSVQueryResponse(const String& csvData, std::vector<CountyInfo>& data);
	static bool ProcessCSVQueryLine(const String& line, CountyInfo& info);
	static bool ReadExistingCountyData(cJSON* row, CountyInfo& data);
	static std::vector<unsigned int> DetermineDeleteUpdateAdd(
		std::vector<CountyInfo>& existingData, std::vector<ObservationInfo>& newData);
	static bool CopyExistingDataForCounty(const ObservationInfo& entry,
		const std::vector<CountyInfo>& existingData, CountyInfo& newData,
		KMLLibraryManager& kmlLibrary,
		const std::vector<EBirdInterface::RegionInfo>& regionInfo);
	static std::vector<ObservationInfo>::const_iterator NewDataIncludesMatchForCounty(
		const std::vector<ObservationInfo>& newData, const CountyInfo& county);
	static bool ProbabilityDataHasChanged(const ObservationInfo& newData, const CountyInfo& existingData);
	static std::vector<unsigned int> FindDuplicatesAndBlanksToRemove(std::vector<CountyInfo>& existingData);

	static void LookupAndAssignKML(KMLLibraryManager& kmlLibrary, CountyInfo& data);

	template<typename T>
	static bool Read(cJSON* item, T& value);

	static Color InterpolateColor(const Color& minColor, const Color& maxColor, const double& value);
	static String ColorToHexString(const Color& c);
	static void GetHSV(const Color& c, double& hue, double& saturation, double& value);
	static Color ColorFromHSV( const double& hue, const double& saturation, const double& value);

	static String BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info);

	static GFTI::TableInfo BuildTableLayout();

	struct MapJobInfo : public ThreadPool::JobInfoBase
	{
		MapJobInfo() = default;
		MapJobInfo(CountyInfo& info, const ObservationInfo& frequencyInfo,
			const std::vector<EBirdInterface::RegionInfo>& regionNames, KMLLibraryManager& kmlLibrary,
			MapPageGenerator& mpg)
			: info(info), frequencyInfo(frequencyInfo), regionNames(regionNames),
			kmlLibrary(kmlLibrary), mpg(mpg) {}

		CountyInfo& info;
		const ObservationInfo& frequencyInfo;
		const std::vector<EBirdInterface::RegionInfo>& regionNames;
		KMLLibraryManager& kmlLibrary;
		MapPageGenerator& mpg;

		void DoJob() override;
	};

	std::map<String, std::vector<EBirdInterface::RegionInfo>> countryRegionInfoMap;

	bool VerifyTableStyles(GoogleFusionTablesInterface& fusionTables,
		const String& tableId, std::vector<unsigned int>& styleIds);
	static GoogleFusionTablesInterface::StyleInfo CreateStyle(const String& tableId,
		const String& month);

	bool VerifyTableTemplates(GoogleFusionTablesInterface& fusionTables,
		const String& tableId, std::vector<unsigned int>& templateIds);
	static GoogleFusionTablesInterface::TemplateInfo CreateTemplate(const String& tableId,
		const String& month);
	bool DeleteRowsBatch(GoogleFusionTablesInterface& fusionTables,
		const String& tableId, const std::vector<unsigned int>& rowIds);

	static bool GetConfirmationFromUser();
	static std::vector<unsigned int> FindInvalidSpeciesDataToRemove(GFTI& fusionTables, const String& tableId);
	static bool FindInvalidSpeciesDataInJSONResponse(cJSON* root, std::vector<unsigned int>& invalidRows);

	static std::vector<String> GetCountryCodeList(const std::vector<ObservationInfo>& observationProbabilities);
	static String AssembleCountyName(const String& country, const String& state, const String& county);
};

template<typename T>
bool MapPageGenerator::Read(cJSON* item, T& value)
{
	IStringStream ss;
	ss.str(UString::ToStringType(item->valuestring));
	return !(ss >> value).fail();
}

#endif// MAP_PAGE_GENERATOR_H_
