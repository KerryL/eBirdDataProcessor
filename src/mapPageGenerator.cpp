// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating web page that embeds google map with custom markers.
//        To view table data, go to:
//        https://fusiontables.google.com/data?docid=1YzezTGDbYkUwOqvZ_yo0nKU8JjbAoEjYw01Sgcga

// Local headers
#include "mapPageGenerator.h"
#include "stringUtilities.h"
#include "utilities.h"
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

const String MapPageGenerator::birdProbabilityTableName(_T("Bird Probability Table"));
const std::array<MapPageGenerator::NamePair, 12> MapPageGenerator::monthNames = {
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

const ThrottledSection::Clock::duration MapPageGenerator::fusionTablesAPIMinDuration(std::chrono::milliseconds(static_cast<long long>(60000.0 / GFTI::writeRequestRateLimit)));// 30 requests per minute

const unsigned int MapPageGenerator::columnCount(42);
const unsigned int MapPageGenerator::importCellCountLimit(100000);
const unsigned int MapPageGenerator::importSizeLimit(1024 * 1024);// 1 MB

MapPageGenerator::MapPageGenerator(const String& kmlLibraryPath, const String& eBirdAPIKey, const String& mapsAPIKey) :
	fusionTablesAPIRateLimiter(fusionTablesAPIMinDuration), ebi(eBirdAPIKey), kmlLibrary(kmlLibraryPath, eBirdAPIKey, mapsAPIKey, log)
{
	log.Add(Cout);
	std::unique_ptr<OFStream> f(std::make_unique<OFStream>("temp.log"));// TODO:  Remove
	log.Add(std::move(f));
}

bool MapPageGenerator::WriteBestLocationsViewerPage(const String& htmlFileName,
	const String& googleMapsKey, const String& eBirdAPIKey,
	const std::vector<ObservationInfo>& observationProbabilities,
	const String& clientId, const String& clientSecret)
{
	OStringStream contents;
	const Keys keys(googleMapsKey, eBirdAPIKey, clientId, clientSecret);
	WriteHeadSection(contents, keys, observationProbabilities);
	WriteBody(contents);

	OFStream file(htmlFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}
	file << contents.str();

	return true;
}

void MapPageGenerator::WriteHeadSection(OStream& f, const Keys& keys,
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

void MapPageGenerator::WriteBody(OStream& f)
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

void MapPageGenerator::WriteScripts(OStream& f, const Keys& keys,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	double northeastLatitude, northeastLongitude;
	double southwestLatitude, southwestLongitude;
	String tableId;
	std::vector<unsigned int> styleIds;
	std::vector<unsigned int> templateIds;

	if (!CreateFusionTable(observationProbabilities, northeastLatitude,
		northeastLongitude, southwestLatitude, southwestLongitude, tableId,
		keys, styleIds, templateIds))
	{
		Cerr << "Failed to create fusion table\n";
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
	std::vector<ObservationInfo> observationProbabilities,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	String& tableId, const Keys& keys, std::vector<unsigned int>& styleIds,
	std::vector<unsigned int>& templateIds)
{
	GFTI fusionTables(_T("Bird Probability Tool"), keys.clientId, keys.clientSecret);
	std::vector<GFTI::TableInfo> tableList;
	if (!fusionTables.ListTables(tableList))
	{
		Cerr << "Failed to generate list of existing tables\n";
		return false;
	}

	tableId.clear();
	std::vector<CountyInfo> existingData;
	for (const auto& t : tableList)
	{
		if (t.name.compare(birdProbabilityTableName) == 0 && t.columns.size() == columnCount)// TODO:  Better check necessary?
		{
			log << "Found existing table " << t.tableId << std::endl;
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
			Cerr << "Failed to create fusion table\n";
			return false;
		}

		tableId = tableInfo.tableId;
		log << "Created new fusion table " << tableId << std::endl;
		fusionTablesAPIRateLimiter.Wait();
		if (!fusionTables.SetTableAccess(tableInfo.tableId, GoogleFusionTablesInterface::TableAccess::Public))
			Cerr << "Failed to make table public\n";
	}

	const auto invalidSpeciesDataRowsToDelete(FindInvalidSpeciesDataToRemove(fusionTables, tableId));
	if (invalidSpeciesDataRowsToDelete.size() > 0)
	{
		log << "Deleting " << invalidSpeciesDataRowsToDelete.size() << " rows with invalid species data" << std::endl;
		if (!GetConfirmationFromUser())
			return false;

		if (!DeleteRowsBatch(fusionTables, tableId, invalidSpeciesDataRowsToDelete))
		{
			Cerr << "Failed to remove rows with invalid species data\n";
			return false;
		}
	}

	const auto duplicateRowsToDelete(FindDuplicatesAndBlanksToRemove(existingData));
	if (duplicateRowsToDelete.size() > 0)
	{
		log << "Deleting " << duplicateRowsToDelete.size() << " duplicate or blank entires" << std::endl;
		if (!GetConfirmationFromUser())
			return false;

		if (!DeleteRowsBatch(fusionTables, tableId, duplicateRowsToDelete))
		{
			Cerr << "Failed to remove duplicates\n";
			return false;
		}
	}

	const auto rowsToDelete(DetermineDeleteUpdateAdd(existingData, observationProbabilities));
	if (rowsToDelete.size() > 0)
	{
		log << "Deleting " << rowsToDelete.size() << " rows to prepare for update" << std::endl;
		if (!GetConfirmationFromUser())
			return false;

		if (!DeleteRowsBatch(fusionTables, tableId, rowsToDelete))
		{
			Cerr << "Failed to remove rows for update\n";
			return false;
		}
	}

	if (observationProbabilities.size() == 0)
	{
		// TODO:  Wouldn't have populated lat/lon stuff yet.
		log << "All data up-to-date" << std::endl;
		return true;
	}

	log << "Retrieving county location data" << std::endl;
	const auto countryCodes(GetCountryCodeList(observationProbabilities));
	const auto countries(ebi.GetSubRegions(_T("world"), EBirdInterface::RegionType::Country));
	for (const auto& c : countries)
		countryLevelRegionInfoMap[c.code] = c;
	for (const auto& c : countryCodes)
		countryRegionInfoMap[c] = GetFullCountrySubRegionList(c);

	// Eliminate observations that are not reported at the lowest available region detail
	observationProbabilities.erase(std::remove_if(observationProbabilities.begin(), observationProbabilities.end(), [this](const EBirdDataProcessor::YearFrequencyInfo& y)
	{
		const String countryCode(Utilities::ExtractCountryFromRegionCode(y.locationCode));
		const String stateCode(Utilities::ExtractStateFromRegionCode(y.locationCode));
		const auto& subRegionList(countryRegionInfoMap[countryCode]);
		if (subRegionList.size() == 1 && stateCode.empty())
			return false;// No subregions
		else if (subRegionList.size() > 1 && stateCode.empty())
			return true;// Subregions exist, but none was specified

		const String matchString(countryCode + _T("-") + stateCode);
		for (const auto& sr : subRegionList)
		{
			if (sr.code.substr(0, matchString.length()).compare(matchString) == 0)
			{
				if (sr.code.length() > matchString.length() &&
					sr.code[matchString.length()] == Char('-'))
					return true;
				return false;
			}
		}
		return false;
	}), observationProbabilities.end());

	std::vector<CountyInfo> countyInfo(observationProbabilities.size());
	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	auto countyIt(countyInfo.begin());
	for (const auto& entry : observationProbabilities)
	{
		assert(entry.locationCode.length() >= 2);
		countyIt->frequencyInfo = std::move(entry.frequencyInfo);
		countyIt->probabilities = std::move(entry.probabilities);
		countyIt->code = entry.locationCode;

		const auto& nameLookupData(countryRegionInfoMap.find(entry.locationCode.substr(0, 2))->second);
		if (!CopyExistingDataForCounty(entry, existingData, *countyIt, nameLookupData))
			pool.AddJob(std::make_unique<MapJobInfo>(
				*countyIt, entry, nameLookupData, *this));

		++countyIt;
	}

	pool.WaitForAllJobsComplete();

	// TODO:  Use eBird API to get lat/lon range to send to map html
	// What about parsing KML to find extents?
	// This hack will work for now...
	northeastLatitude = 71.5;
	northeastLongitude = -66.26;
	southwestLatitude = 18.79;
	southwestLongitude = 171.7;

	log << "Preparing data for upload" << std::endl;
	OStringStream ss;
	unsigned int rowCount(0);
	unsigned int cellCount(0);
	for (const auto& c : countyInfo)
	{
		assert(!c.state.empty() && !c.country.empty() && !c.county.empty() && !c.name.empty() && !c.code.empty());
		ss << c.state << ',' << c.county << ',' << c.country << ",\"" << c.name << "\","
			<< c.code << ",\"" << c.geometryKML << '"';

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

			ss.str(String());
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
		log << "Failed to verify table styles" << std::endl;
		return false;
	}

	if (!VerifyTableTemplates(fusionTables, tableId, templateIds))
	{
		log << "Failed to verify table templates" << std::endl;
		return false;
	}

	return true;
}

std::vector<String> MapPageGenerator::GetCountryCodeList(const std::vector<ObservationInfo>& observationProbabilities)
{
	std::set<String> codes;
	for (const auto& o : observationProbabilities)
		codes.insert(o.locationCode.substr(0, 2));

	std::vector<String> codesVector(codes.size());
	auto it(codesVector.begin());
	for (const auto& c : codes)
	{
		*it = c;
		++it;
	}

	return codesVector;
}

bool MapPageGenerator::GetConfirmationFromUser()
{
	Cout << "Continue? (y/n) ";
	String response;
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

bool MapPageGenerator::DeleteRowsBatch(GoogleFusionTablesInterface& fusionTables,
	const String& tableId, const std::vector<unsigned int>& rowIds)
{
	const unsigned int maxDeleteSize(500);
	auto startIt(rowIds.begin());
	while (startIt != rowIds.end())
	{
		const unsigned int batchSize(std::min(maxDeleteSize,
			static_cast<unsigned int>(std::distance(startIt, rowIds.end()))));
		auto endIt(startIt + batchSize);
		fusionTablesAPIRateLimiter.Wait();
		if (!fusionTables.DeleteRows(tableId, std::vector<unsigned int>(startIt, endIt)))
			return false;
		startIt = endIt;
	}

	return true;
}

bool MapPageGenerator::UploadBuffer(GFTI& fusionTables, const String& tableId, const String& buffer)
{
	fusionTablesAPIRateLimiter.Wait();
	if (!fusionTables.Import(tableId, buffer))
	{
		Cerr << "Failed to import data\n";
		return false;
	}

	return true;
}

bool MapPageGenerator::ProcessJSONQueryResponse(cJSON* root, std::vector<CountyInfo>& data)
{
	cJSON* rowsArray(cJSON_GetObjectItem(root, "rows"));
	if (!rowsArray)
	{
		//Cerr << "Failed to get rows array\n";
		cJSON_Delete(root);
		return true;// Don't fail if there's no rows array - likely empty table
	}

	std::vector<CountyInfo> newData(cJSON_GetArraySize(rowsArray));
	unsigned int i(0);
	for (auto& row : newData)
	{
		cJSON* rowRoot(cJSON_GetArrayItem(rowsArray, i++));
		if (!rowRoot)
		{
			cJSON_Delete(root);
			Cerr << "Failed to read existing row\n";
			return false;
		}

		if (!ReadExistingCountyData(rowRoot, row))
		{
			cJSON_Delete(root);
			return false;
		}
	}

	data.insert(data.end(), newData.begin(), newData.end());
	cJSON_Delete(root);
	return true;
}

bool MapPageGenerator::ProcessCSVQueryResponse(const String& csvData, std::vector<CountyInfo>& data)
{
	IStringStream inData(csvData);
	String line;
	const String headingLine(_T("rowid,State,County,Country,Name,Code,Geometry,Probability-Jan,Probability-Feb,Probability-Mar,Probability-Apr,Probability-May,Probability-Jun,Probability-Jul,Probability-Aug,Probability-Sep,Probability-Oct,Probability-Nov,Probability-Dec"));
	std::getline(inData, line);
	if (line.compare(headingLine) != 0)
	{
		Cerr << "Heading line does not match expected response\n";
		return false;
	}

	while (std::getline(inData, line))
	{
		CountyInfo info;
		if (!ProcessCSVQueryLine(line, info))
			return false;
		data.push_back(info);
	}

	return true;
}

bool MapPageGenerator::ProcessCSVQueryLine(const String& line, CountyInfo& info)
{
	IStringStream ss(line);
	if (!EBirdDataProcessor::ParseToken(ss, _T("row ID"), info.rowId))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("state"), info.state))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("county"), info.county))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("country"), info.country))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("name"), info.name))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("code"), info.code))
		return false;

	if (!EBirdDataProcessor::ParseToken(ss, _T("geometry"), info.geometryKML))
		return false;

	for (auto& p : info.probabilities)
	{
		if (!EBirdDataProcessor::ParseToken(ss, _T("probability"), p))
			return false;
	}

	for (auto& f : info.frequencyInfo)
		f.clear();

	return false;
}

