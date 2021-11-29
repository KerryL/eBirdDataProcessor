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

const UString::String EBirdInterface::apiRoot(_T("https://api.ebird.org/v2/"));
const UString::String EBirdInterface::observationDataPath(_T("data/obs/"));
const UString::String EBirdInterface::productListsPath(_T("product/lists/"));
const UString::String EBirdInterface::recentPath(_T("recent/"));
const UString::String EBirdInterface::taxonomyLookupEndpoint(_T("ref/taxonomy/ebird"));
const UString::String EBirdInterface::regionReferenceEndpoint(_T("ref/region/list/"));
const UString::String EBirdInterface::hotspotReferenceEndpoint(_T("ref/hotspot/"));
const UString::String EBirdInterface::regionInfoEndpoint(_T("ref/region/info/"));

const UString::String EBirdInterface::speciesCodeTag(_T("speciesCode"));
const UString::String EBirdInterface::commonNameTag(_T("comName"));
const UString::String EBirdInterface::scientificNameTag(_T("sciName"));
const UString::String EBirdInterface::locationNameTag(_T("locName"));
const UString::String EBirdInterface::userDisplayNameTag(_T("userDisplayName"));
const UString::String EBirdInterface::locationIDTag(_T("locID"));
const UString::String EBirdInterface::submissionIDTag(_T("subID"));
const UString::String EBirdInterface::latitudeTag(_T("lat"));
const UString::String EBirdInterface::longitudeTag(_T("lng"));
const UString::String EBirdInterface::locationObjectTag(_T("loc"));
const UString::String EBirdInterface::howManyTag(_T("howMany"));
const UString::String EBirdInterface::speciesCountTag(_T("numSpecies"));
const UString::String EBirdInterface::countryCodeTag(_T("countryCode"));
const UString::String EBirdInterface::subnational1CodeTag(_T("subnational1Code"));
const UString::String EBirdInterface::subnational2CodeTag(_T("subnational2Code"));
const UString::String EBirdInterface::observationDateTag(_T("obsDt"));
const UString::String EBirdInterface::observationTimeTag(_T("obsTime"));
const UString::String EBirdInterface::isNotHotspotTag(_T("locationPrivate"));
const UString::String EBirdInterface::isReviewedTag(_T("obsReviewed"));
const UString::String EBirdInterface::isValidTag(_T("obsValid"));
const UString::String EBirdInterface::locationPrivateTag(_T("locationPrivate"));

const UString::String EBirdInterface::countryTypeName(_T("country"));
const UString::String EBirdInterface::subNational1TypeName(_T("subnational1"));
const UString::String EBirdInterface::subNational2TypeName(_T("subnational2"));

const UString::String EBirdInterface::nameTag(_T("name"));
const UString::String EBirdInterface::codeTag(_T("code"));
const UString::String EBirdInterface::resultTag(_T("result"));

const UString::String EBirdInterface::errorTag(_T("errors"));
const UString::String EBirdInterface::titleTag(_T("title"));
const UString::String EBirdInterface::statusTag(_T("status"));

const UString::String EBirdInterface::eBirdTokenHeader(_T("X-eBirdApiToken: "));

std::unordered_map<UString::String, EBirdInterface::NameInfo> EBirdInterface::commonToScientificMap;
std::unordered_map<UString::String, UString::String> EBirdInterface::scientificToCommonMap;

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsWithRecentObservationsOf(
	const UString::String& speciesCode, const UString::String& region, const unsigned int& recentPeriod)
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

