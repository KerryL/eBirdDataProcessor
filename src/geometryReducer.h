// File:  geometryReducer.h
// Date:  6/25/2019
// Auth:  K. Loux
// Desc:  Implements the Ramer-Douglas-Peucker algorithm to reduce the number of points in the kml geometry data.

#ifndef GEOMETRY_REDUCER_H_
#define GEOMETRY_REDUCER_H_

// Local headers
#include "point.h"

// Standard C++ headers
#include <vector>

class GeometryReducer
{
public:
	GeometryReducer(const double& epsilon) : epsilon(epsilon) {}

	void Reduce(std::vector<Point>& polygon) const;

private:
	static const std::vector<Point>::size_type sizeThreshold;
	const double epsilon;

	struct Vector3D
	{
		double x;
		double y;
		double z;
	};

	void ComputeLine(const Point& startPoint, const Point& endPoint, Vector3D& point, Vector3D& direction) const;
	double GetPerpendicularDistance(const Vector3D& point, const Vector3D& direction, const Point& testPoint) const;
	std::vector<Point> DoReduction(std::vector<Point> polygon) const;

	Vector3D GetWGS84(const double& latitude, const double& longitude) const;
};

#endif// GEOMETRY_REDUCER_H_
