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

	AddConfigItem(_T("COUNTRY"), config.locationFilters.country);
	AddConfigItem(_T("STATE"), config.locationFilters.state);
	AddConfigItem(_T("COUNTY"), config.locationFilters.county);
	AddConfigItem(_T("LOCATION"), config.locationFilters.location);

	AddConfigItem(_T("LIST_TYPE"), config.listType);

	AddConfigItem(_T("SCORE_RARITIES"), config.generateRarityScores);
	AddConfigItem(_T("SPECIES_COUNT_ONLY"), config.speciesCountOnly);
	AddConfigItem(_T("INCLUDE_PARTIAL_IDS"), config.includePartialIDs);

	AddConfigItem(_T("TOD_SPECIES"), config.timeOfDayParameters.commonNames);
	AddConfigItem(_T("TOD_OUTPUT_FILE"), config.timeOfDayParameters.outputFile);
	AddConfigItem(_T("REGION_DATA_OUTPUT_FILE"), config.timeOfDayParameters.splitRegionDataFile);

	AddConfigItem(_T("PROB_VS_TIME_OUTPUT_FILE"), config.timeOfYearParameters.outputFile);
	AddConfigItem(_T("PROB_VS_TIME_MAX_PROB"), config.timeOfYearParameters.maxProbability);
	AddConfigItem(_T("PROB_VS_TIME_SPECIES"), config.timeOfYearParameters.commonNames);

	AddConfigItem(_T("MEDIA_LIST_HTML"), config.mediaListHTML);
	AddConfigItem(_T("MEDIA_FILE"), config.mediaFileName);
	AddConfigItem(_T("SHOW_PHOTO_NEEDS"), config.showOnlyPhotoNeeds);
	AddConfigItem(_T("SHOW_AUDIO_NEEDS"), config.showOnlyAudioNeeds);

	AddConfigItem(_T("YEAR"), config.timeFilters.year);
	AddConfigItem(_T("MONTH"), config.timeFilters.month);
	AddConfigItem(_T("WEEK"), config.timeFilters.week);
	AddConfigItem(_T("DAY"), config.timeFilters.day);

	AddConfigItem(_T("SORT_FIRST"), config.primarySort);
	AddConfigItem(_T("SORT_SECOND"), config.secondarySort);

	AddConfigItem(_T("SHOW_UNIQUE_OBS"), config.uniqueObservations);

	AddConfigItem(_T("TARGET_AREA"), config.targetNeedArea);

	AddConfigItem(_T("CALENDAR"), config.generateTargetCalendar);
	AddConfigItem(_T("TOP_COUNT"), config.calendarParameters.topBirdCount);
	AddConfigItem(_T("TARGET_INFO_FILE_NAME"), config.calendarParameters.targetInfoFileName);
	AddConfigItem(_T("RECENT_PERIOD"), config.calendarParameters.recentObservationPeriod);
	AddConfigItem(_T("GOOGLE_MAPS_KEY"), config.calendarParameters.googleMapsAPIKey);
	AddConfigItem(_T("HOME_LOCATION"), config.calendarParameters.homeLocation);

	AddConfigItem(_T("FREQUENCY_FILES"), config.frequencyFilePath);

	AddConfigItem(_T("FIND_MAX_NEEDS"), config.findMaxNeedsLocations);
	AddConfigItem(_T("CLEANUP_KML_NAMES"), config.locationFindingParameters.cleanupKMLLocationNames);
	AddConfigItem(_T("KML_REDUCTION_LIMIT"), config.locationFindingParameters.kmlReductionLimit);
	AddConfigItem(_T("GEO_JSON_PRECISION"), config.locationFindingParameters.geoJSONPrecision);
	AddConfigItem(_T("KML_LIBRARY"), config.locationFindingParameters.kmlLibraryPath);
	AddConfigItem(_T("OUTPUT_BASE_FILE_NAME"), config.locationFindingParameters.baseOutputFileName);

	AddConfigItem(_T("HIGH_DETAIL"), config.highDetailCountries);

	AddConfigItem(_T("FIND_BEST_TRIPS"), config.findBestTripLocations);
	AddConfigItem(_T("TOP_LOCATION_COUNT"), config.bestTripParameters.topLocationCount);
	AddConfigItem(_T("MIN_OBS_COUNT"), config.bestTripParameters.minimumObservationCount);
	AddConfigItem(_T("MIN_LIKLIHOOD"), config.bestTripParameters.minimumLiklihood);

	AddConfigItem(_T("EBIRD_API_KEY"), config.eBirdApiKey);
	
	AddConfigItem(_T("DATASET"), config.eBirdDatasetPath);

	AddConfigItem(_T("COMPARE"), config.doComparison);
	
	AddConfigItem(_T("COMMENT_GROUP"), config.commentGroupString);

	AddConfigItem(_T("HUNT_SPECIES"), config.speciesHunt.commonName);
	AddConfigItem(_T("HUNT_LATITUDE"), config.speciesHunt.latitude);
	AddConfigItem(_T("HUNT_LONGITUDE"), config.speciesHunt.longitude);
	AddConfigItem(_T("HUNT_RADIUS"), config.speciesHunt.radius);
}

