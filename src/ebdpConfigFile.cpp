// File:  ebdpConfigFile.cpp
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration file object.

// Local headers
#include "ebdpConfigFile.h"

void EBDPConfigFile::BuildConfigItems()
{
	AddConfigItem(_T("OBS_DATA_FILE"), config.dataFileName);
	AddConfigItem(_T("OUTPUT_FILE"), config.outputFileName);

	AddConfigItem(_T("COUNTRY"), config.countryFilter);
	AddConfigItem(_T("STATE"), config.stateFilter);
	AddConfigItem(_T("COUNTY"), config.countyFilter);
	AddConfigItem(_T("LOCATION"), config.locationFilter);

	AddConfigItem(_T("LIST_TYPE"), config.listType);

	AddConfigItem(_T("SCORE_RARITIES"), config.generateRarityScores);
	AddConfigItem(_T("SPECIES_COUNT_ONLY"), config.speciesCountOnly);
	AddConfigItem(_T("INCLUDE_PARTIAL_IDS"), config.includePartialIDs);

	AddConfigItem(_T("PHOTO_FILE"), config.photoFileName);
	AddConfigItem(_T("SHOW_PHOTO_NEEDS"), config.showOnlyPhotoNeeds);

	AddConfigItem(_T("YEAR"), config.yearFilter);
	AddConfigItem(_T("MONTH"), config.monthFilter);
	AddConfigItem(_T("WEEK"), config.weekFilter);
	AddConfigItem(_T("DAY"), config.dayFilter);

	AddConfigItem(_T("SORT_FIRST"), config.primarySort);
	AddConfigItem(_T("SORT_SECOND"), config.secondarySort);

	AddConfigItem(_T("SHOW_UNIQUE_OBS"), config.uniqueObservations);

	AddConfigItem(_T("CALENDAR"), config.generateTargetCalendar);
	AddConfigItem(_T("TARGET_AREA"), config.targetNeedArea);
	AddConfigItem(_T("TOP_COUNT"), config.topBirdCount);
	AddConfigItem(_T("FREQUENCY_FILES"), config.frequencyFilePath);
	AddConfigItem(_T("TARGET_INFO_FILE_NAME"), config.targetInfoFileName);
	AddConfigItem(_T("RECENT_PERIOD"), config.recentObservationPeriod);

	AddConfigItem(_T("GOOGLE_MAPS_KEY"), config.googleMapsAPIKey);
	AddConfigItem(_T("HOME_LOCATION"), config.homeLocation);
	AddConfigItem(_T("EBIRD_API_KEY"), config.eBirdApiKey);

	AddConfigItem(_T("FIND_MAX_NEEDS"), config.findMaxNeedsLocations);
	AddConfigItem(_T("KML_LIBRARY"), config.kmlLibraryPath);

	AddConfigItem(_T("OAUTH_CLIENT_ID"), config.oAuthClientId);
	AddConfigItem(_T("OAUTH_CLIENT_SECRET"), config.oAuthClientSecret);

	AddConfigItem(_T("DATASET"), config.eBirdDatasetPath);
}

void EBDPConfigFile::AssignDefaults()
{
	config.listType = EBDPConfig::ListType::Life;
	config.speciesCountOnly = false;
	config.includePartialIDs = false;

	config.yearFilter = 0;
	config.monthFilter = 0;
	config.weekFilter = 0;
	config.dayFilter = 0;

	config.primarySort = EBDPConfig::SortBy::None;
	config.secondarySort = EBDPConfig::SortBy::None;

	config.uniqueObservations = EBDPConfig::UniquenessType::None;

	config.targetNeedArea = EBDPConfig::TargetNeedArea::None;
	config.generateTargetCalendar = false;
	config.generateRarityScores = false;
	config.topBirdCount = 20;
	config.recentObservationPeriod = 15;

	config.showOnlyPhotoNeeds = false;
	config.findMaxNeedsLocations = false;
}

bool EBDPConfigFile::ConfigIsOK()
{
	bool configurationOK(true);

	if (!GeneralConfigIsOK())
		configurationOK = false;

	if (!FrequencyHarvestConfigIsOK())
		configurationOK = false;

	if (!TargetCalendarConfigIsOK())
		configurationOK = false;

	if (!FindMaxNeedsConfigIsOK())
		configurationOK = false;

	if (!RaritiesConfigIsOK())
		configurationOK = false;

	return configurationOK;
}

