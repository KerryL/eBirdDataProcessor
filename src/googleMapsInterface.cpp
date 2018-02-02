// File:  googleMapsInterface.cpp
// Date:  9/28/2017
// Auth:  K. Loux
// Desc:  Object for interfacing with the Google Maps web service.

// Local headers
#include "googleMapsInterface.h"

// Standard C++ headers
#include <cassert>
#include <iostream>

const std::string GoogleMapsInterface::apiRoot("https://maps.googleapis.com/maps/api/");
const std::string GoogleMapsInterface::directionsEndPoint("directions/json");
const std::string GoogleMapsInterface::geocodeEndPoint("geocode/json");
const std::string GoogleMapsInterface::statusKey("status");
const std::string GoogleMapsInterface::okStatus("OK");
const std::string GoogleMapsInterface::errorMessageKey("error_message");
const std::string GoogleMapsInterface::routesKey("routes");
const std::string GoogleMapsInterface::summaryKey("summary");
const std::string GoogleMapsInterface::copyrightKey("copyrights");
const std::string GoogleMapsInterface::legsKey("legs");
const std::string GoogleMapsInterface::warningsKey("warnings");
const std::string GoogleMapsInterface::distanceKey("distance");
const std::string GoogleMapsInterface::durationKey("duration");
const std::string GoogleMapsInterface::valueKey("value");
const std::string GoogleMapsInterface::textKey("text");
const std::string GoogleMapsInterface::resultsKey("results");
const std::string GoogleMapsInterface::formattedAddressKey("formatted_address");
const std::string GoogleMapsInterface::geometryKey("geometry");
const std::string GoogleMapsInterface::boundsKey("bounds");
const std::string GoogleMapsInterface::northeastKey("northeast");
const std::string GoogleMapsInterface::southwestKey("southwest");
const std::string GoogleMapsInterface::locationKey("location");
const std::string GoogleMapsInterface::locationTypeKey("location_type");
const std::string GoogleMapsInterface::latitudeKey("lat");
const std::string GoogleMapsInterface::longitudeKey("lng");
const std::string GoogleMapsInterface::viewportKey("viewport");
const std::string GoogleMapsInterface::addressComponentsKey("address_components");
const std::string GoogleMapsInterface::longNameKey("long_name");
const std::string GoogleMapsInterface::shortNameKey("short_name");
const std::string GoogleMapsInterface::typesKey("types");
const std::string GoogleMapsInterface::placeIDKey("place_id");

GoogleMapsInterface::GoogleMapsInterface(const std::string& userAgent, const std::string& apiKey) : JSONInterface(userAgent), apiKey(apiKey)
{
}

GoogleMapsInterface::Directions GoogleMapsInterface::GetDirections(const std::string& from,
	const std::string& to, const TravelMode& mode, const Units& units) const
{
	const std::string requestURL(apiRoot + directionsEndPoint
		+ BuildRequestString(from, to, mode, false, units));
	std::string response;
	if (!DoCURLGet(requestURL, response))
	{
		std::cerr << "Failed to process GET request\n";
		return Directions();
	}

	std::vector<Directions> directions;
	if (!ProcessDirectionsResponse(response, directions))
		return Directions();

	assert(directions.size() == 1);

	return directions.front();
}

std::vector<GoogleMapsInterface::Directions> GoogleMapsInterface::GetMultipleDirections(const std::string& from,
	const std::string& to, const TravelMode& mode, const Units& units) const
{
	const std::string requestURL(apiRoot + directionsEndPoint
		+ BuildRequestString(from, to, mode, true, units));
	std::string response;
	if (!DoCURLGet(requestURL, response))
	{
		std::cerr << "Failed to process GET request\n";
		return std::vector<Directions>();
	}

	std::vector<Directions> directions;
	if (!ProcessDirectionsResponse(response, directions))
		return std::vector<Directions>();

	return directions;
}