void EBDPConfigFile::AssignDefaults()
{
	config.listType = EBDPConfig::ListType::Life;
	config.speciesCountOnly = false;
	config.includePartialIDs = false;

	config.timeFilters.year = 0;
	config.timeFilters.month = 0;
	config.timeFilters.week = 0;
	config.timeFilters.day = 0;

	config.primarySort = EBDPConfig::SortBy::None;
	config.secondarySort = EBDPConfig::SortBy::None;

	config.uniqueObservations = EBDPConfig::UniquenessType::None;

	config.targetNeedArea = EBDPConfig::TargetNeedArea::None;

	config.generateTargetCalendar = false;
	config.calendarParameters.topBirdCount = 20;
	config.calendarParameters.recentObservationPeriod = 15;

	config.generateRarityScores = false;

	config.showOnlyPhotoNeeds = -1;
	config.showOnlyAudioNeeds = -1;

	config.findMaxNeedsLocations = false;
	config.locationFindingParameters.kmlReductionLimit = 0.0;
	config.locationFindingParameters.cleanupKMLLocationNames = false;
	config.locationFindingParameters.geoJSONPrecision = -1;
	config.locationFindingParameters.baseOutputFileName = _T("bestLocations");

	config.findBestTripLocations = false;
	config.bestTripParameters.minimumLiklihood = 5.0;
	config.bestTripParameters.minimumObservationCount = 2000;
	config.bestTripParameters.topLocationCount = 10;

	config.doComparison = false;

	config.speciesHunt.latitude = 0.0;
	config.speciesHunt.longitude = 0.0;
	config.speciesHunt.radius = 0.0;

	config.timeOfYearParameters.maxProbability = 0.0;
}

bool EBDPConfigFile::ConfigIsOK()
{
	bool configurationOK(true);

	if (!GeneralConfigIsOK())
		configurationOK = false;

	if (!FrequencyHarvestConfigIsOK() && !TimeOfDayConfigIsOK())
		configurationOK = false;

	if (!TimeOfYearConfigIsOK())
		configurationOK = false;

	if (!TargetCalendarConfigIsOK())
		configurationOK = false;

	if (!FindMaxNeedsConfigIsOK())
		configurationOK = false;

	if (!RaritiesConfigIsOK())
		configurationOK = false;

	if (!BestTripConfigIsOK())
		configurationOK = false;

	if (!SpeciesHuntConfigIsOK())
		configurationOK = false;

	return configurationOK;
}