bool MapPageGenerator::GetExistingCountyData(std::vector<CountyInfo>& data,
	GFTI& fusionTables, const String& tableId)
{
	cJSON* root(nullptr);
	String csvData;
	const String query(_T("SELECT ROWID,State,County,Country,Name,Code,Geometry,'Probability-Jan','Probability-Feb','Probability-Mar','Probability-Apr','Probability-May','Probability-Jun','Probability-Jul','Probability-Aug','Probability-Sep','Probability-Oct','Probability-Nov','Probability-Dec' FROM ") + tableId);
	const String nonTypedOption(_T("&typed=false"));
	if (!fusionTables.SubmitQuery(query + nonTypedOption, root, &csvData))
		return false;

	if (root)
		return ProcessJSONQueryResponse(root, data);

	// Apparent bug in Fusion Table media download - rowid field is ignored.
	// So if we hit a limit (have a populated csvData and no root), use LIMIT
	// and OFFSET to modify the query, piecing together the data until we have
	// all of it.
	if (csvData.empty())// Shouldn't have no root and also no csvData
		return false;

	const unsigned int batchSize(500);
	unsigned int count(0);
	do
	{
		OStringStream offsetAndLimit;
		offsetAndLimit << " OFFSET " << count * batchSize << " LIMIT " << batchSize;
		if (!fusionTables.SubmitQuery(query + offsetAndLimit.str() + nonTypedOption, root))
			return false;

		if (!ProcessJSONQueryResponse(root, data))
			return false;

		root = nullptr;
		++count;
	} while (data.size() == count * batchSize);
	return true;
	//return ProcessCSVQueryResponse(csvData, data);
}

