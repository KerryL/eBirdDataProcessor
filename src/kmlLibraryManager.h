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

// Standard C++ headers
#include <unordered_map>

class KMLLibraryManager
{
public:
	KMLLibraryManager(const String& libraryPath, const String& eBirdAPIKey);

	String GetKML(const String& country, const String& subNational1, const String& subNational2);

private:
	const String libraryPath;

	typedef std::unordered_map<String, String> KMLMapType;
	std::unordered_map<String, String> kmlMemory;

	bool LoadKMLFromLibrary(const String& country);
	bool DownloadAndStoreKML(const String& country, const GlobalKMLFetcher::DetailLevel& detailLevel);

	static String BuildLocationIDString(const String& country, const String& subNational1, const String& subNational2);
	static String BuildSubNationalIDString(const String& subNational1, const String& subNational2);

	static String ExtractName(const String& kmlData, const std::string::size_type& offset);

	struct AdditionalArguments
	{
	};

	struct GeometryExtractionArguments : public AdditionalArguments
	{
		GeometryExtractionArguments(const String& countryName,
			std::unordered_map<String, String>& tempMap) : countryName(countryName), tempMap(tempMap) {}
		const String& countryName;
		std::unordered_map<String, String>& tempMap;
	};

	struct ParentRegionFinderArguments : public AdditionalArguments
	{
		ParentRegionFinderArguments(const String& countryName, String& modifiedKML,
			KMLLibraryManager& self) : countryName(countryName), modifiedKML(modifiedKML), self(self) {}
		const String& countryName;
		KMLLibraryManager& self;
		String& modifiedKML;
		std::string::size_type sourceTellP = 0;
	};

	typedef bool(*PlacemarkFunction)(const String&, const std::string::size_type&, const AdditionalArguments&);

	static bool ForEachPlacemarkTag(const String& kmlData, PlacemarkFunction func, const AdditionalArguments& args);
	static bool ExtractRegionGeometry(const String& kmlData, const std::string::size_type& offset, const AdditionalArguments& args);
	static bool FixPlacemarkNames(const String& kmlData, const std::string::size_type& offset, const AdditionalArguments& args);

	static bool ContainsMoreThanOneMatch(const String& s, const String& pattern);
	static String CreatePlacemarkNameString(const String& name);

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
			double latitude;// [deg]
			double longitude;// [deg]
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
};

#endif// KML_LIBRARY_MANAGER_H_
