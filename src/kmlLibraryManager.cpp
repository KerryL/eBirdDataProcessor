// File:  kmlLibraryManager.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

// Local headers
#include "kmlLibraryManager.h"
#include "zipper.h"

// Standard C++ headers
#include <cassert>
#include <cctype>

KMLLibraryManager::KMLLibraryManager(const String& libraryPath,
	const String& eBirdAPIKey) : libraryPath(libraryPath), ebi(eBirdAPIKey)
{
}

String KMLLibraryManager::GetKML(const String& country, const String& subNational1, const String& subNational2)
{
	// Check for kml already in memory
	const String locationIDString(BuildLocationIDString(country, subNational1, subNational2));
	auto it(kmlMemory.find(locationIDString));
	if (it != kmlMemory.end())
		return it->second;

	if (LoadKMLFromLibrary(country))
	{
		it = kmlMemory.find(locationIDString);
		if (it != kmlMemory.end())
			return it->second;
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
		if (LoadKMLFromLibrary(country))
		{
			it = kmlMemory.find(locationIDString);
			if (it != kmlMemory.end())
				return it->second;
		}
	}

	return String();
}

// Load by country
bool KMLLibraryManager::LoadKMLFromLibrary(const String& country)
{
	Zipper z;
	const String archiveFileName(country + _T(".kmz"));
	if (!z.OpenArchiveFile(archiveFileName))
	{
		Cerr << "Failed to open '" << archiveFileName << "' for input\n";
		return false;
	}

	std::string bytes;
	if (!z.ExtractFile(0, bytes))
		Cerr << "Failed to extract kml file from '" << archiveFileName << "':  " << z.GetErrorString() << '\n';

	std::unordered_map<String, String> tempMap;
	GeometryExtractionArguments args(country, tempMap);
	if (!ForEachPlacemarkTag(UString::ToStringType(bytes), ExtractRegionGeometry, args))
		return false;

	//kmlMemory.merge(std::move(tempMap));// requires C++ 17 - not sure if there's any reason to prefer it over line below
	kmlMemory.insert(tempMap.begin(), tempMap.end());

	return true;
}

// Download by country
bool KMLLibraryManager::DownloadAndStoreKML(const String& country,
	const GlobalKMLFetcher::DetailLevel& detailLevel)
{
	GlobalKMLFetcher fetcher;
	std::string result;
	if (!fetcher.FetchKML(country, detailLevel, result))
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

	if (detailLevel == GlobalKMLFetcher::DetailLevel::SubNational2)
	{
		String modifiedKML;
		ParentRegionFinderArguments args(country, modifiedKML, *this);
		if (!ForEachPlacemarkTag(UString::ToStringType(unzippedKML), FixPlacemarkNames, args))
			return false;
		unzippedKML = UString::ToNarrowString(modifiedKML);
	}

	std::string zippedModifiedKML;
	if (!z.CreateArchiveFile(_T("test")))
	{
		Cerr << "Failed to create kmz archive\n";
		return false;
	}

	if (!z.AddFile(country + _T(".kml"), unzippedKML))
	{
		Cerr << "Failed to add kml data to archive\n";
		return false;
	}

	return z.CloseArchive();
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
	const String nameStartTag(_T("<name>"));
	const String nameEndTag(_T("</name>"));
	const auto nameStart(kmlData.find(nameStartTag, offset));
	if (nameStart == std::string::npos)
		return String();

	const auto nameEnd(kmlData.find(nameEndTag, nameStart));
	if (nameEnd == std::string::npos)
		return String();

	return kmlData.substr(nameStart + nameStartTag.length(), nameEnd - nameStart - nameStartTag.length());
}

bool KMLLibraryManager::ForEachPlacemarkTag(const String& kmlData,
	PlacemarkFunction func, const AdditionalArguments& args)
{
	const String placemarkStartTag(_T("<Placemark>"));
	std::string::size_type next(0);
	while (next = kmlData.find(placemarkStartTag, next), next != std::string::npos)
	{
		const String placemarkEndTag(_T("</Placemark>"));
		const std::string::size_type placemarkEnd(kmlData.find(placemarkEndTag, next));
		if (placemarkEnd == std::string::npos)
		{
			Cerr << "Failed to find expected placemark end tag\n";
			return false;
		}

		// TODO:  Verify that description is NOT:
		// <description><![CDATA[Water body]]></description>
		//
		// Should be one of (certainly many more as well):
		// <![CDATA[County]]>
		// <![CDATA[Borough]]>
		// <![CDATA[Census Area]]>
		// <![CDATA[Municipality]]>
		// <![CDATA[City And Borough]]>
		// <![CDATA[City And County]]>
		// <![CDATA[District]]>
		// <![CDATA[Parish]]>
		// <![CDATA[Independent City]]>
		// <![CDATA[State]]>
		// <![CDATA[Federal District]]>
		// <![CDATA[Ken]]>
		// <![CDATA[Do]]>
		// <![CDATA[Fu]]>
		// <![CDATA[To]]>

		if (!func(kmlData, next, args))
			return false;

		next = placemarkEnd;
	}

	return true;
}

