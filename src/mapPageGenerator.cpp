// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.
//        To view table data, go to:
//        https://fusiontables.google.com/data?docid=1YzezTGDbYkUwOqvZ_yo0nKU8JjbAoEjYw01Sgcga

// Local headers
#include "mapPageGenerator.h"
#include "googleMapsInterface.h"
#include "frequencyDataHarvester.h"

// Standard C++ headers
#include <sstream>
#include <iomanip>
#include <cctype>
#include <iostream>
#include <thread>
#include <algorithm>
#include <cassert>

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

const unsigned int MapPageGenerator::mapsAPIRateLimit(45);// [requests per sec] (actual limit is 50/sec, but our algorithm isn't perfect...)
const ThrottledSection::Clock::duration MapPageGenerator::mapsAPIMinDuration(std::chrono::milliseconds(static_cast<long long>(1000.0 / mapsAPIRateLimit)));// 50 requests per second
const ThrottledSection::Clock::duration MapPageGenerator::fusionTablesAPIMinDuration(std::chrono::milliseconds(static_cast<long long>(60000.0 / GFTI::writeRequestRateLimit)));// 30 requests per minute

const unsigned int MapPageGenerator::columnCount(42);
const unsigned int MapPageGenerator::importCellCountLimit(100000);
const unsigned int MapPageGenerator::importSizeLimit(1024 * 1024);// 1 MB

MapPageGenerator::MapPageGenerator() : mapsAPIRateLimiter(mapsAPIMinDuration), fusionTablesAPIRateLimiter(fusionTablesAPIMinDuration)
{
}

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
	std::vector<CountyInfo> existingData;
	for (const auto& t : tableList)
	{
		if (t.name.compare(birdProbabilityTableName) == 0 && t.columns.size() == columnCount)// TODO:  Better check necessary?
		{
			std::cout << "Found existing table " << t.tableId << std::endl;
			tableId = t.tableId;

			if (GetExistingCountyData(existingData, fusionTables, tableId))
				break;

			tableId.clear();
		}
	}

	if (tableId.empty())
	{
		GFTI::TableInfo tableInfo(BuildTableLayout());
		fusionTablesAPIRateLimiter.Wait();
		if (!fusionTables.CreateTable(tableInfo))
		{
			std::cerr << "Failed to create fusion table\n";
			return false;
		}

		tableId = tableInfo.tableId;
		std::cout << "Created new fusion table " << tableId << std::endl;
		fusionTablesAPIRateLimiter.Wait();
		if (!fusionTables.SetTableAccess(tableInfo.tableId, GoogleFusionTablesInterface::TableAccess::Public))
			std::cerr << "Failed to make table public\n";
	}

	const auto duplicateRowsToDelete(FindDuplicatesAndBlanksToRemove(existingData));
	if (duplicateRowsToDelete.size() > 0)
	{
		std::cout << "Deleting " << duplicateRowsToDelete.size() << " duplicate entires" << std::endl;
		if (!DeleteRowsBatch(fusionTables, tableId, duplicateRowsToDelete))
		{
			std::cerr << "Failed to remove duplicates\n";
			return false;
		}
	}

	const auto rowsToDelete(DetermineDeleteUpdateAdd(existingData, observationProbabilities));
	if (rowsToDelete.size() > 0)
	{
		std::cout << "Deleting " << rowsToDelete.size() << " rows to prepare for update" << std::endl;
		if (!DeleteRowsBatch(fusionTables, tableId, rowsToDelete))
		{
			std::cerr << "Failed to remove rows for update\n";
			return false;
		}
	}

	std::cout << "Retrieving geometry data" << std::endl;
	std::vector<CountyGeometry> geometry;
	if (!GetCountyGeometry(fusionTables, geometry))
		return false;

	std::cout << "Retrieving county location data" << std::endl;
	// NOTE:  Google maps geocoding API has rate limit of 50 queries per sec, and usage limit of 2500 queries per day
	std::vector<CountyInfo> countyInfo(observationProbabilities.size());
	ThreadPool pool(std::thread::hardware_concurrency() * 2, mapsAPIRateLimit);
	auto countyIt(countyInfo.begin());
	for (const auto& entry : observationProbabilities)
	{
		countyIt->frequencyInfo = std::move(entry.frequencyInfo);
		countyIt->probabilities = std::move(entry.probabilities);
		if (!CopyExistingDataForCounty(entry, existingData, *countyIt, geometry))
			pool.AddJob(std::make_unique<MapJobInfo>(
				*countyIt, entry, keys.googleMapsKey, geometry, *this));

		++countyIt;
	}

	pool.WaitForAllJobsComplete();

	std::cout << "Preparing data for upload" << std::endl;
	std::ostringstream ss;
	unsigned int rowCount(0);
	unsigned int cellCount(0);
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

		++rowCount;
		cellCount += columnCount;

		const double sizeAllowanceFactor(1.5);
		const unsigned int averageRowSize(static_cast<unsigned int>(ss.str().length() / rowCount * sizeAllowanceFactor));
		if (cellCount + columnCount > importCellCountLimit ||
			ss.str().length() + averageRowSize > importSizeLimit)
		{
			if (!UploadBuffer(fusionTables, tableId, ss.str()))
				return false;

			ss.str("");
			ss.clear();
			rowCount = 0;
			cellCount = 0;
		}
	}

	if (!ss.str().empty())
	{
		if (!UploadBuffer(fusionTables, tableId, ss.str()))
			return false;
	}

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