std::vector<EBirdInterface::LocationInfo> EBirdInterface::GetHotspotsInRegion(const UString::String& region)
{
	UString::OStringStream request;
	request << apiRoot << hotspotReferenceEndpoint << region << "?fmt=json";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<LocationInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetHotspotsInRegion())\n";
		Cerr << response.c_str() << '\n';
		return std::vector<LocationInfo>();
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
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

		if (region.length() > 2)
		{
			if (!ReadJSON(item, subnational1CodeTag, h.subnational1Code))
			{
				Cerr << "Failed to get hotspot subnational 1 code for item " << i << '\n';
				hotspots.clear();
				break;
			}

			if (region.length() > 5)
			{
				if (!ReadJSON(item, subnational2CodeTag, h.subnational2Code))
				{
					Cerr << "Failed to get hotspot subnational 2 code for item " << i << '\n';
					hotspots.clear();
					break;
				}
			}
		}

		++i;
	}

	cJSON_Delete(root);
	return hotspots;
}

void EBirdInterface::PrintErrorInfo(const std::vector<ErrorInfo>& errors)
{
	for (const auto& e : errors)
		Cout << "Error " << e.code << " : " << e.title << " : " << e.status << std::endl;
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
		UString::String dateString;
		if (!ReadJSON(item, observationDateTag, dateString))
		{
			Cerr << "Failed to get observation date and time for item\n";
			return false;
		}

		UString::IStringStream ss(dateString);
		if ((ss >> std::get_time(&info.observationDate, _T("%Y-%m-%d"))).fail())
		{
			Cerr << "Failed to get observation date for item\n";
			return false;
		}

		info.dateIncludesTimeInfo = false;
	}

	if (!ReadJSON(item, howManyTag, info.count))
	{
		Cerr << "Failed to get observation count\n";
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

bool EBirdInterface::ReadJSONChecklistData(cJSON* item, ChecklistInfo& info)
{
	if (!ReadJSON(item, submissionIDTag, info.submissionId))
	{
		Cerr << "Failed to get submission ID\n";
		return false;
	}

	if (!ReadJSON(item, userDisplayNameTag, info.userDisplayName))
	{
		Cerr << "Failed to get user display name\n";
		return false;
	}

	if (!ReadJSON(item, speciesCountTag, info.speciesCount))
	{
		Cerr << "Failed to get number of species\n";
		return false;
	}

	UString::String s;
	if (!ReadJSON(item, observationDateTag, s))
	{
		Cerr << "Failed to get observation date\n";
		return false;
	}
	// TODO:  Populate time structure

	if (ReadJSON(item, observationTimeTag, s))
	{
		// TODO:  Populate time structure
	}
	else
		info.dateIncludesTimeInfo = false;

	if (!ReadJSONLocationData(item, info.locationInfo))
		return false;

	return true;
}

bool EBirdInterface::ReadJSONLocationData(cJSON* item, LocationInfo& info)
{
	cJSON* locationObject(cJSON_GetObjectItem(item, UString::ToNarrowString(locationObjectTag).c_str()));
	if (!locationObject)
	{
		Cerr << "Failed to get location object\n";
		return false;
	}

	if (!ReadJSON(locationObject, locationIDTag, info.id))
	{
		Cerr << "Failed to read location ID\n";
		return false;
	}

	if (!ReadJSON(locationObject, nameTag, info.name))
	{
		Cerr << "Failed to read location name\n";
		return false;
	}

	if (!ReadJSON(locationObject, latitudeTag, info.latitude))
	{
		Cerr << "Failed to read latitude\n";
		return false;
	}

	if (!ReadJSON(locationObject, longitudeTag, info.longitude))
	{
		Cerr << "Failed to read longitude\n";
		return false;
	}

	if (!ReadJSON(locationObject, countryCodeTag, info.countryCode))
	{
		Cerr << "Failed to read country code\n";
		return false;
	}

	if (!ReadJSON(locationObject, subnational1CodeTag, info.subnational1Code))
	{
		Cerr << "Failed to read subnational 1 code\n";
		return false;
	}

	if (!ReadJSON(locationObject, subnational2CodeTag, info.subnational2Code))
	{
		Cerr << "Failed to read subnational 2 code\n";
		return false;
	}

	return true;
}

std::vector<EBirdInterface::ObservationInfo> EBirdInterface::GetRecentObservationsOfSpeciesInRegion(
	const UString::String& speciesCode, const UString::String& region, const unsigned int& recentPeriod,
	const bool& includeProvisional, const bool& hotspotsOnly)
{
	UString::OStringStream request;
	request << apiRoot << observationDataPath << region << '/' << recentPath
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
		Cerr << "Failed to parse returned string (GetRecentObservationsOfSpeciesInRegion())\n";
		Cerr << response.c_str() << '\n';
		return std::vector<ObservationInfo>();
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
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

std::vector<EBirdInterface::ChecklistInfo> EBirdInterface::GetChecklistFeed(const UString::String& region,
	const unsigned short& year, const unsigned short& month, const unsigned short& day)
{
	UString::OStringStream request;
	request << apiRoot << productListsPath << region << '/' << year << '/' << month << '/' << day << "?maxResults=200";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<ChecklistInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetChecklistFeed())\n";
		Cerr << response.c_str() << '\n';
		return std::vector<ChecklistInfo>();
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
		return std::vector<ChecklistInfo>();
	}

	std::vector<ChecklistInfo> checklists;
	const unsigned int arraySize(cJSON_GetArraySize(root));
	unsigned int i;
	for (i = 0; i < arraySize; ++i)
	{
		cJSON* item(cJSON_GetArrayItem(root, i));
		if (!item)
		{
			Cerr << "Failed to get checklist array item\n";
			checklists.clear();
			break;
		}

		ChecklistInfo info;
		if (!ReadJSONChecklistData(item, info))
		{
			checklists.clear();
			break;
		}

		checklists.push_back(info);
	}

	cJSON_Delete(root);
	return checklists;
}

