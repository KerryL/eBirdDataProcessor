// File:  kmlToGeoJSONConverter.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Converts KML to GeoJSON coordinate arrays.

// Local headers
#include "kmlToGeoJSONConverter.h"

// Standard C++ headers
#include <iostream>
#include <sstream>

KMLToGeoJSONConverter::KMLToGeoJSONConverter(const std::string& kml)
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
		}
		polygonPosition = lrPosition;
	}

	return true;
}

std::string::size_type KMLToGeoJSONConverter::GetTagPosition(const std::string& kml,
	const std::string& tag, const std::string::size_type& start)
{
	return kml.find(tag, start);
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

	if ((ss >> point.y).fail())
		return false;

	if (ss.peek() != ' ')
		return false;

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
