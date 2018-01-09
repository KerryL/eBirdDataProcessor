// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

// Local headers
#include "mapPageGenerator.h"
#include "googleMapsInterface.h"

// Standard C++ headers
#include <sstream>
#include <iomanip>
#include <cctype>
#include <iostream>
#include <thread>
#include <algorithm>

const std::string MapPageGenerator::birdProbabilityTableName("Bird Probability Table");

bool MapPageGenerator::WriteBestLocationsViewerPage(const std::string& htmlFileName,
	const std::string& googleMapsKey,
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities,
	const std::string& clientId, const std::string& clientSecret)
{
	std::ofstream file(htmlFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}

	WriteHeadSection(file);

	const Keys keys(googleMapsKey, clientId, clientSecret);
	WriteBody(file, keys, observationProbabilities);

	return true;
}

void MapPageGenerator::WriteHeadSection(std::ofstream& f)
{
	f << "<!DOCTYPE html>\n"
		<< "<html>\n"
		<< "  <head>\n"
		<< "    <title>Best Locations for New Species</title>\n"
		<< "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
		<< "    <meta charset=\"utf-8\">\n"
		<< "    <style>\n"
		<< "      #map {\n"
		<< "        height: 100%;\n"
		<< "      }\n"
		<< "      html, body {\n"
		<< "        height: 100%;\n"
		<< "        margin: 0;\n"
		<< "        padding: 0;\n"
		<< "      }\n"
		<< "    </style>\n"
		<< "  </head>\n";
}

void MapPageGenerator::WriteBody(std::ofstream& f, const Keys& keys,
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities)
{
	double northeastLatitude, northeastLongitude;
	double southwestLatitude, southwestLongitude;
	std::string tableId;

	if (!CreateFusionTable(observationProbabilities, northeastLatitude,
		northeastLongitude, southwestLatitude, southwestLongitude, tableId, keys))
	{
		std::cerr << "Failed to create fusion table\n";
		return;
	}

	const double centerLatitude(0.5 * (northeastLatitude + southwestLatitude));
	const double centerLongitude(0.5 * (northeastLongitude + southwestLongitude));

	f << "  <body>\n"
		<< "    <div id=\"map\"></div>\n"
    	<< "    <script>\n"
    	<< "      var map;\n"
    	<< "      function initMap() {\n"
    	<< "        map = new google.maps.Map(document.getElementById('map'), {\n"
    	<< "          zoom: 16,\n"
    	<< "          center: new google.maps.LatLng(" << centerLatitude 
			<< ',' << centerLongitude << "),\n"
    	<< "          mapTypeId: 'roadmap'\n"
    	<< "        });\n"
		<< '\n'
		<< "        map.fitBounds({north:" << northeastLatitude << ", east:" << northeastLongitude
			<< ", south:" << southwestLatitude << ", west:" << southwestLongitude << "});\n"
		<< '\n'
		<< "        var countyLayer = new google.maps.FusionTablesLayer({\n"
        << "          query: {\n"
        << "            select: 'geometry',\n"
        << "            from: '" << tableId << "'\n"
		<< "          }\n";
		/*<< "            where: \"'State-County' IN (";

		bool needComma(false);
		for (const auto& stateCounty : stateCountyList)
		{
			if (needComma)
				f << ", ";
			needComma = true;
			f << '\'' << CleanQueryString(stateCounty) << '\'';
		}

		f << ")\"\n"
        << "          },\n"
        << "          styles: [{\n"
        << "            polygonOptions: {\n"
        << "              fillColor: '#00FF00',\n"
        << "              fillOpacity: 0.3,\n"
		<< "              strokeColor: '#FFFFFF'\n"
        << "            }\n"
		<< "          }";

		for (const auto& stateCounty : stateCountyList)
		{
			// TODO:  This method is no good - fusion tables only supports 5 different style options
			f << ",{\n"
			<< "            where: \"'State-County' = '" << stateCounty << "'\",\n"
			<< "            polygonOptions: {\n"
			<< "              fillColor: '" << ComputeColor(stateCounty, observationProbabilities) << "'\n"
			<< "            }\n"
			<< "          }";
		}

        f << "]\n"*/
        f << "        });\n"
		<< "        countyLayer.setMap(map);\n"
		<< "      }\n"
		<< "    </script>\n"
		<< "    <script async defer src=\"https://maps.googleapis.com/maps/api/js?key=" << keys.googleMapsKey << "&callback=initMap\">\n"
		<< "    </script>\n"
		<< "  </body>\n"
		<< "</html>";
}

