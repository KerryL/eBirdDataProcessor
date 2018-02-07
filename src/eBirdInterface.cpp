// File:  eBirdInterface.cpp
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.  See https://confluence.cornell.edu/display/CLOISAPI/eBird+API+1.1.

// Local headers
#include "eBirdInterface.h"
#include "bestObservationTimeEstimator.h"
#include "email/cJSON/cJSON.h"
#include "email/curlUtilities.h"

// Standard C++ headers
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <map>
#include <cassert>
#include <iomanip>

const std::string EBirdInterface::apiRoot("https://ebird.org/ws2.0/");
const std::string EBirdInterface::observationDataPath("data/obs/");
const std::string EBirdInterface::recentPath("recent/");
const std::string EBirdInterface::taxonomyLookupEndpoint("ref/taxonomy/ebird");
const std::string EBirdInterface::regionReferenceEndpoint("ref/region/list/");
const std::string EBirdInterface::hotspotReferenceEndpoint("ref/hotspot/");

const std::string EBirdInterface::speciesCodeTag("speciesCode");
const std::string EBirdInterface::commonNameTag("comName");
const std::string EBirdInterface::scientificNameTag("sciName");
const std::string EBirdInterface::locationNameTag("locName");
const std::string EBirdInterface::locationIDTag("locID");
const std::string EBirdInterface::latitudeTag("lat");
const std::string EBirdInterface::longitudeTag("lng");
const std::string EBirdInterface::countryCodeTag("countryCode");
const std::string EBirdInterface::subnational1CodeTag("subnational1Code");
const std::string EBirdInterface::subnational2CodeTag("subnational2Code");
const std::string EBirdInterface::observationDateTag("obsDt");
const std::string EBirdInterface::isNotHotspotTag("locationPrivate");
const std::string EBirdInterface::isReviewedTag("obsReviewed");
const std::string EBirdInterface::isValidTag("obsValid");
const std::string EBirdInterface::locationPrivateTag("locationPrivate");

const std::string EBirdInterface::countryTypeName("country");
const std::string EBirdInterface::subNational1TypeName("subnational1");
const std::string EBirdInterface::subNational2TypeName("subnational2");

const std::string EBirdInterface::nameTag("name");
const std::string EBirdInterface::codeTag("code");

const std::string EBirdInterface::eBirdTokenHeader("X-eBirdApiToken: ");

