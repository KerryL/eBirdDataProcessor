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

	// TODO:  Parse bytes and store in hash table
	// Need to split on each <placemark> tag set

	return false;
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
	//CleanUpLineEndings(unzippedKML);

	if (detailLevel == GlobalKMLFetcher::DetailLevel::SubNational2)
	{
		// TODO:  Need extra processing here
		// Hmm, looks like downloaded files do NOT inlcude more information than just region name (i.e. no super-region ID).
		// Maybe need to do something fancy and complicated to get multiple levels and compare lat/lon points to determine super-region?
		// Use BuildSubNationalIDString() to create strings for placemark tags
		// Would be good to have eBirds sub-region list here, so we know which super-regions need to be checked (and skip the check if name is unique)
	}

	std::string zippedModifiedKML;
	//if (!z.CreateArchiveBytes(zippedModifiedKML))
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

	const String fileName(libraryPath + country + _T(".kmz"));
	std::ofstream file(UString::ToNarrowString(fileName).c_str(), std::ios::binary);
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << fileName << "' for output\n";
		return false;
	}

	return !(file << zippedModifiedKML).fail();
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

void KMLLibraryManager::CleanUpLineEndings(std::string& s)
{
	ReplaceAllInstancesWith(s, "\r\n", "\n");
	ReplaceAllInstancesWith(s, "\r", "\n");
}

void KMLLibraryManager::ReplaceAllInstancesWith(std::string& s, const std::string& oldString, const std::string& newString)
{
	const int t(sizeof(std::string::size_type));
	std::string::size_type next(0);
	while (next = s.find(oldString, next), next != std::string::npos)
		s.replace(next, oldString.length(), newString);
}
