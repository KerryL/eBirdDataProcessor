// File:  kmlLibraryManager.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

// Local headers
#include "kmlLibraryManager.h"
#include "zipper.h"

// OS headers
#include <sys/stat.h>

// Standard C++ headers
#include <cassert>
#include <cctype>
#include <mutex>

KMLLibraryManager::KMLLibraryManager(const String& libraryPath,
	const String& eBirdAPIKey, std::basic_ostream<String::value_type>& log) : libraryPath(libraryPath), log(log), ebi(eBirdAPIKey)
{
}

String KMLLibraryManager::GetKML(const String& country, const String& subNational1, const String& subNational2)
{
	assert(!country.empty());
	assert((!subNational2.empty() && !subNational1.empty()) || subNational2.empty());
	const String locationIDString(BuildLocationIDString(country, subNational1, subNational2));
	String kml;
	if (GetKMLFromMemory(locationIDString, kml))
		return kml;

	if (LoadKMLFromLibrary(country, locationIDString))
	{
		if (GetKMLFromMemory(locationIDString, kml))
			return kml;

		log << "KML for '" << country << "' loaded, but no match for '" << locationIDString << '\'' << std::endl;
		return String();
	}

	const GlobalKMLFetcher::DetailLevel detailLevel([subNational1, subNational2]()
	{
		if (subNational2.empty())
		{
			if (subNational1.empty())
				return GlobalKMLFetcher::DetailLevel::Country;
			return GlobalKMLFetcher::DetailLevel::SubNational1;
		}

		return GlobalKMLFetcher::DetailLevel::SubNational2;
	}());

	if (DownloadAndStoreKML(country, detailLevel))
	{
		if (LoadKMLFromLibrary(country, locationIDString))
		{
			if (GetKMLFromMemory(locationIDString, kml))
				return kml;
			log << "Downloaded KML for '" << country << "', but no match for '" << locationIDString << '\'' << std::endl;
		}
	}

	return String();
}

bool KMLLibraryManager::GetKMLFromMemory(const String& locationId, String& kml) const
{
	std::shared_lock<std::shared_mutex> lock(mutex);
	auto it(kmlMemory.find(locationId));
	if (it == kmlMemory.end())
		return false;

	kml = it->second;
	return true;
}

// Load by country
bool KMLLibraryManager::LoadKMLFromLibrary(const String& country, const String& locationId)
{
	if (!loadManager.TryAccess(country))
	{
		loadManager.WaitOn(country);
		return true;// Assume other thread succeeded
	}

	MutexUtilities::AccessManager::AccessHelper helper(country, loadManager);
	if (kmlMemory.find(locationId) != kmlMemory.end())
		return true;// Another thread loaded it while we were transfering from shared to exclusive access

	log << "Attempting to load KML data from archive for '" << country << '\'' << std::endl;

	Zipper z;
	const String archiveFileName(libraryPath + country + _T(".kmz"));
	if (!z.OpenArchiveFile(archiveFileName))
	{
		log << "Failed to open '" << archiveFileName << "' for input" << std::endl;
		return false;
	}

	std::string bytes;
	if (!z.ExtractFile(0, bytes))
		log << "Failed to extract kml file from '" << archiveFileName << "':  " << z.GetErrorString() << std::endl;

	std::unordered_map<String, String> tempMap;
	GeometryExtractionArguments args(country, tempMap);
	if (!ForEachPlacemarkTag(UString::ToStringType(bytes), ExtractRegionGeometry, args))
		return false;

	std::lock_guard<std::shared_mutex> lock(mutex);
	//kmlMemory.merge(std::move(tempMap));// requires C++ 17 - not sure if there's any reason to prefer it over line below
	kmlMemory.insert(tempMap.begin(), tempMap.end());

	return true;
}

