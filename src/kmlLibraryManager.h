// File:  kmlLibraryManager.h
// Date:  3/9/2018
// Auth:  K. Loux
// Desc:  Object for managing KML library.

#ifndef KML_LIBRARY_MANAGER_H_
#define KML_LIBRARY_MANAGER_H_

// Local headers
#include "utilities/uString.h"

class KMLLibraryManager
{
public:
	KMLLibraryManager(const String& libraryPath);

	/*enum class Detail
	{
		Country,
		SubNational1,
		SubNational2
	};*/

	String GetKML(const String& country, const String& subNational1, const String& subNational2);

private:
	const String libraryPath;
};

#endif// KML_LIBRARY_MANAGER_H_