std::unordered_map<std::string, EBirdInterface::NameInfo> EBirdInterface::commonToScientificMap;
std::unordered_map<std::string, std::string> EBirdInterface::scientificToCommonMap;

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsWithRecentObservationsOf(
	const std::string& speciesCode, const std::string& region, const unsigned int& recentPeriod)
{
	const bool allowProvisional(true);
	const bool hotspotsOnly(true);

	const auto recentObservations(GetRecentObservationsOfSpeciesInRegion(speciesCode, region,
		recentPeriod, allowProvisional, hotspotsOnly));
	const auto hotspots(GetHotspotsInRegion(region));

	std::vector<LocationInfo> hotspotsWithObservations;
	for (const auto& o : recentObservations)
	{
		for (const auto& h : hotspots)
		{
			if (o.locationID.compare(h.id) == 0)
			{
				hotspotsWithObservations.push_back(h);
				break;
			}
		}
	}

	return hotspotsWithObservations;
}

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsInRegion(const std::string& region)
{
	std::ostringstream request;
	request << apiRoot << hotspotReferenceEndpoint << region << "?fmt=json";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<LocationInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetHotspotsInRegion())\n";
		std::cerr << response << '\n';
		return std::vector<LocationInfo>();
	}

	std::vector<LocationInfo> hotspots(cJSON_GetArraySize(root));
	unsigned int i(0);
	for (auto& h : hotspots)
	{
		cJSON* item(cJSON_GetArrayItem(root, i));
		if (!item)
		{
			std::cerr << "Failed to get hotspot array item\n";
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, locationNameTag, h.name))
		{
			std::cerr << "Failed to get hotspot name for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, locationIDTag, h.id))
		{
			std::cerr << "Failed to get hotspot id for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, latitudeTag, h.latitude))
		{
			std::cerr << "Failed to get hotspot latitude for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, longitudeTag, h.longitude))
		{
			std::cerr << "Failed to get hotspot longitude for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, countryCodeTag, h.countryCode))
		{
			std::cerr << "Failed to get hotspot country code for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, subnational1CodeTag, h.subnational1Code))
		{
			std::cerr << "Failed to get hotspot subnational 1 code for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, subnational2CodeTag, h.subnational2Code))
		{
			std::cerr << "Failed to get hotspot subnational 2 code for item " << i << '\n';
			hotspots.clear();
			break;
		}

		++i;
	}

	cJSON_Delete(root);
	return hotspots;
}

bool EBirdInterface::ReadJSONObservationData(cJSON* item, ObservationInfo& info)
{
	if (!ReadJSON(item, speciesCodeTag, info.speciesCode))
	{
		std::cerr << "Failed to get species code\n";
		return false;
	}

	if (!ReadJSON(item, commonNameTag, info.commonName))
	{
		std::cerr << "Failed to get common name for item\n";
		return false;
	}

	if (!ReadJSON(item, scientificNameTag, info.scientificName))
	{
		std::cerr << "Failed to get scientific name for item\n";
		return false;
	}

	if (!ReadJSON(item, observationDateTag, info.observationDate))
	{
		// Try without time info before declaring a failure
		std::string dateString;
		if (!ReadJSON(item, observationDateTag, dateString))
		{
			std::cerr << "Failed to get observation date and time for item\n";
			return false;
		}

		std::istringstream ss(dateString);
		if ((ss >> std::get_time(&info.observationDate, "%Y-%m-%d")).fail())
		{
			std::cerr << "Failed to get observation date for item\n";
			return false;
		}

		info.dateIncludesTimeInfo = false;
	}

	if (!ReadJSON(item, locationNameTag, info.count))
	{
		std::cerr << "Failed to get location name for item\n";
		return false;
	}

	if (!ReadJSON(item, locationNameTag, info.locationName))
	{
		std::cerr << "Failed to get location name for item\n";
		return false;
	}

	if (!ReadJSON(item, locationIDTag, info.locationID))
	{
		std::cerr << "Failed to get location id for item\n";
		return false;
	}

	if (!ReadJSON(item, isNotHotspotTag, info.isNotHotspot))
	{
		std::cerr << "Failed to get hotspot status for item\n";
		return false;
	}

	if (!ReadJSON(item, latitudeTag, info.latitude))
	{
		std::cerr << "Failed to get location latitude for item\n";
		return false;
	}

	if (!ReadJSON(item, longitudeTag, info.longitude))
	{
		std::cerr << "Failed to get location longitude for item\n";
		return false;
	}

	if (!ReadJSON(item, isReviewedTag, info.observationReviewed))
	{
		std::cerr << "Failed to get observation reviewed flag for item\n";
		return false;
	}

	if (!ReadJSON(item, isValidTag, info.observationValid))
	{
		std::cerr << "Failed to get observation valid flag for item\n";
		return false;
	}

	if (!ReadJSON(item, locationPrivateTag, info.locationPrivate))
	{
		std::cerr << "Failed to get location private flag\n";
		return false;
	}

	return true;
}

std::vector<EBirdInterface::ObservationInfo> EBirdInterface::GetRecentObservationsOfSpeciesInRegion(
	const std::string& speciesCode, const std::string& region, const unsigned int& recentPeriod,
	const bool& includeProvisional, const bool& hotspotsOnly)
{
	std::ostringstream request;
	request << apiRoot << observationDataPath << region << '/' << recentPath << '/'
		<< speciesCode << "?back=" << recentPeriod
		<< "&includeProvisional=";

	if (includeProvisional)
		request << "true";
	else
		request << "false";

	request << "&hotspot=";
	if (hotspotsOnly)
		request << "true";
	else
		request << "false";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<ObservationInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetRecentObservationsOfSpeciesInRegion())\n";
		std::cerr << response << '\n';
		return std::vector<ObservationInfo>();
	}

	std::vector<ObservationInfo> observations;
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* item(cJSON_GetArrayItem(root, i));
		if (!item)
		{
			std::cerr << "Failed to get observation array item\n";
			observations.clear();
			break;
		}

		ObservationInfo info;
		if (!ReadJSONObservationData(item, info))
		{
			observations.clear();
			break;
		}

		observations.push_back(info);
	}

	cJSON_Delete(root);
	return observations;
}

bool EBirdInterface::AddTokenToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string(eBirdTokenHeader
		+ static_cast<const TokenData*>(data)->token).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append token to header in AddTokenToCurlHeader\n";
		return false;
	}

	headerList = curl_slist_append(headerList, "Content-Type: application/json");
	if (!headerList)
	{
		std::cerr << "Failed to append content type to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	return true;
}

