// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating a web page that overlays observation information on an interactive map.

// Local headers
#include "mapPageGenerator.h"
#include "stringUtilities.h"
#include "utilities.h"
#include "kmlToGeoJSONConverter.h"
#include "utilities/mutexUtilities.h"

// Standard C++ headers
#include <iomanip>
#include <cctype>
#include <iostream>
#include <thread>
#include <algorithm>
#include <cassert>
#include <set>
#include <mutex>
#include <cmath>

const UString::String MapPageGenerator::htmlExtension(_T(".html"));
const UString::String MapPageGenerator::dataExtension(_T(".js"));

const std::array<MapPageGenerator::NamePair, 48> MapPageGenerator::weekNames = {
	NamePair(_T("Jan"), _T("January")),
	NamePair(_T("Feb"), _T("February")),
	NamePair(_T("Mar"), _T("March")),
	NamePair(_T("Apr"), _T("April")),
	NamePair(_T("May"), _T("May")),
	NamePair(_T("Jun"), _T("June")),
	NamePair(_T("Jul"), _T("July")),
	NamePair(_T("Aug"), _T("August")),
	NamePair(_T("Sep"), _T("September")),
	NamePair(_T("Oct"), _T("October")),
	NamePair(_T("Nov"), _T("November")),
	NamePair(_T("Dec"), _T("December"))};

MapPageGenerator::MapPageGenerator(const LocationFindingParameters& locationFindingParameters,
	const std::vector<UString::String>& highDetailCountries,
	const UString::String& eBirdApiKey, const UString::String& kmlLibraryPath) : highDetailCountries(highDetailCountries),
	ebi(eBirdApiKey), kmlLibrary(kmlLibraryPath, eBirdApiKey, UString::String()/*Google maps key?*/,
		log, locationFindingParameters.cleanupKMLLocationNames, locationFindingParameters.geoJSONPrecision),
	kmlReductionLimit(locationFindingParameters.kmlReductionLimit)
{
	log.Add(Cout);
	/*std::unique_ptr<UString::OFStream> f(std::make_unique<UString::OFStream>("temp.log"));// TODO:  Remove
	log.Add(std::move(f));//*/
}

bool MapPageGenerator::WriteBestLocationsViewerPage(const UString::String& baseOutputFileName,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	if (!WriteHTML(baseOutputFileName + htmlExtension, baseOutputFileName + dataExtension))
		return false;

	if (!WriteGeoJSONData(baseOutputFileName + dataExtension, observationProbabilities))
		return false;

	return true;
}

bool MapPageGenerator::WriteHTML(const UString::String& fileName, const UString::String& dataFileName) const
{
	UString::OFStream file(fileName);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}

	file << "<!DOCTYPE html>\n<html>\n";
	WriteHeadSection(file);
	WriteBody(file, dataFileName);
	file << "</html>\n";

	return true;
}

void MapPageGenerator::WriteHeadSection(UString::OStream& f)
{
	f << "  <head>\n"
		<< "    <title>Best Locations for New Species</title>\n"
		<< "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n"
		<< "    <meta charset=\"utf-8\">\n"
		<< "    <style>\n"
		<< "      #mapid {\n"
		<< "        height: 95%;\n"
		<< "      }\n"
		<< "      html, body {\n"
		<< "        height: 100%;\n"
		<< "        margin: 0;\n"
		<< "        padding: 0;\n"
		<< "      }\n"
		<< "	  .info { padding: 6px 8px; font: 14px/16px Arial, Helvetica, sans-serif; background: white; background: rgba(255,255,255,0.8); box-shadow: 0 0 15px rgba(0,0,0,0.2); border-radius: 5px; }\n"
		<< "	  .info h4 { margin: 0 0 5px; color: #777; }\n"
		<< "      .legend { text-align: left; line-height: 18px; color: #555; }\n"
		<< "      .legend i { width: 18px; height: 18px; float: left; margin-right: 8px; opacity: 0.7; }\n"
		<< "	  #speciesList { width:100%; }\n"
		<< "    </style>\n"
		<< "    <link rel=\"stylesheet\" href=\"https://unpkg.com/leaflet@1.7.1/dist/leaflet.css\" integrity=\"sha512-xodZBNTC5n17Xt2atTPuE1HxjVMSvLVW9ocqUKLsCC5CXdbqCmblAshOMAS6/keqq/sMZMZ19scR4PsZChSR7A==\" crossorigin=\"\"/>\n"
		<< "    <script src=\"https://unpkg.com/leaflet@1.7.1/dist/leaflet.js\" integrity=\"sha512-XQoYMqMTK8LvdxXYG3nZ448hOEQiglfqkJs1NOQV44cWnUrBc8PkAOcXy20w0vlaXaVUearIOBhiXZ5V3ynxwA==\" crossorigin=\"\"></script>\n"
		<< "  </head>\n\n";
}

