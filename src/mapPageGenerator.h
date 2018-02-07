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

// Standard C++ headers
#include <fstream>
#include <map>

class MapPageGenerator
{
public:
	MapPageGenerator();
	typedef EBirdDataProcessor::YearFrequencyInfo ObservationInfo;

	bool WriteBestLocationsViewerPage(const std::string& htmlFileName,
		const std::string& googleMapsKey, const std::string& eBirdAPIKey,
		const std::vector<ObservationInfo>& observationInfo,
		const std::string& clientId, const std::string& clientSecret);

private:
	static const std::string birdProbabilityTableName;

	struct NamePair
	{
		NamePair() = default;
		NamePair(const std::string& shortName, const std::string& longName)
			: shortName(shortName), longName(longName) {}

		std::string shortName;
		std::string longName;
	};

	static const std::array<NamePair, 12> monthNames;

	struct Keys
	{
		Keys(const std::string& googleMapsKey, const std::string& eBirdAPIKey, const std::string& clientId,
			const std::string& clientSecret) : googleMapsKey(googleMapsKey), eBirdAPIKey(eBirdAPIKey),
			clientId(clientId), clientSecret(clientSecret) {}

		const std::string googleMapsKey;
		const std::string eBirdAPIKey;
		const std::string clientId;
		const std::string clientSecret;
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

	bool UploadBuffer(GFTI& fusionTables, const std::string& tableId, const std::string& buffer);

	void WriteHeadSection(std::ostream& f, const Keys& keys,
		const std::vector<ObservationInfo>& observationProbabilities);
	static void WriteBody(std::ostream& f);
	void WriteScripts(std::ostream& f, const Keys& keys,
		const std::vector<ObservationInfo>& observationProbabilities);
	bool CreateFusionTable(
		std::vector<ObservationInfo> observationProbabilities,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		std::string& tableId, const Keys& keys, std::vector<unsigned int>& styleIds,
		std::vector<unsigned int>& templateIds);

	static std::string CleanQueryString(const std::string& s);
	static std::string ComputeColor(const double& frequency);

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
		std::string name;
		std::string state;
		std::string county;
		std::string country;
		std::string code;

		std::string geometryKML;

		std::array<double, 12> probabilities;
		std::array<std::vector<EBirdDataProcessor::FrequencyInfo>, 12> frequencyInfo;

		unsigned int rowId = 0;
	};

	struct CountyGeometry
	{
		std::string state;
		std::string county;
		std::string kml;
	};

	static bool GetExistingCountyData(std::vector<CountyInfo>& data,
		GFTI& fusionTables, const std::string& tableId);
	static bool ProcessJSONQueryResponse(cJSON* root, std::vector<CountyInfo>& data);
	static bool ProcessCSVQueryResponse(const std::string& csvData, std::vector<CountyInfo>& data);
	static bool ProcessCSVQueryLine(const std::string& line, CountyInfo& info);
	static bool ReadExistingCountyData(cJSON* row, CountyInfo& data);
	static std::vector<unsigned int> DetermineDeleteUpdateAdd(
		std::vector<CountyInfo>& existingData, std::vector<ObservationInfo>& newData);
	static bool CopyExistingDataForCounty(const ObservationInfo& entry,
		const std::vector<CountyInfo>& existingData, CountyInfo& newData,
		const std::vector<CountyGeometry>& geometry);
	static std::vector<ObservationInfo>::const_iterator NewDataIncludesMatchForCounty(
		const std::vector<ObservationInfo>& newData, const CountyInfo& county);
	static bool ProbabilityDataHasChanged(const ObservationInfo& newData, const CountyInfo& existingData);
	static std::vector<unsigned int> FindDuplicatesAndBlanksToRemove(std::vector<CountyInfo>& existingData);

	static void LookupAndAssignKML(const std::vector<CountyGeometry>& geometry, CountyInfo& data);

	template<typename T>
	static bool Read(cJSON* item, T& value);

	static Color InterpolateColor(const Color& minColor, const Color& maxColor, const double& value);
	static std::string ColorToHexString(const Color& c);
	static void GetHSV(const Color& c, double& hue, double& saturation, double& value);
	static Color ColorFromHSV( const double& hue, const double& saturation, const double& value);

	static std::string BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info);

	static GFTI::TableInfo BuildTableLayout();

	static bool GetCountyGeometry(GoogleFusionTablesInterface& fusionTables, std::vector<CountyGeometry>& geometry);

	struct MapJobInfo : public ThreadPool::JobInfoBase
	{
		MapJobInfo() = default;
		MapJobInfo(CountyInfo& info, const ObservationInfo& frequencyInfo,
			const std::vector<EBirdInterface::RegionInfo>& regionNames, const std::vector<CountyGeometry>& geometry,
			MapPageGenerator& mpg)
			: info(info), frequencyInfo(frequencyInfo), regionNames(regionNames),
			geometry(geometry), mpg(mpg) {}

		CountyInfo& info;
		const ObservationInfo& frequencyInfo;
		const std::vector<EBirdInterface::RegionInfo>& regionNames;
		const std::vector<CountyGeometry>& geometry;
		MapPageGenerator& mpg;

		void DoJob() override;
	};

	std::map<std::string, std::vector<EBirdInterface::RegionInfo>> countryRegionInfoMap;

	bool VerifyTableStyles(GoogleFusionTablesInterface& fusionTables,
		const std::string& tableId, std::vector<unsigned int>& styleIds);
	static GoogleFusionTablesInterface::StyleInfo CreateStyle(const std::string& tableId,
		const std::string& month);

	bool VerifyTableTemplates(GoogleFusionTablesInterface& fusionTables,
		const std::string& tableId, std::vector<unsigned int>& templateIds);
	static GoogleFusionTablesInterface::TemplateInfo CreateTemplate(const std::string& tableId,
		const std::string& month);
	bool DeleteRowsBatch(GoogleFusionTablesInterface& fusionTables,
		const std::string& tableId, const std::vector<unsigned int>& rowIds);

	static bool GetConfirmationFromUser();
	static std::vector<unsigned int> FindInvalidSpeciesDataToRemove(GFTI& fusionTables, const std::string& tableId);
	static bool FindInvalidSpeciesDataInJSONResponse(cJSON* root, std::vector<unsigned int>& invalidRows);

	static std::vector<std::string> GetCountryCodeList(const std::vector<ObservationInfo>& observationProbabilities);
};

template<typename T>
bool MapPageGenerator::Read(cJSON* item, T& value)
{
	std::istringstream ss;
	ss.str(item->valuestring);
	return !(ss >> value).fail();
}

#endif// MAP_PAGE_GENERATOR_H_