bool GoogleMapsInterface::ProcessDirectionsResponse(const std::string& response, std::vector<Directions>& directions) const
{
	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse response (GoogleMapsInterface::ProcessResponse)\n";
		std::cerr << response << std::endl;
		return false;
	}

	std::string status;
	if (!ReadJSON(root, statusKey, status))
	{
		std::cerr << "Failed to read status information\n";
		cJSON_Delete(root);
		return false;
	}

	if (status.compare(okStatus) != 0)
	{
		std::cerr << "Directions request response is not OK.  Status = " << status << '\n';

		std::string errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			std::cerr << errorMessage << '\n';

		cJSON_Delete(root);
		return false;
	}

	cJSON *routes(cJSON_GetObjectItem(root, routesKey.c_str()));
	if (!routes)
	{
		std::cerr << "Failed to get routes array from response\n";
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
			std::cerr << "Failed to process route " << i << std::endl;
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
		std::cerr << "Failed to read route item\n";
		return false;
	}

	if (!ReadJSON(route, summaryKey, d.summary))
	{
		std::cerr << "Failed to read summary\n";
		return false;
	}

	if (!ReadJSON(route, copyrightKey, d.copyright))
	{
		std::cerr << "Failed to read copyright\n";
		return false;
	}

	ReadJSON(route, warningsKey, d.warnings);// Not required - don't fail if none are present

	cJSON *legs(cJSON_GetObjectItem(route, legsKey.c_str()));
	if (!legs)
	{
		std::cerr << "Failed to get legs array\n";
		return false;
	}

	const unsigned int legCount(cJSON_GetArraySize(legs));
	unsigned int i;
	for (i = 0; i < legCount; ++i)
	{
		Leg l;
		if (!ProcessLeg(cJSON_GetArrayItem(legs, i), l))
		{
			std::cerr << "Failed to process leg " << i << std::endl;
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
		std::cerr << "Failed to read leg item\n";
		return false;
	}

	cJSON* distance(cJSON_GetObjectItem(leg, distanceKey.c_str()));
	if (!distance)
	{
		std::cerr << "Failed to get distance info\n";
		return false;
	}

	if (!ProcessValueTextItem(distance, l.distance))
		return false;

	cJSON* duration(cJSON_GetObjectItem(leg, durationKey.c_str()));
	if (!duration)
	{
		std::cerr << "Failed to get duration info\n";
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
		std::cerr << "Failed to read value\n";
		return false;
	}

	if (!ReadJSON(item, textKey, info.text))
	{
		std::cerr << "Failed to read text\n";
		return false;
	}

	return true;
}

std::string GoogleMapsInterface::BuildRequestString(const std::string& origin, const std::string& destination,
	const TravelMode& mode, const bool& alternativeRoutes, const Units& units) const
{
	return BuildRequestString(origin, destination, apiKey, GetModeString(mode), alternativeRoutes, GetUnitString(units));
}

std::string GoogleMapsInterface::BuildRequestString(const std::string& origin, const std::string& destination,
	const std::string& key, const std::string& mode, const bool& alternativeRoutes, const std::string& units)
{
	assert(!origin.empty());
	assert(!destination.empty());
	assert(!key.empty());

	std::string request("?origin=" + SanitizeAddress(origin) + "&destination=" + SanitizeAddress(destination) + "&key=" + key);

	if (!mode.empty())
		request.append("&mode=" + mode);// Valid modes are "driving", "walking", "bicycling" and "transit"

	if (alternativeRoutes)
		request.append("&alternatives=true");
	else
		request.append("&alternatives=false");

	if (!units.empty())
		request.append("&units=" + units);// Valid units are "metric" and "imperial"

	return URLEncode(request);
}

std::string GoogleMapsInterface::GetModeString(const TravelMode& mode)
{
	switch (mode)
	{
	case TravelMode::Driving:
		return "driving";

	case TravelMode::Walking:
		return "walking";

	case TravelMode::Bicycling:
		return "bicycling";

	case TravelMode::Transit:
		return "transit";
	}

	assert(false);
	return std::string();
}

std::string GoogleMapsInterface::GetUnitString(const Units& units)
{
	switch (units)
	{
	case Units::Metric:
		return "metric";

	case Units::Imperial:
		return "imperial";
	}

	assert(false);
	return std::string();
}

std::string GoogleMapsInterface::SanitizeAddress(const std::string& s)
{
	std::string address;

	for (const auto& c : s)
	{
		if (c == ' ')
			address.push_back('+');
		else
			address.push_back(c);
	}

	return address;
}

bool GoogleMapsInterface::LookupCoordinates(const std::string& searchString,
	std::string& formattedAddress, double& latitude, double& longitude,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	const std::string& preferNameContaining, std::string* statusRet) const
{
	std::vector<std::string> names;
	if (!preferNameContaining.empty())
		names.push_back(preferNameContaining);

	return LookupCoordinates(searchString, formattedAddress, latitude, longitude,
		northeastLatitude, northeastLongitude, southwestLatitude, southwestLongitude,
		names, statusRet);
}