void MapPageGenerator::WriteBody(UString::OStream& f, const UString::String& dataFileName)
{
	f << "  <body>\n"
		<< "    <div id=\"mapid\"></div>\n\n"
		<< "	<script type=\"text/javascript\" src=\"" << dataFileName << "\"></script>\n\n"
		<< "	<div style='font-family: sans-serif'>\n"
		<< "      <label>Select Week:</label>\n"
		<< "      <select id=\"weekSelect\" onchange=\"updateMap()\">\n"
		<< "        <option value=\"0\">January 1</option>\n"
		<< "        <option value=\"1\">January 8</option>\n"
		<< "        <option value=\"2\">January 15</option>\n"
		<< "        <option value=\"3\">January 22</option>\n"
		<< "        <option value=\"4\">February 1</option>\n"
		<< "        <option value=\"5\">February 8</option>\n"
		<< "        <option value=\"6\">February 15</option>\n"
		<< "        <option value=\"7\">February 22</option>\n"
		<< "        <option value=\"8\">March 1</option>\n"
		<< "        <option value=\"9\">March 8</option>\n"
		<< "        <option value=\"10\">March 15</option>\n"
		<< "        <option value=\"11\">March 22</option>\n"
		<< "        <option value=\"12\">April 1</option>\n"
		<< "        <option value=\"13\">April 8</option>\n"
		<< "        <option value=\"14\">April 15</option>\n"
		<< "        <option value=\"15\">April 22</option>\n"
		<< "        <option value=\"16\">May 1</option>\n"
		<< "        <option value=\"17\">May 8</option>\n"
		<< "        <option value=\"18\">May 15</option>\n"
		<< "        <option value=\"19\">May 22</option>\n"
		<< "        <option value=\"20\">June 1</option>\n"
		<< "        <option value=\"21\">June 8</option>\n"
		<< "        <option value=\"22\">June 15</option>\n"
		<< "        <option value=\"23\">June 22</option>\n"
		<< "        <option value=\"24\">July 1</option>\n"
		<< "        <option value=\"25\">July 8</option>\n"
		<< "        <option value=\"26\">July 15</option>\n"
		<< "        <option value=\"27\">July 22</option>\n"
		<< "        <option value=\"28\">August 1</option>\n"
		<< "        <option value=\"29\">August 8</option>\n"
		<< "        <option value=\"30\">August 15</option>\n"
		<< "        <option value=\"31\">August 22</option>\n"
		<< "        <option value=\"32\">September 1</option>\n"
		<< "        <option value=\"33\">September 8</option>\n"
		<< "        <option value=\"34\">September 15</option>\n"
		<< "        <option value=\"35\">September 22</option>\n"
		<< "        <option value=\"36\">October 1</option>\n"
		<< "        <option value=\"37\">October 8</option>\n"
		<< "        <option value=\"38\">October 15</option>\n"
		<< "        <option value=\"39\">October 22</option>\n"
		<< "        <option value=\"40\">November 1</option>\n"
		<< "        <option value=\"41\">November 8</option>\n"
		<< "        <option value=\"42\">November 15</option>\n"
		<< "        <option value=\"43\">November 22</option>\n"
		<< "        <option value=\"44\">December 1</option>\n"
		<< "        <option value=\"45\">December 8</option>\n"
		<< "        <option value=\"46\">December 15</option>\n"
		<< "        <option value=\"47\">December 22</option>\n"
		<< "        <option value=\"-1\">Cycle</option>\n"
		<< "      </select>\n"
		<< "    </div>\n\n";
	WriteScripts(f);
	f << "  </body>\n";
}

