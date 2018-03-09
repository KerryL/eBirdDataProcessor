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
	z.OpenArchiveFile(archiveFileName);
	if (!z.ArchiveIsOpen())
	{
		Cerr << "Failed to open '" << archiveFileName << "' for input\n";
		return false;
	}

	std::string bytes;
	if (!z.ExtractFile(0, bytes))
		Cerr << "Failed to extract kml file from '" << archiveFileName << "':  " << z.GetErrorString() << '\n';

	// TODO:  Parse bytes and store in hash table
	// Need to split on each <placemark> tag set
	// Hmm, looks like downloaded files do NOT inlcude more information than just region name (i.e. no super-region ID).
	// Maybe need to do something fancy and complicated to get multiple levels and compare lat/lon points to determine super-region?

	return false;
}

// Download by country
bool KMLLibraryManager::DownloadAndStoreKML(const String& country,
	const GlobalKMLFetcher::DetailLevel& detailLevel)
{
	GlobalKMLFetcher fetcher;
	std::string result;
	fetcher.FetchKML(country, detailLevel, result);

	const String fileName(libraryPath + country + _T(".kmz"));
	std::ofstream file(UString::ToNarrowString(fileName).c_str(), std::ios::binary);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}

	return !(file << result).fail();
}

String KMLLibraryManager::BuildLocationIDString(const String& country,
	const String& subNational1, const String& subNational2)
{
	// TODO:  Implement
	return String();
}