bool MapPageGenerator::ReadExistingCountyData(cJSON* row, CountyInfo& data)
{
	cJSON* rowID(cJSON_GetArrayItem(row, 0));
	cJSON* state(cJSON_GetArrayItem(row, 1));
	cJSON* county(cJSON_GetArrayItem(row, 2));
	cJSON* country(cJSON_GetArrayItem(row, 3));
	cJSON* name(cJSON_GetArrayItem(row, 4));
	cJSON* code(cJSON_GetArrayItem(row, 5));
	cJSON* kml(cJSON_GetArrayItem(row, 6));
	cJSON* jan(cJSON_GetArrayItem(row, 7));
	cJSON* feb(cJSON_GetArrayItem(row, 8));
	cJSON* mar(cJSON_GetArrayItem(row, 9));
	cJSON* apr(cJSON_GetArrayItem(row, 10));
	cJSON* may(cJSON_GetArrayItem(row, 11));
	cJSON* jun(cJSON_GetArrayItem(row, 12));
	cJSON* jul(cJSON_GetArrayItem(row, 13));
	cJSON* aug(cJSON_GetArrayItem(row, 14));
	cJSON* sep(cJSON_GetArrayItem(row, 15));
	cJSON* oct(cJSON_GetArrayItem(row, 16));
	cJSON* nov(cJSON_GetArrayItem(row, 17));
	cJSON* dec(cJSON_GetArrayItem(row, 18));

	if (!rowID || !state || !county || !country || !name || !code || !kml ||
		!jan || !feb || !mar || !apr || !may || !jun ||
		!jul || !aug || !sep || !oct || !nov || !dec)
	{
		Cerr << "Failed to get county information from existing row\n";
		return false;
	}

	data.state = UString::ToStringType(state->valuestring);
	data.county = UString::ToStringType(county->valuestring);
	data.country = UString::ToStringType(country->valuestring);
	data.name = UString::ToStringType(name->valuestring);
	data.code = UString::ToStringType(code->valuestring);
	data.geometryKML = UString::ToStringType(kml->valuestring);

	if (!Read(rowID, data.rowId))
	{
		Cerr << "Failed to read row ID\n";
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
		Cerr << "Failed to read existing probability\n";
		return false;
	}

	return true;
}

