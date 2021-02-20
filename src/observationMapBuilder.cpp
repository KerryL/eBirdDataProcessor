// File:  observationMapBuilder.cpp
// Date:  11/29/2020
// Auth:  K. Loux
// Desc:  Object for building observation map HTML and JS files.

// Local headers
#include "observationMapBuilder.h"
#include "kmlToGeoJSONConverter.h"

bool ObservationMapBuilder::Build(const UString::String& outputFileName, const UString::String& kmlBoundaryFileName,
	const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo) const
{
	UString::String kml;
	if (!kmlBoundaryFileName.empty())
	{
		UString::IFStream file(kmlBoundaryFileName);
		UString::OStringStream buffer;
		buffer << file.rdbuf();
		kml = buffer.str();
	}
	
	auto lastDot(outputFileName.find_last_of('.'));
	const UString::String jsFileName(outputFileName.substr(0, lastDot) + _T(".js"));
	const UString::String htmlFileName(outputFileName.substr(0, lastDot) + _T(".html"));
	
	if (!WriteDataFile(jsFileName, kml, mapInfo))
		return false;
		
	if (!WriteHTMLFile(htmlFileName))
		return false;

	return true;
}

bool ObservationMapBuilder::WriteDataFile(const UString::String& fileName, const UString::String& kml, const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo) const
{
	UString::OFStream file(fileName);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}
	
	const double& kmlReductionLimit(0.0);// 0.0 == don't do any reduction
	cJSON* geoJSON;
	if (!CreateJSONData(kml, kmlReductionLimit, geoJSON))
		return false;

	const auto jsonString(cJSON_PrintUnformatted(geoJSON));
	if (!jsonString)
	{
		Cerr << "Failed to generate JSON string\n";
		cJSON_Delete(geoJSON);
		return false;
	}

	file << "var boundaryData = " << jsonString << ";\n";
	free(jsonString);
	cJSON_Delete(geoJSON);

	cJSON* mapInfoJSON;
	if (!CreateJSONData(mapInfo, mapInfoJSON))
		return false;
		
	const auto dataJSONString(cJSON_PrintUnformatted(mapInfoJSON));
	if (!dataJSONString)
	{
		Cerr << "Failed to generate map info JSON string\n";
		cJSON_Delete(mapInfoJSON);
		return false;
	}
	
	file << "var mapInfo = " << dataJSONString << ";\n";
	free(dataJSONString);
	cJSON_Delete(mapInfoJSON);
	
	return true;
}

