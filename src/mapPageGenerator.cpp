// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.
//        To view table data, go to:
//        https://fusiontables.google.com/data?docid=1YzezTGDbYkUwOqvZ_yo0nKU8JjbAoEjYw01Sgcga

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
#include <cassert>

// TODO:  Comment this out for normal builds - this prevent usage of maps API quota
//#define DONT_CALL_MAPS_API

const std::string MapPageGenerator::birdProbabilityTableName("Bird Probability Table");
const std::array<MapPageGenerator::NamePair, 12> MapPageGenerator::monthNames = {
	NamePair("Jan", "January"),
	NamePair("Feb", "February"),
	NamePair("Mar", "March"),
	NamePair("Apr", "April"),
	NamePair("May", "May"),
	NamePair("Jun", "June"),
	NamePair("Jul", "July"),
	NamePair("Aug", "August"),
	NamePair("Sep", "September"),
	NamePair("Oct", "October"),
	NamePair("Nov", "November"),
	NamePair("Dec", "December")};

bool MapPageGenerator::WriteBestLocationsViewerPage(const std::string& htmlFileName,
	const std::string& googleMapsKey,
	const std::vector<ObservationInfo>& observationProbabilities,
	const std::string& clientId, const std::string& clientSecret)
{
	std::ofstream file(htmlFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}

	const Keys keys(googleMapsKey, clientId, clientSecret);
	WriteHeadSection(file, keys, observationProbabilities);
	WriteBody(file);

	return true;
}

void MapPageGenerator::WriteHeadSection(std::ofstream& f, const Keys& keys,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	f << "<!DOCTYPE html>\n"
		<< "<html>\n"
		<< "  <head>\n"
		<< "    <title>Best Locations for New Species</title>\n"
		<< "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
		<< "    <meta charset=\"utf-8\">\n"
		<< "    <style>\n"
		<< "      #map {\n"
		<< "        height: 95%;\n"
		<< "      }\n"
		<< "      html, body {\n"
		<< "        height: 100%;\n"
		<< "        margin: 0;\n"
		<< "        padding: 0;\n"
		<< "      }\n"
		<< "    </style>\n";

	WriteScripts(f, keys, observationProbabilities);

	f << "  </head>\n";
}

void MapPageGenerator::WriteBody(std::ofstream& f)
{
	f << "  <body>\n"
		<< "    <div id=\"map\"></div>\n"
		<< "    <div style='font-family: sans-serif'>\n"
    	<< "      <label>Select Month:</label>\n"
    	<< "      <select id=\"month\">\n";

    unsigned int i(0);
    for (const auto& m : monthNames)
		f << "        <option value=\"" << i++ << "\">" << m.longName << "</option>\n";

	f << "        <option value=\"-1\">Cycle</option>\n"
		<< "      </select>\n"
		<< "    </div>\n"
		<< "  </body>\n"
		<< "</html>";
}