// Download by country
bool KMLLibraryManager::DownloadAndStoreKML(const String& country,
	const GlobalKMLFetcher::DetailLevel& detailLevel)
{
	if (!downloadManager.TryAccess(country))
	{
		downloadManager.WaitOn(country);
		return true;// Assume other thread succeeded
	}

	MutexUtilities::AccessManager::AccessHelper helper(country, downloadManager);
	const String kmzFileName(libraryPath + country + _T(".kmz"));
	if (FileExists(kmzFileName))
		return true;// Another thread downloaded it while we were transfering from shared to exclusive access

	log << "Attempting to download KML data for '" << country << '\'' << " at detail level " << static_cast<int>(detailLevel) << std::endl;
	GlobalKMLFetcher fetcher(log);
	std::string result;
	if (!fetcher.FetchKML(country, detailLevel, result))
	{
		log << "Failed to download KML for '" << country << '\'' << std::endl;
		return false;
	}

	Zipper z;
	if (!z.OpenArchiveBytes(result))
	{
		log << "Failed to open kmz data" << std::endl;
		return false;
	}

	std::string unzippedKML;
	if (!z.ExtractFile(0, unzippedKML))
	{
		log << "Failed to extract file from kmz archive" << std::endl;
		return false;
	}

	z.CloseArchive();

	// New (v3.6) GADM format does not require fixing
	/*if (detailLevel == GlobalKMLFetcher::DetailLevel::SubNational2)
	{
		String modifiedKML;
		ParentRegionFinderArguments args(country, modifiedKML, *this);
		const auto unzippedKMLString(UString::ToStringType(unzippedKML));
		if (!ForEachPlacemarkTag(unzippedKMLString, FixPlacemarkNames, args))
			return false;

		// Also need to write the remainder of the file (after the last placemark tag)
		modifiedKML.append(unzippedKMLString.substr(args.sourceTellP));
		unzippedKML = UString::ToNarrowString(modifiedKML);
	}*/

	if (!z.CreateArchiveFile(kmzFileName))
	{
		log << "Failed to create kmz archive" << std::endl;
		return false;
	}

	if (!z.AddFile(country + _T(".kml"), unzippedKML))
	{
		log << "Failed to add kml data to archive" << std::endl;
		return false;
	}

	return z.CloseArchive();
}

bool KMLLibraryManager::FileExists(const String& fileName)
{
	struct stat buffer;
	return stat(UString::ToNarrowString(fileName).c_str(), &buffer) == 0;
}

String KMLLibraryManager::BuildLocationIDString(const String& country,
	const String& subNational1, const String& subNational2)
{
	return country + _T(":") + BuildSubNationalIDString(subNational1, subNational2);
}

String KMLLibraryManager::BuildSubNationalIDString(const String& subNational1, const String& subNational2)
{
	if (subNational2.empty())
		return subNational1;
	return subNational1 + _T(":") + subNational2;
}

String KMLLibraryManager::ExtractName(const String& kmlData, const std::string::size_type& offset)
{
	return ExtractTagValue(kmlData, offset, _T("name"));
}

bool KMLLibraryManager::ForEachPlacemarkTag(const String& kmlData,
	PlacemarkFunction func, AdditionalArguments& args) const
{
	const String placemarkStartTag(_T("<Placemark>"));
	std::string::size_type next(0);
	while (next = kmlData.find(placemarkStartTag, next), next != std::string::npos)
	{
		const String placemarkEndTag(_T("</Placemark>"));
		const std::string::size_type placemarkEnd(kmlData.find(placemarkEndTag, next));
		if (placemarkEnd == std::string::npos)
		{
			log << "Failed to find expected placemark end tag" << std::endl;
			return false;
		}

		// List of acceptable description types is long and location dependent
		// (i.e. County, Census Area, and Ken/Do/Fu/To for Japan...), so easier
		// to check known descriptions to discard.  Algorithm must be designed
		// to handle cases where unwanted regions get through, however - this
		// is only a performance improvement.
		if (DescriptionIsUnwanted(kmlData, next))
		{
			next = placemarkEnd;
			continue;
		}

		if (!func(kmlData, next, args))
			return false;

		next = placemarkEnd;
	}

	return true;
}

