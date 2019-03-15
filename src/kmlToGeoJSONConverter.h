// File:  kmlToGeoJSONConverter.h
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Converts KML to GeoJSON coordinate arrays.

#ifndef KML_TO_GEOJSON_CONVERTER_H_
#define KML_TO_GEOJSON_CONVERTER_H_

// Local headers
#include "email/cJSON/cJSON.h"

// Standard C++ headers
#include <string>
#include <vector>

class KMLToGeoJSONConverter
{
public:
	KMLToGeoJSONConverter(const std::string& kml);

	cJSON* GetGeoJSON() const;

private:
	bool kmlParsedOK;
	bool ParseKML(const std::string& kml);

	struct Point
	{
		double x;
		double y;
	};

	typedef std::vector<Point> LinearRing;
	typedef std::vector<LinearRing> Polygon;
	std::vector<Polygon> polygons;

	static std::string::size_type GoToNextPolygon(const std::string& kml, const std::string::size_type& start);
	static std::string::size_type GetPolygonEndLocation(const std::string& kml, const std::string::size_type& start);
	static std::string::size_type GoToNextLinearRing(const std::string& kml, const std::string::size_type& start);
	static bool ExtractCoordinates(const std::string& kml, const std::string::size_type& start, Point& point);
};

#endif// KML_TO_GEOJSON_CONVERTER_H_