void MapPageGenerator::WriteScripts(UString::OStream& f)
{
	f << "    <script type=\"text/javascript\">\n"
		<< "      var map = L.map('mapid').setView([37.8, -96], 4);\n\n"
		<< "      L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {\n"
		<< "        maxZoom: 18,\n"
		<< "        attribution: '&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors',\n"
		<< "        id: 'mapbox.light'\n"
		<< "      }).addTo(map);\n\n"
		<< "      var info = L.control();\n\n"
		<< "      info.onAdd = function (map) {\n"
		<< "        this._div = L.DomUtil.create('div', 'info');\n"
		<< "        this.update();\n"
		<< "        return this._div;\n"
		<< "      };\n\n"
		<< "      info.update = function (props) {\n"
		<< "        var probability = 0;\n"
		<< "        if (props) {\n"
		<< "          probability = props.weekData[week].probability;\n"
		<< "        }\n"
		<< "        this._div.innerHTML = '<h4>Probability of Needed Observation</h4>Week Starting '\n"
		<< "          + GetWeekText(week) + '<br />' + (props ?\n"
		<< "          '<b>' + props.name + '</b><br />' + probability.toFixed(2) + ' %<br />' +\n"
		<< "          '<select name=\"Needed Species\" size=\"10\" id=\"speciesList\">' +\n"
		<< "          '</select>'\n"
		<< "          : 'Select a region');\n\n"
		<< "        if (props) {\n"
		<< "          var fragment = document.createDocumentFragment();\n"
		<< "          props.weekData[week].birds.forEach(function(species, index) {\n"
		<< "            var opt = document.createElement('option');\n"
		<< "            opt.text = species;\n"
		<< "            opt.value = species;\n"
		<< "            fragment.appendChild(opt);\n"
		<< "            });\n"
		<< "          document.getElementById('speciesList').appendChild(fragment);\n"
		<< "        }\n"
		<< "      };\n\n"
		<< "      function GetWeekText(week) {\n"
		<< "        if (week == 0)\n"
		<< "          return 'January 1';\n"
		<< "        else if (week == 1)\n"
		<< "          return 'January 8';\n"
		<< "        else if (week == 2)\n"
		<< "          return 'January 15';\n"
		<< "        else if (week == 3)\n"
		<< "          return 'January 22';\n"
		<< "        else if (week == 4)\n"
		<< "          return 'February 1';\n"
		<< "        else if (week == 5)\n"
		<< "          return 'February 8';\n"
		<< "        else if (week == 6)\n"
		<< "          return 'February 15';\n"
		<< "        else if (week == 7)\n"
		<< "          return 'February 22';\n"
		<< "        else if (week == 8)\n"
		<< "          return 'March 1';\n"
		<< "        else if (week == 9)\n"
		<< "          return 'March 8';\n"
		<< "        else if (week == 10)\n"
		<< "          return 'March 15';\n"
		<< "        else if (week == 11)\n"
		<< "          return 'March 22';\n"
		<< "        else if (week == 12)\n"
		<< "          return 'April 1';\n"
		<< "        else if (week == 13)\n"
		<< "          return 'April 8';\n"
		<< "        else if (week == 14)\n"
		<< "          return 'April 15';\n"
		<< "        else if (week == 15)\n"
		<< "          return 'April 22';\n"
		<< "        else if (week == 16)\n"
		<< "          return 'May 1';\n"
		<< "        else if (week == 17)\n"
		<< "          return 'May 8';\n"
		<< "        else if (week == 18)\n"
		<< "          return 'May 15';\n"
		<< "        else if (week == 19)\n"
		<< "          return 'May 22';\n"
		<< "        else if (week == 20)\n"
		<< "          return 'June 1';\n"
		<< "        else if (week == 21)\n"
		<< "          return 'June 8';\n"
		<< "        else if (week == 22)\n"
		<< "          return 'June 15';\n"
		<< "        else if (week == 23)\n"
		<< "          return 'June 22';\n"
		<< "        else if (week == 24)\n"
		<< "          return 'July 1';\n"
		<< "        else if (week == 25)\n"
		<< "          return 'July 8';\n"
		<< "        else if (week == 26)\n"
		<< "          return 'July 15';\n"
		<< "        else if (week == 27)\n"
		<< "          return 'July 22';\n"
		<< "        else if (week == 28)\n"
		<< "          return 'August 1';\n"
		<< "        else if (week == 29)\n"
		<< "          return 'August 8';\n"
		<< "        else if (week == 30)\n"
		<< "          return 'August 15';\n"
		<< "        else if (week == 31)\n"
		<< "          return 'August 22';\n"
		<< "        else if (week == 32)\n"
		<< "          return 'September 1';\n"
		<< "        else if (week == 33)\n"
		<< "          return 'September 8';\n"
		<< "        else if (week == 34)\n"
		<< "          return 'September 1';\n"
		<< "        else if (week == 35)\n"
		<< "          return 'September 2';\n"
		<< "        else if (week == 36)\n"
		<< "          return 'October 1';\n"
		<< "        else if (week == 37)\n"
		<< "          return 'October 8';\n"
		<< "        else if (week == 38)\n"
		<< "          return 'October 15';\n"
		<< "        else if (week == 39)\n"
		<< "          return 'October 22';\n"
		<< "        else if (week == 40)\n"
		<< "          return 'November 1';\n"
		<< "        else if (week == 41)\n"
		<< "          return 'November 8';\n"
		<< "        else if (week == 42)\n"
		<< "          return 'November 15';\n"
		<< "        else if (week == 43)\n"
		<< "          return 'November 22';\n"
		<< "        else if (week == 44)\n"
		<< "          return 'December 1';\n"
		<< "        else if (week == 45)\n"
		<< "          return 'December 8';\n"
		<< "        else if (week == 46)\n"
		<< "          return 'December 15';\n"
		<< "        else if (week == 47)\n"
		<< "          return 'December 22';\n"
		<< "        else\n"
		<< "          return 'Error';\n"
		<< "      }\n\n"
		<< "      info.addTo(map);\n\n"
		<< "      var geoJson;\n"
		<< "      function buildColorLayer() {\n"
		<< "        geoJson = L.geoJson(regionData, {\n"
		<< "          style: style,\n"
		<< "          onEachFeature: onEachFeature\n"
		<< "        }).addTo(map);\n"
		<< "      }\n\n"
		<< "      var unhighlightOnExit = true;\n"
		<< "      var highlightOnEnter = true;\n"
		<< "      var week = 0;\n"
		<< "      var cycle = false;\n"
		<< "      var intervalHandle;\n"
		<< "      function updateMap() {\n"
		<< "        var weekSelect = document.getElementById('weekSelect');\n"
		<< "        week = weekSelect.options[weekSelect.selectedIndex].value;\n"
		<< "        if (week == -1) {\n"
		<< "          week = 0;\n"
		<< "          intervalHandle = setInterval(cycleWeek, 2000);\n"
		<< "        } else if (intervalHandle) {\n"
		<< "		  clearInterval(intervalHandle);\n"
		<< "		  intervalHandle = null;\n"
		<< "		}\n\n"
		<< "        updateMapDisplay();\n"
		<< "      }\n\n"
		<< "      function updateMapDisplay() {\n"
		<< "        geoJson.clearLayers();\n"
		<< "		unhighlightOnExit = true;\n"
		<< "        highlightOnEnter = true;\n"
		<< "        buildColorLayer();\n"
		<< "        info.update();\n"
		<< "	  }\n\n"
		<< "      function cycleWeek() {\n"
		<< "		week++;\n"
		<< "		if (week == 48) {\n"
		<< "		  week = 0;\n"
		<< "		}\n\n"
		<< "		updateMapDisplay();\n"
		<< "	  }\n\n"
		<< "      function getColor(d) {\n"
		<< "        return d > 90 ? '#800026' :\n"
		<< "          d > 80  ? '#BD0026' :\n"
		<< "          d > 70  ? '#E31A1C' :\n"
		<< "          d > 60  ? '#FC4E2A' :\n"
		<< "          d > 50   ? '#FD8D3C' :\n"
		<< "          d > 40   ? '#FEB24C' :\n"
		<< "          d > 30   ? '#FED976' :\n"
		<< "          d > 15   ? '#FFEDA0' :\n"
		<< "          d == 0   ? '#A9A9A9' :\n"
		<< "          '#FFFFCC';\n"
		<< "        }\n\n"
		<< "      function style(feature) {\n"
		<< "        return {\n"
		<< "          weight: 2,\n"
		<< "          opacity: 1,\n"
		<< "          color: 'white',\n"
		<< "          dashArray: '1',\n"
		<< "          fillOpacity: 0.3,\n"
		<< "          fillColor: getColor(feature.properties.weekData[week].probability)\n"
		<< "        };\n"
		<< "      }\n\n"
		<< "      var lastClicked;\n"
		<< "      function onClick(e) {\n"
		<< "		if (lastClicked) {\n"
		<< "          resetHighlight(lastClicked);\n"
		<< "        }\n"
		<< "        lastClicked = e;\n\n"
		<< "		highlightFeature(e);\n\n"
		<< "		highlightOnEnter = false;\n"
		<< "		unhighlightOnExit = false;\n\n"
		<< "		if (intervalHandle) {\n"
		<< "		  clearInterval(intervalHandle);\n"
		<< "		}\n\n"
		<< "        e.originalEvent.cancelBubble = true;\n"
		<< "	  }\n\n"
		<< "      function highlightFeature(e) {\n"
		<< "        var layer = e.target;\n\n"
		<< "        layer.setStyle({\n"
		<< "          weight: 5,\n"
		<< "          color: '#666',\n"
		<< "          dashArray: '',\n"
		<< "          fillOpacity: 0.5\n"
		<< "        });\n\n"
		<< "        if (!L.Browser.ie && !L.Browser.opera && !L.Browser.edge) {\n"
		<< "          layer.bringToFront();\n"
		<< "        }\n\n"
		<< "        info.update(layer.feature.properties);\n"
		<< "      }\n\n"
		<< "      function resetHighlight(e) {\n"
		<< "          geoJson.resetStyle(e.target);\n"
		<< "          info.update();\n"
		<< "      }\n\n"
		<< "      map.on('click', function(e) {\n"
		<< "        if (e.originalEvent.cancelBubble)\n"
		<< "          return;\n\n"
		<< "        if (lastClicked) {\n"
		<< "          resetHighlight(lastClicked);\n"
		<< "          highlightOnEnter = true;\n"
		<< "          unhighlightOnExit = true;\n"
		<< "        }\n"
		<< "      });\n\n"
		<< "      function onMouseOver(e) {\n"
		<< "        if (highlightOnEnter) {\n"
		<< "			highlightFeature(e);\n"
		<< "		}\n"
		<< "	  }\n\n"
		<< "	  function onMouseExit(e) {\n"
		<< "		  if (unhighlightOnExit) {\n"
		<< "			resetHighlight(e);\n"
		<< "		  }\n"
		<< "	  }\n\n"
		<< "      function onEachFeature(feature, layer) {\n"
		<< "        layer.on({\n"
		<< "          mouseover: onMouseOver,\n"
		<< "          mouseout: onMouseExit,\n"
		<< "          click: onClick\n"
		<< "        });\n"
		<< "      }\n\n"
		<< "      var legend = L.control({position: 'bottomright'});\n\n"
		<< "      legend.onAdd = function (map) {\n"
		<< "        var div = L.DomUtil.create('div', 'info legend'),\n"
		<< "          grades = [0, 15, 30, 40, 50, 60, 70, 80, 90],\n"
		<< "          labels = [],\n"
		<< "          from, to;\n\n"
		<< "        for (var i = 0; i < grades.length; i++) {\n"
		<< "          from = grades[i];\n"
		<< "          to = grades[i + 1];\n\n"
		<< "        labels.push(\n"
		<< "          '<i style=\"background:' + getColor(from + 1) + '\"></i> ' +\n"
		<< "          from + (to ? '&ndash;' + to : '+') + '%');\n"
		<< "        }\n\n"
		<< "        div.innerHTML = labels.join('<br>');\n"
		<< "        return div;\n"
		<< "      };\n\n"
		<< "      legend.addTo(map);\n\n"
		<< "      buildColorLayer();\n\n"
		<< "    </script>\n";
}