String KMLLibraryManager::ExtractTagValue(const String& kmlData,
	const std::string::size_type& offset, const String& tag)
{
	const String startTag(_T("<") + tag + _T(">"));
	const auto start(kmlData.find(startTag, offset));
	if (start == std::string::npos)
		return String();

	const String endTag([&tag]()
	{
		const auto space(tag.find(_T(" ")));
		if (space == std::string::npos)
			return _T("</") + tag + _T(">");
		return _T("</") + tag.substr(0, space) + _T(">");
	}());
	const auto end(kmlData.find(endTag, start));
	if (end == std::string::npos)
		return String();

	return kmlData.substr(start + startTag.length(), end - start - startTag.length());
}

String KMLLibraryManager::ExtractDescription(const String& kmlData, const std::string::size_type& offset)
{
	return ExtractTagValue(kmlData, offset, _T("description"));
}

bool KMLLibraryManager::DescriptionIsUnwanted(const String& kmlData, const std::string::size_type& offset)
{
	const String bodyOfWaterDescription(_T("<![CDATA[Water body]]>"));
	const String description(ExtractDescription(kmlData, offset));
	return description.compare(bodyOfWaterDescription) == 0;
}

bool KMLLibraryManager::ExtractRegionGeometry(const String& kmlData,
	const std::string::size_type& offset, AdditionalArguments& args)
{
	// NOTE:  Possibly need to search for <Polygon> tag if <MultiGeometry> is not found.
	// I think <MultiGeometry> is only necessary if there are multiple polygons, although
	// gadm.org seems to wrap all polygon tags in multigeometry tags, so for now we'll
	// leave this.
	const String multiGeometryStartTag(_T("<MultiGeometry>"));
	const String multiGeometryEndTag(_T("</MultiGeometry>"));
	String geometryEndTag(multiGeometryEndTag);
	std::string::size_type geometryStart(kmlData.find(multiGeometryStartTag, offset));
	if (geometryStart == std::string::npos)
	{
		const String geometryStartTag(_T("<Polygon>"));
		geometryEndTag = _T("</Polygon>");
		geometryStart = kmlData.find(geometryStartTag, offset);
		if (geometryStart == std::string::npos)
		{
			Cerr << "Failed to find geometry start tag\n";
			return false;
		}
	}

	
	const std::string::size_type geometryEnd(kmlData.find(geometryEndTag, geometryStart));
	if (geometryEnd == std::string::npos)
	{
		Cerr << "Failed to find geometry end tag\n";
		return false;
	}

	// Some historical KML library data is in GADM 2.8 format - new format is GADM 3.6
	// Difference is primarily the way placemark names are stored.  We try the 3.6 way first, and if it fails we try the 2.8 way.
	String name;
	const String nameSR1(ExtractTagValue(kmlData, offset, _T("SimpleData name=\"NAME_1\"")));
	const String nameSR2(ExtractTagValue(kmlData, offset, _T("SimpleData name=\"NAME_2\"")));
	if (nameSR1.empty())
		name = ExtractName(kmlData, offset);
	else if (nameSR2.empty())
		name = nameSR1;
	else
		name = nameSR1 + _T(":") + nameSR2;

	if (name.empty())
	{
		Cerr << "Failed to extract placemark name from KML data\n";
		return false;
	}

	auto& geometryArgs(static_cast<GeometryExtractionArguments&>(args));
	geometryArgs.tempMap[geometryArgs.countryName + _T(":") + name] =
		kmlData.substr(geometryStart, geometryEnd - geometryStart + geometryEndTag.length());
	return true;
}