bool GoogleMapsInterface::LookupCoordinates(const std::string& searchString,
	std::string& formattedAddress, double& latitude, double& longitude,
	double& northeastLatitude, double& northeastLongitude,
	double& southwestLatitude, double& southwestLongitude,
	const std::vector<std::string>& preferNamesContaining, std::string* statusRet) const
{
	const std::string requestURL(apiRoot + geocodeEndPoint
		+ "?address=" + SanitizeAddress(searchString) + "&key=" + apiKey);
	std::string response;
	if (!DoCURLGet(requestURL, response))
	{
		std::cerr << "Failed to process GET request\n";
		return false;
	}

	std::vector<GeocodeInfo> info;
	if (!ProcessGeocodeResponse(response, info, statusRet))
		return false;

	assert(info.size() > 0);
	if (info.size() > 1)
		std::cerr << "Warning:  Multiple results found for '" << searchString << "' - using first result\n";

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

bool GoogleMapsInterface::ProcessGeocodeResponse(const std::string& response,
	std::vector<GeocodeInfo>& info, std::string* statusRet) const
{
	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse response (GoogleMapsInterface::LookupCoordinates)\n";
		std::cerr << response << std::endl;
		return false;
	}

	std::string status;
	if (!ReadJSON(root, statusKey, status))
	{
		std::cerr << "Failed to read status information\n";
		cJSON_Delete(root);
		return false;
	}

	if (statusRet)
		*statusRet = status;

	if (status.compare(okStatus) != 0)
	{
		std::cerr << "Geocode request response is not OK.  Status = " << status << '\n';

		std::string errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			std::cerr << errorMessage << '\n';

		cJSON_Delete(root);
		return false;
	}

	cJSON* results(cJSON_GetObjectItem(root, resultsKey.c_str()));
	if (!results)
	{
		std::cerr << "Failed to read geocode results\n";
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
			std::cerr << "Failed to get result entry " << i << '\n';
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
			std::cerr << "Failed to read formatted address\n";
			cJSON_Delete(root);
			return false;
		}

		if (!ReadJSON(resultEntry, placeIDKey, resultInfo.placeID))
		{
			std::cerr << "Failed to read place ID\n";
			cJSON_Delete(root);
			return false;
		}

		if (!ReadJSONArrayToVector(resultEntry, typesKey, resultInfo.types))
		{
			std::cerr << "Failed to result types\n";
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
	cJSON* addressComponents(cJSON_GetObjectItem(results, addressComponentsKey.c_str()));
	if (!addressComponents)
	{
		std::cerr << "Failed to read address component information\n";
		return false;
	}

	components.resize(cJSON_GetArraySize(addressComponents));
	unsigned int i(0);
	for (auto& info : components)
	{
		cJSON* item(cJSON_GetArrayItem(addressComponents, i));
		if (!item)
		{
			std::cerr << "Failed to read address component " << i << '\n';
			return false;
		}

		if (!ReadJSON(item, longNameKey, info.longName))
		{
			std::cerr << "Failed to read long name for component " << i << '\n';
			return false;
		}

		if (!ReadJSON(item, shortNameKey, info.shortName))
		{
			std::cerr << "Failed to read short name for component " << i << '\n';
			return false;
		}

		if (!ReadJSONArrayToVector(item, typesKey, info.types))
		{
			std::cerr << "Failed to read types for component " << i << '\n';
			return false;
		}

		++i;
	}

	return true;
}

bool GoogleMapsInterface::ProcessGeometry(cJSON* results, GeocodeInfo& info) const
{
	cJSON* geometry(cJSON_GetObjectItem(results, geometryKey.c_str()));
	if (!geometry)
	{
		std::cerr << "Failed to read geometry information\n";
		return false;
	}

	cJSON* bounds(cJSON_GetObjectItem(geometry, boundsKey.c_str()));
	if (!bounds)
	{
		std::cerr << "Failed to find bounds\n";
		return false;
	}

	if (!ReadBoundsPair(bounds, info.northeastBound, info.southwestBound))
	{
		std::cerr << "Failed to read bounds\n";
		return false;
	}

	cJSON* location(cJSON_GetObjectItem(geometry, locationKey.c_str()));
	if (!location)
	{
		std::cerr << "Failed to find location\n";
		return false;
	}

	if (!ReadLatLongPair(location, info.location))
	{
		std::cerr << "Failed to read location\n";
		return false;
	}

	if (!ReadJSON(geometry, locationTypeKey, info.locationType))
	{
		std::cerr << "Failed to read location type\n";
		return false;
	}

	cJSON* viewport(cJSON_GetObjectItem(geometry, viewportKey.c_str()));
	if (!viewport)
	{
		std::cerr << "Failed to find viewport\n";
		return false;
	}

	if (!ReadBoundsPair(viewport, info.northeastViewport, info.southwestViewport))
	{
		std::cerr << "Failed to read viewport\n";
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
	cJSON* ne(cJSON_GetObjectItem(json, northeastKey.c_str()));
	if (!ne)
		return false;
	if (!ReadLatLongPair(ne, northeast))
		return false;

	cJSON* sw(cJSON_GetObjectItem(json, southwestKey.c_str()));
	if (!sw)
		return false;
	if (!ReadLatLongPair(sw, southwest))
		return false;

	return true;
}