// For each row in existingData, there are three possible outcomes:
// 1.  newData has no entry corresponding to existing row -> Delete row
//     -> Delete row from table and delete entry from existingData (already does not exist in newData)
// 2.  newData and row have corresponding entries, probability data is the same -> Leave row as-is
//     -> Delete entry from existingData and newData, but do not delete from table
// 3.  newData and row have corresponding entries, probability data is different -> Update row (Delete + re-import)
//     -> Delete row from table, but do not delete from existingData or newData
std::vector<unsigned int> MapPageGenerator::DetermineDeleteUpdateAdd(
	std::vector<CountyInfo>& existingData, std::vector<ObservationInfo>& newData)
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

			if (!ProbabilityDataHasChanged(*matchingEntry, c) &&
				!c.name.empty() && !c.country.empty())// Could also consider checking for empty geometry here?
			{
				newData.erase(matchingEntry);
				return true;// Leave row as-is (Case #2)
			}

			// Need to delete row, but don't remove it from exisingData (Case #3)
			deletedRows.push_back(c.rowId);
			return false;
		}), existingData.end());

	return deletedRows;
}

std::vector<unsigned int> MapPageGenerator::FindInvalidSpeciesDataToRemove(
	GFTI& fusionTables, const String& tableId)
{
	std::vector<unsigned int> invalidRows;
	for (const auto& month : monthNames)
	{
		cJSON* root(nullptr);
		String csvData;
		const String query(_T("SELECT ROWID FROM ") + tableId + _T(" WHERE 'Species-") + month.shortName + _T("' MATCHES '(0.00%)'"));
		const String nonTypedOption(_T("&typed=false"));
		if (!fusionTables.SubmitQuery(query + nonTypedOption, root, &csvData))
			break;

		if (root)
		{
			FindInvalidSpeciesDataInJSONResponse(root, invalidRows);
			continue;
		}

		// Apparent bug in Fusion Table media download - rowid field is ignored.
		// So if we hit a limit (have a populated csvData and no root), use LIMIT
		// and OFFSET to modify the query, piecing together the data until we have
		// all of it.
		if (csvData.empty())// Shouldn't have no root and also no csvData
			return invalidRows;

		std::vector<unsigned int> additionalRows;
		const unsigned int batchSize(100);
		unsigned int count(0);
		do
		{
			OStringStream offsetAndLimit;
			offsetAndLimit << " OFFSET " << count * batchSize << " LIMIT " << batchSize;
			if (!fusionTables.SubmitQuery(query + offsetAndLimit.str() + nonTypedOption, root))
				break;

			if (!FindInvalidSpeciesDataInJSONResponse(root, additionalRows))
				break;

			root = nullptr;
			++count;
		} while (additionalRows.size() == count * batchSize);
		invalidRows.insert(invalidRows.end(), additionalRows.begin(), additionalRows.end());
	}

	std::sort(invalidRows.begin(), invalidRows.end());
	invalidRows.erase(std::unique(invalidRows.begin(), invalidRows.end()), invalidRows.end());
	return invalidRows;
}