bool MapPageGenerator::CreateFusionTable(
	const std::vector<EBirdDataProcessor::YearFrequencyInfo>& observationProbabilities,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	std::string& tableId, const Keys& keys)
{
	GFTI fusionTables("Bird Probability Tool", keys.clientId, keys.clientSecret);
	std::vector<GFTI::TableInfo> tableList;
	if (!fusionTables.ListTables(tableList))
	{
		std::cerr << "Failed to generate list of existing tables\n";
		return false;
	}

	tableId.clear();
	for (const auto& t : tableList)
	{
		if (t.name.compare(birdProbabilityTableName) == 0)
		{
			std::cout << "Found existing table " << t.tableId << std::endl;
			tableId = t.tableId;
			if (!fusionTables.DeleteAllRows(t.tableId))
				std::cerr << "Warning:  Failed to delete existing rows from table\n";
			break;
		}
	}

	if (tableId.empty())
	{
		GFTI::TableInfo tableInfo(BuildTableLayout());
		if (!fusionTables.CreateTable(tableInfo))
		{
			std::cerr << "Failed to create fusion table\n";
			return false;
		}

		tableId = tableInfo.tableId;
		std::cout << "Created new fusion table " << tableId << std::endl;
		if (!fusionTables.SetTableAccess(tableInfo.tableId, GoogleFusionTablesInterface::TableAccess::Public))
			std::cerr << "Failed to make table public\n";
	}

	std::vector<CountyGeometry> geometry;
	if (!GetCountyGeometry(fusionTables, geometry))
		return false;

	std::vector<CountyInfo> countyInfo(observationProbabilities.size());
	std::vector<std::thread> threads(countyInfo.size());
	auto countyIt(countyInfo.begin());
	auto threadIt(threads.begin());
	for (const auto& entry : observationProbabilities)
	{
		*threadIt = std::thread(PopulateCountyInfo, std::ref(*countyIt), std::ref(entry), keys.googleMapsKey, geometry);
		++threadIt;
		++countyIt;
	}

	for (auto& t : threads)
		t.join();

	std::ostringstream ss;
	for (const auto& c : countyInfo)
	{
		if (ss.str().empty())
		{
			northeastLatitude = c.latitude;
			northeastLongitude = c.longitude;
			southwestLatitude = c.latitude;
			southwestLongitude = c.longitude;
		}

		if (c.neLatitude > northeastLatitude)
			northeastLatitude = c.neLatitude;
		if (c.neLongitude > northeastLongitude)
			northeastLongitude = c.neLongitude;
		if (c.swLatitude < southwestLatitude)
			southwestLatitude = c.swLatitude;
		if (c.swLongitude < southwestLongitude)
			southwestLongitude = c.swLongitude;

		ss << c.state << ',' << c.county << ',' << c.state + '-' + c.county << ",\"" << c.name << "\","
			<< c.latitude << ' ' << c.longitude;
		for (const auto& p : c.probabilities)
			ss << ',' << p * 100.0;
		for (const auto& p : c.probabilities)
			ss << ',' << ComputeColor(p);
		ss << ',' << c.geometryKML <<'\n';
	}

	if (!fusionTables.Import(tableId, ss.str()))
	{
		std::cerr << "Failed to import data\n";
		return false;
	}

	return true;
}

