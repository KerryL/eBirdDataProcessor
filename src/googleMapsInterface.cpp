// File:  googleMapsInterface.cpp
// Date:  9/28/2017
// Auth:  K. Loux
// Desc:  Object for interfacing with the Google Maps web service.

// Local headers
#include "googleMapsInterface.h"

// Standard C++ headers
#include <cassert>

const String GoogleMapsInterface::apiRoot(_T("https://maps.googleapis.com/maps/api/"));
const String GoogleMapsInterface::directionsEndPoint(_T("directions/json"));
const String GoogleMapsInterface::geocodeEndPoint(_T("geocode/json"));
const String GoogleMapsInterface::statusKey(_T("status"));
const String GoogleMapsInterface::okStatus(_T("OK"));
const String GoogleMapsInterface::errorMessageKey(_T("error_message"));
const String GoogleMapsInterface::routesKey(_T("routes"));
const String GoogleMapsInterface::summaryKey(_T("summary"));
const String GoogleMapsInterface::copyrightKey(_T("copyrights"));
const String GoogleMapsInterface::legsKey(_T("legs"));
const String GoogleMapsInterface::warningsKey(_T("warnings"));
const String GoogleMapsInterface::distanceKey(_T("distance"));
const String GoogleMapsInterface::durationKey(_T("duration"));
const String GoogleMapsInterface::valueKey(_T("value"));
const String GoogleMapsInterface::textKey(_T("text"));
const String GoogleMapsInterface::resultsKey(_T("results"));
const String GoogleMapsInterface::formattedAddressKey(_T("formatted_address"));
const String GoogleMapsInterface::geometryKey(_T("geometry"));
const String GoogleMapsInterface::boundsKey(_T("bounds"));
const String GoogleMapsInterface::northeastKey(_T("northeast"));
const String GoogleMapsInterface::southwestKey(_T("southwest"));
const String GoogleMapsInterface::locationKey(_T("location"));
const String GoogleMapsInterface::locationTypeKey(_T("location_type"));
const String GoogleMapsInterface::latitudeKey(_T("lat"));
const String GoogleMapsInterface::longitudeKey(_T("lng"));
const String GoogleMapsInterface::viewportKey(_T("viewport"));
const String GoogleMapsInterface::addressComponentsKey(_T("address_components"));
const String GoogleMapsInterface::longNameKey(_T("long_name"));
const String GoogleMapsInterface::shortNameKey(_T("short_name"));
const String GoogleMapsInterface::typesKey(_T("types"));
const String GoogleMapsInterface::placeIDKey(_T("place_id"));

GoogleMapsInterface::GoogleMapsInterface(const String& userAgent, const String& apiKey) : JSONInterface(userAgent), apiKey(apiKey)
{
}

GoogleMapsInterface::Directions GoogleMapsInterface::GetDirections(const String& from,
	const String& to, const TravelMode& mode, const Units& units) const
{
	const String requestURL(apiRoot + directionsEndPoint
		+ BuildRequestString(from, to, mode, false, units));
	String response;
	if (!DoCURLGet(requestURL, response))
	{
		Cerr << "Failed to process GET request\n";
		return Directions();
	}

	std::vector<Directions> directions;
	if (!ProcessDirectionsResponse(response, directions))
		return Directions();

	assert(directions.size() == 1);

	return directions.front();
}

std::vector<GoogleMapsInterface::Directions> GoogleMapsInterface::GetMultipleDirections(const String& from,
	const String& to, const TravelMode& mode, const Units& units) const
{
	const String requestURL(apiRoot + directionsEndPoint
		+ BuildRequestString(from, to, mode, true, units));
	String response;
	if (!DoCURLGet(requestURL, response))
	{
		Cerr << "Failed to process GET request\n";
		return std::vector<Directions>();
	}

	std::vector<Directions> directions;
	if (!ProcessDirectionsResponse(response, directions))
		return std::vector<Directions>();

	return directions;
}

