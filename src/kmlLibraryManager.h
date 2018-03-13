// File:  kmlLibraryManager.h
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

#ifndef KML_LIBRARY_MANAGER_H_
#define KML_LIBRARY_MANAGER_H_

// Local headers
#include "globalKMLFetcher.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <unordered_map>

class KMLLibraryManager
{
public:
	KMLLibraryManager(const String& libraryPath);

	String GetKML(const String& country, const String& subNational1, const String& subNational2);

private:
	const String libraryPath;

	std::unordered_map<String, String> kmlMemory;

	bool LoadKMLFromLibrary(const String& country);
	bool DownloadAndStoreKML(const String& country, const GlobalKMLFetcher::DetailLevel& detailLevel);

	static String BuildLocationIDString(const String& country, const String& subNational1, const String& subNational2);
	static String BuildSubNationalIDString(const String& subNational1, const String& subNational2);
};

#endif// KML_LIBRARY_MANAGER_H_