void MapPageGenerator::PopulateCountyInfo(CountyInfo& info,
	const EBirdDataProcessor::YearFrequencyInfo& frequencyInfo,
	const std::string& googleMapsKey, const std::vector<CountyGeometry>& geometry)
{
	if (!GetStateAbbreviationFromFileName(frequencyInfo.locationHint, info.state))
		std::cerr << "Warning:  Failed to get state abberviation for '" << frequencyInfo.locationHint << "'\n";

	if (!GetCountyNameFromFileName(frequencyInfo.locationHint, info.county))
		std::cerr << "Warning:  Failed to get county name for '" << frequencyInfo.locationHint << "'\n";

	if (!GetLatitudeAndLongitudeFromCountyAndState(info.state, info.county + " County",
		info.latitude, info.longitude, info.neLatitude,
		info.neLongitude, info.swLatitude, info.swLongitude, info.name, googleMapsKey))
		std::cerr << "Warning:  Failed to get location information for '" << frequencyInfo.locationHint << "'\n";

	const std::string endString(" County");
	std::string::size_type endPosition(std::min(
		info.name.find(endString), info.name.find(',')));
	if (endPosition == std::string::npos)
		std::cerr << "Warning:  Failed to extract county name from '" << info.name << "'\n";
	else
		info.county = info.name.substr(0, endPosition);

	const std::string::size_type saintStart(info.name.find("St "));
	if (saintStart != std::string::npos)
	{
		info.name.insert(saintStart + 2, ".");
		info.county = info.name.substr(0, endPosition + 1);
	}

	info.county = info.name.substr(0, info.name.find(','));// TODO:  Make robust
	info.probabilities = std::move(frequencyInfo.probabilities);

	for (const auto& g : geometry)
	{
		if (g.state.compare(info.state) == 0 && g.county.compare(info.county) == 0)
		{
			info.geometryKML = g.kml;
			break;
		}
	}
}

GoogleFusionTablesInterface::TableInfo MapPageGenerator::BuildTableLayout()
{
	GFTI::TableInfo tableInfo;
	tableInfo.name = birdProbabilityTableName;
	tableInfo.description = "Table of bird observation probabilities";
	tableInfo.isExportable = true;

	tableInfo.columns.push_back(GFTI::ColumnInfo("State", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("County", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("State-County", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Name", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Location", GFTI::ColumnType::Location));

	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Jan", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Feb", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Mar", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Apr", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-May", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Jun", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Jul", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Aug", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Sep", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Oct", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Nov", GFTI::ColumnType::Number));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-Dec", GFTI::ColumnType::Number));

	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Jan", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Feb", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Mar", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Apr", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-May", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Jun", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Jul", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Aug", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Sep", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Oct", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Nov", GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo("Color-Dec", GFTI::ColumnType::String));

	tableInfo.columns.push_back(GFTI::ColumnInfo("Geometry", GFTI::ColumnType::Location));

	return tableInfo;
}

bool MapPageGenerator::GetLatitudeAndLongitudeFromCountyAndState(const std::string& state,
	const std::string& county, double& latitude, double& longitude,
	double& neLatitude, double& neLongitude, double& swLatitude, double& swLongitude,
	std::string& geographicName, const std::string& googleMapsKey)
{
	GoogleMapsInterface gMap("County Lookup Tool", googleMapsKey);
	if (!gMap.LookupCoordinates(county + " " + state, geographicName,
		latitude, longitude, neLatitude, neLongitude, swLatitude, swLongitude,
		"County"))
		return false;

	// TODO:  Check address string to make sure its a good match

	return true;
}

bool MapPageGenerator::GetStateAbbreviationFromFileName(const std::string& fileName, std::string& state)
{
	const std::string searchString("FrequencyData.csv");
	const std::string::size_type position(fileName.find(searchString));
	if (position == std::string::npos || position < 2)
	{
		std::cerr << "Failed to extract state abbreviation from '" << fileName << "'\n";
		return false;
	}

	state = fileName.substr(position - 2, 2);

	return true;
}

bool MapPageGenerator::GetCountyNameFromFileName(const std::string& fileName, std::string& county)
{
	const std::string searchString("FrequencyData.csv");
	const std::string::size_type position(fileName.find(searchString));
	if (position == std::string::npos || position < 3)
	{
		std::cerr << "Failed to extract county name from '" << fileName << "'\n";
		return false;
	}

	county = fileName.substr(0, position - 2);

	return true;
}

std::string MapPageGenerator::CleanNameString(const std::string& s)
{
	std::string clean;

	for (const auto& c : s)
	{
		if (c == '\'')
			clean.append("\\'");
		else
			clean.push_back(c);
	}

	return clean;
}

std::string MapPageGenerator::CleanQueryString(const std::string& s)
{
	std::string clean;

	for (const auto& c : s)
	{
		if (c == '\'')
			clean.append("''");
		else
			clean.push_back(c);
	}

	return clean;
}