std::vector<EBirdInterface::ObservationInfo> EBirdInterface::GetRecentObservationsNear(
	const double& latitude, const double& longitude, const unsigned int& radius,
	const unsigned int& recentPeriod, const bool& includeProvisional, const bool& hotspotsOnly)
{
	UString::OStringStream request;
	request << apiRoot << observationDataPath << "geo/recent?back=" << recentPeriod
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

	request.precision(2);
	request << "&lat=" << std::fixed << latitude << "&lng=" << std::fixed << longitude << "&dist=" << radius;

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<ObservationInfo>();

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetRecentObservationsNear())\n";
		Cerr << response.c_str() << '\n';
		return std::vector<ObservationInfo>();
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
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
	headerList = curl_slist_append(headerList, UString::ToNarrowString(UString::String(eBirdTokenHeader
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

UString::String EBirdInterface::GetScientificNameFromCommonName(const UString::String& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return UString::String();
	}

	return commonToScientificMap[commonName].scientificName;
}

bool EBirdInterface::FetchEBirdNameData()
{
	UString::OStringStream request;
	request << apiRoot << taxonomyLookupEndpoint << "?cat=species"
		<< "&locale=en"
		<< "&fmt=json";

	std::string response;
	if (!DoCURLGet(request.str(), response))
		return false;

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetScientificNameFromCommonName())\n";
		Cerr << response.c_str() << '\n';
		return false;
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
		return false;
	}

	BuildNameMaps(root);
	cJSON_Delete(root);
	return true;
}

UString::String EBirdInterface::GetSpeciesCodeFromCommonName(const UString::String& commonName)
{
	if (commonToScientificMap.empty())
	{
		if (!FetchEBirdNameData())
			return UString::String();
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

		UString::String commonName;
		if (!ReadJSON(nameInfo, commonNameTag, commonName))
		{
			Cerr << "Failed to read common name tag\n";
			continue;
		}

		UString::String scientificName;
		if (!ReadJSON(nameInfo, scientificNameTag, scientificName))
		{
			Cerr << "Failed to read scientific name tag\n";
			continue;
		}

		UString::String code;
		if (!ReadJSON(nameInfo, speciesCodeTag, code))
		{
			Cerr << "Failed to read species code\n";
			continue;
		}

		commonToScientificMap[commonName] = NameInfo(scientificName, code);
		scientificToCommonMap[scientificName] = commonName;
	}
}

