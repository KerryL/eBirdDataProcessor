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
		ss.str(a.dateString.substr(dash2a));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(dash2b));
		ss >> bValue;
		if (bValue < aValue)
			return false;
		else if (bValue > aValue)
			return true;
			
		// Month
		ss.clear();
		ss.str(a.dateString.substr(0, dash1a));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(0, dash1b));
		ss >> bValue;
		if (bValue < aValue)
			return false;
		else if (bValue > aValue)
			return true;
			
		// Day
		ss.clear();
		ss.str(a.dateString.substr(dash1a, dash2a - dash1a));
		ss >> aValue;
		ss.clear();
		ss.str(b.dateString.substr(dash1b, dash2b - dash1b));
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
	// TODO:  Implement
	return false;
}