bool MapPageGenerator::FindInvalidSpeciesDataInJSONResponse(cJSON* root, std::vector<unsigned int>& invalidRows)
{
	assert(root);
	cJSON* rowsArray(cJSON_GetObjectItem(root, "rows"));
	if (!rowsArray)
	{
		//Cerr << "Failed to get rows array\n";
		cJSON_Delete(root);
		return true;// Don't fail if there's no rows array - likely empty table
	}

	std::vector<unsigned int> newData(cJSON_GetArraySize(rowsArray));
	unsigned int i(0);
	for (auto& row : newData)
	{
		cJSON* rowRoot(cJSON_GetArrayItem(rowsArray, i++));
		if (!rowRoot)
		{
			cJSON_Delete(root);
			Cerr << "Failed to read existing row\n";
			return false;
		}

		cJSON* rowIdItem(cJSON_GetArrayItem(rowRoot, 0));
		if (!rowIdItem)
		{
			cJSON_Delete(root);
			Cerr << "Failed to read item from row\n";
			return false;
		}

		if (!Read(rowIdItem, row))
		{
			cJSON_Delete(root);
			return false;
		}
	}

	invalidRows.insert(invalidRows.end(), newData.begin(), newData.end());
	cJSON_Delete(root);
	return true;
}

std::vector<unsigned int> MapPageGenerator::FindDuplicatesAndBlanksToRemove(std::vector<CountyInfo>& existingData)
{
	std::vector<unsigned int> deletedRows;
	auto startIterator(existingData.begin());
	for (const auto item : existingData)
	{
		auto it(++startIterator);
		if (item.code.empty())// If anything else is empty, treat it as an update rather than deleting a blank
		{
			deletedRows.push_back(item.rowId);
			continue;
		}

		bool deletedSelf(false);
		for (; it != existingData.end(); ++it)
		{
			if (item.code.compare(it->code) != 0)
				continue;

			deletedRows.push_back(it->rowId);
			if (!deletedSelf)
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
		if (it->locationCode.compare(county.code) == 0)
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
	const std::vector<EBirdInterface::RegionInfo>& regionInfo)
{
	for (const auto& existing : existingData)
	{
		if (existing.code.compare(entry.locationCode) == 0)
		{
			if (existing.name.empty() || existing.country.empty())
			{
				for (const auto& r : regionInfo)
				{
					if (r.code.compare(entry.locationCode) == 0)
					{
						newData.country = Utilities::ExtractCountryFromRegionCode(r.code);
						newData.state = Utilities::ExtractStateFromRegionCode(r.code);
						newData.county = r.name;
						newData.name = AssembleCountyName(newData.county, newData.state, newData.county);
						break;
					}
				}
			}
			else
			{
				newData.country = existing.country;
				newData.state = existing.state;
				newData.county = existing.county;
				newData.name = existing.name;
			}

			newData.geometryKML = std::move(existing.geometryKML);

			if (newData.geometryKML.empty())
				LookupAndAssignKML(newData);

			assert(!newData.country.empty() && !newData.name.empty() && !newData.code.empty());
			return true;
		}
	}

	return false;
}

String MapPageGenerator::AssembleCountyName(const String& country, const String& state, const String& county)
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
	String countryName, stateName;
	LookupEBirdRegionNames(data.country, data.state, countryName, stateName);
	if (countryName.empty())// Should never happen
	{
		std::shared_lock<std::shared_mutex> l(codeToNameMapMutex);
		log << "BAD THING HERE!" << std::endl;
		log << data.country << " - " << data.state << std::endl;
		log << "info map size = " << countryRegionInfoMap[data.country].size() << std::endl;
		for (const auto& c : countryRegionInfoMap[data.country])
			log << c.code << " :: " << c.name << std::endl;
		log << eBirdRegionCodeToNameMap.find(data.country)->second << std::endl;
		assert(false);
	}
	data.geometryKML = kmlLibrary.GetKML(countryName, stateName, data.county);
	if (data.geometryKML.empty())
		log << "\rWarning:  Geometry not found for '" << data.code << '\'' << std::endl;
}

void MapPageGenerator::AddRegionCodesToMap(const String& parentCode, const EBirdInterface::RegionType& regionType)
{
	auto regionInfo(ebi.GetSubRegions(parentCode, regionType));
	for (const auto& r : regionInfo)
		eBirdRegionCodeToNameMap[r.code] = r.name;
}

void MapPageGenerator::LookupEBirdRegionNames(const String& countryCode,
	const String& subRegion1Code, String& country, String& subRegion1)
{
	const String fullSubRegionCode([&countryCode, &subRegion1Code]()
	{
		if (subRegion1Code.empty())
			return countryCode;
		return countryCode + Char('-') + subRegion1Code;
	}());
	std::shared_lock<std::shared_mutex> lock(codeToNameMapMutex);
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
	String stateName(info.state), countryName(info.country);
	for (const auto& r : regionNames)
	{
		if (r.code.compare(info.country) == 0)
			countryName = r.name;
		else if (r.code.compare(info.country + _T("-") + info.state) == 0)
			stateName = r.name;

		if (r.code.compare(frequencyInfo.locationCode) == 0)
		{
			auto hyphen(frequencyInfo.locationCode.find(Char('-')));
			if (hyphen != std::string::npos)
			{
				hyphen = frequencyInfo.locationCode.find(Char('-'), hyphen + 1);
				if (hyphen != std::string::npos)
					info.county = r.name;
			}

			info.name = AssembleCountyName(countryName, stateName, info.county);
			break;
		}
	}

	assert(!info.country.empty() && !info.name.empty() && !info.code.empty());
	mpg.LookupAndAssignKML(info);
}

GoogleFusionTablesInterface::TableInfo MapPageGenerator::BuildTableLayout()
{
	GFTI::TableInfo tableInfo;
	tableInfo.name = birdProbabilityTableName;
	tableInfo.description = _T("Table of bird observation probabilities");
	tableInfo.isExportable = true;

	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("State"), GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("County"), GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Country"), GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Name"), GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Code"), GFTI::ColumnType::String));
	tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Geometry"), GFTI::ColumnType::Location));

	for (const auto& m : monthNames)
	{
		tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Probability-") + m.shortName, GFTI::ColumnType::Number));
		tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Color-") + m.shortName, GFTI::ColumnType::String));
		tableInfo.columns.push_back(GFTI::ColumnInfo(_T("Species-") + m.shortName, GFTI::ColumnType::String));
	}

	assert(tableInfo.columns.size() == columnCount);

	return tableInfo;
}