bool EBDPConfigFile::FrequencyHarvestConfigIsOK()
{
	bool configurationOK(true);

	if (!config.eBirdDatasetPath.empty() && config.frequencyFilePath.empty())
	{
		Cerr << GetKey(config.eBirdDatasetPath) << " requires " << GetKey(config.frequencyFilePath) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::TargetCalendarConfigIsOK()
{
	if (!config.generateTargetCalendar)
		return true;
    
	bool configurationOK(true);

	if (config.frequencyFilePath.empty())
	{
		Cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.eBirdApiKey.empty())
	{
		Cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None)
	{
		Cerr << "Cannot specify both " << GetKey(config.generateTargetCalendar) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::FindMaxNeedsConfigIsOK()
{
	if (!config.findMaxNeedsLocations)
		return true;
    
	bool configurationOK(true);

	if (config.oAuthClientId.empty() || config.oAuthClientSecret.empty())
	{
		Cerr << GetKey(config.findMaxNeedsLocations) << " requires "
			<< GetKey(config.oAuthClientId) << " and " << GetKey(config.oAuthClientSecret) << '\n';
		configurationOK = false;
	}

	if (config.kmlLibraryPath.empty())
	{
		Cerr << GetKey(config.findMaxNeedsLocations) << " requires " << GetKey(config.kmlLibraryPath) << '\n';
		configurationOK = false;
	}

	if (config.eBirdApiKey.empty())
	{
		Cerr << GetKey(config.findMaxNeedsLocations) << " requires " << GetKey(config.eBirdApiKey) << '\n';
		configurationOK = false;
	}
    
    if (config.frequencyFilePath.empty())
	{
		Cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.findMaxNeedsLocations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::GeneralConfigIsOK()
{
	bool configurationOK(true);

	if (config.dataFileName.empty())
	{
		Cerr << "Must specify '" << GetKey(config.dataFileName) << "'\n";
		configurationOK = false;
	}

	if (!config.countryFilter.empty() &&
		config.countryFilter.length() != 2)
	{
		Cerr << "Country (" << GetKey(config.countryFilter) << ") must be specified using 2-digit abbreviation\n";
		configurationOK = false;
	}

	if (!config.stateFilter.empty() &&
		(config.stateFilter.length() < 2 || config.stateFilter.length() > 3))
	{
		Cerr << "State/providence (" << GetKey(config.stateFilter) << ") must be specified using 2- or 3-digit abbreviation\n";
		configurationOK = false;
	}

	if (config.dayFilter > 31)
	{
		Cerr << "Day (" << GetKey(config.dayFilter) << ") must be in the range 0 - 31\n";
		configurationOK = false;
	}

	if (config.monthFilter > 12)
	{
		Cerr << "Month (" << GetKey(config.monthFilter) << ") must be in the range 0 - 12\n";
		configurationOK = false;
	}

	if (config.weekFilter > 53)
	{
		Cerr << "Week (" << GetKey(config.weekFilter) << ") must be in the range 0 - 53\n";
		configurationOK = false;
	}

	if (config.recentObservationPeriod < 1 || config.recentObservationPeriod > 30)
	{
		Cerr << GetKey(config.recentObservationPeriod) << " must be between 1 and 30\n";
		configurationOK = false;
	}

	/*if (!config.googleMapsAPIKey.empty() && config.homeLocation.empty())
	{
		Cerr << "Must specify " << GetKey(config.homeLocation) << " when using " << GetKey(config.googleMapsAPIKey) << '\n';
		configurationOK = false;
	}*/

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None && !config.countryFilter.empty())
	{
		Cerr << "Cannot specify both " << GetKey(config.countryFilter) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	if (config.showOnlyPhotoNeeds && config.photoFileName.empty())
	{
		Cerr << "Must specify " << GetKey(config.photoFileName) << " when using " << GetKey(config.showOnlyPhotoNeeds) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::RaritiesConfigIsOK()
{
	if (!config.generateRarityScores)
		return true;
    
	bool configurationOK(true);

	if (config.frequencyFilePath.empty())
	{
		Cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.generateRarityScores) << '\n';
		configurationOK = false;
	}

	if (config.eBirdApiKey.empty())
	{
		Cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.generateRarityScores) << '\n';
		configurationOK = false;
	}

	if (config.generateTargetCalendar)
	{
		Cerr << "Cannot specify both " << GetKey(config.generateRarityScores) << " and " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None)
	{
		Cerr << "Cannot specify both " << GetKey(config.generateRarityScores) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}
