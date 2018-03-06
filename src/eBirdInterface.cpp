// File:  eBirdInterface.cpp
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.  See https://confluence.cornell.edu/display/CLOISAPI/eBird+API+1.1.

// Local headers
#include "eBirdInterface.h"
#include "bestObservationTimeEstimator.h"
#include "stringUtilities.h"
#include "email/cJSON/cJSON.h"
#include "email/curlUtilities.h"

// Standard C++ headers
#include <cctype>
#include <algorithm>
#include <map>
#include <cassert>
#include <iomanip>

const String EBirdInterface::apiRoot(_T("https://ebird.org/ws2.0/"));
const String EBirdInterface::observationDataPath(_T("data/obs/"));
const String EBirdInterface::recentPath(_T("recent/"));
const String EBirdInterface::taxonomyLookupEndpoint(_T("ref/taxonomy/ebird"));
const String EBirdInterface::regionReferenceEndpoint(_T("ref/region/list/"));
const String EBirdInterface::hotspotReferenceEndpoint(_T("ref/hotspot/"));

const String EBirdInterface::speciesCodeTag(_T("speciesCode"));
const String EBirdInterface::commonNameTag(_T("comName"));
const String EBirdInterface::scientificNameTag(_T("sciName"));
const String EBirdInterface::locationNameTag(_T("locName"));
const String EBirdInterface::locationIDTag(_T("locID"));
const String EBirdInterface::latitudeTag(_T("lat"));
const String EBirdInterface::longitudeTag(_T("lng"));
const String EBirdInterface::countryCodeTag(_T("countryCode"));
const String EBirdInterface::subnational1CodeTag(_T("subnational1Code"));
const String EBirdInterface::subnational2CodeTag(_T("subnational2Code"));
const String EBirdInterface::observationDateTag(_T("obsDt"));
const String EBirdInterface::isNotHotspotTag(_T("locationPrivate"));
const String EBirdInterface::isReviewedTag(_T("obsReviewed"));
const String EBirdInterface::isValidTag(_T("obsValid"));
const String EBirdInterface::locationPrivateTag(_T("locationPrivate"));

const String EBirdInterface::countryTypeName(_T("country"));
const String EBirdInterface::subNational1TypeName(_T("subnational1"));
const String EBirdInterface::subNational2TypeName(_T("subnational2"));

const String EBirdInterface::nameTag(_T("name"));
const String EBirdInterface::codeTag(_T("code"));

const String EBirdInterface::eBirdTokenHeader(_T("X-eBirdApiToken: "));

