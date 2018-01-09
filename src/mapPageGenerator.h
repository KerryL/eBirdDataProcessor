// File:  mapPageGenerator.h
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

#ifndef MAP_PAGE_GENERATOR_H_
#define MAP_PAGE_GENERATOR_H_

// Local headers
#include "eBirdDataProcessor.h"
#include "googleFusionTablesInterface.h"

// Standard C++ headers
#include <fstream>

class MapPageGenerator
{
public:
	static bool WriteBestLocationsViewerPage(const std::string& htmlFileName,
		const std::string& googleMapsKey,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities,
		const std::string& clientId, const std::string& clientSecret);

private:
	static const std::string birdProbabilityTableName;
	struct Keys
	{
		Keys(const std::string& googleMapsKey, const std::string& clientId,
			const std::string& clientSecret) : googleMapsKey(googleMapsKey),
			clientId(clientId), clientSecret(clientSecret) {}

		const std::string googleMapsKey;
		const std::string clientId;
		const std::string clientSecret;
	};

	typedef GoogleFusionTablesInterface GFTI;

	static void WriteHeadSection(std::ofstream& f);
	static void WriteBody(std::ofstream& f, const Keys& keys,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities);
	static bool CreateFusionTable(
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		std::string& tableId, const Keys& keys);
	static bool GetLatitudeAndLongitudeFromCountyAndState(const std::string& state,
		const std::string& county, double& latitude, double& longitude,
		double& neLatitude, double& neLongitude, double& swLatitude, double& swLongitude,
		std::string& geographicName, const std::string& googleMapsKey);
	static bool GetStateAbbreviationFromFileName(const std::string& fileName, std::string& state);
	static bool GetCountyNameFromFileName(const std::string& fileName, std::string& county);

	static std::string CleanNameString(const std::string& s);
	static std::string CleanQueryString(const std::string& s);
	static std::string CleanFileName(const std::string& s);
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

		double latitude;
		double longitude;
		double neLatitude;
		double neLongitude;
		double swLatitude;
		double swLongitude;

		std::string geometryKML;

		std::array<double, 12> probabilities;
	};

	struct CountyGeometry
	{
		std::string state;
		std::string county;
		std::string kml;
	};

	static void PopulateCountyInfo(CountyInfo& info,
		const EBirdDataProcessor::YearFrequencyInfo& frequencyInfo,
		const std::string& googleMapsKey, const std::vector<CountyGeometry>& geometry);

	static Color InterpolateColor(const Color& minColor, const Color& maxColor, const double& value);
	static std::string ColorToHexString(const Color& c);
	static void GetHSV(const Color& c, double& hue, double& saturation, double& value);
	static Color ColorFromHSV( const double& hue, const double& saturation, const double& value);

	static GFTI::TableInfo BuildTableLayout();

	static bool GetCountyGeometry(GoogleFusionTablesInterface& fusionTables, std::vector<CountyGeometry>& geometry);
};

#endif// MAP_PAGE_GENERATOR_H_
