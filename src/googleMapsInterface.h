// File:  googleMapsInterface.h
// Date:  9/28/2017
// Auth:  K. Loux
// Desc:  Object for interfacing with the Google Maps web service.

#ifndef GOOGLE_MAPS_INTERFACE_H_
#define GOOGLE_MAPS_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"

// Standard C++ headers
#include <vector>

class GoogleMapsInterface : public JSONInterface
{
public:
	GoogleMapsInterface(const std::string& userAgent, const std::string& apiKey);

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
		std::string text;
	};

	struct Leg
	{
		DistanceInfo distance;
		DistanceInfo duration;
	};

	struct Directions
	{
		std::string summary;
		std::string copyright;
		std::vector<Leg> legs;
		std::vector<std::string> warnings;
	};

	Directions GetDirections(const std::string& from, const std::string& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;
	std::vector<Directions> GetMultipleDirections(const std::string& from, const std::string& to,
		const TravelMode& mode = TravelMode::Driving, const Units& units = Units::Imperial) const;

private:
	const std::string apiKey;

	static const std::string apiRoot;
	static const std::string statusKey;
	static const std::string okStatus;
	static const std::string errorMessageKey;
	static const std::string routesKey;
	static const std::string summaryKey;
	static const std::string copyrightKey;
	static const std::string legsKey;
	static const std::string warningsKey;
	static const std::string distanceKey;
	static const std::string durationKey;
	static const std::string valueKey;
	static const std::string textKey;

	std::string BuildRequestString(const std::string& origin, const std::string& destination,
		const TravelMode& mode, const bool& alternativeRoutes, const Units& units) const;
	static std::string BuildRequestString(const std::string& origin, const std::string& destination,
		const std::string& key, const std::string& mode, const bool& alternativeRoutes, const std::string& units);

	static std::string GetModeString(const TravelMode& mode);
	static std::string GetUnitString(const Units& units);

	bool ProcessResponse(const std::string& response, std::vector<Directions>& directions) const;
	bool ProcessRoute(cJSON* route, Directions& d) const;
	bool ProcessLeg(cJSON* leg, Leg& l) const;
	bool ProcessValueTextItem(cJSON* item, DistanceInfo& info) const;

	static std::string SanitizeAddress(const std::string& s);
};

#endif// GOOGLE_MAPS_INTERFACE_H_