UString::String EBirdInterface::GetRegionCode(const UString::String& country,
	const UString::String& state, const UString::String& county)
{
	UString::String countryCode(GetCountryCode(country));
	if (countryCode.empty())
		return UString::String();

	if (!state.empty())
	{
		const UString::String stateCode(GetStateCode(countryCode, state));
		if (stateCode.empty())
			return UString::String();

		if (!county.empty())
			return GetCountyCode(stateCode, county);

		return stateCode;
	}

	return countryCode;
}

std::vector<UString::String> EBirdInterface::GetRegionCodes(const std::vector<UString::String>& countries,
	const std::vector<UString::String>& states, const std::vector<UString::String>& counties)
{
	assert(countries.size() == states.size() || states.empty());
	assert(states.size() == counties.size() || counties.empty());

	std::vector<UString::String> codes;
	for (unsigned int i = 0; i < countries.size(); ++i)
	{
		if (states.empty())
			codes.push_back(GetRegionCode(countries[i]));
		else if (counties.empty())
			codes.push_back(GetRegionCode(countries[i], states[i]));
		else
			codes.push_back(GetRegionCode(countries[i], states[i], counties[i]));
	}
	return codes;
}

UString::String EBirdInterface::GetUserInputOnResponse(const UString::String& s, const UString::String& field)
{
	Cout << "Multiple matches.  Please specify desired " << field << ":\n\n";

	UString::IStringStream ss(s);
	UString::String line;
	std::getline(ss, line);
	Cout << line << '\n';// heading row

	std::map<unsigned int, UString::String> selectionMap;
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
		return UString::String();

	return selectionMap[selection];
}

