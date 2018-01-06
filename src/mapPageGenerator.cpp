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
		<< "            title: feature.name + ' (' + feature.info + ')',\n"
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
			f << '\'' << CleanString(stateCounty) << '\'';
		}

		f << ")\"\n"
        << "          },\n"
        << "          styles: [{\n"
        << "            polygonOptions: {\n"
        << "              fillColor: '#00FF00',\n"
        << "              fillOpacity: 0.3,\n"
		<< "              strokeColor: '#FFFFFF'\n"
        << "            }\n"
        /*<< "          }, {
        << "            where: 'birds > 300',
        << "            polygonOptions: {
        << "              fillColor: '#0000FF'
        << "            }
        << "          }, {
        << "            where: 'population > 5',
        << "            polygonOptions: {
        << "              fillOpacity: 1.0
        << "            }*/
        << "          }]\n"
        << "        });\n"
		<< "        countyLayer.setMap(map);\n"
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

		const std::string endString(" County, ");
		std::string::size_type endPosition(std::min(
			geographicName.find(endString), geographicName.find(',')));
		if (endPosition == std::string::npos)
		{
			std::cerr << "Warning:  Failed to extract county name from '" << geographicName << "'\n";

			// TODO:  Move this bit outside - when we need to include this logic, it's not
			// because the search failed, it's because the WHERE with fusion table data
			// didn't include all of the desired results (which we cannot check for)
			const std::string::size_type saintStart(geographicName.find("St "));
			if (saintStart != std::string::npos)
			{
				geographicName.insert(saintStart + 2, ".");
				stateCountyList.push_back(state + "-" + geographicName.substr(0, endPosition + 1));
			}
		}
		else
			stateCountyList.push_back(state + "-" + geographicName.substr(0, endPosition));

		county = geographicName.substr(0, geographicName.find(','));// TODO:  Make robust
		// Also, problems with things like "Brooklyn" vs. "King's County".  Both names
		// are returned from the maps search, but we need to be able to determine which
		// one to use

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
			<< "            name: '" << CleanString(county) << ", " << state << "',\n"
			<< "            info: '" << entry.frequency * 100 << "%'\n"
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

std::string MapPageGenerator::CleanString(const std::string& s)
{
	std::string clean;

	for (const auto& c : s)
	{
		if (c == '\'')
			clean.append("&apos;");
		else
			clean.push_back(c);
	}

	return clean;
}