bool ObservationMapBuilder::CreateJSONData(const UString::String& kml, const double& kmlReductionLimit, cJSON*& geoJSON)
{
	geoJSON = cJSON_CreateObject();
	if (!geoJSON)
	{
		Cerr << "Failed to create geometry JSON object for writing\n";
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

	auto r(cJSON_CreateObject());
	if (!r)
	{
		Cerr << "Failed to create region JSON object\n";
		return false;
	}

	cJSON_AddItemToArray(regions, r);
	if (!BuildGeometryJSON(kml, kmlReductionLimit, r))
		return false;

	return true;
}

bool ObservationMapBuilder::BuildGeometryJSON(const UString::String& kml, const double& kmlReductionLimit, cJSON* json)
{
	cJSON_AddStringToObject(json, "type", "Feature");

	auto geometryData(cJSON_CreateObject());
	if (!geometryData)
	{
		Cerr << "Failed to create geometry data JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "properties", geometryData);

	KMLToGeoJSONConverter kmlToGeoJson(UString::ToNarrowString(kml), kmlReductionLimit);
	auto geometry(kmlToGeoJson.GetGeoJSON());
	if (!geometry)
		return false;

	cJSON_AddItemToObject(json, "geometry", geometry);
	return true;
}

bool ObservationMapBuilder::CreateJSONData(const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo, cJSON*& mapJSON)
{
	mapJSON = cJSON_CreateObject();
	if (!mapJSON)
	{
		Cerr << "Failed to create map JSON object for writing\n";
		return false;
	}

	cJSON_AddStringToObject(mapJSON, "type", "FeatureCollection");

	auto locations(cJSON_CreateArray());
	if (!locations)
	{
		Cerr << "Failed to create locations JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(mapJSON, "features", locations);

	for (const auto& m : mapInfo)
	{
		auto jsonMapInfo(cJSON_CreateObject());
		if (!jsonMapInfo)
		{
			Cerr << "Failed to create map info JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(locations, jsonMapInfo);
		if (!BuildLocationJSON(m, jsonMapInfo))
			return false;
	}

	return true;
}

bool ObservationMapBuilder::BuildLocationJSON(const EBirdDatasetInterface::MapInfo& mapInfo, cJSON* json)
{
	cJSON_AddStringToObject(json, "name", UString::ToNarrowString(mapInfo.locationName).c_str());
	cJSON_AddNumberToObject(json, "latitude", mapInfo.latitude);
	cJSON_AddNumberToObject(json, "longitude", mapInfo.longitude);
	
	auto checklistList(cJSON_CreateArray());
	if (!checklistList)
	{
		Cerr << "Failed to create checklist list JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "checklists", checklistList);
	auto checklistCopy(mapInfo.checklists);
	std::sort(checklistCopy.begin(), checklistCopy.end(),
		[](const EBirdDatasetInterface::MapInfo::ChecklistInfo& a, const EBirdDatasetInterface::MapInfo::ChecklistInfo& b)
	{
		const auto dash1a(a.dateString.find('-'));
		const auto dash1b(b.dateString.find('-'));
		const auto dash2a(a.dateString.find_last_of('-'));
		const auto dash2b(b.dateString.find_last_of('-'));
		assert(dash1a != std::string::npos && dash2a != std::string::npos && dash1b != std::string::npos && dash2b != std::string::npos);
		assert(dash1a != dash2a && dash1b != dash2b);
		
		UString::IStringStream ss;
		unsigned int aValue, bValue;
		
		// Year
		ss.str(a.dateString.substr(dash2a + 1));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(dash2b + 1));
		ss >> bValue;
		if (aValue > bValue)
			return true;
		else if (aValue < bValue)
			return false;
			
		// Month
		ss.clear();
		ss.str(a.dateString.substr(0, dash1a));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(0, dash1b));
		ss >> bValue;
		if (aValue > bValue)
			return true;
		else if (aValue < bValue)
			return false;
			
		// Day
		ss.clear();
		ss.str(a.dateString.substr(dash1a + 1, dash2a - dash1a - 1));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(dash1b + 1, dash2b - dash1b - 1));
		ss >> bValue;
		return aValue > bValue;
	});
	for (const auto& c : checklistCopy)
	{
		auto checklistJSON(cJSON_CreateObject());
		if (!checklistJSON)
		{
			Cerr << "Failed to create checklist JSON object\n";
			return false;
		}
		
		cJSON_AddStringToObject(checklistJSON, "checklistID", UString::ToNarrowString(c.id).c_str());
		cJSON_AddStringToObject(checklistJSON, "date", UString::ToNarrowString(c.dateString).c_str());
		cJSON_AddNumberToObject(checklistJSON, "speciesCount", c.speciesCount);
		cJSON_AddItemToArray(checklistList, checklistJSON);
	}

	return true;
}

bool ObservationMapBuilder::WriteHTMLFile(const UString::String& fileName) const
{
	/*
	 * <!DOCTYPE html>
<html>
  <head>
    <title>CBC Area 12</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <meta charset="utf-8">
    <style>
      #mapid {
        height: 95%;
      }
      html, body {
        height: 100%;
        margin: 0;
        padding: 0;
      }
	  .info { padding: 6px 8px; font: 14px/16px Arial, Helvetica, sans-serif; background: white; background: rgba(255,255,255,0.8); box-shadow: 0 0 15px rgba(0,0,0,0.2); border-radius: 5px; }
	  .info h4 { margin: 0 0 5px; color: #777; }
      .legend { text-align: left; line-height: 18px; color: #555; }
      .legend i { width: 18px; height: 18px; float: left; margin-right: 8px; opacity: 0.7; }
	  #speciesList { width:100%; }
    </style>
	<link rel="stylesheet" href="https://unpkg.com/leaflet@1.4.0/dist/leaflet.css" integrity="sha512-puBpdR0798OZvTTbP4A8Ix/l+A4dHDD0DGqYW6RQ+9jxkRFclaxxQb/SJAWZfWAkuyeQUytO7+7N4QKrDh+drA==" crossorigin=""/>
	<script src="https://unpkg.com/leaflet@1.4.0/dist/leaflet.js" integrity="sha512-QVftwZFqvtRNi0ZyCtsznlKSWOStnDORoefr1enyq5mVL4tmKB3S/EnC3rRJcxCPavG10IcrVGSmPh6Qw5lwrg==" crossorigin=""></script>
  </head>

  <body>
    <div id="mapid"></div>

	<script type="text/javascript" src="cbcArea12.js"></script>
	
	<div style='font-family: sans-serif'>
	<label>Start Date:</label>
      <input type="text" id="startDate" name="startDate" onchange="updateMapDisplay()"></script>
      <label>End Date:</label>
      <input type="text" id="endDate" name="endDate" onchange="updateMapDisplay()"></input>
    </div>

    <script type="text/javascript">
      var map = L.map('mapid').setView([40.2, -74.84], 13);

      // Leaflet gray map imagery
    //  L.tileLayer('https://api.tiles.mapbox.com/v4/{id}/{z}/{x}/{y}.png?access_token=pk.eyJ1IjoibWFwYm94IiwiYSI6ImNpejY4NXVycTA2emYycXBndHRqcmZ3N3gifQ.rJcFIG214AriISLbB6B5aw', {
    //    maxZoom: 18,
    //    attribution: 'Map data &copy; <a href="https://www.openstreetmap.org/">OpenStreetMap</a> contributors, ' +
    //    '<a href="https://creativecommons.org/licenses/by-sa/2.0/">CC-BY-SA</a>, ' +
    //    'Imagery © <a href="https://www.mapbox.com/">Mapbox</a>',
    //    id: 'mapbox.light'
    //  }).addTo(map);
      // Google maps hybrid imagery
      L.tileLayer('http://{s}.google.com/vt/lyrs=s,h&x={x}&y={y}&z={z}',{
        maxZoom: 20, subdomains:['mt0','mt1','mt2','mt3']}).addTo(map);

      var info = L.control();

      info.onAdd = function (map) {
        this._div = L.DomUtil.create('div', 'info');
        return this._div;
      };

      info.addTo(map);

      var geoJson;
      function buildBorderLayer() {
        geoJson = L.geoJson(boundaryData, {
          style: style,
          onEachFeature: onEachFeature
        }).addTo(map);
      }
      
      function dateIsInRange(date, startYear, startMonth, startDay, endYear, endMonth, endDay) {
        var testDateParsed = parseDate(date);
        var testDate = new Date(testDateParsed[0], testDateParsed[1] - 1, testDateParsed[2], 12, 0, 0, 0);// Noon for test date
        var startDate = new Date(startYear, startMonth - 1, startDay, 0, 0, 0, 0);// early
        var endDate = new Date(endYear, endMonth - 1, endDay, 23, 59, 59, 0);// late
        
        if (startYear == 0 && endYear == 0) {// Make comparison without considering year
          if (startMonth == 0 && endMonth == 0) {// No range specified
			  return true;
          }
          
          startDate.setFullYear(testDate.getFullYear());
          endDate.setFullYear(testDate.getFullYear());
          if (endDate < startDate) {
            if (testDate < endDate) {
              testDate.setFullYear(testDate.getFullYear() + 1);
            }
            endDate.setFullYear(endDate.getFullYear() + 1);
          }
        }
        else {
          if (startMonth == 0 && endMonth == 0) {// Only years were specified
			  return testDate.getFullYear() >= startYear && testDate.getFullYear() <= endYear;
          }
          // Otherwise, we have fully specified dates
        }

        return testDate >= startDate && testDate <= endDate;
      }
      
      // This should work with dates specified without years as well as dates specified with years or only years
      function parseDate(date) {
        var year = 0;
        var month = 0;
        var day = 0;
        
        var sep;
        if ((date.match(/-/g) || []).length > (date.match('//g') || []).length) {
          sep = '-';
        }
        else {
			sep = '/';
        }

        if (date.length == 4 && date.indexOf(sep) == -1) {// Year only
			year = date;
        }
        else if (date.length >= 3 && date.indexOf(sep) == date.lastIndexOf(sep)) {// No year
          month = parseInt(date.substr(0, date.indexOf(sep)));
          day = parseInt(date.slice(date.lastIndexOf(sep) + 1, date.length));
        }
        else if (date.length >= 8) {// Fully specified
          month = parseInt(date.substr(0, date.indexOf(sep)));
          day = parseInt(date.slice(date.indexOf(sep) + 1, date.lastIndexOf('-')));
          year = parseInt(date.slice(date.lastIndexOf(sep) + 1, date.length));
        }

        return [year, month, day];
      }
      
      var markerLayerGroup = L.layerGroup().addTo(map);
      function buildPointLayer() {
		  var startDate = parseDate(document.getElementById('startDate').value);
		  var startDateYear = startDate[0];
		  var startDateMonth = startDate[1];
		  var startDateDay = startDate[2];
		  
		  var endDate = parseDate(document.getElementById('endDate').value);
		  var endDateYear = endDate[0];
		  var endDateMonth = endDate[1];
		  var endDateDay = endDate[2];

		  for (const location of mapInfo.features) {
		    var marker = L.marker([location.latitude, location.longitude]);
		    var popupText = "<h3>" + location.name + "</h3><p><table><tr><td>Checklist Link</td><td>Species Count</td></tr>";
		    var checklistCount = 0;
		    for (const checklist of location.checklists) {
              if (dateIsInRange(checklist.date, startDateYear, startDateMonth, startDateDay, endDateYear, endDateMonth, endDateDay)) {
                popupText += '<tr><td><a href="https://ebird.org/checklist/' + checklist.checklistID + '" target="_blank">' + checklist.date + '</a></td><td>' + checklist.speciesCount + '</td></tr>';
                ++checklistCount;
              }
			}

            // Don't add the marker if there aren't any checklists (within the specified date range)
			if (checklistCount == 0)
              continue;

			popupText += '</table></p>';
			// TODO:  Needs scrolling section (so title doesn't scroll)
			marker.addTo(markerLayerGroup);
		    marker.bindPopup(popupText, { maxHeight:300 });
	    }
	  }

      function updateMapDisplay() {
        geoJson.setStyle(style);
        markerLayerGroup.clearLayers();
        buildPointLayer();
	  }

      function style(feature) {
        return {
          weight: 2,
          opacity: 1,
          color: 'red',
          dashArray: '1',
          fillOpacity: 0.0,
          fillColor: 'white'
        };
      }

      function onClick(e) {
        e.originalEvent.cancelBubble = true;
	  }

      map.on('click', function(e) {
        if (e.originalEvent.cancelBubble)
          return;
      });

      function onEachFeature(feature, layer) {
        layer.on({
          click: onClick
        });
      }

      buildBorderLayer();
      buildPointLayer();

    </script>
  </body>
</html>

*/
	// TODO:  Implement
	return false;
}
