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
#include "throttledSection.h"
#include "googleMapsInterface.h"

// Standard C++ headers
#include <unordered_map>
#include <shared_mutex>
#include <unordered_set>

class KMLLibraryManager
{
public:
	KMLLibraryManager(const UString::String& libraryPath, const UString::String& eBirdAPIKey,
		const UString::String& mapsAPIKey, std::basic_ostream<UString::String::value_type>& log,
		const bool& cleanUpLocationNames, const int& geoJSONPrecision);

	UString::String GetKML(const UString::String& country, const UString::String& subNational1, const UString::String& subNational2);

private:
	const UString::String libraryPath;
	std::basic_ostream<UString::String::value_type>& log;
	const bool cleanUpLocationNames;
	const int geoJSONPrecision;

	static const ThrottledSection::Clock::duration mapsAccessDelta;
	mutable ThrottledSection mapsAPIRateLimiter;
	GoogleMapsInterface mapsInterface;

	typedef std::unordered_map<UString::String, UString::String> KMLMapType;
	KMLMapType kmlMemory;

	bool LoadKMLFromLibrary(const UString::String& country, const UString::String& locationId, UString::String& kml);
	bool DownloadAndStoreKML(const UString::String& country, const GlobalKMLFetcher::DetailLevel& detailLevel,
		const UString::String& locationId, UString::String& kml);

	bool NonLockingLoadKMLFromLibrary(const UString::String& country, const UString::String& locationId, UString::String& kml);

	static UString::String BuildLocationIDString(const UString::String& country, const UString::String& subNational1, const UString::String& subNational2);
	static UString::String BuildSubNationalIDString(const UString::String& subNational1, const UString::String& subNational2);

	static UString::String ExtractTagValue(const UString::String& kmlData, const std::string::size_type& offset, const UString::String& tag);
	static UString::String ExtractName(const UString::String& kmlData, const std::string::size_type& offset);
	static UString::String ExtractDescription(const UString::String& kmlData, const std::string::size_type& offset);
	static bool DescriptionIsUnwanted(const UString::String& kmlData, const std::string::size_type& offset);

	bool CountryLoadedFromLibrary(const UString::String& country) const;

	struct AdditionalArguments
	{
	};

	struct GeometryExtractionArguments : public AdditionalArguments
	{
		GeometryExtractionArguments(const UString::String& countryName,
			KMLMapType& tempMap, const int& geoJSONPrecision) : countryName(countryName),
			tempMap(tempMap), geoJSONPrecision(geoJSONPrecision) {}
		const UString::String& countryName;
		KMLMapType& tempMap;
		const int& geoJSONPrecision;
	};

	struct ParentRegionFinderArguments : public AdditionalArguments
	{
		ParentRegionFinderArguments(const UString::String& countryName, UString::String& modifiedKML,
			KMLLibraryManager& self) : countryName(countryName), modifiedKML(modifiedKML), self(self) {}
		const UString::String& countryName;
		UString::String& modifiedKML;
		KMLLibraryManager& self;
		std::string::size_type sourceTellP = 0;
	};

	struct GeometryInfo;// Forward declaration

	struct ParentGeometryExtractionArguments : public AdditionalArguments
	{
		ParentGeometryExtractionArguments(const UString::String& countryName,
			std::unordered_map<UString::String, GeometryInfo>& geometryInfo) : countryName(countryName), geometryInfo(geometryInfo) {}
		const UString::String& countryName;
		std::unordered_map<UString::String, GeometryInfo>& geometryInfo;
	};

	typedef bool(*PlacemarkFunction)(const UString::String&, const std::string::size_type&, AdditionalArguments&);

	bool ForEachPlacemarkTag(const UString::String& kmlData, PlacemarkFunction func, AdditionalArguments& args) const;
	static bool ExtractRegionGeometry(const UString::String& kmlData, const std::string::size_type& offset, AdditionalArguments& args);
	bool FixPlacemarkNames(const UString::String& kmlData, const std::string::size_type& offset, AdditionalArguments& args) const;
	static bool ExtractParentRegionGeometry(const UString::String& kmlData, const std::string::size_type& offset, AdditionalArguments& args);

	static bool ContainsMoreThanOneMatch(const UString::String& s, const UString::String& pattern);
	static UString::String CreatePlacemarkNameString(const UString::String& name);

	static void ExpandSaintAbbr(UString::String& s);
	static void ExpandSainteAbbr(UString::String& s);