void MapPageGenerator::WriteScripts(std::ofstream& f, const Keys& keys,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	double northeastLatitude, northeastLongitude;
	double southwestLatitude, southwestLongitude;
	std::string tableId;
	std::vector<unsigned int> styleIds;
	std::vector<unsigned int> templateIds;

	if (!CreateFusionTable(observationProbabilities, northeastLatitude,
		northeastLongitude, southwestLatitude, southwestLongitude, tableId,
		keys, styleIds, templateIds))
	{
		std::cerr << "Failed to create fusion table\n";
		return;
	}

	const double centerLatitude(0.5 * (northeastLatitude + southwestLatitude));
	const double centerLongitude(0.5 * (northeastLongitude + southwestLongitude));

	time_t now(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	struct tm nowTime(*localtime(&now));

	f << "    <script type=\"text/javascript\">\n"
    	<< "      var map;\n"
    	<< "      var monthIndex = 0;\n"
		<< "      var monthStyles = [";

	bool needComma(false);
	for (const auto& s : styleIds)
	{
		if (needComma)
			f << ',';
		needComma = true;
		f << s;
	}

	f << "];\n"
		<< "      var monthTemplates = [";

	needComma = false;
	for (const auto& t : templateIds)
	{
		if (needComma)
			f << ',';
		needComma = true;
		f << t;
	}

	f << "];\n"
		<< "      var countyLayer;\n"
		<< "      var timerHandle;\n"
		<< '\n'
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
		<< "        countyLayer = new google.maps.FusionTablesLayer({\n"
        << "          query: {\n"
        << "            select: 'geometry',\n"
        << "            from: '" << tableId << "'\n"
		<< "          },\n"
		<< "          map: map,\n"
		<< "          styleId: " << styleIds[nowTime.tm_mon] << ",\n"
		<< "          templateId: " << templateIds[nowTime.tm_mon] << '\n'
		<< "        });\n"
		<< "        countyLayer.setMap(map);\n"
		<< '\n'
		<< "        google.maps.event.addDomListener(document.getElementById('month'),\n"
        << "          'change', function() {\n"
        << "            var selectedStyle = this.value;\n"
        << "            if (selectedStyle == -1)\n"
        << "                  timerHandle = setInterval(setNextStyle, 1500);\n"
        << "            else\n"
        << "            {\n"
        << "              countyLayer.set('styleId', monthStyles[selectedStyle]);\n"
		<< "              countyLayer.set('templateId', monthTemplates[selectedStyle]);\n"
        << "              clearInterval(timerHandle);\n"
		<< "            }\n"
        << "          });\n"
		<< "      }\n"
		<< '\n'
		<< "      function setNextStyle() {\n"
		<< "        monthIndex++;\n"
		<< "        if (monthIndex > 11)\n"
		<< "          monthIndex = 0;\n"
		<< "        countyLayer.set('styleId', monthStyles[monthIndex]);\n"
		<< "        countyLayer.set('templateId', monthTemplates[monthIndex]);\n"
		<< "      }\n"
		<< "    </script>\n"
		<< "    <script async defer src=\"https://maps.googleapis.com/maps/api/js?key=" << keys.googleMapsKey << "&callback=initMap\">\n"
		<< "    </script>\n";
}

bool MapPageGenerator::CreateFusionTable(
	const std::vector<ObservationInfo>& observationProbabilities,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	std::string& tableId, const Keys& keys, std::vector<unsigned int>& styleIds,
	std::vector<unsigned int>& templateIds)
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
		if (t.name.compare(birdProbabilityTableName) == 0 && t.columns.size() == 42)// TODO:  Better check necessary?
		{
			std::cout << "Found existing table " << t.tableId << std::endl;
			tableId = t.tableId;

#ifndef DONT_CALL_MAPS_API
			if (!fusionTables.DeleteAllRows(t.tableId))// TODO:  Don't do this any more?
				std::cerr << "Warning:  Failed to delete existing rows from table\n";
#endif// DONT_CALL_MAPS_API
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
	if (!GetCountyGeometry(fusionTables, geometry))// TODO:  Only if necessary?
		return false;

	// NOTE:  Google maps geocoding API has rate limit of 50 queries per sec, and usage limit of 2500 queries per day
	// In order to prevent exceeding the daily query limit, we should only update the data in the rows,
	// and not touch the name, geometry or location columns unless it's necessary. (TODO)
	std::vector<CountyInfo> countyInfo(observationProbabilities.size());
	const unsigned int rateLimit(25);// Half of published limit to give us some buffer
	GoogleMapsThreadPool pool(std::thread::hardware_concurrency() * 2, rateLimit);
	auto countyIt(countyInfo.begin());
	for (const auto& entry : observationProbabilities)
	{
		// TODO:  Populate known info here?  So we don't have to contact Google Maps if we don't have to?  Only update probability data?
		countyIt->frequencyInfo = std::move(entry.frequencyInfo);
		pool.AddJob(std::make_unique<GoogleMapsThreadPool::MapJobInfo>(
			PopulateCountyInfo, *countyIt, entry, keys.googleMapsKey, geometry));
		++countyIt;
	}

	pool.WaitForAllJobsComplete();

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
			<< c.latitude << ' ' << c.longitude << ",\"" << c.geometryKML << '"';

		unsigned int i(0);
		for (const auto& p : c.probabilities)
			ss << ',' << p * 100.0 << ',' << ComputeColor(p) << ",\"" << BuildSpeciesInfoString(c.frequencyInfo[i++]) << '"';

		ss << '\n';
	}

#ifndef DONT_CALL_MAPS_API
	if (!fusionTables.Import(tableId, ss.str()))// TODO:  Update instead of import?
	{
		std::cerr << "Failed to import data\n";
		return false;
	}
#endif// DONT_CALL_MAPS_API

	if (!VerifyTableStyles(fusionTables, tableId, styleIds))
	{
		std::cerr << "Failed to verify table styles\n";
		return false;
	}

	if (!VerifyTableTemplates(fusionTables, tableId, templateIds))
	{
		std::cerr << "Failed to verify table templates\n";
		return false;
	}

	return true;
}

