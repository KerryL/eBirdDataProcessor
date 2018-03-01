// File:  ebdpConfigFile.cpp
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration file object.

// Local headers
#include "ebdpConfigFile.h"

void EBDPConfigFile::BuildConfigItems()
{
	AddConfigItem("OBS_DATA_FILE", config.dataFileName);
	AddConfigItem("OUTPUT_FILE", config.outputFileName);

	AddConfigItem("COUNTRY", config.countryFilter);
	AddConfigItem("STATE", config.stateFilter);
	AddConfigItem("COUNTY", config.countyFilter);
	AddConfigItem("LOCATION", config.locationFilter);

	AddConfigItem("LIST_TYPE", config.listType);

	AddConfigItem("SCORE_RARITIES", config.generateRarityScores);
	AddConfigItem("SPECIES_COUNT_ONLY", config.speciesCountOnly);
	AddConfigItem("INCLUDE_PARTIAL_IDS", config.includePartialIDs);

	AddConfigItem("PHOTO_FILE", config.photoFileName);
	AddConfigItem("SHOW_PHOTO_NEEDS", config.showOnlyPhotoNeeds);

	AddConfigItem("YEAR", config.yearFilter);
	AddConfigItem("MONTH", config.monthFilter);
	AddConfigItem("WEEK", config.weekFilter);
	AddConfigItem("DAY", config.dayFilter);

	AddConfigItem("SORT_FIRST", config.primarySort);
	AddConfigItem("SORT_SECOND", config.secondarySort);

	AddConfigItem("SHOW_UNIQUE_OBS", config.uniqueObservations);

	AddConfigItem("CALENDAR", config.generateTargetCalendar);
	AddConfigItem("TARGET_AREA", config.targetNeedArea);
	AddConfigItem("HARVEST_FREQUENCY", config.harvestFrequencyData);
	AddConfigItem("BULK_FREQUENCY_UPDATE", config.bulkFrequencyUpdate);
	AddConfigItem("AUDIT_FREQUENCY_DATA", config.auditFrequencyData);
	AddConfigItem("FIPS_START", config.fipsStart);
	AddConfigItem("TOP_COUNT", config.topBirdCount);
	AddConfigItem("FREQUENCY_FILES", config.frequencyFilePath);
	AddConfigItem("TARGET_INFO_FILE_NAME", config.targetInfoFileName);
	AddConfigItem("RECENT_PERIOD", config.recentObservationPeriod);

	AddConfigItem("GOOGLE_MAPS_KEY", config.googleMapsAPIKey);
	AddConfigItem("HOME_LOCATION", config.homeLocation);
	AddConfigItem("EBIRD_API_KEY", config.eBirdApiKey);

	AddConfigItem("FIND_MAX_NEEDS", config.findMaxNeedsLocations);

	AddConfigItem("OAUTH_CLIENT_ID", config.oAuthClientId);
	AddConfigItem("OAUTH_CLIENT_SECRET", config.oAuthClientSecret);
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

	config.auditFrequencyData = false;
	config.bulkFrequencyUpdate = false;
	config.harvestFrequencyData = false;
	config.findMaxNeedsLocations = false;

	config.fipsStart = 0;
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

	if (config.fipsStart > 0 && !config.bulkFrequencyUpdate)
	{
		std::cerr << GetKey(config.fipsStart) << " requires " << GetKey(config.bulkFrequencyUpdate) << '\n';
		configurationOK = false;
	}

	if (config.bulkFrequencyUpdate && config.frequencyFilePath.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.bulkFrequencyUpdate) << '\n';
		configurationOK = false;
	}

	if (config.auditFrequencyData && config.frequencyFilePath.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.auditFrequencyData) << '\n';
		configurationOK = false;
	}

	if (config.findMaxNeedsLocations && config.frequencyFilePath.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.findMaxNeedsLocations) << '\n';
		configurationOK = false;
	}

	if (config.harvestFrequencyData && config.countryFilter.empty())
	{
		std::cerr << "Must specify " << GetKey(config.countryFilter) << " when using " << GetKey(config.harvestFrequencyData) << '\n';
		configurationOK = false;
	}

	if (config.bulkFrequencyUpdate && config.harvestFrequencyData)
	{
		std::cerr << "Cannot specify both " << GetKey(config.bulkFrequencyUpdate) << " and " << GetKey(config.harvestFrequencyData) << '\n';
		configurationOK = false;
	}

	if (config.bulkFrequencyUpdate && config.countryFilter.empty())
	{
		std::cerr << "Must specify " << GetKey(config.countryFilter) << " when using " << GetKey(config.bulkFrequencyUpdate) << '\n';
		configurationOK = false;
	}

	if (config.bulkFrequencyUpdate && config.stateFilter.empty())
	{
		std::cerr << "Must specify " << GetKey(config.stateFilter) << " when using " << GetKey(config.bulkFrequencyUpdate) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::TargetCalendarConfigIsOK()
{
	bool configurationOK(true);

	if (config.generateTargetCalendar && config.frequencyFilePath.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.generateTargetCalendar && config.eBirdApiKey.empty())
	{
		std::cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.generateTargetCalendar && config.uniqueObservations != EBDPConfig::UniquenessType::None)
	{
		std::cerr << "Cannot specify both " << GetKey(config.generateTargetCalendar) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::FindMaxNeedsConfigIsOK()
{
	bool configurationOK(true);

	if (config.findMaxNeedsLocations &&
		(config.oAuthClientId.empty() || config.oAuthClientSecret.empty()))
	{
		std::cerr << GetKey(config.findMaxNeedsLocations) << " requires "
			<< GetKey(config.oAuthClientId) << " and " << GetKey(config.oAuthClientSecret) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::GeneralConfigIsOK()
{
	bool configurationOK(true);

	if (config.dataFileName.empty())
	{
		std::cerr << "Must specify '" << GetKey(config.dataFileName) << "'\n";
		configurationOK = false;
	}

	if (!config.countryFilter.empty() &&
		config.countryFilter.length() != 2)
	{
		std::cerr << "Country (" << GetKey(config.countryFilter) << ") must be specified using 2-digit abbreviation\n";
		configurationOK = false;
	}

	if (!config.stateFilter.empty() &&
		(config.stateFilter.length() < 2 || config.stateFilter.length() > 3))
	{
		std::cerr << "State/providence (" << GetKey(config.stateFilter) << ") must be specified using 2- or 3-digit abbreviation\n";
		configurationOK = false;
	}

	if (config.dayFilter > 31)
	{
		std::cerr << "Day (" << GetKey(config.dayFilter) << ") must be in the range 0 - 31\n";
		configurationOK = false;
	}

	if (config.monthFilter > 12)
	{
		std::cerr << "Month (" << GetKey(config.monthFilter) << ") must be in the range 0 - 12\n";
		configurationOK = false;
	}

	if (config.weekFilter > 53)
	{
		std::cerr << "Week (" << GetKey(config.weekFilter) << ") must be in the range 0 - 53\n";
		configurationOK = false;
	}

	if (config.recentObservationPeriod < 1 || config.recentObservationPeriod > 30)
	{
		std::cerr << GetKey(config.recentObservationPeriod) << " must be between 1 and 30\n";
		configurationOK = false;
	}

	/*if (!config.googleMapsAPIKey.empty() && config.homeLocation.empty())
	{
		std::cerr << "Must specify " << GetKey(config.homeLocation) << " when using " << GetKey(config.googleMapsAPIKey) << '\n';
		configurationOK = false;
	}*/

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None && !config.countryFilter.empty())
	{
		std::cerr << "Cannot specify both " << GetKey(config.countryFilter) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	if (config.showOnlyPhotoNeeds && config.photoFileName.empty())
	{
		std::cerr << "Must specify " << GetKey(config.photoFileName) << " when using " << GetKey(config.showOnlyPhotoNeeds) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}

bool EBDPConfigFile::RaritiesConfigIsOK()
{
	bool configurationOK(true);

	if (config.generateRarityScores && config.frequencyFilePath.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFilePath) << " when using " << GetKey(config.generateRarityScores) << '\n';
		configurationOK = false;
	}

	if (config.generateRarityScores && config.eBirdApiKey.empty())
	{
		std::cerr << "Must specify " << GetKey(config.eBirdApiKey) << " when using " << GetKey(config.generateRarityScores) << '\n';
		configurationOK = false;
	}

	if (config.generateRarityScores && config.generateTargetCalendar)
	{
		std::cerr << "Cannot specify both " << GetKey(config.generateRarityScores) << " and " << GetKey(config.generateTargetCalendar) << '\n';
		configurationOK = false;
	}

	if (config.generateRarityScores && config.uniqueObservations != EBDPConfig::UniquenessType::None)
	{
		std::cerr << "Cannot specify both " << GetKey(config.generateRarityScores) << " and " << GetKey(config.uniqueObservations) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}