String MapPageGenerator::CleanQueryString(const String& s)
{
	String clean;

	for (const auto& c : s)
	{
		if (c == Char('\''))
			clean.append(_T("''"));
		else
			clean.push_back(c);
	}

	return clean;
}

String MapPageGenerator::ComputeColor(const double& frequency)
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

String MapPageGenerator::ColorToHexString(const Color& c)
{
	OStringStream hex;
	hex << "#" << std::hex
		<< std::setw(2) << std::setfill(Char('0')) << static_cast<int>(c.red * 255.0)
		<< std::setw(2) << std::setfill(Char('0')) << static_cast<int>(c.green * 255.0)
		<< std::setw(2) << std::setfill(Char('0')) << static_cast<int>(c.blue * 255.0);

	String hexString(hex.str());
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

bool MapPageGenerator::VerifyTableStyles(GoogleFusionTablesInterface& fusionTables,
	const String& tableId, std::vector<unsigned int>& styleIds)
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

GoogleFusionTablesInterface::StyleInfo MapPageGenerator::CreateStyle(const String& tableId,
	const String& month)
{
	GoogleFusionTablesInterface::StyleInfo style;
	style.name = month;
	style.hasPolygonOptions = true;
	style.tableId = tableId;

	GoogleFusionTablesInterface::StyleInfo::Options fillColorOption;
	fillColorOption.type = GoogleFusionTablesInterface::StyleInfo::Options::Type::Complex;
	fillColorOption.key = _T("fillColorStyler");
	fillColorOption.c.push_back(GoogleFusionTablesInterface::StyleInfo::Options(_T("columnName"), _T("Color-") + month));
	style.polygonOptions.push_back(fillColorOption);

	return style;
}

bool MapPageGenerator::VerifyTableTemplates(GoogleFusionTablesInterface& fusionTables,
	const String& tableId, std::vector<unsigned int>& templateIds)
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
	const String& tableId, const String& month)
{
	GoogleFusionTablesInterface::TemplateInfo info;
	info.name = month;
	info.tableId = tableId;

	OStringStream ss;
	ss << "<div class='googft-info-window' style='font-family: sans-serif'>\n"
		<< "<p><b>{Name}</b></p>\n"
		<< "<p>Probability of Observing a Lifer: {Probability-" << std::setprecision(2) << std::fixed << month << "}%</p>\n"
		<< "<p>Likely Species:</p>\n"
		<< "<div style=\"height:100px;width:300px;border:1px solid #0C0C0C;overflow:auto;\">{Species-" << month << "}</div>\n"
		<< "</div>";

	info.body = ss.str();

	return info;
}

