// File:  eBirdDataProcessorApp.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Application class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessorApp.h"
#include "eBirdDataProcessor.h"
#include "ebdpConfigFile.h"
#include "frequencyDataHarvester.h"

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <sstream>
#include <iostream>
#include <cassert>
#include <fstream>

int main(int argc, char *argv[])
{
	EBirdDataProcessorApp app;
	return app.Run(argc, argv);
}

int EBirdDataProcessorApp::Run(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cout << "Usage:  " << argv[0] << " <config file name>" << std::endl;
		return 1;
	}

	const std::string configFileName(argv[1]);
	EBDPConfigFile configFile;
	if (!configFile.ReadConfiguration(configFileName))
		return 1;

	EBirdDataProcessor processor;
	if (!processor.Parse(configFile.GetConfig().dataFileName))
		return 1;

	if (configFile.GetConfig().uniqueObservations != EBDPConfig::UniquenessType::None)
		processor.GenerateUniqueObservationsReport(configFile.GetConfig().uniqueObservations);

	// Remove entires that don't fall withing the specified locations
	if (!configFile.GetConfig().locationFilter.empty() && configFile.GetConfig().targetNeedArea == EBDPConfig::TargetNeedArea::None)
		processor.FilterLocation(configFile.GetConfig().locationFilter, configFile.GetConfig().countyFilter,
			configFile.GetConfig().stateFilter, configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().countyFilter.empty() && configFile.GetConfig().targetNeedArea <= EBDPConfig::TargetNeedArea::Region)
		processor.FilterCounty(configFile.GetConfig().countyFilter, configFile.GetConfig().stateFilter,
			configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().stateFilter.empty() && configFile.GetConfig().targetNeedArea <= EBDPConfig::TargetNeedArea::Subnational1)
		processor.FilterState(configFile.GetConfig().stateFilter, configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().countryFilter.empty() && configFile.GetConfig().targetNeedArea <= EBDPConfig::TargetNeedArea::Country)
		processor.FilterCountry(configFile.GetConfig().countryFilter);

	// Remove entries that don't fall within the specified dates
	if (configFile.GetConfig().yearFilter > 0)
		processor.FilterYear(configFile.GetConfig().yearFilter);

	if (configFile.GetConfig().monthFilter > 0)
		processor.FilterMonth(configFile.GetConfig().monthFilter);

	if (configFile.GetConfig().weekFilter > 0)
		processor.FilterWeek(configFile.GetConfig().weekFilter);

	if (configFile.GetConfig().dayFilter > 0)
		processor.FilterDay(configFile.GetConfig().dayFilter);

	if (!configFile.GetConfig().includePartialIDs)
		processor.FilterPartialIDs();

	if (!configFile.GetConfig().photoFileName.empty())
		processor.ReadPhotoList(configFile.GetConfig().photoFileName);

	// TODO:  species count only?

	if (configFile.GetConfig().auditFrequencyData)
	{
		if (!EBirdDataProcessor::AuditFrequencyData(configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().eBirdApiKey))
		{
			std::cerr << "Audit failed\n";
			return 1;
		}
	}
	else if (configFile.GetConfig().generateRarityScores)
		processor.GenerateRarityScores(configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().listType, configFile.GetConfig().eBirdApiKey,
			configFile.GetConfig().countryFilter, configFile.GetConfig().stateFilter,
			configFile.GetConfig().countyFilter);
	else if (configFile.GetConfig().findMaxNeedsLocations)
	{
		if (!processor.FindBestLocationsForNeededSpecies(
			configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().googleMapsAPIKey, configFile.GetConfig().eBirdApiKey,
			configFile.GetConfig().oAuthClientId, configFile.GetConfig().oAuthClientSecret))
			return 1;
	}
	else if (!configFile.GetConfig().harvestFrequencyData &&
		!configFile.GetConfig().generateTargetCalendar &&
		!configFile.GetConfig().bulkFrequencyUpdate)
	{
		processor.SortData(configFile.GetConfig().primarySort, configFile.GetConfig().secondarySort);

		const std::string list(processor.GenerateList(configFile.GetConfig().listType, configFile.GetConfig().showOnlyPhotoNeeds));
		std::cout << list << std::endl;

		if (!configFile.GetConfig().outputFileName.empty())
		{
			std::ofstream outFile(configFile.GetConfig().outputFileName.c_str());
			if (!outFile.is_open() || !outFile.good())
			{
				std::cerr << "Failed to open '" << configFile.GetConfig().outputFileName << "' for output\n";
				return 1;
			}

			outFile << list << std::endl;
		}
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (configFile.GetConfig().bulkFrequencyUpdate)
	{
		FrequencyDataHarvester harvester;
		if (!harvester.DoBulkFrequencyHarvest(configFile.GetConfig().countryFilter,
			configFile.GetConfig().stateFilter, configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().fipsStart, configFile.GetConfig().eBirdApiKey))
		{
			curl_global_cleanup();
			return 1;
		}
	}
	else
	{
		if (configFile.GetConfig().harvestFrequencyData)
		{
			FrequencyDataHarvester harvester;
			if (!harvester.GenerateFrequencyFile(configFile.GetConfig().countryFilter,
				configFile.GetConfig().stateFilter, configFile.GetConfig().countyFilter,
				configFile.GetConfig().frequencyFilePath, configFile.GetConfig().eBirdApiKey))
			{
				std::cerr << "Failed to generate frequency file\n";
				curl_global_cleanup();
				return 1;
			}
		}

		if (configFile.GetConfig().generateTargetCalendar)
		{
			if (configFile.GetConfig().topBirdCount == 0)
			{
				std::cerr << "Attempting to generate target calendar, but top bird count == 0\n";
				curl_global_cleanup();
				return 1;
			}

			processor.GenerateTargetCalendar(configFile.GetConfig().topBirdCount,
				configFile.GetConfig().outputFileName, configFile.GetConfig().frequencyFilePath,
				configFile.GetConfig().countryFilter, configFile.GetConfig().stateFilter, configFile.GetConfig().countyFilter,
				configFile.GetConfig().recentObservationPeriod,
				configFile.GetConfig().targetInfoFileName, configFile.GetConfig().homeLocation,
				configFile.GetConfig().googleMapsAPIKey, configFile.GetConfig().eBirdApiKey);
		}
	}

	curl_global_cleanup();
	return 0;
}
