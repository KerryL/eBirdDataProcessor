// File:  eBirdDataProcessorApp.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Application class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessorApp.h"
#include "eBirdDataProcessor.h"
#include "ebdpConfigFile.h"
#include "frequencyDataHarvester.h"
#include "email/oAuth2Interface.h"

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

	// Remove entires that don't fall withing the specified locations
	if (!configFile.GetConfig().locationFilter.empty())
		processor.FilterLocation(configFile.GetConfig().locationFilter, configFile.GetConfig().countyFilter,
			configFile.GetConfig().stateFilter, configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().countyFilter.empty())
		processor.FilterCounty(configFile.GetConfig().countyFilter, configFile.GetConfig().stateFilter,
			configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().stateFilter.empty())
		processor.FilterState(configFile.GetConfig().stateFilter, configFile.GetConfig().countryFilter);
	else if (!configFile.GetConfig().countryFilter.empty())
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

	// TODO:  species count only?

	curl_global_init(CURL_GLOBAL_DEFAULT);

	processor.SortData(static_cast<EBirdDataProcessor::SortBy>(configFile.GetConfig().primarySort),
		static_cast<EBirdDataProcessor::SortBy>(configFile.GetConfig().secondarySort));

	const std::string list(processor.GenerateList(
		static_cast<EBirdDataProcessor::ListType>(configFile.GetConfig().listType)));
	std::cout << list << std::endl;

	if (!configFile.GetConfig().outputFileName.empty())
	{
		std::ofstream outFile(configFile.GetConfig().outputFileName.c_str());
		if (!outFile.is_open() || !outFile.good())
		{
			std::cerr << "Failed to open '" << configFile.GetConfig().outputFileName << "' for output\n";
			curl_global_cleanup();
			return 1;
		}

		outFile << list << std::endl;
	}

	if (configFile.GetConfig().harvestFrequencyData)
	{
		FrequencyDataHarvester harvester;
		if (!harvester.GenerateFrequencyFile(configFile.GetConfig().countryFilter,
			configFile.GetConfig().stateFilter, configFile.GetConfig().countyFilter,
			configFile.GetConfig().frequencyFileName))
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
			configFile.GetConfig().outputFileName, configFile.GetConfig().frequencyFileName,
			configFile.GetConfig().countryFilter, configFile.GetConfig().stateFilter, configFile.GetConfig().countyFilter,
			configFile.GetConfig().recentObservationPeriod,
			configFile.GetConfig().targetInfoFileName, configFile.GetConfig().homeLocation, configFile.GetConfig().googleMapsAPIKey);
	}

	curl_global_cleanup();
	return 0;
}