	EBirdInterface ebi;
	std::unordered_map<UString::String, std::vector<EBirdInterface::RegionInfo>> subRegion1Data;// key is country name
	std::unordered_map<UString::String, std::vector<EBirdInterface::RegionInfo>> subRegion2Data;// key generated with BuildLocationIDString() (empty third argument)

	const std::vector<EBirdInterface::RegionInfo>& GetSubRegion1Data(const UString::String& countryName);
	const std::vector<EBirdInterface::RegionInfo>& GetSubRegion2Data(const UString::String& countryName, const EBirdInterface::RegionInfo& regionInfo);

	bool LookupParentRegionName(const UString::String& country, const UString::String& subregion2Name, UString::String& parentRegionName);
	bool LookupParentRegionName(const UString::String& country, const UString::String& subregion2Name, const UString::String& childKML, UString::String& parentRegionName);

	std::vector<EBirdInterface::RegionInfo> FindRegionsWithSubRegionMatchingName(const UString::String& country, const UString::String& name);

	static bool GetUserConfirmation();

	struct GeometryInfo
	{
		GeometryInfo(const UString::String& kml);
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
		static PolygonList ExtractPolygons(const UString::String& kml);
	};

	bool GetParentGeometryInfo(const UString::String& country);

	std::unordered_map<UString::String, GeometryInfo> geometryInfo;// key generated with BuildLocationIDString() (empty third argument)
	GeometryInfo GetGeometryInfoByName(const UString::String& countryName, const UString::String& parentName);
	static bool BoundingBoxWithinParentBox(const GeometryInfo::BoundingBox& parent, const GeometryInfo::BoundingBox& child);
	static bool PointIsWithinPolygons(const GeometryInfo::Point& p, const GeometryInfo& geometry);
	static bool SegmentsIntersect(const GeometryInfo::Point& segment1Point1, const GeometryInfo::Point& segment1Point2,
		const GeometryInfo::Point& segment2Point1, const GeometryInfo::Point& segment2Point2);
	static GeometryInfo::Point ChooseRobustPoint(const GeometryInfo& geometry);

	static bool ContainsOnlyWhitespace(const UString::String& s);
	static bool RegionNamesMatch(const UString::String& name1, const UString::String& name2);

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

	bool GetKMLFromMemory(const UString::String& locationId, UString::String& kml) const;
	bool NonLockingGetKMLFromMemory(const UString::String& locationId, UString::String& kml) const;

	mutable std::shared_timed_mutex mutex;
	mutable std::mutex userInputMutex;
	mutable std::mutex gMapResultMutexeBird;
	mutable std::mutex gMapResultMutexGADM;
	mutable std::mutex userAlreadyAnsweredMutex;
	mutable std::mutex mappedMutex;
	mutable std::mutex kmzWriteMutex;
	MutexUtilities::AccessManager loadManager;
	MutexUtilities::AccessManager downloadManager;
	MutexUtilities::AccessManager geometryManager;

	// Sloppy to use mutable here - needs redesign
	mutable std::map<UString::String, std::vector<GoogleMapsInterface::PlaceInfo>> eBirdNameGMapResults;
	mutable std::map<UString::String, std::vector<GoogleMapsInterface::PlaceInfo>> gadmNameGMapResults;
	mutable std::unordered_set<UString::String> userAnsweredList;
	mutable std::unordered_set<UString::String> kmlMappedList;

	static bool FileExists(const UString::String& fileName);

	bool CheckForInexactMatch(const UString::String& locationId, UString::String& kml) const;
	static UString::String ExtractCountryFromLocationId(const UString::String& id);
	static UString::String ExtractSubNational1FromLocationId(const UString::String& id);
	bool MakeCorrectionInKMZ(const UString::String& country,
		const UString::String& originalSubNationalMashUp, const UString::String& newSubNationalMashUp) const;

	static bool StringsAreSimilar(const UString::String& a, const UString::String& b, const double& threshold);
	static std::vector<UString::String> GenerateLetterPairs(const UString::String& s);
	static std::vector<UString::String> GenerateWordLetterPairs(const UString::String& s);

	static UString::String AdjustPrecision(const UString::String& kml, const int& precision);

	bool OpenKMLArchive(const UString::String& fileName, UString::String& kml) const;
};

#endif// KML_LIBRARY_MANAGER_H_
