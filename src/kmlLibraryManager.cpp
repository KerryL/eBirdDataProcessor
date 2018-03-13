// File:  kmlLibraryManager.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

// Local headers
#include "kmlLibraryManager.h"
#include "zipper.h"

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
	fetcher.FetchKML(country, detailLevel, result);

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
		if (placemarkArgs.self.LookupParentRegionName(parentRegionName))
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
	if (subregion1Data.find(country) == subregion1Data.end())
		subregion1Data[country] = ebi.GetSubRegions(ebi.GetCountryCode(country), EBirdInterface::RegionType::SubNational1);

	if (subregion1Data[country].size() == 0)
		return false;

	const auto sr1List(subregion1Data[country]);
	for (const auto& sr1 : sr1List)
	{
		const String indexString(BuildLocationIDString(country, sr1.name, String()));
		if (subregion2Data.find(indexString) == subregion2Data.end())
			subregion2Data[indexString] = ebi.GetSubRegions(sr1.code, EBirdInterface::RegionType::SubNational2);

		const auto sr2List(subregion2Data[indexString]);
		for (const auto& sr2 : sr2List)
		{
			if (sr2.name.compare(subregion2Name) == 0)
			{
				parentRegionName = sr1.name;
				return true;
			}
		}
	}

	return false;
}

bool KMLLibraryManager::LookupParentRegionName(String& parentRegionName)
{
	// 1. Use eBird region names to identify candidate parent regions
	// 2. Use bounding-box method to reduce number of possible candidates
	// 3. If # candidates > 1: (also check if 0 error)
	// 4.   Use ray-casting to check if a point (any random point from within child kml?) is within parent

	return false;
}
