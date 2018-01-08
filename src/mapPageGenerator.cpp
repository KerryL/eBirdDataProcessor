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

bool MapPageGenerator::WriteBestLocationsViewerPage(const std::string& htmlFileName,
	const std::string& googleMapsKey,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities)
{
	std::ofstream file(htmlFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}

	WriteHeadSection(file);
	WriteBody(file, googleMapsKey, observationProbabilities);

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

void MapPageGenerator::WriteBody(std::ofstream& f, const std::string& googleMapsKey,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities)
{
	std::ostringstream markerLocations;
	double northeastLatitude, northeastLongitude;
	double southwestLatitude, southwestLongitude;
	std::vector<std::string> stateCountyList;

	WriteMarkerLocations(markerLocations, observationProbabilities,
		northeastLatitude, northeastLongitude, southwestLatitude, southwestLongitude,
		stateCountyList, googleMapsKey);

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

		<< markerLocations.str()// filled above

		<< "        features.forEach(function(feature) {\n"
		<< "          var marker = new google.maps.Marker({\n"
        << "            position: feature.position,\n"
		// title is "hover text"
		// label puts text inside marker (so more than one character overflows)
		<< "            title: feature.name + ' (' + feature.value + ')',\n"
		<< "            map: map\n"
        << "          });\n"
		<< '\n'
		/*<< "          var infoWindow = new google.maps.InfoWindow({\n"
		<< "            content: feature.info\n"
		<< "          });"
		<< '\n'
		<< "          marker.addListener('click', function() {\n"
		<< "            infoWindow.open(map, marker);\n"
		<< "          });\n"*/// TODO:  Possibly list most likely species in this window?
        << "        });\n"
		<< '\n'
		<< "        var countyLayer = new google.maps.FusionTablesLayer({\n"
        << "          query: {\n"
        << "            select: 'geometry',\n"
        << "            from: '1xdysxZ94uUFIit9eXmnw1fYc6VcQiXhceFd_CVKa',\n"// hash for US county boundaries fusion table
		<< "            where: \"'State-County' IN (";

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

        f << "]\n"
        << "        });\n"
		<< "        countyLayer.setMap(map);\n"
		<< "      }\n"
		<< "    </script>\n"
		<< "    <script async defer src=\"https://maps.googleapis.com/maps/api/js?key=" << googleMapsKey << "&callback=initMap\">\n"
		<< "    </script>\n"
		<< "  </body>\n"
		<< "</html>";
}

// TODO:  Consider multithreading all of the "bulk" stuff - contacting google maps for county info, reading frequency data, etc.  Maybe not for frequency harvesting (eBird has limits on crawling in robots.txt), but for google apis.

void MapPageGenerator::WriteMarkerLocations(std::ostream& f,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	std::vector<std::string>& stateCountyList, const std::string& googleMapsKey)
{
	f << "        var features = [";

	bool first(true);
	for (const auto& entry : observationProbabilities)
	{
		std::string state, county;
		if (!GetStateAbbreviationFromFileName(entry.species, state))
			continue;

		if (!GetCountyNameFromFileName(entry.species, county))
			continue;

		double newLatitude, newLongitude;
		double newNELatitude, newNELongitude;
		double newSWLatitude, newSWLongitude;
		std::string geographicName;
		if (!GetLatitudeAndLongitudeFromCountyAndState(state, county + " County",
			newLatitude, newLongitude, newNELatitude,
			newNELongitude, newSWLatitude, newSWLongitude, geographicName, googleMapsKey))
			continue;

		const std::string endString(" County");
		std::string::size_type endPosition(std::min(
			geographicName.find(endString), geographicName.find(',')));
		if (endPosition == std::string::npos)
		{
			std::cerr << "Warning:  Failed to extract county name from '" << geographicName << "'\n";
		}
		else
			stateCountyList.push_back(state + "-" + geographicName.substr(0, endPosition));

		const std::string::size_type saintStart(geographicName.find("St "));
		if (saintStart != std::string::npos)
		{
			geographicName.insert(saintStart + 2, ".");
			stateCountyList.back() = state + "-" + geographicName.substr(0, endPosition + 1);
		}

		county = geographicName.substr(0, geographicName.find(','));// TODO:  Make robust

		if (first)
		{
			first = false;

			northeastLatitude = newLatitude;
			northeastLongitude = newLongitude;
			southwestLatitude = newLatitude;
			southwestLongitude = newLongitude;
		}
		else
			f << ',';

		if (newNELatitude > northeastLatitude)
			northeastLatitude = newNELatitude;
		if (newNELongitude > northeastLongitude)
			northeastLongitude = newNELongitude;
		if (newSWLatitude < southwestLatitude)
			southwestLatitude = newSWLatitude;
		if (newSWLongitude < southwestLongitude)
			southwestLongitude = newSWLongitude;

		f << '\n';

		f << "          {\n"
			<< "            position: new google.maps.LatLng(" << newLatitude << ',' << newLongitude << "),\n"
			<< "            probability: " << entry.frequency << ",\n"
			<< "            name: '" << CleanNameString(county) << ", " << state << "',\n"
			<< "            value: '" << entry.frequency * 100 << "%'\n"
			<< "          }";
	}

	f << "\n        ];\n";
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

std::string MapPageGenerator::ComputeColor(const std::string& stateCounty,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities)
{
	assert(stateCounty.size() > 3);
	const std::string stateAbbr(stateCounty.substr(0, 2));
	const std::string countyName(stateCounty.substr(3));
	const std::string matchString(CleanFileName(countyName) + stateAbbr + "FrequencyData.csv");

	const Color minColor(0.0, 0.75, 0.365);// Greenish
	const Color maxColor(1.0, 0.0, 0.0);// Red

	for (const auto& entry : observationProbabilities)
	{
		if (entry.species.compare(matchString) == 0)
			return ColorToHexString(InterpolateColor(minColor, maxColor, entry.frequency));
	}

std::cout << "no match for " << stateCounty << " (matchString = " << matchString << ")" << std::endl;
	
	return "#000000";
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
	hex << "#" << std::hex << std::setw(2) << std::setfill('0')
		<< static_cast<int>(c.red * 255.0)
		<< static_cast<int>(c.green * 255.0)
		<< static_cast<int>(c.blue * 255.0);
	
	return hex.str();
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

