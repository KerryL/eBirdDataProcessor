// File:  kmlLibraryManager.cpp
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

// Local headers
#include "kmlLibraryManager.h"
#include "globalKMLFetcher.h"
#include "zipper.h"

KMLLibraryManager::KMLLibraryManager(const String& libraryPath) : libraryPath(libraryPath)
{
}

String KMLLibraryManager::GetKML(const String& country, const String& subNational1, const String& subNational2)
{
	// TODO:  Implement
	// Check in order:
	// 1.  KML already in memory
	// 2.  KML in library on disk
	// 3.  Download KML (and then store on disk)
	return String();
}

/*
GlobalKMLFetcher fetcher;
	std::string result;
	fetcher.FetchKML(_T("Japan"), GlobalKMLFetcher::DetailLevel::SubNational1, result);
	{
		std::ofstream("japan.kmz", std::ios::binary) << result;
	}
	Zipper z;
	z.OpenArchiveFile(_T("japan.kmz"));
	if (z.ArchiveIsOpen())
	{
		auto con(z.ListContents());
		if (con.size() == 1)
		{
			Cout << "Extracting '" << con.front() << '\'' << std::endl;
			std::string bytes;
			if (!z.ExtractFile(con.front(), bytes))
				Cerr << "Failed\n";
			else
				std::ofstream("data.kml") << bytes;
		}
	}*/