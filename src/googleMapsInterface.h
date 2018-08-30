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
	GoogleMapsInterface(const UString::String& userAgent, const UString::String& apiKey);

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
		UString::String text;
	};

	struct Leg
	{
		DistanceInfo distance;
		DistanceInfo duration;
	};

	struct Directions
	{
		UString::String summary;
		UString::String copyright;
		std::vector<Leg> legs;
		std::vector<UString::String> warnings;
	};

	Directions GetDirections(const UString::String& from, const UString::String& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;
	std::vector<Directions> GetMultipleDirections(const UString::String& from, const UString::String& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;

	bool LookupCoordinates(const UString::String& searchString, UString::String& formattedAddress,
		double& latitude, double& longitude,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		const UString::String& preferNameContaining = UString::String(), UString::String* statusRet = nullptr) const;

	bool LookupCoordinates(const UString::String& searchString, UString::String& formattedAddress,
		double& latitude, double& longitude,
		double& northeastLatitude, double& northeastLongitude,
		double& southwestLatitude, double& southwestLongitude,
		const std::vector<UString::String>& preferNamesContaining = std::vector<UString::String>(), UString::String* statusRet = nullptr) const;

	struct PlaceInfo
	{
		UString::String formattedAddress;
		UString::String name;
		double latitude;
		double longitude;
		double neLatitude;
		double neLongitude;
		double swLatitude;
		double swLongitude;
	};

	bool LookupPlace(const UString::String& searchString, std::vector<PlaceInfo>& info, UString::String* statusRet = nullptr) const;

private:
	const UString::String apiKey;

	static const UString::String apiRoot;
	static const UString::String directionsEndPoint;
	static const UString::String geocodeEndPoint;
	static const UString::String placeSearchEndPoint;
	static const UString::String statusKey;
	static const UString::String okStatus;
	static const UString::String errorMessageKey;
	static const UString::String routesKey;
	static const UString::String summaryKey;
	static const UString::String copyrightKey;
	static const UString::String legsKey;
	static const UString::String warningsKey;
	static const UString::String distanceKey;
	static const UString::String durationKey;
	static const UString::String valueKey;
	static const UString::String textKey;
	static const UString::String resultsKey;
	static const UString::String formattedAddressKey;
	static const UString::String geometryKey;
	static const UString::String boundsKey;
	static const UString::String northeastKey;
	static const UString::String southwestKey;
	static const UString::String locationKey;
	static const UString::String locationTypeKey;
	static const UString::String viewportKey;
	static const UString::String latitudeKey;
	static const UString::String longitudeKey;
	static const UString::String addressComponentsKey;
	static const UString::String longNameKey;
	static const UString::String shortNameKey;
	static const UString::String nameKey;
	static const UString::String typesKey;
	static const UString::String placeIDKey;

	UString::String BuildRequestString(const UString::String& origin, const UString::String& destination,
		const TravelMode& mode, const bool& alternativeRoutes, const Units& units) const;
	static UString::String BuildRequestString(const UString::String& origin, const UString::String& destination,
		const UString::String& key, const UString::String& mode, const bool& alternativeRoutes, const UString::String& units);

	static UString::String GetModeString(const TravelMode& mode);
	static UString::String GetUnitString(const Units& units);

	bool ProcessDirectionsResponse(const UString::String& response, std::vector<Directions>& directions) const;
	bool ProcessRoute(cJSON* route, Directions& d) const;
	bool ProcessLeg(cJSON* leg, Leg& l) const;
	bool ProcessValueTextItem(cJSON* item, DistanceInfo& info) const;

	static UString::String SanitizeAddress(const UString::String& s);

	struct GeocodeInfo
	{
		struct ComponentInfo
		{
			UString::String longName;
			UString::String shortName;
			std::vector<UString::String> types;
		};

		std::vector<ComponentInfo> addressComponents;
		UString::String formattedAddress;

		struct LatLongPair
		{
			double latitude;
			double longitude;
		};

		LatLongPair northeastBound;
		LatLongPair southwestBound;
		LatLongPair location;
		UString::String locationType;

		LatLongPair northeastViewport;
		LatLongPair southwestViewport;

		UString::String placeID;
		std::vector<UString::String> types;
	};

	bool ProcessGeocodeResponse(const UString::String& response, std::vector<GeocodeInfo>& info, UString::String* statusRet) const;
	bool ProcessAddressComponents(cJSON* results, std::vector<GeocodeInfo::ComponentInfo>& components) const;
	bool ProcessGeometry(cJSON* results, GeocodeInfo& info) const;
	bool ReadLatLongPair(cJSON* json, GeocodeInfo::LatLongPair& latLong) const;
	bool ReadBoundsPair(cJSON* json, GeocodeInfo::LatLongPair& northeast, GeocodeInfo::LatLongPair& southwest) const;

	bool ProcessPlaceResponse(const UString::String& response, std::vector<PlaceInfo>& info, UString::String* statusRet) const;
};

#endif// GOOGLE_MAPS_INTERFACE_H_