bool MapPageGenerator::DeleteRowsBatch(GoogleFusionTablesInterface& fusionTables,
	const std::string& tableId, const std::vector<unsigned int>& rowIds)
{
	const unsigned int maxDeleteSize(500);
	auto startIt(rowIds.begin());
	while (startIt != rowIds.end())
	{
		const unsigned int batchSize(std::min(maxDeleteSize,
			static_cast<unsigned int>(std::distance(startIt, rowIds.end()))));
		auto endIt(rowIds.begin() + batchSize);
		fusionTablesAPIRateLimiter.Wait();
		if (!fusionTables.DeleteRows(tableId, std::vector<unsigned int>(startIt, endIt)))
			return false;
		startIt = endIt;
	}

	return true;
}

bool MapPageGenerator::UploadBuffer(GFTI& fusionTables, const std::string& tableId, const std::string& buffer)
{
	fusionTablesAPIRateLimiter.Wait();
	if (!fusionTables.Import(tableId, buffer))
	{
		std::cerr << "Failed to import data\n";
		return false;
	}

	return true;
}

bool MapPageGenerator::ProcessJSONQueryResponse(cJSON* root, std::vector<CountyInfo>& data)
{
	cJSON* rowsArray(cJSON_GetObjectItem(root, "rows"));
	if (!rowsArray)
	{
		std::cerr << "Failed to get rows array\n";
		cJSON_Delete(root);
		return false;
	}

	data.resize(cJSON_GetArraySize(rowsArray));
	unsigned int i(0);
	for (auto& row : data)
	{
		cJSON* rowRoot(cJSON_GetArrayItem(rowsArray, i++));
		if (!rowRoot)
		{
			cJSON_Delete(root);
			std::cerr << "Failed to read existing row\n";
			return false;
		}

		if (!ReadExistingCountyData(rowRoot, row))
		{
			cJSON_Delete(root);
			return false;
		}
	}

	cJSON_Delete(root);
	return true;
}

bool MapPageGenerator::ProcessCSVQueryResponse(const std::string& csvData, std::vector<CountyInfo>& data)
{
	// TODO:  Implement
	return false;
}

bool MapPageGenerator::GetExistingCountyData(std::vector<CountyInfo>& data,
	GFTI& fusionTables, const std::string& tableId)
{
	cJSON* root(nullptr);
	std::string csvData;
	const std::string query("SELECT ROWID,State,County,Name,Location,Geometry,'Probability-Jan','Probability-Feb','Probability-Mar','Probability-Apr','Probability-May','Probability-Jun','Probability-Jul','Probability-Aug','Probability-Sep','Probability-Oct','Probability-Nov','Probability-Dec' FROM " + tableId + "&typed=false");
	if (!fusionTables.SubmitQuery(query, root, &csvData))
		return false;

	if (root)
		return ProcessJSONQueryResponse(root, data);
	return ProcessCSVQueryResponse(csvData, data);
}

