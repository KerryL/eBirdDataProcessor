// File:  mapPageGenerator.h
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

#ifndef MAP_PAGE_GENERATOR_H_
#define MAP_PAGE_GENERATOR_H_

// Local headers
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <fstream>

class MapPageGenerator
{
public:
	static bool WriteBestLocationsViewerPage(const std::string& htmlFileName,
		const std::string& googleMapsKey,
		const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities,
		const std::string& clientId, const std::string& clientSecret);

private:
	struct Keys
	{
		Keys(const std::string& googleMapsKey, const std::string& clientId,
			const std::string& clientSecret) : googleMapsKey(googleMapsKey),
			clientId(clientId), clientSecret(clientSecret) {}

		const std::string googleMapsKey;
		const std::string clientId;
		const std::string clientSecret;
	};

	static void WriteHeadSection(std::ofstream& f);
	static void WriteBody(std::ofstream& f, const Keys& keys,
		const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities);
	static bool CreateFusionTable(
		const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities,
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

	static Color InterpolateColor(const Color& minColor, const Color& maxColor, const double& value);
	static std::string ColorToHexString(const Color& c);
	static void GetHSV(const Color& c, double& hue, double& saturation, double& value);
	static Color ColorFromHSV( const double& hue, const double& saturation, const double& value);
};

#endif// MAP_PAGE_GENERATOR_H_