void MapPageGenerator::PopulateCountyInfo(const GoogleMapsThreadPool::JobInfo& jobInfo)
{
	const GoogleMapsThreadPool::MapJobInfo& mapJobInfo(static_cast<const GoogleMapsThreadPool::MapJobInfo&>(jobInfo));
	CountyInfo& info(mapJobInfo.info);
	const ObservationInfo& frequencyInfo(mapJobInfo.frequencyInfo);

	if (!GetStateAbbreviationFromFileName(frequencyInfo.locationHint, info.state))
		std::cerr << "Warning:  Failed to get state abberviation for '" << frequencyInfo.locationHint << "'\n";

	if (!GetCountyNameFromFileName(frequencyInfo.locationHint, info.county))
		std::cerr << "Warning:  Failed to get county name for '" << frequencyInfo.locationHint << "'\n";

#ifndef DONT_CALL_MAPS_API
	// TODO:  Only if necessary?
	if (!GetLatitudeAndLongitudeFromCountyAndState(info.state, info.county + " County",
		info.latitude, info.longitude, info.neLatitude,
		info.neLongitude, info.swLatitude, info.swLongitude, info.name,
		mapJobInfo.googleMapsKey))
		std::cerr << "Warning:  Failed to get location information for '" << frequencyInfo.locationHint << "'\n";
#endif// DONT_CALL_MAPS_API

	const std::string::size_type saintStart(info.name.find("St "));
	if (saintStart != std::string::npos)
		info.name.insert(saintStart + 2, ".");

	info.county = info.name.substr(0, info.name.find(','));// TODO:  Make robust
	info.probabilities = std::move(frequencyInfo.probabilities);

	for (const auto& g : mapJobInfo.geometry)// TODO:  Only if necessary?
	{
		std::string countyString(StripCountyFromName(info.county));
		if (g.state.compare(info.state) == 0 && g.county.compare(countyString) == 0)
		{
			info.geometryKML = g.kml;
			break;
		}
	}

	if (info.geometryKML.empty())
		std::cerr << "Warning:  Geometry not found for '" << info.name << "'\n";
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
	tableInfo.columns.push_back(GFTI::ColumnInfo("Geometry", GFTI::ColumnType::Location));

	for (const auto& m : monthNames)
	{
		tableInfo.columns.push_back(GFTI::ColumnInfo("Probability-" + m.shortName, GFTI::ColumnType::Number));
		tableInfo.columns.push_back(GFTI::ColumnInfo("Color-" + m.shortName, GFTI::ColumnType::String));
		tableInfo.columns.push_back(GFTI::ColumnInfo("Species-" + m.shortName, GFTI::ColumnType::String));
	}

	return tableInfo;
}