bool MapPageGenerator::ReadExistingCountyData(cJSON* row, CountyInfo& data)
{
	cJSON* rowID(cJSON_GetArrayItem(row, 0));
	cJSON* state(cJSON_GetArrayItem(row, 1));
	cJSON* county(cJSON_GetArrayItem(row, 2));
	cJSON* name(cJSON_GetArrayItem(row, 3));
	cJSON* location(cJSON_GetArrayItem(row, 4));
	cJSON* kml(cJSON_GetArrayItem(row, 5));
	cJSON* jan(cJSON_GetArrayItem(row, 6));
	cJSON* feb(cJSON_GetArrayItem(row, 7));
	cJSON* mar(cJSON_GetArrayItem(row, 8));
	cJSON* apr(cJSON_GetArrayItem(row, 9));
	cJSON* may(cJSON_GetArrayItem(row, 10));
	cJSON* jun(cJSON_GetArrayItem(row, 11));
	cJSON* jul(cJSON_GetArrayItem(row, 12));
	cJSON* aug(cJSON_GetArrayItem(row, 13));
	cJSON* sep(cJSON_GetArrayItem(row, 14));
	cJSON* oct(cJSON_GetArrayItem(row, 15));
	cJSON* nov(cJSON_GetArrayItem(row, 16));
	cJSON* dec(cJSON_GetArrayItem(row, 17));

	if (!rowID || !state || !county || !name || !location || !kml ||
		!jan || !feb || !mar || !apr || !may || !jun ||
		!jul || !aug || !sep || !oct || !nov || !dec)
	{
		std::cerr << "Failed to get county information from existing row\n";
		return false;
	}

	data.state = state->valuestring;
	data.county = county->valuestring;
	data.geometryKML = kml->valuestring;
	data.name = name->valuestring;

	if (!Read(rowID, data.rowId))
	{
		std::cerr << "Failed to read row ID\n";
		return false;
	}

	if (!Read(jan, data.probabilities[0]) ||
		!Read(feb, data.probabilities[1]) ||
		!Read(mar, data.probabilities[2]) ||
		!Read(apr, data.probabilities[3]) ||
		!Read(may, data.probabilities[4]) ||
		!Read(jun, data.probabilities[5]) ||
		!Read(jul, data.probabilities[6]) ||
		!Read(aug, data.probabilities[7]) ||
		!Read(sep, data.probabilities[8]) ||
		!Read(oct, data.probabilities[9]) ||
		!Read(nov, data.probabilities[10]) ||
		!Read(dec, data.probabilities[11]))
	{
		std::cerr << "Failed to read existing probability\n";
		return false;
	}

	std::istringstream locationStream(location->valuestring);
	std::string s;
	std::getline(locationStream, s, ' ');
	std::istringstream ss(s);
	if ((ss >> data.latitude).fail())
	{
		std::cerr << "Failed to read latitude\n";
		return false;
	}

	std::getline(locationStream, s, ' ');
	ss.clear();
	ss.str(s);
	if ((ss >> data.longitude).fail())
	{
		std::cerr << "Failed to read longitude\n";
		return false;
	}

	// TODO:  Can this be improved?  Parse KML?
	data.neLatitude = data.latitude;
	data.neLongitude = data.longitude;
	data.swLatitude = data.latitude;
	data.swLongitude = data.longitude;

	return true;
}

// For each row in existingData, there are three possible outcomes:
// 1.  newData has no entry corresponding to existing row -> Delete row
//     -> Delete row from table and delete entry from existingData
// 2.  newData and row have corresponding entries, probability data is the same -> Leave row as-is
//     -> Delete entry from existingData, but do not delete from table
// 3.  newData and row have corresponding entries, probability data is different -> Update row (Delete + re-import)
//     -> Delete row from table, but do not delete from existingData
std::vector<unsigned int> MapPageGenerator::DetermineDeleteUpdateAdd(
	std::vector<CountyInfo>& existingData, const std::vector<ObservationInfo>& newData)
{
	std::vector<unsigned int> deletedRows;
	existingData.erase(std::remove_if(existingData.begin(), existingData.end(),
		[&deletedRows, &newData](const CountyInfo& c)
		{
			auto matchingEntry(NewDataIncludesMatchForCounty(newData, c));
			if (matchingEntry == newData.end())
			{
				// Need to delete row, and we can also remove row from existingData (Case #1)
				deletedRows.push_back(c.rowId);
				return true;
			}

			if (!ProbabilityDataHasChanged(*matchingEntry, c))
				return true;// Leave row as-is (Case #2)

			// Need to delete row, but don't remove it from exisingData (Case #3)
			deletedRows.push_back(c.rowId);
			return false;
		}), existingData.end());

	return deletedRows;
}