std::vector<EBirdInterface::RegionInfo> EBirdInterface::GetSubRegions(
	const UString::String& regionCode, const RegionType& type)
{
	if (type == RegionType::MostDetailAvailable)
	{
		auto sn2(GetSubRegions(regionCode, RegionType::SubNational2));
		if (!sn2.empty())
			return sn2;

		auto sn1(GetSubRegions(regionCode, RegionType::SubNational1));
		if (!sn1.empty())
			return sn1;

		if (regionCode.compare(_T("world")) != 0)// No subregions for the specified region code
			return std::vector<RegionInfo>();

		return GetSubRegions(regionCode, RegionType::Country);
	}

	const UString::String regionTypeString([&type]()
	{
		if (type == RegionType::Country)
			return _T("country");
		else if (type == RegionType::SubNational1)
			return _T("subnational1");
		else// if (type == RegionType::SubNational2)
			return _T("subnational2");
	}());
	UString::OStringStream request;
	request << apiRoot << regionReferenceEndpoint << regionTypeString << '/' << regionCode << "?fmt=json";

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return std::vector<RegionInfo>();

	// It seems sometimes we get HTML response instead of JSON.  Attempt to handle that here.
	const std::string htmlStart("<!doctype html>");
	if (response.substr(0, htmlStart.length()).compare(htmlStart) == 0)
	{
		Cerr << "Warning:  Got HTML response to '" << request.str() << "'; trying once more\n";
		// Try one more time before giving up
		if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
			return std::vector<RegionInfo>();
	}

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetSubRegions())\n";
		Cerr << "Request was: " << request.str() << '\n';
		Cerr << "Response was: " << response.c_str() << '\n';
		return std::vector<RegionInfo>();
	}

	std::vector<ErrorInfo> errors;
	if (ResponseHasErrors(root, errors))
	{
		if (errors.size() == 1 && errors.front().status.compare(_T("500")) == 0)// "No enum constant" error -> indicates no subregions, even at country level
		{
			const auto countryCode(GetCountryCode(regionCode));
			auto it(storedRegionInfo.cbegin());
			for (; it != storedRegionInfo.end(); ++it)
			{
				if (it->code.compare(countryCode) == 0)
					break;
			}

			if (it == storedRegionInfo.end())
			{
				Cerr << "Failed to find information for '" << regionCode << "'\n";
				return std::vector<RegionInfo>();
			}

			RegionInfo countryInfo;
			countryInfo.code = countryCode;
			countryInfo.name = it->name;
			return std::vector<RegionInfo>(1, countryInfo);
		}
		else
		{
			Cerr << "Request for subregion info failed\n";
			for (const auto& e : errors)
				Cerr << "  Error:  " << e.title << "; status = " << e.status << "; code = " << e.code << '\n';
			return std::vector<RegionInfo>();
		}
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

bool EBirdInterface::ResponseHasErrors(cJSON *root, std::vector<ErrorInfo>& errors)
{
	cJSON* errorsNode(cJSON_GetObjectItem(root, UString::ToNarrowString(errorTag).c_str()));
	if (!errorsNode)
		return false;

	errors.resize(cJSON_GetArraySize(errorsNode));
	unsigned int i(0);
	for (auto& e : errors)
	{
		cJSON* item(cJSON_GetArrayItem(errorsNode, i++));
		if (!item)
		{
			Cerr << "Failed to read error " << i << '\n';
			return true;
		}

		if (!ReadJSON(item, codeTag, e.code))
		{
			Cerr << "Failed to read error code\n";
			break;
		}

		if (!ReadJSON(item, statusTag, e.status))
		{
			Cerr << "Failed to read error status\n";
			break;
		}

		if (!ReadJSON(item, titleTag, e.title))
		{
			Cerr << "Failed to read error title\n";
			break;
		}
	}

	return true;
}

bool EBirdInterface::NameMatchesRegion(const UString::String& name, const RegionInfo& region)
{
	assert(name.length() > 0 && region.name.length() > 0 && region.code.length() > 0);

	auto localToLower([](const UString::Char& c)
	{
		return static_cast<UString::Char>(::tolower(c));
	});

	UString::String lowerName(name);
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), localToLower);
	UString::String lowerRegion(region.name);
	std::transform(lowerRegion.begin(), lowerRegion.end(), lowerRegion.begin(), localToLower);
	UString::String lowerCode(region.code);
	std::transform(lowerCode.begin(), lowerCode.end(), lowerCode.begin(), localToLower);

	if (lowerName.compare(lowerRegion) == 0)
		return true;
	else if (lowerName.compare(lowerCode) == 0)
		return true;
	/*else if (lowerCode.length() > lowerName.length() &&
		lowerCode.substr(lowerCode.length() - lowerName.length()).compare(lowerName) == 0)
		return true;
	else if (lowerName.length() > lowerCode.length() &&
		lowerName.substr(lowerName.length() - lowerCode.length()).compare(lowerCode) == 0)
		return true;*/// I think this was an attempt to match names more liberally, but really it's a bug.  Need to use something far more sophisticated if we want to handle inexact matches.

	// Also consider that the region code will always be fully qualified (i.e. XX-YY),
	// but the name or code we're trying to match may not be (i.e. YY only)
	auto lastDash(lowerCode.find_last_of(UString::Char('-')));
	if (lastDash != std::string::npos)
	{
		if (lowerName.compare(lowerCode.substr(lastDash + 1)) == 0)
			return true;
	}

	return false;
}

UString::String EBirdInterface::GetCountryCode(const UString::String& country)
{
	if (storedRegionInfo.empty())
		BuildCountryInfo();

	for (const auto& r : storedRegionInfo)
	{
		if (NameMatchesRegion(country, r))
			return r.code;
	}

	Cerr << "Failed to find country code for '" << country << "'\n";
	return UString::String();
}