bool KMLLibraryManager::ExtractRegionGeometry(const String& kmlData,
	const std::string::size_type& offset, const AdditionalArguments& args)
{
	// NOTE:  Possibly need to search for <Polygon> tag if <MultiGeometry> is not found.
	// I think <MultiGeometry> is only necessary if there are multiple polygons, although
	// gadm.org seems to wrap all polygon tags in multigeometry tags, so for now we'll
	// leave this.
	const String geometryStartTag(_T("<MultiGeometry>"));
	const std::string::size_type geometryStart(kmlData.find(geometryStartTag, offset));
	if (geometryStart == std::string::npos)
	{
		Cerr << "Failed to find geometry start tag\n";
		return false;
	}

	const String geometryEndTag(_T("</MultiGeometry>"));
	const std::string::size_type geometryEnd(kmlData.find(geometryEndTag, geometryStart));
	if (geometryEnd == std::string::npos)
	{
		Cerr << "Failed to find geometry end tag\n";
		return false;
	}

	const String name(ExtractName(kmlData, offset));
	if (name.empty())
	{
		Cerr << "Failed to extract placemark name from KML data\n";
		return false;
	}

	auto geometryArgs(static_cast<const GeometryExtractionArguments&>(args));
	geometryArgs.tempMap[geometryArgs.countryName + _T(":") + name] =
		kmlData.substr(geometryStart, geometryEnd - geometryStart + geometryEndTag.length());
	return true;
}

bool KMLLibraryManager::FixPlacemarkNames(const String& kmlData,
	const std::string::size_type& offset, const AdditionalArguments& args)
{
	const String name(ExtractName(kmlData, offset));
	const String placemarkNameString(CreatePlacemarkNameString(name));

	auto placemarkArgs(static_cast<const ParentRegionFinderArguments&>(args));

	String parentRegionName;
	if (ContainsMoreThanOneMatch(kmlData, placemarkNameString))
	{
		const auto placemarkEnd(kmlData.find(_T("</Placemark>"), offset));
		if (placemarkEnd == std::string::npos)
		{
			Cerr << "Failed to match expected end of placemark\n";
			return false;
		}

		if (!placemarkArgs.self.LookupParentRegionName(placemarkArgs.countryName,
			name, kmlData.substr(offset, placemarkEnd - offset), parentRegionName))
		{
			Cerr << "Failed to find parent region name (geometry method)\n";
			return false;
		}
	}
	else
	{
		if (!placemarkArgs.self.LookupParentRegionName(placemarkArgs.countryName, name, parentRegionName))
		{
			Cerr << "Failed to find parent region name (unique name method)\n";
			return false;
		}
	}

	const String locationID(placemarkArgs.self.BuildSubNationalIDString(parentRegionName, name));
	placemarkArgs.modifiedKML.append(kmlData.substr(placemarkArgs.sourceTellP, offset - placemarkArgs.sourceTellP) + CreatePlacemarkNameString(locationID));

	const String endNameTag(_T("</name>"));
	placemarkArgs.sourceTellP = kmlData.find(endNameTag, offset);
	if (placemarkArgs.sourceTellP == std::string::npos)
	{
		Cerr << "Failed to find expected end name tag\n";
		return false;
	}
	placemarkArgs.sourceTellP += endNameTag.length();

	return true;
}

