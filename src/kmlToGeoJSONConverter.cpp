// File:  kmlToGeoJSONConverter.cpp
// Date:  3/9/2019
// Auth:  K. Loux
// Desc:  Converts KML to GeoJSON coordinate arrays.

// Local headers
#include "kmlToGeoJSONConverter.h"
#include "geometryReducer.h"

// Standard C++ headers
#include <iostream>
#include <sstream>

KMLToGeoJSONConverter::KMLToGeoJSONConverter(const std::string& kml, const double& reductionLimit) : reductionLimit(reductionLimit)
{
	kmlParsedOK = ParseKML(kml);
}

bool KMLToGeoJSONConverter::ParseKML(const std::string& kml)
{
	std::string::size_type polygonPosition(0);
	while (polygonPosition = GoToNextPolygon(kml, polygonPosition), polygonPosition != std::string::npos)
	{
		polygons.push_back(Polygon());
		auto polygonEnd(GetPolygonEndLocation(kml, polygonPosition));
		auto lrPosition(polygonPosition);
		while (lrPosition = GoToNextLinearRing(kml, lrPosition), lrPosition < polygonEnd)
		{
			polygons.back().push_back(LinearRing());
			auto position(lrPosition);
			Point point;
			while (ExtractCoordinates(kml, position, point))
				polygons.back().back().push_back(point);
			lrPosition = position;

			if (reductionLimit > 0.0)
			{
				GeometryReducer reducer(reductionLimit);
				reducer.Reduce(polygons.back().back());
			}
		}
		//polygonPosition = lrPosition;// This was a bug - it sometimes advances too far, resulting in skipped polygons
	}

	return true;
}

std::string::size_type KMLToGeoJSONConverter::GetTagPosition(const std::string& kml,
	const std::string& tag, const std::string::size_type& start)
{
	const auto location(kml.find(tag, start));
	if (location == std::string::npos)
		return std::string::npos;
	return location + tag.length();
}

std::string::size_type KMLToGeoJSONConverter::GoToNextPolygon(const std::string& kml, const std::string::size_type& start)
{
	return GetTagPosition(kml, "<Polygon>", start);
}

std::string::size_type KMLToGeoJSONConverter::GetPolygonEndLocation(const std::string& kml, const std::string::size_type& start)
{
	return GetTagPosition(kml, "</Polygon>", start);
}

std::string::size_type KMLToGeoJSONConverter::GoToNextLinearRing(const std::string& kml, const std::string::size_type& start)
{
	return GetTagPosition(kml, "<LinearRing><coordinates>", start);
}

bool KMLToGeoJSONConverter::ExtractCoordinates(const std::string& kml, std::string::size_type& start, Point& point)
{
	const unsigned int chunkSize(40);
	std::istringstream ss(kml.substr(start, chunkSize));
	if ((ss >> point.x).fail())
		return false;

	if (ss.peek() != ',')
		return false;
	ss.ignore();

	if ((ss >> point.y).fail())
		return false;

	if (ss.peek() != ' ')
		return false;

	ss.ignore();
	start += ss.tellg();

	return true;
}

cJSON* KMLToGeoJSONConverter::GetGeoJSON() const
{
	auto geometry(cJSON_CreateObject());
	if (!geometry)
	{
		std::cerr << "Faile to create geometry JSON object\n";
		return nullptr;
	}

	cJSON_AddStringToObject(geometry, "type", "MultiPolygon");
	auto polygonArray(cJSON_CreateArray());
	if (!polygonArray)
	{
		std::cerr << "Faile to create polygon array JSON object\n";
		cJSON_Delete(geometry);
		return nullptr;
	}

	cJSON_AddItemToObject(geometry, "coordinates", polygonArray);

	for (const auto& p : polygons)
	{
		auto linearRingArray(cJSON_CreateArray());
		if (!linearRingArray)
		{
			std::cerr << "Faile to create polygon JSON object\n";
			cJSON_Delete(geometry);
			return nullptr;
		}

		cJSON_AddItemToArray(polygonArray, linearRingArray);

		for (const auto& linearRing : p)
		{
			auto coordArray(cJSON_CreateArray());
			if (!coordArray)
			{
				std::cerr << "Faile to create linear ring JSON object\n";
				cJSON_Delete(geometry);
				return nullptr;
			}

			cJSON_AddItemToArray(linearRingArray, coordArray);

			for (const auto& point : linearRing)
			{
				auto coord(cJSON_CreateArray());
				if (!coord)
				{
					std::cerr << "Faile to create coordinate JSON object\n";
					cJSON_Delete(geometry);
					return nullptr;
				}

				cJSON_AddItemToArray(coordArray, coord);

				cJSON_AddItemToArray(coord, cJSON_CreateNumber(point.x));
				cJSON_AddItemToArray(coord, cJSON_CreateNumber(point.y));
			}
		}
	}

	return geometry;
}

std::string KMLToGeoJSONConverter::GetKML() const
{
	std::ostringstream ss;
	ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n  <Placemark>\n    <MultiGeometry>";

	for (const auto& p : polygons)
	{
		for (const auto& lr : p)
		{
			ss << "<Polygon><outerBoundaryIs><LinearRing><coordinates>";
			for (const auto& point : lr)
				ss << point.x << ',' << point.y << ' ';
			ss << "</coordinates></LinearRing></outerBoundaryIs></Polygon>";
		}
	}

	ss << "\n    </MultiGeometry>\n  </Placemark>\n</kml>";
	return ss.str();
}