UString::String EBirdInterface::GetStateCode(const UString::String& countryCode, const UString::String& state)
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
		return UString::String();
	}

	if (countryInfo->subnational1Info.empty())
		countryInfo->subnational1Info = std::move(BuildSubNational1Info(countryInfo->code));

	for (const auto& r : countryInfo->subnational1Info)
	{
		if (NameMatchesRegion(state, r))
			return r.code;
	}

	Cerr << "Failed to find state code for '" << state << "' in '" << countryCode << "'\n";
	return UString::String();
}

UString::String EBirdInterface::GetCountyCode(const UString::String& stateCode, const UString::String& county)
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
		return UString::String();
	}

	if (subNational1Info->subnational2Info.empty())
		subNational1Info->subnational2Info = std::move(GetSubRegions(subNational1Info->code, RegionType::SubNational2));

	for (const auto& r : subNational1Info->subnational2Info)
	{
		if (NameMatchesRegion(county, r))
			return r.code;
	}

	Cerr << "Failed to find county code for '" << county << "' in '" << stateCode << "'\n";
	return UString::String();
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

std::vector<EBirdInterface::SubNational1Info> EBirdInterface::BuildSubNational1Info(const UString::String& countryCode)
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

EBirdInterface::Protocol EBirdInterface::MapProtocolCodeToProtocol(const std::string& code)
{
	assert(code.length() == 3);
	assert(code[0] == 'P');
	
	if (code.compare("P20") == 0)
		return Protocol::Incidental;
	else if (code.compare("P21") == 0)
		return Protocol::Stationary;
	else if (code.compare("P22") == 0)
		return Protocol::Traveling;
	else if (code.compare("P23") == 0)
		return Protocol::Area;
	else if (code.compare("P33") == 0)
		return Protocol::Banding;
	else if (code.compare("P41") == 0)
		return Protocol::RustyBlackbirdSMB;
	else if (code.compare("P46") == 0)
		return Protocol::CWCPointCount;
	else if (code.compare("P47") == 0)
		return Protocol::CWCAreaSearch;
	else if (code.compare("P48") == 0)
		return Protocol::Random;
	else if (code.compare("P52") == 0)
		return Protocol::OiledBirds;
	else if (code.compare("P54") == 0)
		return Protocol::NocturnalFlightCall;
	else if (code.compare("P58") == 0)
		return Protocol::AudobonCoastalBirdSurvey;
	else if (code.compare("P59") == 0)
		return Protocol::TNCCaliforniaWaterbirdCount;
	else if (code.compare("P60") == 0)
		return Protocol::Paleagic;
	else if (code.compare("P62") == 0)
		return Protocol::Historical;
	else if (code.compare("P69") == 0)
		return Protocol::CaliforniaBrownPelicanSurvey;
	else if (code.compare("P73") == 0)
		return Protocol::PROALAS;
	else if (code.compare("P74") == 0)
		return Protocol::InternationalShorebirdSurvey;
	else if (code.compare("P75") == 0)
		return Protocol::TricoloredBlackbirdWinterSurvey;

	return Protocol::Other;
}

UString::String EBirdInterface::GetRegionName(const UString::String& code) const
{
	UString::OStringStream request;
	request << apiRoot << regionInfoEndpoint << code;

	std::string response;
	if (!DoCURLGet(URLEncode(request.str()), response, AddTokenToCurlHeader, &tokenData))
		return code;

	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetRegionName())\n";
		Cerr << response.c_str() << '\n';
		return code;
	}

	std::vector<ErrorInfo> errorInfo;
	if (ResponseHasErrors(root, errorInfo))
	{
		PrintErrorInfo(errorInfo);
		return code;
	}

	UString::String name;
	if (!ReadJSON(root, resultTag, name))
	{
		Cerr << "Failed to get result of GetRegionName()\n";
		return code;
	}

	return name;
}