bool EBDPConfigFile::TimeOfDayConfigIsOK()
{
	bool configurationOK(true);

	if (!config.eBirdDatasetPath.empty() &&
		(config.timeOfDayParameters.commonNames.empty() || config.timeOfDayParameters.outputFile.empty()))
	{
		Cerr << "Time-of-day analysis requires " << GetKey(config.timeOfDayParameters.outputFile) << " and at least one " << GetKey(config.timeOfDayParameters.commonNames) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::TimeOfYearConfigIsOK()
{
	bool configurationOK(true);

	if (!config.timeOfYearParameters.commonNames.empty() && config.timeOfYearParameters.maxProbability > 0.0)
	{
		Cerr << "Cannot specify both " << GetKey(config.timeOfYearParameters.commonNames) << " and " << GetKey(config.timeOfYearParameters.maxProbability) << '\n';
		configurationOK = false;
	}

	if (!config.timeOfYearParameters.outputFile.empty() && config.timeOfYearParameters.commonNames.empty() && config.timeOfYearParameters.maxProbability <= 0.0)
	{
		Cerr << "Time-of-year analysis requires that either " << GetKey(config.timeOfYearParameters.maxProbability) << " or at least one " << GetKey(config.timeOfYearParameters.commonNames) << " be specified\n";
		configurationOK = false;
	}

	if (!config.timeOfYearParameters.outputFile.empty() && config.frequencyFilePath.empty())
	{
		Cerr << "Time-of-year analysis requires that " << GetKey(config.frequencyFilePath) << " be specified\n";
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::FrequencyHarvestConfigIsOK()
{
	bool configurationOK(true);

	if (!config.timeOfDayParameters.commonNames.empty() || !config.timeOfDayParameters.outputFile.empty())
		return true;

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

	if (config.calendarParameters.topBirdCount == 0)
	{
		Cerr << GetKey(config.calendarParameters.topBirdCount) << " must be greater than zero\n";
		configurationOK = false;
	}

	if (config.calendarParameters.recentObservationPeriod < 1 || config.calendarParameters.recentObservationPeriod > 30)
	{
		Cerr << GetKey(config.calendarParameters.recentObservationPeriod) << " must be between 1 and 30\n";
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::FindMaxNeedsConfigIsOK()
{
	if (!config.findMaxNeedsLocations)
		return true;
    
	bool configurationOK(true);

	if (config.locationFindingParameters.kmlLibraryPath.empty())
	{
		Cerr << GetKey(config.findMaxNeedsLocations) << " requires " << GetKey(config.locationFindingParameters.kmlLibraryPath) << '\n';
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
	
	if (config.locationFindingParameters.baseOutputFileName.empty())
	{
		Cerr << GetKey(config.locationFindingParameters.baseOutputFileName) << " must not be empty\n";
		configurationOK = false;
	}

	for (const auto& c : config.highDetailCountries)
	{
		if (c.length() != 2)
		{
			Cerr << GetKey(config.highDetailCountries) << " must use 2-letter country codes\n";
			configurationOK = false;
			break;
		}
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

	if (!config.locationFilters.country.empty() &&
		!config.locationFilters.state.empty())
	{
		if (config.locationFilters.state.size() != config.locationFilters.country.size())
		{
			Cerr << "Must specify the same number of " << GetKey(config.locationFilters.country) << " and " << GetKey(config.locationFilters.state) << " parameters\n";
			configurationOK = false;
		}

		if (!config.locationFilters.county.empty() &&
			config.locationFilters.county.size() != config.locationFilters.state.size())
		{
			Cerr << "Must specify the same number of " << GetKey(config.locationFilters.state) << " and " << GetKey(config.locationFilters.county) << " parameters\n";
			configurationOK = false;
		}
	}

	for (const auto& p : config.locationFilters.country)
	{
		if (p.empty())
		{
			Cerr << "Country (" << GetKey(config.locationFilters.country) << ") must not be blank\n";
			configurationOK = false;
		}

		if (p.length() != 2)
		{
			Cerr << "Country (" << GetKey(config.locationFilters.country) << ") must be specified using 2-digit abbreviation\n";
			configurationOK = false;
		}
	}

	for (const auto& p : config.locationFilters.state)
	{
		if (!p.empty() &&
			(p.length() < 2 || p.length() > 3))
		{
			Cerr << "State/providence (" << GetKey(config.locationFilters.state) << ") must be specified using 2- or 3-digit abbreviation\n";
			configurationOK = false;
		}
	}

	if (config.timeFilters.day > 31)
	{
		Cerr << "Day (" << GetKey(config.timeFilters.day) << ") must be in the range 0 - 31\n";
		configurationOK = false;
	}

	if (config.timeFilters.month > 12)
	{
		Cerr << "Month (" << GetKey(config.timeFilters.month) << ") must be in the range 0 - 12\n";
		configurationOK = false;
	}

	if (config.timeFilters.week > 52)
	{
		Cerr << "Week (" << GetKey(config.timeFilters.week) << ") must be in the range 0 - 52\n";
		configurationOK = false;
	}

	/*if (!config.googleMapsAPIKey.empty() && config.homeLocation.empty())
	{
		Cerr << "Must specify " << GetKey(config.homeLocation) << " when using " << GetKey(config.googleMapsAPIKey) << '\n';
		configurationOK = false;
	}*/

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None && !config.locationFilters.country.empty())
	{
		Cerr << "Cannot specify both " << GetKey(config.locationFilters.country) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	if (config.showOnlyPhotoNeeds > 0 && config.mediaFileName.empty())
	{
		Cerr << "Must specify " << GetKey(config.mediaFileName) << " when using " << GetKey(config.showOnlyPhotoNeeds) << '\n';
		configurationOK = false;
	}

	if (config.showOnlyAudioNeeds > 0 && config.mediaFileName.empty())
	{
		Cerr << "Must specify " << GetKey(config.mediaFileName) << " when using " << GetKey(config.showOnlyAudioNeeds) << '\n';
		configurationOK = false;
	}

	if (!config.mediaListHTML.empty() && config.mediaFileName.empty())
	{
		Cerr << "Must specify " << GetKey(config.mediaFileName) << " when " << GetKey(config.mediaListHTML) << " is specified\n";
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

bool EBDPConfigFile::BestTripConfigIsOK()
{
	if (!config.findBestTripLocations)
		return true;

	bool configurationOK(true);

	if (config.frequencyFilePath.empty())
	{
		Cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.findBestTripLocations) << '\n';
		configurationOK = false;
	}

	if (config.outputFileName.empty())
	{
		Cerr << "Must specify " << GetKey(config.outputFileName) << " when using " << GetKey(config.findBestTripLocations) << '\n';
		configurationOK = false;
	}

	if (config.eBirdApiKey.empty())
	{
		Cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.findBestTripLocations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::SpeciesHuntConfigIsOK()
{
	if (config.speciesHunt.commonName.empty())
		return true;

	bool configurationOK(true);

	if (config.eBirdApiKey.empty())
	{
		Cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.speciesHunt.commonName) << '\n';
		configurationOK = false;
	}

	if (config.speciesHunt.radius <= 0.0)
	{
		Cerr << GetKey(config.speciesHunt.radius) << "must be strictly positive\n";
		configurationOK = false;
	}

	return configurationOK;
}
