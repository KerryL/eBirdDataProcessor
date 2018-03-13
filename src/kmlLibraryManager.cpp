// File:  kmlLibraryManager.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

// Local headers
#include "kmlLibraryManager.h"
#include "zipper.h"

KMLLibraryManager::KMLLibraryManager(const String& libraryPath) : libraryPath(libraryPath)
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

	const String placemarkStartTag(_T("<Placemark>"));
	String countryKMLData(UString::ToStringType(bytes));
	std::string::size_type next(0);
	while (next = countryKMLData.find(placemarkStartTag, next), next != std::string::npos)
	{
		const String placemarkEndTag(_T("</Placemark>"));
		const std::string::size_type placemarkEnd(countryKMLData.find(placemarkEndTag, next));
		if (placemarkEnd == std::string::npos)
		{
			Cerr << "Failed to find expected placemark end tag\n";
			return false;
		}

		// NOTE:  Possibly need to search for <Polygon> tag if <MultiGeometry> is not found.
		// I think <MultiGeometry> is only necessary if there are multiple polygons, although
		// gadm.org seems to wrap all polygon tags in multigeometry tags, so for now we'll
		// leave this.
		const String geometryStartTag(_T("<MultiGeometry>"));
		const std::string::size_type geometryStart(countryKMLData.find(geometryStartTag, next));
		if (geometryStart == std::string::npos)
		{
			Cerr << "Failed to find geometry start tag\n";
			return false;
		}

		const String geometryEndTag(_T("</MultiGeometry>"));
		const std::string::size_type geometryEnd(countryKMLData.find(geometryEndTag, geometryStart));
		if (geometryEnd == std::string::npos)
		{
			Cerr << "Failed to find geometry end tag\n";
			return false;
		}

		const String name(ExtractName(countryKMLData, next));
		if (name.empty())
		{
			Cerr << "Failed to extract placemark name from KML data\n";
			return false;
		}

		tempMap[country + _T(":") + name] = countryKMLData.substr(geometryStart, geometryEnd - geometryStart + geometryEndTag.length());

		next = placemarkEnd;
	}

	//kmlMemory.merge(std::move(tempMap));// required C++ 17
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
		// TODO:  Need extra processing here
		// Hmm, looks like downloaded files do NOT inlcude more information than just region name (i.e. no super-region ID).
		// Maybe need to do something fancy and complicated to get multiple levels and compare lat/lon points to determine super-region?
		// Use BuildSubNationalIDString() to create strings for placemark tags
		// Would be good to have eBirds sub-region list here, so we know which super-regions need to be checked (and skip the check if name is unique)
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