bool MapPageGenerator::GetLatitudeAndLongitudeFromCountyAndState(const std::string& state,
	const std::string& county, double& latitude, double& longitude,
	double& neLatitude, double& neLongitude, double& swLatitude, double& swLongitude,
	std::string& geographicName, const std::string& googleMapsKey)
{
	const unsigned int maxAttempts(5);
	unsigned int i;
	for (i = 0; i < maxAttempts; ++i)
	{
		// According to Google docs, if we get this error, another attempt
		// may succeed.  In practice, this seems to be true.
		const std::string unknownErrorStatus("UNKNOWN_ERROR");
		std::string status;
		GoogleMapsInterface gMap("County Lookup Tool", googleMapsKey);
		if (gMap.LookupCoordinates(county + " " + state, geographicName,
			latitude, longitude, neLatitude, neLongitude, swLatitude, swLongitude,
			"County", &status))
			break;
		else if (status.compare(unknownErrorStatus) != 0 || i == maxAttempts - 1)
			return false;
	}

	if (geographicName.empty())
	{
		std::cerr << "Invalid data (empty name) returned from coordinate lookup for " << county << ", " << state << '\n';
		return false;
	}

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

std::string MapPageGenerator::StripCountyFromName(const std::string& s)
{
	const std::string endString(" County");
	std::string clean(s);
	std::string::size_type endPosition(clean.find(endString));
	if (endPosition != std::string::npos)
		clean = clean.substr(0, endPosition);

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
	const Color zeroColor(0.3, 0.3, 0.3);// Grey

	if (frequency == 0)
		return ColorToHexString(zeroColor);

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
	std::transform(hexString.begin(), hexString.end(), hexString.begin(), ::toupper);
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
	const std::string query("SELECT 'State Abbr','County Name',geometry FROM " + usCountyBoundaryTableId + "&typed=false");
	cJSON* root;
	if (!fusionTables.SubmitQuery(query, root))
		return false;

	cJSON* rowsArray(cJSON_GetObjectItem(root, "rows"));
	if (!rowsArray)
	{
		std::cerr << "Failed to get rows array\n";
		cJSON_Delete(root);
		return false;
	}

	geometry.resize(cJSON_GetArraySize(rowsArray));
	unsigned int i(0);
	for (auto& g : geometry)
	{
		cJSON* item(cJSON_GetArrayItem(rowsArray, i));
		if (!item)
		{
			std::cerr << "Failed to get array item " << i << '\n';
			cJSON_Delete(root);
			return false;
		}

		cJSON* state(cJSON_GetArrayItem(item, 0));
		cJSON* county(cJSON_GetArrayItem(item, 1));
		cJSON* kml(cJSON_GetArrayItem(item, 2));
		if (!state || !county || !kml)
		{
			std::cerr << "Failed to get row information for row " << i << '\n';
			cJSON_Delete(root);
			return false;
		}

		g.state = state->valuestring;
		g.county = county->valuestring;
		g.kml = kml->valuestring;

		++i;
	}

	cJSON_Delete(root);
	return true;
}

bool MapPageGenerator::VerifyTableStyles(GoogleFusionTablesInterface& fusionTables,
	const std::string& tableId, std::vector<unsigned int>& styleIds)
{
	std::vector<GoogleFusionTablesInterface::StyleInfo> styles;
	if (!fusionTables.ListStyles(tableId, styles))
		return false;

	if (styles.size() == 12)// TODO:  Should we do a better check?
		return true;

	for (const auto& s : styles)
	{
		if (!fusionTables.DeleteStyle(tableId, s.styleId))
			return false;
	}

	styles.clear();
	for (const auto& m : monthNames)
		styles.push_back(CreateStyle(tableId, m.shortName));

	styleIds.resize(styles.size());
	auto idIt(styleIds.begin());
	for (auto& s : styles)
	{
		if (!fusionTables.CreateStyle(tableId, s))
			return false;
		*idIt = s.styleId;
		++idIt;
	}

	return true;
}

GoogleFusionTablesInterface::StyleInfo MapPageGenerator::CreateStyle(const std::string& tableId,
	const std::string& month)
{
	GoogleFusionTablesInterface::StyleInfo style;
	style.name = month;
	style.hasPolygonOptions = true;
	style.tableId = tableId;

	GoogleFusionTablesInterface::StyleInfo::Options polygonOptions;
	polygonOptions.type = GoogleFusionTablesInterface::StyleInfo::Options::Type::Complex;
	polygonOptions.key = "fillColorStyler";
	polygonOptions.c.push_back(GoogleFusionTablesInterface::StyleInfo::Options("columnName", "Color-" + month));
	style.polygonOptions.push_back(polygonOptions);

	return style;
}

bool MapPageGenerator::VerifyTableTemplates(GoogleFusionTablesInterface& fusionTables,
	const std::string& tableId, std::vector<unsigned int>& templateIds)
{
	std::vector<GoogleFusionTablesInterface::TemplateInfo> templates;
	if (!fusionTables.ListTemplates(tableId, templates))
		return false;

	if (templates.size() == 12)// TODO:  Should we do a better check?
		return true;

	for (const auto& t : templates)
	{
		if (!fusionTables.DeleteTemplate(tableId, t.templateId))
			return false;
	}

	templates.clear();
	for (const auto& m : monthNames)
		templates.push_back(CreateTemplate(tableId, m.shortName));

	templateIds.resize(templates.size());
	auto idIt(templateIds.begin());
	for (auto& t : templates)
	{
		if (!fusionTables.CreateTemplate(tableId, t))
			return false;
		*idIt = t.templateId;
		++idIt;
	}

	return true;
}

GoogleFusionTablesInterface::TemplateInfo MapPageGenerator::CreateTemplate(
	const std::string& tableId, const std::string& month)
{
	GoogleFusionTablesInterface::TemplateInfo info;
	info.name = month;
	info.tableId = tableId;

	std::ostringstream ss;
	ss << "<div class='googft-info-window' style='font-family: sans-serif'>\n"
		<< "<p><b>{Name}</b></p>\n"
		<< "<p>Probability of Observing a Lifer: {Probability-" << std::setprecision(2) << std::fixed << month << "}%</p>\n"
		<< "<p>Likely Species:</p>\n"
		<< "<div style=\"height:100px;width:300px;border:1px solid #0C0C0C;overflow:auto;\">{Species-" << month << "}</div>\n"
		<< "</div>";

	info.body = ss.str();

	return info;
}

std::string MapPageGenerator::BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info)
{
	std::ostringstream ss;
	ss << std::setprecision(2) << std::fixed;
	for (const auto& s : info)
	{
		if (!ss.str().empty())
			ss << "<br>";
		ss << s.species << " (" << s.frequency << "%)";
	}

	return ss.str();
}