std::unordered_map<String, EBirdInterface::NameInfo> EBirdInterface::commonToScientificMap;
std::unordered_map<String, String> EBirdInterface::scientificToCommonMap;

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsWithRecentObservationsOf(
	const String& speciesCode, const String& region, const unsigned int& recentPeriod)
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

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsInRegion(const String& region)
{
	OStringStream request;
	request << apiRoot << hotspotReferenceEndpoint << region << "?fmt=json";

	String response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<LocationInfo>();

	cJSON *root(cJSON_Parse(UString::ToNarrowString<String>(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetHotspotsInRegion())\n";
		Cerr << response << '\n';
		return std::vector<LocationInfo>();
	}

	std::vector<LocationInfo> hotspots(cJSON_GetArraySize(root));
	unsigned int i(0);
	for (auto& h : hotspots)
	{
		cJSON* item(cJSON_GetArrayItem(root, i));
		if (!item)
		{
			Cerr << "Failed to get hotspot array item\n";
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, locationNameTag, h.name))
		{
			Cerr << "Failed to get hotspot name for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, locationIDTag, h.id))
		{
			Cerr << "Failed to get hotspot id for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, latitudeTag, h.latitude))
		{
			Cerr << "Failed to get hotspot latitude for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, longitudeTag, h.longitude))
		{
			Cerr << "Failed to get hotspot longitude for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, countryCodeTag, h.countryCode))
		{
			Cerr << "Failed to get hotspot country code for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, subnational1CodeTag, h.subnational1Code))
		{
			Cerr << "Failed to get hotspot subnational 1 code for item " << i << '\n';
			hotspots.clear();
			break;
		}

		if (!ReadJSON(item, subnational2CodeTag, h.subnational2Code))
		{
			Cerr << "Failed to get hotspot subnational 2 code for item " << i << '\n';
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
		Cerr << "Failed to get species code\n";
		return false;
	}

	if (!ReadJSON(item, commonNameTag, info.commonName))
	{
		Cerr << "Failed to get common name for item\n";
		return false;
	}

	if (!ReadJSON(item, scientificNameTag, info.scientificName))
	{
		Cerr << "Failed to get scientific name for item\n";
		return false;
	}

	if (!ReadJSON(item, observationDateTag, info.observationDate))
	{
		// Try without time info before declaring a failure
		String dateString;
		if (!ReadJSON(item, observationDateTag, dateString))
		{
			Cerr << "Failed to get observation date and time for item\n";
			return false;
		}

		IStringStream ss(dateString);
		if ((ss >> std::get_time(&info.observationDate, _T("%Y-%m-%d"))).fail())
		{
			Cerr << "Failed to get observation date for item\n";
			return false;
		}

		info.dateIncludesTimeInfo = false;
	}

	if (!ReadJSON(item, locationNameTag, info.count))
	{
		Cerr << "Failed to get location name for item\n";
		return false;
	}

	if (!ReadJSON(item, locationNameTag, info.locationName))
	{
		Cerr << "Failed to get location name for item\n";
		return false;
	}

	if (!ReadJSON(item, locationIDTag, info.locationID))
	{
		Cerr << "Failed to get location id for item\n";
		return false;
	}

	if (!ReadJSON(item, isNotHotspotTag, info.isNotHotspot))
	{
		Cerr << "Failed to get hotspot status for item\n";
		return false;
	}

	if (!ReadJSON(item, latitudeTag, info.latitude))
	{
		Cerr << "Failed to get location latitude for item\n";
		return false;
	}

	if (!ReadJSON(item, longitudeTag, info.longitude))
	{
		Cerr << "Failed to get location longitude for item\n";
		return false;
	}

	if (!ReadJSON(item, isReviewedTag, info.observationReviewed))
	{
		Cerr << "Failed to get observation reviewed flag for item\n";
		return false;
	}

	if (!ReadJSON(item, isValidTag, info.observationValid))
	{
		Cerr << "Failed to get observation valid flag for item\n";
		return false;
	}

	if (!ReadJSON(item, locationPrivateTag, info.locationPrivate))
	{
		Cerr << "Failed to get location private flag\n";
		return false;
	}

	return true;
}

std::vector<EBirdInterface::ObservationInfo> EBirdInterface::GetRecentObservationsOfSpeciesInRegion(
	const String& speciesCode, const String& region, const unsigned int& recentPeriod,
	const bool& includeProvisional, const bool& hotspotsOnly)
{
	OStringStream request;
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

	String response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<ObservationInfo>();

	cJSON *root(cJSON_Parse(UString::ToNarrowString<String>(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetRecentObservationsOfSpeciesInRegion())\n";
		Cerr << response << '\n';
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
			Cerr << "Failed to get observation array item\n";
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
	headerList = curl_slist_append(headerList, UString::ToNarrowString<String>(String(eBirdTokenHeader
		+ static_cast<const TokenData*>(data)->token)).c_str());
	if (!headerList)
	{
		Cerr << "Failed to append token to header in AddTokenToCurlHeader\n";
		return false;
	}

	headerList = curl_slist_append(headerList, "Content-Type: application/json");
	if (!headerList)
	{
		Cerr << "Failed to append content type to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	return true;
}

String EBirdInterface::GetScientificNameFromCommonName(const String& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return String();
	}

	return commonToScientificMap[commonName].scientificName;
}

bool EBirdInterface::FetchEBirdNameData()
{
	OStringStream request;
	request << apiRoot << taxonomyLookupEndpoint << "?cat=species"
		<< "&locale=en"
		<< "&fmt=json";

	String response;
	if (!DoCURLGet(request.str(), response))
		return false;

	cJSON *root(cJSON_Parse(UString::ToNarrowString<String>(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetScientificNameFromCommonName())\n";
		Cerr << response << '\n';
		return false;
	}

	BuildNameMaps(root);
	cJSON_Delete(root);
	return true;
}

String EBirdInterface::GetSpeciesCodeFromCommonName(const String& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return String();
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

		String commonName;
		if (!ReadJSON(nameInfo, commonNameTag, commonName))
		{
			Cerr << "Failed to read common name tag\n";
			continue;
		}

		String scientificName;
		if (!ReadJSON(nameInfo, scientificNameTag, scientificName))
		{
			Cerr << "Failed to read scientific name tag\n";
			continue;
		}

		String code;
		if (!ReadJSON(nameInfo, speciesCodeTag, code))
		{
			Cerr << "Failed to read species code\n";
			continue;
		}

		commonToScientificMap[commonName] = NameInfo(scientificName, code);
		scientificToCommonMap[scientificName] = commonName;
	}
}

String EBirdInterface::GetRegionCode(const String& country,
	const String& state, const String& county)
{
	String countryCode(GetCountryCode(country));
	if (countryCode.empty())
		return String();

	if (!state.empty())
	{
		const String stateCode(GetStateCode(countryCode, state));
		if (stateCode.empty())
			return String();

		if (!county.empty())
			return GetCountyCode(stateCode, county);

		return stateCode;
	}

	return countryCode;
}

String EBirdInterface::GetUserInputOnResponse(const String& s, const String& field)
{
	Cout << "Multiple matches.  Please specify desired " << field << ":\n\n";

	IStringStream ss(s);
	String line;
	std::getline(ss, line);
	Cout << line << '\n';// heading row

	std::map<unsigned int, String> selectionMap;
	unsigned int i(0);
	while (std::getline(ss, line))
	{
		Cout << ++i << ": " << line << '\n';
		selectionMap[i] = line;
	}

	Cout << std::endl;

	unsigned int selection;
	Cin >> selection;

	if (selection == 0 || selection > i)
		return String();

	return selectionMap[selection];
}

std::vector<EBirdInterface::RegionInfo> EBirdInterface::GetSubRegions(
	const String& regionCode, const RegionType& type)
{
	const String regionTypeString([&type]()
	{
		if (type == RegionType::Country)
			return _T("country");
		else if (type == RegionType::SubNational1)
			return _T("subnational1");
		else// if (type == RegionType::SubNational2)
			return _T("subnational2");
	}());
	OStringStream request;
	request << apiRoot << regionReferenceEndpoint << regionTypeString << '/' << regionCode << ".json";

	String response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<RegionInfo>();

	cJSON *root(cJSON_Parse(UString::ToNarrowString<String>(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetSubRegions())\n";
		Cerr << "Request was: " << request.str() << '\n';
		Cerr << "Response was: " << response << '\n';
		return std::vector<RegionInfo>();
	}

	std::vector<RegionInfo> subRegions(cJSON_GetArraySize(root));
	unsigned int i(0);
	for (auto& r : subRegions)
	{
		cJSON* item(cJSON_GetArrayItem(root, i++));
		if (!item)
		{
			Cerr << "Failed to get sub-region array item\n";
			subRegions.clear();
			break;
		}

		if (!ReadJSON(item, nameTag, r.name))
		{
			Cerr << "Failed to read sub-region name\n";
			subRegions.clear();
			break;
		}

		if (!ReadJSON(item, codeTag, r.code))
		{
			Cerr << "Failed to read sub-region code\n";
			subRegions.clear();
			break;
		}
	}

	cJSON_Delete(root);
	return subRegions;
}

bool EBirdInterface::NameMatchesRegion(const String& name, const RegionInfo& region)
{
	assert(name.length() > 0 && region.name.length() > 0 && region.code.length() > 0);

	String lowerName(name);
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
	String lowerRegion(region.name);
	std::transform(lowerRegion.begin(), lowerRegion.end(), lowerRegion.begin(), ::tolower);
	String lowerCode(region.code);
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

String EBirdInterface::GetCountryCode(const String& country)
{
	if (storedRegionInfo.empty())
		BuildCountryInfo();

	for (const auto& r : storedRegionInfo)
	{
		if (NameMatchesRegion(country, r))
			return r.code;
	}

	Cerr << "Failed to find country code for '" << country << "'\n";
	return String();
}

String EBirdInterface::GetStateCode(const String& countryCode, const String& state)
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
		Cerr << "Failed to find matching entry for country code '" << countryCode << "'\n";
		return String();
	}

	if (countryInfo->subnational1Info.empty())
		countryInfo->subnational1Info = std::move(BuildSubNational1Info(countryInfo->code));

	for (const auto& r : countryInfo->subnational1Info)
	{
		if (NameMatchesRegion(state, r))
			return r.code;
	}

	Cerr << "Failed to find state code for '" << state << "' in '" << countryCode << "'\n";
	return String();
}

String EBirdInterface::GetCountyCode(const String& stateCode, const String& county)
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
		Cerr << "Failed to find matching entry for state code '" << stateCode << "'\n";
		return String();
	}

	if (subNational1Info->subnational2Info.empty())
		subNational1Info->subnational2Info = std::move(GetSubRegions(subNational1Info->code, RegionType::SubNational2));

	for (const auto& r : subNational1Info->subnational2Info)
	{
		if (NameMatchesRegion(county, r))
			return r.code;
	}

	Cerr << "Failed to find county code for '" << county << "' in '" << stateCode << "'\n";
	return String();
}

void EBirdInterface::BuildCountryInfo()
{
	auto regions(GetSubRegions(_T("world"), RegionType::Country));
	storedRegionInfo.resize(regions.size());
	unsigned int i;
	for (i = 0; i < storedRegionInfo.size(); ++i)
	{
		storedRegionInfo[i].code = regions[i].code;
		storedRegionInfo[i].name = regions[i].name;
	}
}

std::vector<EBirdInterface::SubNational1Info> EBirdInterface::BuildSubNational1Info(const String& countryCode)
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
