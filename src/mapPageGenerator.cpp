// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

// Local headers
#include "mapPageGenerator.h"
#include "googleMapsInterface.h"

// Standard C++ headers
#include <sstream>

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

	WriteMarkerLocations(markerLocations, observationProbabilities,
		northeastLatitude, northeastLongitude, southwestLatitude, southwestLongitude,
		googleMapsKey);

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
    	<< "        var iconBase = 'https://maps.google.com/mapfiles/kml/shapes/';\n"
    	<< "        var icons = {\n"
    	<< "          parking: {\n"
    	<< "            icon: iconBase + 'parking_lot_maps.png'\n"
    	<< "          },\n"
    	<< "          library: {\n"
    	<< "            icon: iconBase + 'library_maps.png'\n"
    	<< "          },\n"
    	<< "          info: {\n"
    	<< "            icon: iconBase + 'info-i_maps.png'\n"
    	<< "          }\n"
    	<< "        };\n"

		<< markerLocations.str()// filled above

		<< "        features.forEach(function(feature) {\n"
		<< "          var marker = new google.maps.Marker({\n"
        << "            position: feature.position,\n"
		<< "            icon: icons[feature.type].icon,\n"
		<< "            map: map\n"
        << "          });\n"
        << "        });\n"
		<< "      }\n"
		<< "    </script>\n"
		<< "    <script async defer src=\"https://maps.googleapis.com/maps/api/js?key=" << googleMapsKey << "&callback=initMap\">\n"
		<< "    </script>\n"
		<< "  </body>\n"
		<< "</html>";
}

void MapPageGenerator::WriteMarkerLocations(std::ostream& f,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude, const std::string& googleMapsKey)
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
		if (!GetLatitudeAndLongitudeFromCountyAndState(state, county,
			newLatitude, newLongitude, newNELatitude,
			newNELongitude, newSWLatitude, newSWLongitude, googleMapsKey))
			continue;

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
			newNELatitude = northeastLatitude;
		if (newNELongitude > northeastLongitude)
			newNELongitude = northeastLongitude;
		if (newSWLatitude < southwestLatitude)
			newSWLatitude = southwestLatitude;
		if (newSWLongitude < southwestLongitude)
			newSWLongitude = southwestLongitude;

		f << '\n';

		f << "          {\n"
			<< "            position: new google.maps.LatLng(" << newLatitude << ',' << newLongitude << "),\n"
			<< "            type: 'info'\n"// TODO:  Type should be a function of entry.frequency (to pick the color of the icon)
			<< "        }";
	}

	f << "\n        ];\n";
}

bool MapPageGenerator::GetLatitudeAndLongitudeFromCountyAndState(const std::string& state,
	const std::string& county, double& latitude, double& longitude,
	double& neLatitude, double& neLongitude, double& swLatitude, double& swLongitude,
	const std::string& googleMapsKey)
{
	GoogleMapsInterface gMap("County Lookup Tool", googleMapsKey);
	std::string formattedAddress;
	if (!gMap.LookupCoordinates(county + " " + state, formattedAddress,
		latitude, longitude, neLatitude, neLongitude, swLatitude, swLongitude))
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

