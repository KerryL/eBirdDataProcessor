// File:  mapPageGenerator.cpp
// Date:  81/3/2018
// Auth:  K. Loux
// Desc:  Tool for generating a web page that overlays observation information on an interactive map.

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

const UString::String MapPageGenerator::htmlFileName(_T("observationMap.html"));
const UString::String MapPageGenerator::dataFileName(_T("observationData.js"));

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

MapPageGenerator::MapPageGenerator(const UString::String& kmlLibraryPath, const UString::String& eBirdAPIKey,
	const std::vector<UString::String>& highDetailCountries,
	const bool& cleanUpLocationNames) : highDetailCountries(highDetailCountries),
	ebi(eBirdAPIKey), kmlLibrary(kmlLibraryPath, eBirdAPIKey, UString::String()/*Google maps key?*/, log, cleanUpLocationNames)
{
	log.Add(Cout);
	/*std::unique_ptr<UString::OFStream> f(std::make_unique<UString::OFStream>("temp.log"));// TODO:  Remove
	log.Add(std::move(f));//*/
}

bool MapPageGenerator::WriteBestLocationsViewerPage(const UString::String& outputPath,
	const UString::String& eBirdAPIKey,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	if (!WriteHTML(outputPath))
		return false;

	if (!WriteGeoJSONData(outputPath, eBirdAPIKey, observationProbabilities))
		return false;
	/*UString::OStringStream contents;
	WriteHeadSection(contents, keys, observationProbabilities);
	WriteBody(contents);

	UString::OFStream file(htmlFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << htmlFileName << "' for output\n";
		return false;
	}
	file << contents.str();*/

	return true;
}

bool MapPageGenerator::WriteHTML(const UString::String& outputPath) const
{
	/*const auto fileName(ForceTrailingSlash(outputPath) + htmlFileName);
	UString::OFStream file(fileName);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}

	if (!WriteHeadSection(file))
		return false;

	if (!WriteBody(file))
		return false;*/

	return true;
}

bool MapPageGenerator::WriteGeoJSONData(const UString::String& outputPath,
	const UString::String& eBirdAPIKey, std::vector<ObservationInfo> observationProbabilities)
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
			countyIt->monthInfo[i].frequencyInfo = entry.frequencyInfo[i];
			countyIt->monthInfo[i].probability = entry.probabilities[i];
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
	if (!CreateJSONData(countyInfo, geoJSON))
		return false;

	const auto fileName(UString::ToNarrowString(ForceTrailingSlash(outputPath) + dataFileName));
	std::ofstream file(fileName);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << UString::ToStringType(fileName) << "' for output\n";
		cJSON_Delete(geoJSON);
		return false;
	}

	const auto jsonString(cJSON_Print(geoJSON));
	if (!jsonString)
	{
		Cerr << "Failed to generate JSON string\n";
		cJSON_Delete(geoJSON);
		return false;
	}

	file << "var regionData = " << jsonString << ";\n";
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

bool MapPageGenerator::CreateJSONData(const std::vector<CountyInfo>& observationData, cJSON*& geoJSON)
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

	unsigned int index(0);
	for (const auto& o : observationData)
	{
		auto r(cJSON_CreateObject());
		if (!r)
		{
			Cerr << "Failed to create region JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(regions, r);
		if (!BuildObservationRecord(o, r, index))
			return false;

		++index;
	}

	return true;
}

bool MapPageGenerator::BuildObservationRecord(const CountyInfo& observation, cJSON* json, const unsigned int& index)
{
	cJSON_AddStringToObject(json, "type", "Feature");
	std::ostringstream ss;
	ss << index;
	cJSON_AddStringToObject(json, "id", ss.str().c_str());

	auto observationData(cJSON_CreateObject());
	if (!observationData)
	{
		Cerr << "Failed to create observation data JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "properties", observationData);
	cJSON_AddStringToObject(json, "geometry", UString::ToNarrowString(observation.geometryKML).c_str());

	cJSON_AddStringToObject(observationData, "name", UString::ToNarrowString(observation.name).c_str());
	cJSON_AddStringToObject(observationData, "country", UString::ToNarrowString(observation.country).c_str());
	cJSON_AddStringToObject(observationData, "state", UString::ToNarrowString(observation.state).c_str());
	cJSON_AddStringToObject(observationData, "county", UString::ToNarrowString(observation.county).c_str());

	auto monthData(cJSON_CreateArray());
	if (!monthData)
	{
		Cerr << "Failed to create month data JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(observationData, "monthData", monthData);

	for (const auto& m : observation.monthInfo)
	{
		cJSON* month(cJSON_CreateObject());
		if (!month)
		{
			Cerr << "Failed to create month JSON object\n";
			return false;
		}

		cJSON_AddItemToArray(monthData, month);
		if (!BuildMonthInfo(m, month))
			return false;
	}

	return true;
}

bool MapPageGenerator::BuildMonthInfo(const CountyInfo::MonthInfo& monthInfo, cJSON* json)
{
	cJSON_AddNumberToObject(json, "probability", monthInfo.probability);
	auto speciesList(cJSON_CreateArray());
	if (!speciesList)
	{
		Cerr << "Failed to create species list JSON object\n";
		return false;
	}

	cJSON_AddItemToObject(json, "birds", speciesList);
	for (const auto& m : monthInfo.frequencyInfo)
	{
		std::ostringstream ss;
		ss << UString::ToNarrowString(m.species) << " (" << m.frequency << "%)";
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

void MapPageGenerator::WriteHeadSection(UString::OStream& f)
{
	f << "<!DOCTYPE html>\n"
		<< "<html>\n"
		<< "  <head>\n"
		<< "    <title>Observation Probability Map</title>\n"
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
	f << "  </head>\n";
}

void MapPageGenerator::WriteBody(UString::OStream& f)
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

void MapPageGenerator::WriteScripts(UString::OStream& f, const Keys& keys,
	const std::vector<ObservationInfo>& observationProbabilities)
{
	/*double northeastLatitude, northeastLongitude;
	double southwestLatitude, southwestLongitude;*/
	UString::String tableId;
	std::vector<unsigned int> styleIds;
	std::vector<unsigned int> templateIds;

	/*const double centerLatitude(0.5 * (northeastLatitude + southwestLatitude));
	const double centerLongitude(0.5 * (northeastLongitude + southwestLongitude));*/

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

	/*f << "];\n"
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
		<< "    </script>\n";*/
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

	assert(!info.country.empty() && !info.name.empty() && !info.code.empty());
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