//
std::vector<unsigned int> MapPageGenerator::FindDuplicatesAndBlanksToRemove(std::vector<CountyInfo>& existingData)
{
	std::vector<unsigned int> deletedRows;
	auto startIterator(existingData.begin());
	for (const auto item : existingData)
	{
		bool deletedSelf(false);
		auto it(++startIterator);
		for (; it != existingData.end(); ++it)
		{
			if (item.state.empty() || item.county.empty() ||
				(item.latitude == 0.0 && item.longitude == 0.0 && item.neLatitude == 0.0 &&
				item.swLatitude == 0.0 && item.neLongitude == 0.0 && item.swLongitude == 0.0))
			{
				deletedRows.push_back(item.rowId);
				continue;
			}

			if (it->state.compare(item.state) != 0)
				continue;
			else if (!CountyNamesMatch(item.county, it->county))
				continue;

			deletedRows.push_back(it->rowId);// Always delete the second row
			// If geometry matches, allow the first one in the list to remain, otherwise delete self, too (not sure which set of data is right)
			if (!deletedSelf &&
				(item.latitude != it->latitude || item.longitude != it->longitude ||
				item.neLatitude != it->neLatitude || item.neLongitude != it->neLongitude ||
				item.swLatitude != it->swLatitude || item.swLongitude != it->swLongitude))
			{
				deletedRows.push_back(item.rowId);
				deletedSelf = true;
			}
		}
	}

	existingData.erase(std::remove_if(existingData.begin(), existingData.end(),
		[&deletedRows](const CountyInfo& c)
		{
			for (const auto& r : deletedRows)
			{
				if (c.rowId == r)
					return true;
			}

			return false;
		}), existingData.end());

	return deletedRows;
}

std::vector<MapPageGenerator::ObservationInfo>::const_iterator
	MapPageGenerator::NewDataIncludesMatchForCounty(
	const std::vector<ObservationInfo>& newData, const CountyInfo& county)
{
	auto it(newData.begin());
	for (; it != newData.end(); ++it)
	{
		if (FrequencyDataHarvester::GenerateFrequencyFileName(
			county.state, county.county).compare(FrequencyDataHarvester::StripDirectory(it->locationHint)) == 0)// TODO:  Not really a worse place to put StripDirectory() efficiency-wise...
			break;
	}

	return it;
}

bool MapPageGenerator::ProbabilityDataHasChanged(
	const ObservationInfo& newData, const CountyInfo& existingData)
{
	unsigned int i;
	for (i = 0; i < 12; ++i)
	{
		const double probabilityTolerance(0.1);// [%]
		if (fabs(newData.probabilities[i] * 100.0 - existingData.probabilities[i]) > probabilityTolerance)
			return true;
	}

	return false;
}

bool MapPageGenerator::CopyExistingDataForCounty(const ObservationInfo& entry,
	const std::vector<CountyInfo>& existingData, CountyInfo& newData,
	const std::vector<CountyGeometry>& geometry)
{
	for (const auto& existing : existingData)
	{
		if (FrequencyDataHarvester::GenerateFrequencyFileName(
			existing.state, existing.county).compare(FrequencyDataHarvester::StripDirectory(entry.locationHint)) == 0)
		{
			newData.name = existing.name;
			newData.state = existing.state;
			newData.county = existing.county;

			newData.latitude = existing.latitude;
			newData.longitude = existing.longitude;
			newData.neLatitude = existing.neLatitude;
			newData.neLongitude = existing.neLongitude;
			newData.swLatitude = existing.swLatitude;
			newData.swLongitude = existing.swLongitude;

			newData.geometryKML = std::move(existing.geometryKML);

			if (newData.geometryKML.empty())
				LookupAndAssignKML(geometry, newData);

			return true;
		}
	}

	return false;
}