bool GoogleMapsInterface::ProcessDirectionsResponse(const String& response, std::vector<Directions>& directions) const
{
	cJSON *root(cJSON_Parse(UString::ToNarrowString(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse response (GoogleMapsInterface::ProcessResponse)\n";
		Cerr << response << '\n';
		return false;
	}

	String status;
	if (!ReadJSON(root, statusKey, status))
	{
		Cerr << "Failed to read status information\n";
		cJSON_Delete(root);
		return false;
	}

	if (status.compare(okStatus) != 0)
	{
		Cerr << "Directions request response is not OK.  Status = " << status << '\n';

		String errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			Cerr << errorMessage << '\n';

		cJSON_Delete(root);
		return false;
	}

	cJSON *routes(cJSON_GetObjectItem(root, UString::ToNarrowString(routesKey).c_str()));
	if (!routes)
	{
		Cerr << "Failed to get routes array from response\n";
		cJSON_Delete(root);
		return false;
	}

	const unsigned int routeCount(cJSON_GetArraySize(routes));
	unsigned int i;
	for (i = 0; i < routeCount; ++i)
	{
		Directions d;
		if (!ProcessRoute(cJSON_GetArrayItem(routes, i), d))
		{
			Cerr << "Failed to process route " << i << '\n';
			cJSON_Delete(root);
			return false;
		}

		directions.push_back(d);
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleMapsInterface::ProcessRoute(cJSON* route, Directions& d) const
{
	if (!route)
	{
		Cerr << "Failed to read route item\n";
		return false;
	}

	if (!ReadJSON(route, summaryKey, d.summary))
	{
		Cerr << "Failed to read summary\n";
		return false;
	}

	if (!ReadJSON(route, copyrightKey, d.copyright))
	{
		Cerr << "Failed to read copyright\n";
		return false;
	}

	ReadJSON(route, warningsKey, d.warnings);// Not required - don't fail if none are present

	cJSON *legs(cJSON_GetObjectItem(route, UString::ToNarrowString(legsKey).c_str()));
	if (!legs)
	{
		Cerr << "Failed to get legs array\n";
		return false;
	}

	const unsigned int legCount(cJSON_GetArraySize(legs));
	unsigned int i;
	for (i = 0; i < legCount; ++i)
	{
		Leg l;
		if (!ProcessLeg(cJSON_GetArrayItem(legs, i), l))
		{
			Cerr << "Failed to process leg " << i << '\n';
			return false;
		}

		d.legs.push_back(l);
	}

	return true;
}

bool GoogleMapsInterface::ProcessLeg(cJSON* leg, Leg& l) const
{
	if (!leg)
	{
		Cerr << "Failed to read leg item\n";
		return false;
	}

	cJSON* distance(cJSON_GetObjectItem(leg, UString::ToNarrowString(distanceKey).c_str()));
	if (!distance)
	{
		Cerr << "Failed to get distance info\n";
		return false;
	}

	if (!ProcessValueTextItem(distance, l.distance))
		return false;

	cJSON* duration(cJSON_GetObjectItem(leg, UString::ToNarrowString(durationKey).c_str()));
	if (!duration)
	{
		Cerr << "Failed to get duration info\n";
		return false;
	}

	if (!ProcessValueTextItem(duration, l.duration))
		return false;

	return true;
}

bool GoogleMapsInterface::ProcessValueTextItem(cJSON* item, DistanceInfo& info) const
{
	assert(item);
	if (!ReadJSON(item, valueKey, info.value))
	{
		Cerr << "Failed to read value\n";
		return false;
	}

	if (!ReadJSON(item, textKey, info.text))
	{
		Cerr << "Failed to read text\n";
		return false;
	}

	return true;
}

String GoogleMapsInterface::BuildRequestString(const String& origin, const String& destination,
	const TravelMode& mode, const bool& alternativeRoutes, const Units& units) const
{
	return BuildRequestString(origin, destination, apiKey, GetModeString(mode), alternativeRoutes, GetUnitString(units));
}

String GoogleMapsInterface::BuildRequestString(const String& origin, const String& destination,
	const String& key, const String& mode, const bool& alternativeRoutes, const String& units)
{
	assert(!origin.empty());
	assert(!destination.empty());
	assert(!key.empty());

	String request(_T("?origin=") + SanitizeAddress(origin) + _T("&destination=") + SanitizeAddress(destination) + _T("&key=") + key);

	if (!mode.empty())
		request.append(_T("&mode=") + mode);// Valid modes are "driving", "walking", "bicycling" and "transit"

	if (alternativeRoutes)
		request.append(_T("&alternatives=true"));
	else
		request.append(_T("&alternatives=false"));

	if (!units.empty())
		request.append(_T("&units=") + units);// Valid units are "metric" and "imperial"

	return URLEncode(request);
}

String GoogleMapsInterface::GetModeString(const TravelMode& mode)
{
	switch (mode)
	{
	case TravelMode::Driving:
		return _T("driving");

	case TravelMode::Walking:
		return _T("walking");

	case TravelMode::Bicycling:
		return _T("bicycling");

	case TravelMode::Transit:
		return _T("transit");
	}

	assert(false);
	return String();
}

String GoogleMapsInterface::GetUnitString(const Units& units)
{
	switch (units)
	{
	case Units::Metric:
		return _T("metric");

	case Units::Imperial:
		return _T("imperial");
	}

	assert(false);
	return String();
}

String GoogleMapsInterface::SanitizeAddress(const String& s)
{
	String address;

	for (const auto& c : s)
	{
		if (c == Char(' '))
			address.push_back(Char('+'));
		else
			address.push_back(c);
	}

	return address;
}

bool GoogleMapsInterface::LookupCoordinates(const String& searchString,
	String& formattedAddress, double& latitude, double& longitude,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	const String& preferNameContaining, String* statusRet) const
{
	std::vector<String> names;
	if (!preferNameContaining.empty())
		names.push_back(preferNameContaining);

	return LookupCoordinates(searchString, formattedAddress, latitude, longitude,
		northeastLatitude, northeastLongitude, southwestLatitude, southwestLongitude,
		names, statusRet);
}

bool GoogleMapsInterface::LookupCoordinates(const String& searchString,
	String& formattedAddress, double& latitude, double& longitude,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	const std::vector<String>& preferNamesContaining, String* statusRet) const
{
	const String requestURL(apiRoot + geocodeEndPoint
		+ _T("?address=") + SanitizeAddress(searchString) + _T("&key=") + apiKey);
	String response;
	if (!DoCURLGet(requestURL, response))
	{
		Cerr << "Failed to process GET request\n";
		return false;
	}

	std::vector<GeocodeInfo> info;
	if (!ProcessGeocodeResponse(response, info, statusRet))
		return false;

	assert(info.size() > 0);
	if (info.size() > 1)
		Cerr << "Warning:  Multiple results found for '" << searchString << "' - using first result\n";

	if (!preferNamesContaining.empty())
	{
		for (const auto& name : preferNamesContaining)
		{
			for (const auto& component : info.front().addressComponents)
			{
				if (component.longName.find(name) != std::string::npos ||
					component.shortName.find(name) != std::string::npos)
				{
					formattedAddress = component.longName;
					break;
				}
			}
		}
	}

	if (formattedAddress.empty())
		formattedAddress = info.front().formattedAddress;

	latitude = info.front().location.latitude;
	longitude = info.front().location.longitude;
	northeastLatitude = info.front().northeastBound.latitude;
	northeastLongitude = info.front().northeastBound.longitude;
	southwestLatitude = info.front().southwestBound.latitude;
	southwestLongitude = info.front().southwestBound.longitude;

	return true;
}

bool GoogleMapsInterface::ProcessGeocodeResponse(const String& response,
	std::vector<GeocodeInfo>& info, String* statusRet) const
{
	cJSON *root(cJSON_Parse(UString::ToNarrowString(response).c_str()));
	if (!root)
	{
		Cerr << "Failed to parse response (GoogleMapsInterface::LookupCoordinates)\n";
		Cerr << response << '\n';
		return false;
	}

	String status;
	if (!ReadJSON(root, statusKey, status))
	{
		Cerr << "Failed to read status information\n";
		cJSON_Delete(root);
		return false;
	}

	if (statusRet)
		*statusRet = status;

	if (status.compare(okStatus) != 0)
	{
		Cerr << "Geocode request response is not OK.  Status = " << status << '\n';

		String errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			Cerr << errorMessage << '\n';

		cJSON_Delete(root);
		return false;
	}

	cJSON* results(cJSON_GetObjectItem(root, UString::ToNarrowString(resultsKey).c_str()));
	if (!results)
	{
		Cerr << "Failed to read geocode results\n";
		cJSON_Delete(root);
		return false;
	}

	info.resize(cJSON_GetArraySize(results));
	unsigned int i(0);
	for (auto& resultInfo : info)
	{
		cJSON* resultEntry(cJSON_GetArrayItem(results, i));
		if (!resultEntry)
		{
			Cerr << "Failed to get result entry " << i << '\n';
			cJSON_Delete(root);
			return false;
		}

		if (!ProcessAddressComponents(resultEntry, resultInfo.addressComponents))
		{
			cJSON_Delete(root);
			return false;
		}

		if (!ProcessGeometry(resultEntry, resultInfo))
		{
			cJSON_Delete(root);
			return false;
		}

		if (!ReadJSON(resultEntry, formattedAddressKey, resultInfo.formattedAddress))
		{
			Cerr << "Failed to read formatted address\n";
			cJSON_Delete(root);
			return false;
		}

		if (!ReadJSON(resultEntry, placeIDKey, resultInfo.placeID))
		{
			Cerr << "Failed to read place ID\n";
			cJSON_Delete(root);
			return false;
		}

		if (!ReadJSONArrayToVector(resultEntry, typesKey, resultInfo.types))
		{
			Cerr << "Failed to result types\n";
			cJSON_Delete(root);
			return false;
		}

		++i;
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleMapsInterface::ProcessAddressComponents(cJSON* results,
	std::vector<GeocodeInfo::ComponentInfo>& components) const
{
	cJSON* addressComponents(cJSON_GetObjectItem(results, UString::ToNarrowString(addressComponentsKey).c_str()));
	if (!addressComponents)
	{
		Cerr << "Failed to read address component information\n";
		return false;
	}

	components.resize(cJSON_GetArraySize(addressComponents));
	unsigned int i(0);
	for (auto& info : components)
	{
		cJSON* item(cJSON_GetArrayItem(addressComponents, i));
		if (!item)
		{
			Cerr << "Failed to read address component " << i << '\n';
			return false;
		}

		if (!ReadJSON(item, longNameKey, info.longName))
		{
			Cerr << "Failed to read long name for component " << i << '\n';
			return false;
		}

		if (!ReadJSON(item, shortNameKey, info.shortName))
		{
			Cerr << "Failed to read short name for component " << i << '\n';
			return false;
		}

		if (!ReadJSONArrayToVector(item, typesKey, info.types))
		{
			Cerr << "Failed to read types for component " << i << '\n';
			return false;
		}

		++i;
	}

	return true;
}

bool GoogleMapsInterface::ProcessGeometry(cJSON* results, GeocodeInfo& info) const
{
	cJSON* geometry(cJSON_GetObjectItem(results, UString::ToNarrowString(geometryKey).c_str()));
	if (!geometry)
	{
		Cerr << "Failed to read geometry information\n";
		return false;
	}

	cJSON* bounds(cJSON_GetObjectItem(geometry, UString::ToNarrowString(boundsKey).c_str()));
	if (!bounds)
	{
		Cerr << "Failed to find bounds\n";
		return false;
	}

	if (!ReadBoundsPair(bounds, info.northeastBound, info.southwestBound))
	{
		Cerr << "Failed to read bounds\n";
		return false;
	}

	cJSON* location(cJSON_GetObjectItem(geometry, UString::ToNarrowString(locationKey).c_str()));
	if (!location)
	{
		Cerr << "Failed to find location\n";
		return false;
	}

	if (!ReadLatLongPair(location, info.location))
	{
		Cerr << "Failed to read location\n";
		return false;
	}

	if (!ReadJSON(geometry, locationTypeKey, info.locationType))
	{
		Cerr << "Failed to read location type\n";
		return false;
	}

	cJSON* viewport(cJSON_GetObjectItem(geometry, UString::ToNarrowString(viewportKey).c_str()));
	if (!viewport)
	{
		Cerr << "Failed to find viewport\n";
		return false;
	}

	if (!ReadBoundsPair(viewport, info.northeastViewport, info.southwestViewport))
	{
		Cerr << "Failed to read viewport\n";
		return false;
	}

	return true;
}

bool GoogleMapsInterface::ReadLatLongPair(cJSON* json, GeocodeInfo::LatLongPair& latLong) const
{
	return ReadJSON(json, latitudeKey, latLong.latitude) &&
		ReadJSON(json, longitudeKey, latLong.longitude);
}

bool GoogleMapsInterface::ReadBoundsPair(cJSON* json,
	GeocodeInfo::LatLongPair& northeast, GeocodeInfo::LatLongPair& southwest) const
{
	cJSON* ne(cJSON_GetObjectItem(json, UString::ToNarrowString(northeastKey).c_str()));
	if (!ne)
		return false;
	if (!ReadLatLongPair(ne, northeast))
		return false;

	cJSON* sw(cJSON_GetObjectItem(json, UString::ToNarrowString(southwestKey).c_str()));
	if (!sw)
		return false;
	if (!ReadLatLongPair(sw, southwest))
		return false;

	return true;
}
