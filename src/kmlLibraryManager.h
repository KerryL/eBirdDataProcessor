// File:  kmlLibraryManager.h
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

#ifndef KML_LIBRARY_MANAGER_H_
#define KML_LIBRARY_MANAGER_H_

// Local headers
#include "globalKMLFetcher.h"
#include "eBirdInterface.h"
#include "utilities/uString.h"
#include "utilities/mutexUtilities.h"

// Standard C++ headers
#include <unordered_map>
#include <shared_mutex>

class KMLLibraryManager
{
public:
	KMLLibraryManager(const String& libraryPath, const String& eBirdAPIKey);

	String GetKML(const String& country, const String& subNational1, const String& subNational2);

private:
	const String libraryPath;

	typedef std::unordered_map<String, String> KMLMapType;
	KMLMapType kmlMemory;

	bool LoadKMLFromLibrary(const String& country, const String& locationId);
	bool DownloadAndStoreKML(const String& country, const GlobalKMLFetcher::DetailLevel& detailLevel);

	static String BuildLocationIDString(const String& country, const String& subNational1, const String& subNational2);
	static String BuildSubNationalIDString(const String& subNational1, const String& subNational2);

	static String ExtractTagValue(const String& kmlData, const std::string::size_type& offset, const String& tag);
	static String ExtractName(const String& kmlData, const std::string::size_type& offset);
	static String ExtractDescription(const String& kmlData, const std::string::size_type& offset);
	static bool DescriptionIsUnwanted(const String& kmlData, const std::string::size_type& offset);

	struct AdditionalArguments
	{
	};

	struct GeometryExtractionArguments : public AdditionalArguments
	{
		GeometryExtractionArguments(const String& countryName,
			KMLMapType& tempMap) : countryName(countryName), tempMap(tempMap) {}
		const String& countryName;
		KMLMapType& tempMap;
	};

	struct ParentRegionFinderArguments : public AdditionalArguments
	{
		ParentRegionFinderArguments(const String& countryName, String& modifiedKML,
			KMLLibraryManager& self) : countryName(countryName), modifiedKML(modifiedKML), self(self) {}
		const String& countryName;
		String& modifiedKML;
		KMLLibraryManager& self;
		std::string::size_type sourceTellP = 0;
	};

	struct GeometryInfo;// Forward declaration

	struct ParentGeometryExtractionArguments : public AdditionalArguments
	{
		ParentGeometryExtractionArguments(const String& countryName,
			std::unordered_map<String, GeometryInfo>& geometryInfo) : countryName(countryName), geometryInfo(geometryInfo) {}
		const String& countryName;
		std::unordered_map<String, GeometryInfo>& geometryInfo;
	};

	typedef bool(*PlacemarkFunction)(const String&, const std::string::size_type&, AdditionalArguments&);

	static bool ForEachPlacemarkTag(const String& kmlData, PlacemarkFunction func, AdditionalArguments& args);
	static bool ExtractRegionGeometry(const String& kmlData, const std::string::size_type& offset, AdditionalArguments& args);
	static bool FixPlacemarkNames(const String& kmlData, const std::string::size_type& offset, AdditionalArguments& args);
	static bool ExtractParentRegionGeometry(const String& kmlData, const std::string::size_type& offset, AdditionalArguments& args);

	static bool ContainsMoreThanOneMatch(const String& s, const String& pattern);
	static String CreatePlacemarkNameString(const String& name);

	static void ExpandSaintAbbr(String& s);
	static void ExpandSainteAbbr(String& s);

	EBirdInterface ebi;
	std::unordered_map<String, std::vector<EBirdInterface::RegionInfo>> subRegion1Data;// key is country name
	std::unordered_map<String, std::vector<EBirdInterface::RegionInfo>> subRegion2Data;// key generated with BuildLocationIDString() (empty third argument)

	const std::vector<EBirdInterface::RegionInfo>& GetSubRegion1Data(const String& countryName);
	const std::vector<EBirdInterface::RegionInfo>& GetSubRegion2Data(const String& countryName, const EBirdInterface::RegionInfo& regionInfo);

	bool LookupParentRegionName(const String& country, const String& subregion2Name, String& parentRegionName);
	bool LookupParentRegionName(const String& country, const String& subregion2Name, const String& childKML, String& parentRegionName);

	std::vector<EBirdInterface::RegionInfo> FindRegionsWithSubRegionMatchingName(const String& country, const String& name);

	struct GeometryInfo
	{
		GeometryInfo(const String& kml);
		GeometryInfo(const GeometryInfo& g);
		GeometryInfo(GeometryInfo&& g);

		struct Point
		{
			Point() = default;
			Point(const double& longitude, const double& latitude) : longitude(longitude), latitude(latitude) {}
			
			double longitude;// [deg]
			double latitude;// [deg]
		};

		typedef std::vector<std::vector<Point>> PolygonList;

		const PolygonList polygons;

		struct BoundingBox
		{
			Point northEast;
			Point southWest;
		};
		const BoundingBox bbox;

		static BoundingBox ComputeBoundingBox(const PolygonList& polygonList);
		static PolygonList ExtractPolygons(const String& kml);
	};

	bool GetParentGeometryInfo(const String& country);

	std::unordered_map<String, GeometryInfo> geometryInfo;// key generated with BuildLocationIDString() (empty third argument)
	GeometryInfo GetGeometryInfoByName(const String& countryName, const String& parentName);
	static bool BoundingBoxWithinParentBox(const GeometryInfo::BoundingBox& parent, const GeometryInfo::BoundingBox& child);
	static bool PointIsWithinPolygons(const GeometryInfo::Point& p, const GeometryInfo& geometry);
	static bool SegmentsIntersect(const GeometryInfo::Point& segment1Point1, const GeometryInfo::Point& segment1Point2,
		const GeometryInfo::Point& segment2Point1, const GeometryInfo::Point& segment2Point2);
	static GeometryInfo::Point ChooseRobustPoint(const GeometryInfo& geometry);

	static bool ContainsOnlyWhitespace(const String& s);
	static bool RegionNamesMatch(const String& name1, const String& name2);

	class Vector2D
	{
	public:
		Vector2D() = default;
		Vector2D(const double& x, const double& y) : x(x), y(y) {}

		double x, y;

		Vector2D operator+(const Vector2D& v) const;
		Vector2D operator-(const Vector2D& v) const;
		Vector2D& operator+=(const Vector2D& v);
		Vector2D& operator-=(const Vector2D& v);

		double Cross(const Vector2D& v) const;
		double Dot(const Vector2D& v) const;
	};

	bool GetKMLFromMemory(const String& locationId, String& kml) const;

	mutable std::shared_mutex mutex;
	MutexUtilities::AccessManager loadManager;
	MutexUtilities::AccessManager downloadManager;
	MutexUtilities::AccessManager geometryManager;

	static bool FileExists(const String& fileName);
};

#endif// KML_LIBRARY_MANAGER_H_
