// File:  googleMapsInterface.h
// Date:  9/28/2017
// Auth:  K. Loux
// Desc:  Object for interfacing with the Google Maps web service.

#ifndef GOOGLE_MAPS_INTERFACE_H_
#define GOOGLE_MAPS_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <vector>

class GoogleMapsInterface : public JSONInterface
{
public:
	GoogleMapsInterface(const String& userAgent, const String& apiKey);

	enum class Units
	{
		Metric,
		Imperial
	};

	enum class TravelMode
	{
		Driving,
		Walking,
		Bicycling,
		Transit
	};

	struct DistanceInfo
	{
		double value;// [meters] or [seconds]
		String text;
	};

	struct Leg
	{
		DistanceInfo distance;
		DistanceInfo duration;
	};

	struct Directions
	{
		String summary;
		String copyright;
		std::vector<Leg> legs;
		std::vector<String> warnings;
	};

	Directions GetDirections(const String& from, const String& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;
	std::vector<Directions> GetMultipleDirections(const String& from, const String& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;

	bool LookupCoordinates(const String& searchString, String& formattedAddress,
		double& latitude, double& longitude,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		const String& preferNameContaining = String(), String* statusRet = nullptr) const;

	bool LookupCoordinates(const String& searchString, String& formattedAddress,
		double& latitude, double& longitude,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		const std::vector<String>& preferNamesContaining = std::vector<String>(), String* statusRet = nullptr) const;

	struct PlaceInfo
	{
		String formattedAddress;
		String name;
		double latitude;
		double longitude;
		double neLatitude;
		double neLongitude;
		double swLatitude;
		double swLongitude;
	};

	bool LookupPlace(const String& searchString, std::vector<PlaceInfo>& info, String* statusRet = nullptr) const;

private:
	const String apiKey;

	static const String apiRoot;
	static const String directionsEndPoint;
	static const String geocodeEndPoint;
	static const String placeSearchEndPoint;
	static const String statusKey;
	static const String okStatus;
	static const String errorMessageKey;
	static const String routesKey;
	static const String summaryKey;
	static const String copyrightKey;
	static const String legsKey;
	static const String warningsKey;
	static const String distanceKey;
	static const String durationKey;
	static const String valueKey;
	static const String textKey;
	static const String resultsKey;
	static const String formattedAddressKey;
	static const String geometryKey;
	static const String boundsKey;
	static const String northeastKey;
	static const String southwestKey;
	static const String locationKey;
	static const String locationTypeKey;
	static const String viewportKey;
	static const String latitudeKey;
	static const String longitudeKey;
	static const String addressComponentsKey;
	static const String longNameKey;
	static const String shortNameKey;
	static const String nameKey;
	static const String typesKey;
	static const String placeIDKey;

	String BuildRequestString(const String& origin, const String& destination,
		const TravelMode& mode, const bool& alternativeRoutes, const Units& units) const;
	static String BuildRequestString(const String& origin, const String& destination,
		const String& key, const String& mode, const bool& alternativeRoutes, const String& units);

	static String GetModeString(const TravelMode& mode);
	static String GetUnitString(const Units& units);

	bool ProcessDirectionsResponse(const String& response, std::vector<Directions>& directions) const;
	bool ProcessRoute(cJSON* route, Directions& d) const;
	bool ProcessLeg(cJSON* leg, Leg& l) const;
	bool ProcessValueTextItem(cJSON* item, DistanceInfo& info) const;

	static String SanitizeAddress(const String& s);

	struct GeocodeInfo
	{
		struct ComponentInfo
		{
			String longName;
			String shortName;
			std::vector<String> types;
		};

		std::vector<ComponentInfo> addressComponents;
		String formattedAddress;

		struct LatLongPair
		{
			double latitude;
			double longitude;
		};

		LatLongPair northeastBound;
		LatLongPair southwestBound;
		LatLongPair location;
		String locationType;

		LatLongPair northeastViewport;
		LatLongPair southwestViewport;

		String placeID;
		std::vector<String> types;
	};

	bool ProcessGeocodeResponse(const String& response, std::vector<GeocodeInfo>& info, String* statusRet) const;
	bool ProcessAddressComponents(cJSON* results, std::vector<GeocodeInfo::ComponentInfo>& components) const;
	bool ProcessGeometry(cJSON* results, GeocodeInfo& info) const;
	bool ReadLatLongPair(cJSON* json, GeocodeInfo::LatLongPair& latLong) const;
	bool ReadBoundsPair(cJSON* json, GeocodeInfo::LatLongPair& northeast, GeocodeInfo::LatLongPair& southwest) const;

	bool ProcessPlaceResponse(const String& response, std::vector<PlaceInfo>& info, String* statusRet) const;
};

#endif// GOOGLE_MAPS_INTERFACE_H_