std::string EBirdInterface::GetScientificNameFromCommonName(const std::string& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return std::string();
	}

	return commonToScientificMap[commonName].scientificName;
}

bool EBirdInterface::FetchEBirdNameData()
{
	std::ostringstream request;
	request << apiRoot << taxonomyLookupEndpoint << "?cat=species"
		<< "&locale=en"
		<< "&fmt=json";

	std::string response;
	if (!DoCURLGet(request.str(), response))
		return false;

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetScientificNameFromCommonName())\n";
		std::cerr << response << '\n';
		return false;
	}

	BuildNameMaps(root);
	cJSON_Delete(root);
	return true;
}

std::string EBirdInterface::GetSpeciesCodeFromCommonName(const std::string& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return std::string();
	}

	return commonToScientificMap[commonName].code;
}

void EBirdInterface::BuildNameMaps(cJSON* root)
{
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* nameInfo(cJSON_GetArrayItem(root, i));

		std::string commonName;
		if (!ReadJSON(nameInfo, commonNameTag, commonName))
		{
			std::cerr << "Failed to read common name tag\n";
			continue;
		}

		std::string scientificName;
		if (!ReadJSON(nameInfo, scientificNameTag, scientificName))
		{
			std::cerr << "Failed to read scientific name tag\n";
			continue;
		}

		std::string code;
		if (!ReadJSON(nameInfo, speciesCodeTag, code))
		{
			std::cerr << "Failed to read species code\n";
			continue;
		}

		commonToScientificMap[commonName] = NameInfo(scientificName, code);
		scientificToCommonMap[scientificName] = commonName;
	}
}

std::string EBirdInterface::GetRegionCode(const std::string& country,
	const std::string& state, const std::string& county)
{
	std::string countryCode(GetCountryCode(country));
	if (countryCode.empty())
		return std::string();

	if (!state.empty())
	{
		const std::string stateCode(GetStateCode(countryCode, state));
		if (stateCode.empty())
			return std::string();

		if (!county.empty())
			return GetCountyCode(stateCode, county);

		return stateCode;
	}

	return countryCode;
}

std::string EBirdInterface::GetUserInputOnResponse(const std::string& s, const std::string& field)
{
	std::cout << "Multiple matches.  Please specify desired " << field << ":\n\n";

	std::istringstream ss(s);
	std::string line;
	std::getline(ss, line);
	std::cout << line << '\n';// heading row

	std::map<unsigned int, std::string> selectionMap;
	unsigned int i(0);
	while (std::getline(ss, line))
	{
		std::cout << ++i << ": " << line << '\n';
		selectionMap[i] = line;
	}

	std::cout << std::endl;

	unsigned int selection;
	std::cin >> selection;

	if (selection == 0 || selection > i)
		return std::string();

	return selectionMap[selection];
}

std::vector<EBirdInterface::RegionInfo> EBirdInterface::GetSubRegions(
	const std::string& regionCode, const RegionType& type)
{
	const std::string regionTypeString([&type]()
	{
		if (type == RegionType::Country)
			return "country";
		else if (type == RegionType::SubNational1)
			return "subnational1";
		else// if (type == RegionType::SubNational2)
			return "subnational2";
	}());
	std::ostringstream request;
	request << apiRoot << regionReferenceEndpoint << regionTypeString << '/' << regionCode << ".json";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<RegionInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse returned string (GetSubRegions())\n";
		std::cerr << response << '\n';
		return std::vector<RegionInfo>();
	}

	std::vector<RegionInfo> subRegions(cJSON_GetArraySize(root));
	unsigned int i(0);
	for (auto& r : subRegions)
	{
		cJSON* item(cJSON_GetArrayItem(root, i++));
		if (!item)
		{
			std::cerr << "Failed to get sub-region array item\n";
			subRegions.clear();
			break;
		}

		if (!ReadJSON(item, nameTag, r.name))
		{
			std::cerr << "Failed to read sub-region name\n";
			subRegions.clear();
			break;
		}

		if (!ReadJSON(item, codeTag, r.code))
		{
			std::cerr << "Failed to read sub-region code\n";
			subRegions.clear();
			break;
		}
	}

	cJSON_Delete(root);
	return subRegions;
}