bool KMLLibraryManager::FixPlacemarkNames(const String& kmlData,
	const std::string::size_type& offset, AdditionalArguments& args) const
{
	const String name(ExtractName(kmlData, offset));
	const String placemarkNameString(CreatePlacemarkNameString(name));

	auto& placemarkArgs(static_cast<ParentRegionFinderArguments&>(args));

	String parentRegionName;
	if (ContainsMoreThanOneMatch(kmlData, placemarkNameString))
	{
		const auto placemarkEnd(kmlData.find(_T("</Placemark>"), offset));
		if (placemarkEnd == std::string::npos)
		{
			log << "Failed to match expected end of placemark" << std::endl;
			return false;
		}

		if (!placemarkArgs.self.LookupParentRegionName(placemarkArgs.countryName,
			name, kmlData.substr(offset, placemarkEnd - offset), parentRegionName))
		{
			log << "Failed to find parent region name (geometry method) for " << name << '\'' << std::endl;
			return true;// Non-fatal - could be an area for which eBird does not have a region defined, or eBird combines what is actually multiple administrative areas
		}
	}
	else
	{
		if (!placemarkArgs.self.LookupParentRegionName(placemarkArgs.countryName, name, parentRegionName))
		{
			log << "Failed to find parent region name (unique name method) for " << name << std::endl;
			return true;// Non-fatal - could be an area for which eBird does not have a region defined, or eBird combines what is actually multiple administrative areas
		}
	}

	const auto startOfNameTag(kmlData.find(_T("<name>"), offset));// Required to ensure new insert occurs at same place in file as original tag

	const String locationID(placemarkArgs.self.BuildSubNationalIDString(parentRegionName, name));
	placemarkArgs.modifiedKML.append(kmlData.substr(placemarkArgs.sourceTellP, startOfNameTag - placemarkArgs.sourceTellP) + CreatePlacemarkNameString(locationID));

	const String endNameTag(_T("</name>"));
	placemarkArgs.sourceTellP = kmlData.find(endNameTag, startOfNameTag);
	if (placemarkArgs.sourceTellP == std::string::npos)
	{
		log << "Failed to find expected end name tag" << std::endl;
		return false;
	}
	placemarkArgs.sourceTellP += endNameTag.length();

	return true;
}

bool KMLLibraryManager::ExtractParentRegionGeometry(const String& kmlData,
	const std::string::size_type& offset, AdditionalArguments& args)
{
	const auto placemarkEnd(kmlData.find(_T("</Placemark>"), offset));
	if (placemarkEnd == std::string::npos)
	{
		Cerr << "Failed to match expected end of placemark\n";
		return false;
	}

	const String name(ExtractName(kmlData, offset));
	auto& placemarkArgs(static_cast<ParentGeometryExtractionArguments&>(args));
	placemarkArgs.geometryInfo.insert(std::make_pair(BuildLocationIDString(
		placemarkArgs.countryName, name, String()), GeometryInfo(kmlData.substr(offset, placemarkEnd - offset))));
	return true;
}

String KMLLibraryManager::CreatePlacemarkNameString(const String& name)
{
	return _T("<name>") + name + _T("</name>");
}

bool KMLLibraryManager::ContainsMoreThanOneMatch(const String& s, const String& pattern)
{
	auto location(s.find(pattern));
	if (location == std::string::npos)
		return false;

	return s.find(pattern, location + 1) != std::string::npos;
}

bool KMLLibraryManager::LookupParentRegionName(const String& country, const String& subregion2Name, String& parentRegionName)
{
	auto sr1List(GetSubRegion1Data(country));
	if (sr1List.empty())
		return false;

	for (const auto& sr1 : sr1List)
	{
		const auto sr2List(GetSubRegion2Data(country, sr1));
		for (const auto& sr2 : sr2List)
		{
			if (RegionNamesMatch(sr2.name, subregion2Name))
			{
				parentRegionName = sr1.name;
				return true;
			}
		}
	}

	return false;
}

void KMLLibraryManager::ExpandSaintAbbr(String& s)
{
	// Assumes this abbreviation occurs at most once
	const String saintAbbr(_T("st."));
	auto start(s.find(saintAbbr));
	if (start == std::string::npos)
		return;

	s.replace(start, saintAbbr.length(), _T("saint"));
}