std::string MapPageGenerator::ComputeColor(const double& frequency)
{
	const Color minColor(0.0, 0.75, 0.365);// Greenish
	const Color maxColor(1.0, 0.0, 0.0);// Red
	return ColorToHexString(InterpolateColor(minColor, maxColor, frequency));
}

MapPageGenerator::Color MapPageGenerator::InterpolateColor(
	const Color& minColor, const Color& maxColor, const double& value)
{
	double minHue, minSat, minVal, maxHue, maxSat, maxVal;
	GetHSV(minColor, minHue, minSat, minVal);
	GetHSV(maxColor, maxHue, maxSat, maxVal);

	// Interpolation based on 0..1 range
	return ColorFromHSV(minHue + (maxHue - minHue) * value, minSat + (maxSat - minSat) * value,
		minVal + (maxVal - minVal) * value);
}

std::string MapPageGenerator::ColorToHexString(const Color& c)
{
	std::ostringstream hex;
	hex << "#" << std::hex
		<< std::setw(2) << std::setfill('0') << static_cast<int>(c.red * 255.0)
		<< std::setw(2) << std::setfill('0') << static_cast<int>(c.green * 255.0)
		<< std::setw(2) << std::setfill('0') << static_cast<int>(c.blue * 255.0);
	
	std::string hexString(hex.str());
	std::transform(hexString.begin(), hexString.end(), hexString.begin(), std::toupper);
	return hexString;
}

void MapPageGenerator::GetHSV(const Color& c, double& hue, double& saturation, double& value)
{
	value = std::max(std::max(c.red, c.green), c.blue);
	const double delta(value - std::min(std::min(c.red, c.green), c.blue));

	if (delta == 0)
		hue = 0.0;
	else if (value == c.red)
		hue = fmod((c.green - c.blue) / delta, 6.0) / 6.0;
	else if (value == c.green)
		hue = ((c.blue - c.red) / delta + 2.0) / 6.0;
	else// if (value == c.blue)
		hue = ((c.red - c.green) / delta + 4.0) / 6.0;

	if (hue < 0.0)
		hue += 1.0;
	assert(hue >= 0.0 && hue <= 1.0);

	if (value == 0.0)
		saturation = 0.0;
	else
		saturation = delta / value;
}

MapPageGenerator::Color MapPageGenerator::ColorFromHSV(
	const double& hue, const double& saturation, const double& value)
{
	assert(hue >= 0.0 && hue <= 1.0);
	assert(saturation >= 0.0 && saturation <= 1.0);
	assert(value >= 0.0 && value <= 1.0);

	const double c(value * saturation);
	const double x(c * (1.0 - fabs(fmod(hue * 6.0, 2.0) - 1.0)));
	const double m(value - c);

	if (hue < 1.0 / 6.0)
		return Color(c + m, x + m, m);
	else if (hue < 2.0 / 6.0)
		return Color(x + m, c + m, m);
	else if (hue < 3.0 / 6.0)
		return Color(m, c + m, x + m);
	else if (hue < 4.0 / 6.0)
		return Color(m, x + m, c + m);
	else if (hue < 5.0 / 6.0)
		return Color(x + m, m, c + m);
	//else if (hue < 6.0 / 6.0)
		return Color(c + m, m, x + m);
}

std::string MapPageGenerator::CleanFileName(const std::string& s)
{
	std::string cleanString(s);
	cleanString.erase(std::remove_if(cleanString.begin(), cleanString.end(), [](const char& c)
	{
		if (std::isspace(c) ||
			c == '\'' ||
			c == '.')
			return true;

		return false;
	}), cleanString.end());

	return cleanString;
}

bool MapPageGenerator::GetCountyGeometry(GoogleFusionTablesInterface& fusionTables,
	std::vector<CountyGeometry>& geometry)
{
	const std::string usCountyBoundaryTableId("1xdysxZ94uUFIit9eXmnw1fYc6VcQiXhceFd_CVKa");
	const std::string query("SELECT State,County,geometry FROM " + usCountyBoundaryTableId);
	cJSON* root;
	if (!fusionTables.SubmitQuery(query, root))
		return false;

	// TODO:  Process root to populate geometry

	return true;
}