bool KMLLibraryManager::ExtractParentRegionGeometry(const String& kmlData, const std::string::size_type& offset, const AdditionalArguments& args)
{
	const auto placemarkEnd(kmlData.find(_T("</Placemark>"), offset));
	if (placemarkEnd == std::string::npos)
	{
		Cerr << "Failed to match expected end of placemark\n";
		return false;
	}

	const String name(ExtractName(kmlData, offset));
	auto placemarkArgs(static_cast<const ParentGeometryExtractionArguments&>(args));
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

bool KMLLibraryManager::RegionNamesMatch(const String& name1, const String& name2)
{
	String lower1(name1), lower2(name2);
	std::transform(lower1.begin(), lower1.end(), lower1.begin(), ::tolower);
	std::transform(lower2.begin(), lower2.end(), lower2.begin(), ::tolower);

	lower1.erase(std::remove_if(lower1.begin(), lower1.end(), [](const Char& c)
	{
		return !std::isalnum(c);
	}), lower1.end());
	lower2.erase(std::remove_if(lower2.begin(), lower2.end(), [](const Char& c)
	{
		return !std::isalnum(c);
	}), lower1.end());

	return lower1.compare(lower2) == 0;
}

const std::vector<EBirdInterface::RegionInfo>& KMLLibraryManager::GetSubRegion1Data(const String& countryName)
{
	auto it(subRegion1Data.find(countryName));
	if (it == subRegion1Data.end())
		return subRegion1Data[countryName] = ebi.GetSubRegions(ebi.GetCountryCode(countryName), EBirdInterface::RegionType::SubNational1);

	return it->second;
}

const std::vector<EBirdInterface::RegionInfo>& KMLLibraryManager::GetSubRegion2Data(const String& countryName, const EBirdInterface::RegionInfo& regionInfo)
{
	const String locationID(BuildLocationIDString(countryName, regionInfo.name, String()));
	auto it(subRegion2Data.find(locationID));
	if (it == subRegion2Data.end())
		return subRegion2Data[locationID] = ebi.GetSubRegions(regionInfo.code, EBirdInterface::RegionType::SubNational2);

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

	assert(!parentCandidates.empty());

	if (parentCandidates.size() == 1)
	{
		parentRegionName = parentCandidates.front().name;
		return true;
	}

	assert(false && "Need to implement ray-casting for full solution");
	// TODO:  Use ray-casting to check if a point (any random point from within child kml?) is within parent

	return false;
}

KMLLibraryManager::GeometryInfo KMLLibraryManager::GetGeometryInfoByName(const String& countryName, const String& parentName)
{
	const String indexString(BuildLocationIDString(countryName, parentName, String()));
	auto it(geometryInfo.find(indexString));
	if (it == geometryInfo.end())
	{
		if (!GetParentGeometryInfo(countryName))
			return GeometryInfo(String());

		return geometryInfo.find(indexString)->second;
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
	const double rollover(360.0);

	// Make all longitudes positive
	if (p.southWest.longitude < 0.0)
		p.southWest.longitude += rollover;
	if (p.northEast.longitude < 0.0)
		p.northEast.longitude += rollover;
	if (c.southWest.longitude < 0.0)
		c.southWest.longitude += rollover;
	if (c.northEast.longitude < 0.0)
		c.northEast.longitude += rollover;

	if (c.northEast.longitude > p.northEast.longitude  + epsilon || c.southWest.longitude < p.southWest.longitude - epsilon)
		return false;

	return true;
}

bool KMLLibraryManager::GetParentGeometryInfo(const String& country)
{
	GlobalKMLFetcher fetcher;
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
	BoundingBox bb;
	bb.northEast.latitude = -90.0;
	bb.northEast.longitude = -180.0;
	bb.southWest.latitude = 90.0;
	bb.southWest.longitude = 180.0;

	for (const auto& polygon : polygonList)
	{
		for (const auto& point : polygon)
		{
			if (point.latitude > bb.northEast.latitude)
				bb.northEast.latitude = point.latitude;
			if (point.latitude < bb.southWest.latitude)
				bb.southWest.latitude = point.latitude;
			if (point.longitude > bb.northEast.longitude)
				bb.northEast.longitude = point.longitude;
			if (point.longitude < bb.southWest.longitude)
				bb.southWest.longitude = point.longitude;
		}
	}

	// This method assumes that we'll never want a parent regions are relatively small compared to the Earh.
	// For example, we assume that coordinates spanning from -170 to +170 deg should be interpreted as a 20 deg
	// wide box, not a 340 deg wide box.
	if (fabs(bb.northEast.longitude - bb.southWest.longitude - 360.0) < fabs(bb.northEast.longitude - bb.southWest.longitude))
		std::swap(bb.northEast.longitude, bb.southWest.longitude);

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