void KMLLibraryManager::ExpandSainteAbbr(String& s)
{
	// Assumes this abbreviation occurs at most once
	const String saintAbbr(_T("ste."));
	auto start(s.find(saintAbbr));
	if (start == std::string::npos)
		return;

	s.replace(start, saintAbbr.length(), _T("sainte"));
}

bool KMLLibraryManager::RegionNamesMatch(const String& name1, const String& name2)
{
	String lower1(name1), lower2(name2);
	std::transform(lower1.begin(), lower1.end(), lower1.begin(), ::tolower);
	std::transform(lower2.begin(), lower2.end(), lower2.begin(), ::tolower);

	ExpandSaintAbbr(lower1);
	ExpandSaintAbbr(lower2);

	ExpandSainteAbbr(lower1);
	ExpandSainteAbbr(lower2);

	lower1.erase(std::remove_if(lower1.begin(), lower1.end(), [](const Char& c)
	{
		return !std::isalnum(c);
	}), lower1.end());
	lower2.erase(std::remove_if(lower2.begin(), lower2.end(), [](const Char& c)
	{
		return !std::isalnum(c);
	}), lower2.end());

	return lower1.compare(lower2) == 0;
}

const std::vector<EBirdInterface::RegionInfo>& KMLLibraryManager::GetSubRegion1Data(const String& countryName)
{
	std::shared_lock<std::shared_mutex> sharedLock(mutex);
	auto it(subRegion1Data.find(countryName));
	if (it == subRegion1Data.end())
	{
		MutexUtilities::AccessUpgrader exclusiveLock(sharedLock);
		it = subRegion1Data.find(countryName);
		if (it == subRegion1Data.end())
			return subRegion1Data[countryName] = ebi.GetSubRegions(ebi.GetCountryCode(countryName), EBirdInterface::RegionType::SubNational1);
	}

	return it->second;
}

const std::vector<EBirdInterface::RegionInfo>& KMLLibraryManager::GetSubRegion2Data(const String& countryName, const EBirdInterface::RegionInfo& regionInfo)
{
	const String locationID(BuildLocationIDString(countryName, regionInfo.name, String()));
	std::shared_lock<std::shared_mutex> sharedLock(mutex);
	auto it(subRegion2Data.find(locationID));
	if (it == subRegion2Data.end())
	{
		MutexUtilities::AccessUpgrader exclusiveLock(sharedLock);
		it = subRegion2Data.find(countryName);
		if (it == subRegion2Data.end())
			return subRegion2Data[locationID] = ebi.GetSubRegions(regionInfo.code, EBirdInterface::RegionType::SubNational2);
	}

	return it->second;
}

bool KMLLibraryManager::LookupParentRegionName(const String& country,
	const String& subregion2Name, const String& childKML, String& parentRegionName)
{
	auto parentCandidates(FindRegionsWithSubRegionMatchingName(country, subregion2Name));
	const GeometryInfo childInfo(childKML);
	parentCandidates.erase(std::remove_if(parentCandidates.begin(), parentCandidates.end(), [&country, &childInfo, this](const EBirdInterface::RegionInfo& region)
	{
		return !BoundingBoxWithinParentBox(GetGeometryInfoByName(country, region.name).bbox, childInfo.bbox);
	}), parentCandidates.end());

	if (parentCandidates.empty())
		return false;
	if (parentCandidates.size() == 1)
	{
		parentRegionName = parentCandidates.front().name;
		return true;
	}

	for (const auto& candidate : parentCandidates)
	{
		if (PointIsWithinPolygons(ChooseRobustPoint(childInfo), GetGeometryInfoByName(country, candidate.name)))
		{
			parentRegionName = candidate.name;
			return true;
		}
	}

	return false;
}