void MapPageGenerator::LookupAndAssignKML(const std::vector<CountyGeometry>& geometry, CountyInfo& data)
{
	for (const auto& g : geometry)
	{
		std::string countyString(ToLower(StripCountyFromName(data.county)));
		if (g.state.compare(data.state) == 0 && ToLower(g.county).compare(countyString) == 0)
		{
			data.geometryKML = g.kml;
			break;
		}
	}

	if (data.geometryKML.empty())
		std::cerr << "Warning:  Geometry not found for '" << data.name << "'\n";
}

void MapPageGenerator::MapJobInfo::DoJob()
{
	if (!GetStateAbbreviationFromFileName(frequencyInfo.locationHint, info.state))
		std::cerr << "Warning:  Failed to get state abberviation for '" << frequencyInfo.locationHint << "'\n";

	if (!GetCountyNameFromFileName(frequencyInfo.locationHint, info.county))
		std::cerr << "Warning:  Failed to get county name for '" << frequencyInfo.locationHint << "'\n";

	if (!mpg.GetLatitudeAndLongitudeFromCountyAndState(info.state, info.county + " County",
		info.latitude, info.longitude, info.neLatitude,
		info.neLongitude, info.swLatitude, info.swLongitude, info.name, googleMapsKey))
		std::cerr << "Warning:  Failed to get location information for '" << frequencyInfo.locationHint << "'\n";

	const std::string::size_type saintStart(info.name.find("St "));
	if (saintStart != std::string::npos)
		info.name.insert(saintStart + 2, ".");

	info.county = info.name.substr(0, info.name.find(','));// TODO:  Make robust
	LookupAndAssignKML(geometry, info);
}

bool MapPageGenerator::CountyNamesMatch(const std::string& a, const std::string& b)
{
	if (a.compare(b) == 0)
		return true;

	std::string cleanA(ToLower(StripCountyFromName(a)));
	std::string cleanB(ToLower(StripCountyFromName(b)));
	if (cleanA.compare(cleanB) == 0)
		return true;

	if (CleanFileName(cleanA).compare(CleanFileName(cleanB)) == 0)
		return true;

	return false;
}

std::string MapPageGenerator::ToLower(const std::string& s)
{
	std::string lower(s);
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	return lower;
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

	assert(tableInfo.columns.size() == columnCount);

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
		const std::chrono::steady_clock::time_point start(std::chrono::steady_clock::now());
		// According to Google docs, if we get this error, another attempt
		// may succeed.  In practice, this seems to be true.
		const std::string unknownErrorStatus("UNKNOWN_ERROR");
		std::string status;
		GoogleMapsInterface gMap("County Lookup Tool", googleMapsKey);
		mapsAPIRateLimiter.Wait();
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

	county = FrequencyDataHarvester::StripDirectory(fileName.substr(0, position - 2));

	return true;
}

std::string MapPageGenerator::StripCountyFromName(const std::string& s)
{
	const std::string endString1(" County");
	std::string clean(s);
	std::string::size_type endPosition(clean.find(endString1));
	if (endPosition != std::string::npos)
		clean = clean.substr(0, endPosition);

	const std::string endString2("Parish");
	endPosition = clean.find(endString2);
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
			c == '.' ||
			c == '-')
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
		fusionTablesAPIRateLimiter.Wait();
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
		fusionTablesAPIRateLimiter.Wait();
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

	GoogleFusionTablesInterface::StyleInfo::Options fillColorOption;
	fillColorOption.type = GoogleFusionTablesInterface::StyleInfo::Options::Type::Complex;
	fillColorOption.key = "fillColorStyler";
	fillColorOption.c.push_back(GoogleFusionTablesInterface::StyleInfo::Options("columnName", "Color-" + month));
	style.polygonOptions.push_back(fillColorOption);

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
		fusionTablesAPIRateLimiter.Wait();
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
		fusionTablesAPIRateLimiter.Wait();
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
