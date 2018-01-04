// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.

// Local headers
#include "mapPageGenerator.h"

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
	f << "  <body>\n"
		<< "    <div id=\"map\"></div>\n"
    	<< "    <script>\n"
    	<< "      var map;\n"
    	<< "      function initMap() {\n"
    	<< "        map = new google.maps.Map(document.getElementById('map'), {\n"
    	<< "          zoom: 16,\n"
    	<< "          center: new google.maps.LatLng(-33.91722, 151.23064),\n"// TODO:  Compute proper center location
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
    	<< "        };\n";

	WriteMarkerLocations(f, observationProbabilities);

	f << "        features.forEach(function(feature) {\n"
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

void MapPageGenerator::WriteMarkerLocations(std::ofstream& f,
	const std::vector<EBirdDataProcessor::FrequencyInfo>& observationProbabilities)
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

		double latitude, longitude;
		if (!GetLatitudeAndLongitudeFromCountyAndState(state, county, latitude, longitude))
			continue;

		if (!first)
			f << ',';
		first = false;
		f << '\n';

		f << "          {\n"
			<< "            position: new google.maps.LatLng(" << latitude << ',' << longitude << "),\n"
			<< "            type: 'info'\n"// TODO:  Type should be a function of entry.frequency (to pick the color of the icon)
			<< "        }";
	}

	f << "\n        ];\n";
}

bool MapPageGenerator::GetLatitudeAndLongitudeFromCountyAndState(const std::string& state,
	const std::string& county, double& latitude, double& longitude)
{
	// TODO:  Implement
}

bool MapPageGenerator::GetStateAbbreviationFromFileName(const std::string& fileName, std::string& state)
{
	// TODO:  Implement
}

bool MapPageGenerator::GetCountyNameFromFileName(const std::string& fileName, std::string& county)
{
	// TODO:  Implement
}