KMLLibraryManager::GeometryInfo::Point KMLLibraryManager::ChooseRobustPoint(const GeometryInfo& geometry)
{
	// First, try a point in the middle of the largest polygon
	const std::vector<GeometryInfo::Point>* largestPolygon(&geometry.polygons.front());
	for (const auto& polygon : geometry.polygons)
	{
		if (polygon.size() > largestPolygon->size())
			largestPolygon = &polygon;
	}

	double sumLong(0.0), sumLat(0.0);
	for (const auto& p : *largestPolygon)
	{
		sumLong += p.longitude;
		sumLat += p.latitude;
	}

	GeometryInfo::Point centerPoint(sumLong / largestPolygon->size(), sumLat / largestPolygon->size());

	// Double check, to ensure we didn't pick a point in the middle of a hole
	if (PointIsWithinPolygons(centerPoint, geometry))
		return centerPoint;

	// Backup plan - use three consecutive edge points to find a new point, then verify that it's within the polygon
	unsigned int i;
	for (i = 2; i < largestPolygon->size(); ++i)
	{
		sumLong = (*largestPolygon)[i - 2].longitude + (*largestPolygon)[i - 1].longitude + (*largestPolygon)[i].longitude;
		sumLat = (*largestPolygon)[i - 2].latitude + (*largestPolygon)[i - 1].latitude + (*largestPolygon)[i].latitude;
		GeometryInfo::Point pointNearEdge(sumLong / 3.0, sumLat / 3.0);

		if (PointIsWithinPolygons(pointNearEdge, geometry))
			return pointNearEdge;
	}

	// Last resort - choose an arbitrary boundary point
	return largestPolygon->front();
}

// Implements ray-casting algorithm
bool KMLLibraryManager::PointIsWithinPolygons(const GeometryInfo::Point& p, const GeometryInfo& geometry)
{
	GeometryInfo::Point outsidePoint(geometry.bbox.northEast);
	outsidePoint.longitude += 1.0;// 1 deg is a fairly large step
	unsigned int intersectionCount(0);
	for (const auto& polygon : geometry.polygons)
	{
		unsigned int i;
		for (i = 1; i < polygon.size(); ++i)
		{
			if (SegmentsIntersect(p, outsidePoint, polygon[i], polygon[i - 1]))
				++intersectionCount;
		}
	}

	return intersectionCount % 2 == 1;
}

// Adapted from:  https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool KMLLibraryManager::SegmentsIntersect(const GeometryInfo::Point& segment1Point1,
	const GeometryInfo::Point& segment1Point2, const GeometryInfo::Point& segment2Point1,
	const GeometryInfo::Point& segment2Point2)
{
	Vector2D p11(segment1Point1.longitude, segment1Point1.latitude);
	Vector2D p12(segment1Point2.longitude, segment1Point2.latitude);
	Vector2D p21(segment2Point1.longitude, segment2Point1.latitude);
	Vector2D p22(segment2Point2.longitude, segment2Point2.latitude);

	Vector2D direction1(p12 - p11);
	Vector2D direction2(p22 - p21);
	Vector2D p21ToP11(p21 - p11);

	const auto directionCross(direction1.Cross(direction2));
	const auto segmentToSegmentCross(p21ToP11.Cross(direction1));
	const double t(p21ToP11.Cross(direction2) / direction1.Cross(direction2));
	const double u(p21ToP11.Cross(direction1) / direction1.Cross(direction2));

	if (directionCross == 0 && segmentToSegmentCross == 0)// segments are colinear
	{
		// If segments overlap, consider them to intersect
		const double t0(p21ToP11.Dot(direction1) / direction1.Dot(direction1));
		const double t1(t0 + direction2.Dot(direction1) / direction1.Dot(direction1));
		if ((t0 >= 0.0 && t0 <= 1.0) ||
			(t1 >= 0.0 && t1 <= 1.0))
			return true;
		else if ((t0 > 1.0 && t1 < 0.0) ||
			(t1 > 1.0 && t0 < 0.0))
			return true;
		//return fabs(t0 - t1) < 1.0;// TODO:  Work through this case to ensure understanding and correctness
	}
	else if (directionCross == 0)// segments are parallel
		return false;
	else if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0)
		return true;

	return false;// Not parallel, but non-intersecting
}