bool MapPageGenerator::WriteGeoJSONData(const UString::String& fileName,
	std::vector<ObservationInfo> observationProbabilities)
{
	log << "Retrieving county location data" << std::endl;
	const auto countryCodes(GetCountryCodeList(observationProbabilities));
	const auto countries(ebi.GetSubRegions(_T("world"), EBirdInterface::RegionType::Country));
	for (const auto& c : countries)
		countryLevelRegionInfoMap[c.code] = c;
	for (const auto& c : countryCodes)
	{
		if (std::find(highDetailCountries.begin(), highDetailCountries.end(), c) == highDetailCountries.end())
		{
			countryRegionInfoMap[c] = std::vector<EBirdInterface::RegionInfo>(1, countryLevelRegionInfoMap[c]);
			continue;
		}
		countryRegionInfoMap[c] = GetFullCountrySubRegionList(c);
	}

	std::vector<CountyInfo> countyInfo(observationProbabilities.size());
	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	auto countyIt(countyInfo.begin());
	for (const auto& entry : observationProbabilities)
	{
		assert(entry.locationCode.length() >= 2);
		for (unsigned int i = 0; i < entry.frequencyInfo.size(); ++i)
		{
			countyIt->weekInfo[i].frequencyInfo = entry.frequencyInfo[i];
			countyIt->weekInfo[i].probability = entry.probabilities[i];
		}
		countyIt->code = entry.locationCode;

		const auto nameIt(countryRegionInfoMap.find(entry.locationCode.substr(0, 2)));
		assert(nameIt != countryRegionInfoMap.end());
		const auto& nameLookupData(nameIt->second);
		pool.AddJob(std::make_unique<MapJobInfo>(*countyIt, entry, nameLookupData, *this));

		++countyIt;
	}

	pool.WaitForAllJobsComplete();

	cJSON* geoJSON;
	if (!CreateJSONData(countyInfo, kmlReductionLimit, geoJSON))
		return false;

	std::ofstream file(fileName);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << UString::ToStringType(fileName) << "' for output\n";
		cJSON_Delete(geoJSON);
		return false;
	}

	const auto jsonString(cJSON_PrintUnformatted(geoJSON));
	if (!jsonString)
	{
		Cerr << "Failed to generate JSON string\n";
		cJSON_Delete(geoJSON);
		return false;
	}

	file << "var regionData = " << jsonString << ";\n";
	free(jsonString);
	cJSON_Delete(geoJSON);

	return true;
}