String MapPageGenerator::BuildSpeciesInfoString(const std::vector<EBirdDataProcessor::FrequencyInfo>& info)
{
	OStringStream ss;
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

std::vector<EBirdInterface::RegionInfo> MapPageGenerator::GetFullCountrySubRegionList(const String& countryCode) const
{
	auto regionList(ebi.GetSubRegions(countryCode, EBirdInterface::RegionType::MostDetailAvailable));
	if (regionList.empty())// most detail available is country - use as-is
	{
		const auto countryInfo(countryLevelRegionInfoMap.find(countryCode));
		assert(countryInfo != countryLevelRegionInfoMap.end());
		return std::vector<EBirdInterface::RegionInfo>(1, countryInfo->second);
	}

	const auto firstDash(regionList.front().code.find(Char('-')));
	if (firstDash == std::string::npos)
		return regionList;// I don't think we should ever get here?

	const auto secondDash(regionList.front().code.find(Char('-'), firstDash + 1));
	if (secondDash == std::string::npos)// most detail available is subregion 1 - use as-is
		return regionList;

	// If here, we have subregion2 data.  Let's also add subregion1 data.
	auto subRegion1List(ebi.GetSubRegions(countryCode, EBirdInterface::RegionType::SubNational1));
	regionList.insert(regionList.end(), subRegion1List.begin(), subRegion1List.end());
	return regionList;
}