KMLLibraryManager::GeometryInfo KMLLibraryManager::GetGeometryInfoByName(const String& countryName, const String& parentName)
{
	const String indexString(BuildLocationIDString(countryName, parentName, String()));
	std::shared_lock<std::shared_mutex> lock(mutex);
	auto it(geometryInfo.find(indexString));
	if (it == geometryInfo.end())
	{
		// TODO:  Concurrency could be improved if we used AccessManager here, but it's a little trickier than pattern used in other places.
		MutexUtilities::AccessUpgrader exclusiveLock(lock);
		it = geometryInfo.find(indexString);
		if (it == geometryInfo.end())
		{
			if (!GetParentGeometryInfo(countryName))
				return GeometryInfo(String());

			return geometryInfo.find(indexString)->second;
		}
	}

	return it->second;
}

std::vector<EBirdInterface::RegionInfo> KMLLibraryManager::FindRegionsWithSubRegionMatchingName(const String& country, const String& name)
{
	auto subRegion1List(GetSubRegion1Data(country));
	subRegion1List.erase(std::remove_if(subRegion1List.begin(), subRegion1List.end(), [&country, &name, this](const EBirdInterface::RegionInfo& regionInfo)
	{
		auto subRegion2List(GetSubRegion2Data(country, regionInfo));
		for (const auto& r : subRegion2List)
		{
			if (RegionNamesMatch(r.name, name))
				return false;
		}

		return true;
	}), subRegion1List.end());
	return subRegion1List;
}

bool KMLLibraryManager::BoundingBoxWithinParentBox(const GeometryInfo::BoundingBox& parent, const GeometryInfo::BoundingBox& child)
{
	const double epsilon(0.02);// [deg] tolerance (makes parent size this much larger)
	if (child.northEast.latitude > parent.northEast.latitude + epsilon || child.southWest.latitude < parent.southWest.latitude - epsilon)
		return false;

	// TODO:  The poles require even more special handling - they won't have any points greater
	// than something less than 90 deg latitude, but do contain points at 90 deg, for example.

	// Due to rollover at +/- 180, special handling is required
	GeometryInfo::BoundingBox p(parent);
	GeometryInfo::BoundingBox c(child);
	const double longitudeOffset(500.0);
	p.northEast.longitude += longitudeOffset;
	p.southWest.longitude += longitudeOffset;
	c.northEast.longitude += longitudeOffset;
	c.southWest.longitude += longitudeOffset;

	if (c.northEast.longitude > p.northEast.longitude + epsilon || c.southWest.longitude < p.southWest.longitude - epsilon)
		return false;

	return true;
}

bool KMLLibraryManager::GetParentGeometryInfo(const String& country)
{
	GlobalKMLFetcher fetcher(log);
	std::string result;
	if (!fetcher.FetchKML(country, GlobalKMLFetcher::DetailLevel::SubNational1, result))
		return false;

	Zipper z;
	if (!z.OpenArchiveBytes(result))
	{
		Cerr << "Failed to open kmz data\n";
		return false;
	}

	std::string unzippedKML;
	if (!z.ExtractFile(0, unzippedKML))
	{
		Cerr << "Failed to extract file from kmz archive\n";
		return false;
	}

	z.CloseArchive();

	ParentGeometryExtractionArguments args(country, geometryInfo);
	return ForEachPlacemarkTag(UString::ToStringType(unzippedKML), ExtractParentRegionGeometry, args);
}

bool KMLLibraryManager::ContainsOnlyWhitespace(const String& s)
{
	for (const auto& c : s)
	{
		if (!std::isspace(c))
			return false;
	}

	return true;
}

KMLLibraryManager::GeometryInfo::GeometryInfo(const String& kml) : polygons(ExtractPolygons(kml)), bbox(ComputeBoundingBox(polygons))
{
}

KMLLibraryManager::GeometryInfo::GeometryInfo(const GeometryInfo& g) : polygons(g.polygons), bbox(g.bbox)
{
}