bool EBirdInterface::NameMatchesRegion(const std::string& name, const RegionInfo& region)
{
	assert(name.length() > 0 && region.name.length() > 0 && region.code.length() > 0);

	std::string lowerName(name);
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
	std::string lowerRegion(region.name);
	std::transform(lowerRegion.begin(), lowerRegion.end(), lowerRegion.begin(), ::tolower);
	std::string lowerCode(region.code);
	std::transform(lowerCode.begin(), lowerCode.end(), lowerCode.begin(), ::tolower);

	if (lowerName.compare(lowerRegion) == 0)
		return true;
	else if (lowerName.compare(lowerCode) == 0)
		return true;
	else if (lowerCode.length() > lowerName.length() &&
		lowerCode.substr(lowerCode.length() - lowerName.length()).compare(lowerName) == 0)
		return true;
	else if (lowerName.length() > lowerCode.length() &&
		lowerName.substr(lowerName.length() - lowerCode.length()).compare(lowerCode) == 0)
		return true;

	return false;
}

std::string EBirdInterface::GetCountryCode(const std::string& country)
{
	if (storedRegionInfo.empty())
		BuildCountryInfo();

	for (const auto& r : storedRegionInfo)
	{
		if (NameMatchesRegion(country, r))
			return r.code;
	}

	std::cerr << "Failed to find country code for '" << country << "'\n";
	return std::string();
}

std::string EBirdInterface::GetStateCode(const std::string& countryCode, const std::string& state)
{
	CountryInfo* countryInfo(nullptr);
	for (auto& r : storedRegionInfo)
	{
		if (r.code.compare(countryCode) == 0)
		{
			countryInfo = &r;
			break;
		}
	}

	if (!countryInfo)
	{
		std::cerr << "Failed to find matching entry for country code '" << countryCode << "'\n";
		return std::string();
	}

	if (countryInfo->subnational1Info.empty())
		countryInfo->subnational1Info = std::move(BuildSubNational1Info(countryInfo->code));

	for (const auto& r : countryInfo->subnational1Info)
	{
		if (NameMatchesRegion(state, r))
			return r.code;
	}

	std::cerr << "Failed to find state code for '" << state << "' in '" << countryCode << "'\n";
	return std::string();
}

std::string EBirdInterface::GetCountyCode(const std::string& stateCode, const std::string& county)
{
	SubNational1Info* subNational1Info(nullptr);
	for (auto& country : storedRegionInfo)
	{
		for (auto& r : country.subnational1Info)
		{
			if (r.code.compare(stateCode) == 0)
			{
				subNational1Info = &r;
				break;
			}
		}

		if (subNational1Info)
			break;
	}

	if (!subNational1Info)
	{
		std::cerr << "Failed to find matching entry for state code '" << stateCode << "'\n";
		return std::string();
	}

	if (subNational1Info->subnational2Info.empty())
		subNational1Info->subnational2Info = std::move(GetSubRegions(subNational1Info->code, RegionType::SubNational2));

	for (const auto& r : subNational1Info->subnational2Info)
	{
		if (NameMatchesRegion(county, r))
			return r.code;
	}

	std::cerr << "Failed to find county code for '" << county << "' in '" << stateCode << "'\n";
	return std::string();
}

void EBirdInterface::BuildCountryInfo()
{
	auto regions(GetSubRegions("world", RegionType::Country));
	storedRegionInfo.resize(regions.size());
	unsigned int i;
	for (i = 0; i < storedRegionInfo.size(); ++i)
	{
		storedRegionInfo[i].code = regions[i].code;
		storedRegionInfo[i].name = regions[i].name;
	}
}

std::vector<EBirdInterface::SubNational1Info> EBirdInterface::BuildSubNational1Info(const std::string& countryCode)
{
	auto regions(GetSubRegions(countryCode, RegionType::SubNational1));
	std::vector<EBirdInterface::SubNational1Info> info(regions.size());
	unsigned int i;
	for (i = 0; i < info.size(); ++i)
	{
		info[i].code = regions[i].code;
		info[i].name = regions[i].name;
	}

	return info;
}
