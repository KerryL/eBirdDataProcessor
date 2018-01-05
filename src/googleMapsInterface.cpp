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
const std::string GoogleMapsInterface::latitudeKey("lat");
const std::string GoogleMapsInterface::longitudeKey("lng");

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
	if (!ProcessResponse(response, directions))
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
	if (!ProcessResponse(response, directions))
		return std::vector<Directions>();

	return directions;
}

bool GoogleMapsInterface::ProcessResponse(const std::string& response, std::vector<Directions>& directions) const
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
		return false;
	}

	if (status.compare(okStatus) != 0)
	{
		std::cerr << "Directions request response is not OK.  Status = " << status << '\n';

		std::string errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			std::cerr << errorMessage << '\n';

		return false;
	}

	cJSON *routes(cJSON_GetObjectItem(root, routesKey.c_str()));
	if (!routes)
	{
		std::cerr << "Failed to get routes array from response\n";
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
			return false;
		}

		directions.push_back(d);
	}

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
	double& southwestLatitude, double& southwestLongitude) const
{
	const std::string requestURL(apiRoot + geocodeEndPoint
		+ "?address=" + SanitizeAddress(searchString));
	std::string response;
	if (!DoCURLGet(requestURL, response))
	{
		std::cerr << "Failed to process GET request\n";
		return false;
	}

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
		return false;
	}

	if (status.compare(okStatus) != 0)
	{
		std::cerr << "Geocode request response is not OK.  Status = " << status << '\n';

		std::string errorMessage;
		if (ReadJSON(root, errorMessageKey, errorMessage))
			std::cerr << errorMessage << '\n';

		return false;
	}

	/*if (!ReadJSON(route, summaryKey, d.summary))
	{
		std::cerr << "Failed to read summary\n";
		return false;
	}

resultsKey("results");
const std::string GoogleMapsInterface::formattedAddressKey("formatted_address");
const std::string GoogleMapsInterface::geometryKey("geometry");
const std::string GoogleMapsInterface::boundsKey("bounds");
const std::string GoogleMapsInterface::northeastKey("northeast");
const std::string GoogleMapsInterface::southwestKey("southwest");
const std::string GoogleMapsInterface::locationKey("location");
const std::string GoogleMapsInterface::latitudeKey("lat");
const std::string GoogleMapsInterface::longitudeKey("lng");*/

	return true;
}

bool GoogleMapsInterface::ProcessGeocodeResponse(const std::string& response,
	GeocodeInfo& info) const
{
	// TODO:  Try:  https://maps.googleapis.com/maps/api/geocode/json?address=StMarys+MD
	return false;
}

