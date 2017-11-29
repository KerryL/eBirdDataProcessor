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

	AddConfigItem("YEAR", config.yearFilter);
	AddConfigItem("MONTH", config.monthFilter);
	AddConfigItem("WEEK", config.weekFilter);
	AddConfigItem("DAY", config.dayFilter);

	AddConfigItem("SORT_FIRST", config.primarySort);
	AddConfigItem("SORT_SECOND", config.secondarySort);

	AddConfigItem("CALENDAR", config.generateTargetCalendar);
	AddConfigItem("HARVEST_FREQUENCY", config.harvestFrequencyData);
	AddConfigItem("TOP_COUNT", config.topBirdCount);
	AddConfigItem("FREQUENCY_FILE", config.frequencyFileName);
	AddConfigItem("TARGET_INFO_FILE_NAME", config.targetInfoFileName);
	AddConfigItem("RECENT_PERIOD", config.recentObservationPeriod);

	AddConfigItem("GOOGLE_MAPS_KEY", config.googleMapsAPIKey);
	AddConfigItem("HOME_LOCATION", config.homeLocation);
}

void EBDPConfigFile::AssignDefaults()
{
	config.listType = 0;
	config.speciesCountOnly = false;
	config.includePartialIDs = false;

	config.yearFilter = 0;
	config.monthFilter = 0;
	config.weekFilter = 0;
	config.dayFilter = 0;

	config.primarySort = 0;
	config.secondarySort = 0;

	config.harvestFrequencyData = false;
	config.generateTargetCalendar = false;
	config.topBirdCount = false;
	config.recentObservationPeriod = 15;
}

bool EBDPConfigFile::ConfigIsOK()
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
		config.stateFilter.length() != 2)
	{
		std::cerr << "State/providence (" << GetKey(config.stateFilter) << ") must be specified using 2-digit abbreviation\n";
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

	if (config.listType > 5)
	{
		std::cerr << "List type (" << GetKey(config.listType) << ") must be in the range 0 - 5\n";
		configurationOK = false;
	}

	if (!config.googleMapsAPIKey.empty() && config.homeLocation.empty())
	{
		std::cerr << "Must specify " << GetKey(config.homeLocation) << " when using " << GetKey(config.googleMapsAPIKey) << '\n';
		configurationOK = false;
	}

	if (config.recentObservationPeriod < 1 || config.recentObservationPeriod > 30)
	{
		std::cerr << GetKey(config.recentObservationPeriod) << " must be between 1 and 30\n";
		configurationOK = false;
	}

	if (config.harvestFrequencyData && config.countryFilter.empty())
	{
		std::cerr << "Must specify " << GetKey(config.countryFilter) << " when using " << GetKey(config.harvestFrequencyData) << '\n';
		configurationOK = false;
	}

	if (config.generateRarityScores && config.frequencyFileName.empty())
	{
		std::cerr << "Must specify " << GetKey(config.frequencyFileName) << " when using " << GetKey(config.generateRarityScores) << '\n';
		configurationOK = false;
	}

	return configurationOK;
}