KMLLibraryManager::GeometryInfo::GeometryInfo(GeometryInfo&& g) : polygons(std::move(g.polygons)), bbox(std::move(g.bbox))
{
}

KMLLibraryManager::GeometryInfo::BoundingBox KMLLibraryManager::GeometryInfo::ComputeBoundingBox(const PolygonList& polygonList)
{
	const double longitudeOffset(500.0);// used to handle case of geometry overlapping the international date line
	BoundingBox bb;
	bb.northEast.latitude = -90.0;
	bb.northEast.longitude = -180.0 + longitudeOffset;
	bb.southWest.latitude = 90.0;
	bb.southWest.longitude = 180.0 + longitudeOffset;

	for (const auto& polygon : polygonList)
	{
		for (const auto& point : polygon)
		{
			if (point.latitude > bb.northEast.latitude)
				bb.northEast.latitude = point.latitude;
			if (point.latitude < bb.southWest.latitude)
				bb.southWest.latitude = point.latitude;
			if (point.longitude + longitudeOffset > bb.northEast.longitude)
				bb.northEast.longitude = point.longitude + longitudeOffset;
			if (point.longitude + longitudeOffset < bb.southWest.longitude)
				bb.southWest.longitude = point.longitude + longitudeOffset;
		}
	}

	bb.northEast.longitude -= longitudeOffset;
	bb.southWest.longitude -= longitudeOffset;

	return bb;
}

KMLLibraryManager::GeometryInfo::PolygonList KMLLibraryManager::GeometryInfo::ExtractPolygons(const String& kml)
{
	// Assuming that it's not necessary to check for <outerBoundaryIs> and <LinearRing> tags

	PolygonList polygons;
	const String coordinatesStartTag(_T("<coordinates>"));
	std::string::size_type startIndex(0);
	while (startIndex = kml.find(coordinatesStartTag, startIndex), startIndex != std::string::npos)
	{
		const String coordinatesEndTag(_T("</coordinates>"));
		const auto endIndex(kml.find(coordinatesEndTag, startIndex));
		if (endIndex == std::string::npos)
			break;

		IStringStream ss(kml.substr(startIndex + coordinatesStartTag.length(), endIndex - startIndex - coordinatesStartTag.length()));
		String line;
		std::vector<Point> polygon;
		while (std::getline(ss, line))
		{
			if (ContainsOnlyWhitespace(line))
				continue;

			IStringStream lineSS(line);
			Point p;
			if ((lineSS >> p.longitude).fail())
			{
				Cerr << "Failed to parse longitude value\n";
				return PolygonList();
			}

			lineSS.ignore();
			if ((lineSS >> p.latitude).fail())
			{
				Cerr << "Failed to parse latitude value\n";
				return PolygonList();
			}

			polygon.push_back(p);
		}

		polygons.push_back(polygon);
		startIndex = endIndex;
	}

	return polygons;
}

KMLLibraryManager::Vector2D KMLLibraryManager::Vector2D::operator+(const Vector2D& v) const
{
	Vector2D ret(*this);
	ret += v;
	return ret;
}

KMLLibraryManager::Vector2D KMLLibraryManager::Vector2D::operator-(const Vector2D& v) const
{
	Vector2D ret(*this);
	ret -= v;
	return ret;
}

KMLLibraryManager::Vector2D& KMLLibraryManager::Vector2D::operator+=(const Vector2D& v)
{
	x += v.x;
	y += v.y;

	return *this;
}

KMLLibraryManager::Vector2D& KMLLibraryManager::Vector2D::operator-=(const Vector2D& v)
{
	x -= v.x;
	y -= v.y;

	return *this;
}

double KMLLibraryManager::Vector2D::Cross(const Vector2D& v) const
{
	return x * v.y - y * v.x;
}

double KMLLibraryManager::Vector2D::Dot(const Vector2D& v) const
{
	return x * v.x + y * v.y;
}
