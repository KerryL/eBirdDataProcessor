// File:  ebdpConfigFile.cpp
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration file object.

// Local headers
#include "ebdpAppConfigFile.h"

void EBDPAppConfigFile::BuildConfigItems()
{
	AddConfigItem(_T("OBS_DATA_FILE"), config.dataFileName);
	AddConfigItem(_T("MEDIA_FILE"), config.mediaFileName);
	AddConfigItem(_T("FREQUENCY_FILES"), config.frequencyFilePath);
	AddConfigItem(_T("EBIRD_API_KEY"), config.eBirdApiKey);
	AddConfigItem(_T("KML_LIBRARY"), config.kmlLibraryPath);
	AddConfigItem(_T("GOOGLE_MAPS_KEY"), config.googleMapsAPIKey);
}

void EBDPAppConfigFile::AssignDefaults()
{
}

bool EBDPAppConfigFile::ConfigIsOK()
{
	bool configurationOK(true);

	if (config.dataFileName.empty())
	{
		Cerr << GetKey(config.dataFileName) << " must be specified\n";
		configurationOK = false;
	}

	if (config.mediaFileName.empty())
	{
		Cerr << GetKey(config.mediaFileName) << " must be specified\n";
		configurationOK = false;
	}

	if (config.frequencyFilePath.empty())
	{
		Cerr << GetKey(config.frequencyFilePath) << " must be specified\n";
		configurationOK = false;
	}

	if (config.eBirdApiKey.empty())
	{
		Cerr << GetKey(config.eBirdApiKey) << " must be specified\n";
		configurationOK = false;
	}

	if (config.kmlLibraryPath.empty())
	{
		Cerr << GetKey(config.kmlLibraryPath) << " must be specified\n";
		configurationOK = false;
	}

	// Not required because google is now billing for maps API use - don't want to risk accidentially incurring charges
	/*if (config.googleMapsAPIKey.empty())
	{
		Cerr << GetKey(config.googleMapsAPIKey) << " must be specified\n";
		configurationOK = false;
	}*/

	return configurationOK;
}
