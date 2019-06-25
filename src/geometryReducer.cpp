// File:  geometryReducer.cpp
// Date:  6/25/2019
// Auth:  K. Loux
// Desc:  Implements the Ramer-Douglas-Peucker algorithm to reduce the number of points in the kml geometry data.

// Local headers
#include "geometryReducer.h"

// Standard C++ headers
#include <stack>

void GeometryReducer::Reduce(std::vector<Point>& polygon) const
{
	// Some special handling because we're often working with polygons which start and end with the same point
	const auto halfSize(polygon.size() / 2);
	const auto reducedHalf1(DoReduction(std::vector<Point>(polygon.begin(), polygon.begin() + halfSize)));
	const auto reducedHalf2(DoReduction(std::vector<Point>(polygon.begin() + halfSize, polygon.end())));
	polygon = reducedHalf1;
	polygon.insert(polygon.end(), reducedHalf2.begin(), reducedHalf2.end());
}

std::vector<Point> GeometryReducer::DoReduction(std::vector<Point> polygon) const
{
	std::vector<Point> reduced;
	std::stack<std::vector<Point>> portions;
	portions.push(polygon);

	while (!portions.empty())
	{
		auto top(portions.top());
		portions.pop();

		Vector3D point;
		Vector3D direction;
		ComputeLine(top.front(), top.back(), point, direction);
		double maxDistance(0.0);
		unsigned int splitIndex(0);
		for (unsigned int i = 1; i < top.size(); ++i)
		{
			const double d(GetPerpendicularDistance(point, direction, top[i]));
			if (d > maxDistance)
			{
				splitIndex = i;
				maxDistance = d;
			}
		}

		if (maxDistance > epsilon)
		{
			portions.push(std::vector<Point>(top.begin() + splitIndex, top.end()));
			portions.push(std::vector<Point>(top.begin(), top.begin() + splitIndex));
		}
		else
		{
			reduced.push_back(top.front());
			reduced.push_back(top.back());
		}
	}

	return reduced;
}

void GeometryReducer::ComputeLine(const Point& startPoint, const Point& endPoint, Vector3D& point, Vector3D& direction) const
{
	point = GetWGS84(startPoint.x, startPoint.y);
	const auto p2(GetWGS84(endPoint.x, endPoint.y));
	direction.x = p2.x - point.x;
	direction.y = p2.y - point.y;
	direction.z = p2.z - point.z;
}

// Here, x and y are latitude and longitude in degrees.
// We compute linear distance (no "curvature of the Earth" effects) in km based on a WGS84 ellipsoid.
double GeometryReducer::GetPerpendicularDistance(const Vector3D& point, const Vector3D& direction, const Point& testPoint) const
{
	const double lengthSquared(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);// Shortcut because we know direction is not normalized
	const auto testPointWGS(GetWGS84(testPoint.x, testPoint.y));
	Vector3D a;
	a.x = point.x - testPointWGS.x;
	a.y = point.y - testPointWGS.y;
	a.z = point.z - testPointWGS.z;
	const double t(-(a.x * direction.x + a.y * direction.y + a.z * direction.z) / lengthSquared);

	Vector3D closestPointOnLine;
	closestPointOnLine.x = point.x + t * direction.x;
	closestPointOnLine.y = point.y + t * direction.y;
	closestPointOnLine.z = point.z + t * direction.z;
	return sqrt((closestPointOnLine.x - testPointWGS.x) * (closestPointOnLine.x - testPointWGS.x) +
		(closestPointOnLine.y - testPointWGS.y) * (closestPointOnLine.y - testPointWGS.y) +
		(closestPointOnLine.z - testPointWGS.z) * (closestPointOnLine.z - testPointWGS.z)) / 1000.0;// [km]
}

// Altitude is set to zero.
GeometryReducer::Vector3D GeometryReducer::GetWGS84(const double& latitude, const double& longitude) const
{
	const double latRad(latitude * M_PI / 180.0);
	const double longRad(longitude * M_PI / 180.0);
	const double semiMajorA(6378137.0);// [m]
	const double semiMinorB(6356752.3142);// [m]
	Vector3D wgs84;
	wgs84.x = (semiMajorA / sqrt(pow(cos(latRad), 2) + pow(semiMinorB / semiMajorA, 2) * pow(sin(latRad), 2))) * cos(latRad) * cos(longRad);
	wgs84.y = (semiMajorA / sqrt(pow(cos(latRad), 2) + pow(semiMinorB / semiMajorA, 2) * pow(sin(latRad), 2))) * cos(latRad) * sin(longRad);
	wgs84.z = (semiMinorB / sqrt(pow(cos(latRad), 2) * pow(semiMajorA / semiMinorB, 2) + pow(sin(latRad), 2))) * sin(latRad);

	return wgs84;
}