UString::String MapPageGenerator::ForceTrailingSlash(const UString::String& path)
{
#ifdef _MSW_
	const auto slash(_T('\\'));
#else
	const auto slash(_T('/'));
#endif

	if (path.back() == slash)
		return path;
	return path + slash;
}

bool MapPageGenerator::CreateJSONData(const std::vector<CountyInfo>& observationData, const double& kmlReductionLimit, cJSON*& geoJSON)
{
	geoJSON = cJSON_CreateObject();
	if (!geoJSON)
	{
		Cerr << "Failed to create root JSON object for writing\n";
		return false;
	}

	cJSON_AddStringToObject(geoJSON, "type", "FeatureCollection");

	auto regions(cJSON_CreateArray());
	if (!regions)
	{
		Cerr << "Failed to create regions JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(geoJSON, "features", regions);

	for (const auto& o : observationData)
	{
		auto r(cJSON_CreateObject());
		if (!r)
		{
			Cerr << "Failed to create region JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(regions, r);
		if (!BuildObservationRecord(o, kmlReductionLimit, r))
			return false;
	}

	return true;
}

bool MapPageGenerator::BuildObservationRecord(const CountyInfo& observation, const double& kmlReductionLimit, cJSON* json)
{
	cJSON_AddStringToObject(json, "type", "Feature");

	auto observationData(cJSON_CreateObject());
	if (!observationData)
	{
		Cerr << "Failed to create observation data JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "properties", observationData);

	KMLToGeoJSONConverter kmlToGeoJson(UString::ToNarrowString(observation.geometryKML), kmlReductionLimit);
	auto geometry(kmlToGeoJson.GetGeoJSON());
	if (!geometry)
		return false;

	cJSON_AddItemToObject(json, "geometry", geometry);

	cJSON_AddStringToObject(observationData, "name", UString::ToNarrowString(observation.name).c_str());
	cJSON_AddStringToObject(observationData, "country", UString::ToNarrowString(observation.country).c_str());
	cJSON_AddStringToObject(observationData, "state", UString::ToNarrowString(observation.state).c_str());
	cJSON_AddStringToObject(observationData, "county", UString::ToNarrowString(observation.county).c_str());

	auto weekData(cJSON_CreateArray());
	if (!weekData)
	{
		Cerr << "Failed to create week data JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(observationData, "weekData", weekData);

	for (const auto& m : observation.weekInfo)
	{
		cJSON* week(cJSON_CreateObject());
		if (!week)
		{
			Cerr << "Failed to create week JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(weekData, week);
		if (!BuildWeekInfo(m, week))
			return false;
	}

	return true;
}

bool MapPageGenerator::BuildWeekInfo(CountyInfo::WeekInfo weekInfo, cJSON* json)
{
	cJSON_AddNumberToObject(json, "probability", weekInfo.probability * 100.0);
	auto speciesList(cJSON_CreateArray());
	if (!speciesList)
	{
		Cerr << "Failed to create species list JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "birds", speciesList);
	std::sort(weekInfo.frequencyInfo.begin(), weekInfo.frequencyInfo.end(),
		[](const EBirdDataProcessor::FrequencyInfo& a, const EBirdDataProcessor::FrequencyInfo& b)
	{
		return a.frequency > b.frequency;
	});
	for (const auto& m : weekInfo.frequencyInfo)
	{
		std::ostringstream ss;
		ss << UString::ToNarrowString(m.species) << " (" << std::fixed << std::setprecision(2) << m.frequency << "%)";
		auto species(cJSON_CreateString(ss.str().c_str()));
		if (!species)
		{
			Cerr << "Failed to create species JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(speciesList, species);
	}

	return true;
}

std::vector<UString::String> MapPageGenerator::GetCountryCodeList(const std::vector<ObservationInfo>& observationProbabilities)
{
	std::set<UString::String> codes;
	for (const auto& o : observationProbabilities)
		codes.insert(o.locationCode.substr(0, 2));

	std::vector<UString::String> codesVector(codes.size());
	auto it(codesVector.begin());
	for (const auto& c : codes)
	{
		*it = c;
		++it;
	}

	return codesVector;
}

UString::String MapPageGenerator::WrapEmptyString(const UString::String& s)
{
	if (s.empty())
		return _T("\"\"");
	return s;
}

bool MapPageGenerator::GetConfirmationFromUser()
{
	Cout << "Continue? (y/n) ";
	UString::String response;
	while (response.empty())
	{
		Cin >> response;
		if (StringUtilities::ToLower(response).compare(_T("y")) == 0)
			return true;
		if (StringUtilities::ToLower(response).compare(_T("n")) == 0)
			break;
		response.clear();
	}

	return false;
}

UString::String MapPageGenerator::AssembleCountyName(const UString::String& country, const UString::String& state, const UString::String& county)
{
	if (county.empty())
	{
		if (state.empty())
			return country;
		return state + _T(", ") + country;
	}
	return county + _T(", ") + state + _T(", ") + country;
}

void MapPageGenerator::LookupAndAssignKML(CountyInfo& data)
{
	UString::String countryName, stateName;
	LookupEBirdRegionNames(data.country, data.state, countryName, stateName);
	assert(!countryName.empty());
	data.geometryKML = kmlLibrary.GetKML(countryName, stateName, data.county);
	if (data.geometryKML.empty())
	{

		log << "\rWarning:  Geometry not found for '" << data.code << "\' (" << countryName;
		if (!stateName.empty())
			log << ", " << stateName;
		if (!data.county.empty())
			log << ", " << data.county;
		log << ')' << std::endl;
	}
}

void MapPageGenerator::AddRegionCodesToMap(const UString::String& parentCode, const EBirdInterface::RegionType& regionType)
{
	auto regionInfo(ebi.GetSubRegions(parentCode, regionType));
	for (const auto& r : regionInfo)
		eBirdRegionCodeToNameMap[r.code] = r.name;
}

void MapPageGenerator::LookupEBirdRegionNames(const UString::String& countryCode,
	const UString::String& subRegion1Code, UString::String& country, UString::String& subRegion1)
{
	const UString::String fullSubRegionCode([&countryCode, &subRegion1Code]()
	{
		if (subRegion1Code.empty())
			return countryCode;
		return countryCode + UString::Char('-') + subRegion1Code;
	}());
	std::shared_lock<std::shared_timed_mutex> lock(codeToNameMapMutex);
	auto countryIt(eBirdRegionCodeToNameMap.find(countryCode));
	if (countryIt == eBirdRegionCodeToNameMap.end())
	{
		MutexUtilities::AccessUpgrader exclusiveLock(lock);
		// Always need to re-check to ensure another thread didn't already add the
		// region names during the time we transitioned from shared to exclusive
		countryIt = eBirdRegionCodeToNameMap.find(countryCode);
		if (countryIt == eBirdRegionCodeToNameMap.end())// Confirmed - still need to do this work
		{
			AddRegionCodesToMap(_T("world"), EBirdInterface::RegionType::Country);

			countryIt = eBirdRegionCodeToNameMap.find(countryCode);
			if (countryIt == eBirdRegionCodeToNameMap.end())
			{
				log << "Failed to lookup country name for code '" << countryCode << '\'' << std::endl;
				return;
			}

			country = countryIt->second;
			AddRegionCodesToMap(countryCode, EBirdInterface::RegionType::SubNational1);
		}
		else
			country = countryIt->second;// Can't just pass through and make assignment before returning from function - need to do this before we give up mutex ownership to avoid possibility of invalidating iterator
	}
	else
		country = countryIt->second;

	if (subRegion1Code.empty())
		return;

	auto subNational1It(eBirdRegionCodeToNameMap.find(fullSubRegionCode));
	if (subNational1It == eBirdRegionCodeToNameMap.end())
	{
		MutexUtilities::AccessUpgrader exclusiveLock(lock);
		subNational1It = eBirdRegionCodeToNameMap.find(fullSubRegionCode);
		if (subNational1It == eBirdRegionCodeToNameMap.end())// Confirmed - still need to do this work
		{
			AddRegionCodesToMap(countryCode, EBirdInterface::RegionType::SubNational1);

			subNational1It = eBirdRegionCodeToNameMap.find(fullSubRegionCode);
			if (subNational1It == eBirdRegionCodeToNameMap.end())
			{
				log << "Failed to lookup region name for code '" << fullSubRegionCode << '\'' << std::endl;
				return;
			}
			subRegion1 = subNational1It->second;// Can't just pass through and make assignment before returning from function - need to do this before we give up mutex ownership to avoid possibility of invalidating iterator
		}
		else
			subRegion1 = subNational1It->second;
	}
	else
		subRegion1 = subNational1It->second;
}

void MapPageGenerator::MapJobInfo::DoJob()
{
	info.country = Utilities::ExtractCountryFromRegionCode(frequencyInfo.locationCode);
	info.state = Utilities::ExtractStateFromRegionCode(frequencyInfo.locationCode);
	UString::String stateName(info.state), countryName(info.country);
	for (const auto& r : regionNames)
	{
		if (r.code.compare(info.country) == 0)
			countryName = r.name;
		else if (r.code.compare(info.country + _T("-") + info.state) == 0)
			stateName = r.name;

		if (r.code.compare(frequencyInfo.locationCode) == 0)
		{
			auto hyphen(frequencyInfo.locationCode.find(UString::Char('-')));
			if (hyphen != std::string::npos)
			{
				hyphen = frequencyInfo.locationCode.find(UString::Char('-'), hyphen + 1);
				if (hyphen != std::string::npos)
					info.county = r.name;
			}

			info.name = AssembleCountyName(countryName, stateName, info.county);
			break;
		}
	}
	if (info.name.empty())
	{
		mpg.log << "No name found for region " << frequencyInfo.locationCode << std::endl;
		//assert(!info.country.empty() && !info.name.empty() && !info.code.empty());
	}
	mpg.LookupAndAssignKML(info);
}

UString::String MapPageGenerator::BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info)
{
	UString::OStringStream ss;
	ss << std::setprecision(2) << std::fixed;
	for (const auto& s : info)
	{
		if (s.species.empty())
			Cout << "Found one!" << std::endl;
		if (!ss.str().empty())
			ss << "<br>";
		ss << s.species << " (" << s.frequency << "%)";
	}

	return ss.str();
}

std::vector<EBirdInterface::RegionInfo> MapPageGenerator::GetFullCountrySubRegionList(const UString::String& countryCode) const
{
	auto regionList(ebi.GetSubRegions(countryCode, EBirdInterface::RegionType::MostDetailAvailable));
	if (regionList.empty())// most detail available is country - use as-is
	{
		const auto countryInfo(countryLevelRegionInfoMap.find(countryCode));
		assert(countryInfo != countryLevelRegionInfoMap.end());
		return std::vector<EBirdInterface::RegionInfo>(1, countryInfo->second);
	}

	const auto firstDash(regionList.front().code.find(UString::Char('-')));
	if (firstDash == std::string::npos)
		return regionList;// I don't think we should ever get here?

	const auto secondDash(regionList.front().code.find(UString::Char('-'), firstDash + 1));
	if (secondDash == std::string::npos)// most detail available is subregion 1 - use as-is
		return regionList;

	// If here, we have subregion2 data.  Let's also add subregion1 data.
	auto subRegion1List(ebi.GetSubRegions(countryCode, EBirdInterface::RegionType::SubNational1));
	regionList.insert(regionList.end(), subRegion1List.begin(), subRegion1List.end());
	return regionList;
}
