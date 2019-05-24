// File:  eBirdDataProcessorApp.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Application class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessorApp.h"
#include "eBirdDataProcessor.h"
#include "ebdpConfigFile.h"
#include "utilities/uString.h"
#include "eBirdDatasetInterface.h"
#include "utilities.h"

// Standard C++ headers
#include <cassert>
#include <locale>

int main(int argc, char *argv[])
{
	// Configure global locale for UTF-8
	// All eBird datasets are UTF-8 encoded
	std::locale::global(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));
	Cout.imbue(std::locale());
	Cerr.imbue(std::locale());
	Cin.imbue(std::locale());

	EBirdDataProcessorApp app;
	return app.Run(argc, argv);
}

int EBirdDataProcessorApp::Run(int argc, char *argv[])
{
	if (argc != 2)
	{
		Cout << "Usage:  " << argv[0] << " <config file name>" << std::endl;
		return 1;
	}

	const UString::String configFileName(UString::ToStringType(argv[1]));
	EBDPConfigFile configFile;
	if (!configFile.ReadConfiguration(configFileName))
		return 1;

	const auto& config(configFile.GetConfig());

	if (!config.eBirdDatasetPath.empty())
	{
		EBirdDatasetInterface dataset;
		if (!config.timeOfDayOutputFile.empty())
		{
			if (config.countryFilter.size() > 1)
			{
				Cerr << "Time-of-day analysis for multiple regions is not supported\n";
				return 1;
			}

			EBirdInterface ebi(config.eBirdApiKey);
			const auto regionCode(ebi.GetRegionCode(config.countryFilter.front(),
				config.stateFilter.empty() ? UString::String() : config.stateFilter.front(),
				config.countyFilter.empty() ? UString::String() : config.countyFilter.front()));
			if (!dataset.ExtractTimeOfDayInfo(config.eBirdDatasetPath,
				config.timeOfDataCommonNames, regionCode, config.splitRegionDataFile))
				return 1;
			if (!dataset.WriteTimeOfDayFiles(config.timeOfDayOutputFile, EBirdDatasetInterface::TimeOfDayPeriod::Week))
				return 1;
		}
		else// Ignore all other options and generate global frequency data
		{
			if (!dataset.ExtractGlobalFrequencyData(config.eBirdDatasetPath, config.splitRegionDataFile))
				return 1;
			if (dataset.WriteFrequencyFiles(config.frequencyFilePath))
				return 1;
		}
		return 0;
	}

	EBirdDataProcessor processor;
	if (!processor.Parse(config.dataFileName))
		return 1;

	if (config.uniqueObservations != EBDPConfig::UniquenessType::None)
		processor.GenerateUniqueObservationsReport(config.uniqueObservations);

	// Remove entires that don't fall withing the specified locations
	if (!config.locationFilter.empty() && config.targetNeedArea == EBDPConfig::TargetNeedArea::None)
		processor.FilterLocation(config.locationFilter, config.countyFilter,
			config.stateFilter, config.countryFilter);
	else if (!config.countyFilter.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Region)
		processor.FilterCounty(config.countyFilter, config.stateFilter,
			config.countryFilter);
	else if (!config.stateFilter.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Subnational1)
		processor.FilterState(config.stateFilter, config.countryFilter);
	else if (!config.countryFilter.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Country)
		processor.FilterCountry(config.countryFilter);

	// Remove entries that don't fall within the specified dates
	if (config.yearFilter > 0)
		processor.FilterYear(config.yearFilter);

	if (config.monthFilter > 0)
		processor.FilterMonth(config.monthFilter);

	if (config.weekFilter > 0)
		processor.FilterWeek(config.weekFilter);

	if (config.dayFilter > 0)
		processor.FilterDay(config.dayFilter);
		
	// Remove entries that don't contain the specified comment string
	if (!config.commentGroupString.empty())
		processor.FilterCommentString(config.commentGroupString);

	// Other filter criteria
	if (!config.includePartialIDs)
		processor.FilterPartialIDs();

	if (!config.mediaListHTML.empty())
	{
		processor.GenerateMediaList(config.mediaListHTML, config.mediaFileName);
		return 0;
	}
	else if (!config.mediaFileName.empty())
		processor.ReadMediaList(config.mediaFileName);

	// TODO:  species count only?

	if (config.generateRarityScores)
	{
		if (config.countryFilter.size() > 1)
		{
			Cerr << "Rarity analysis for multiple regions is not supported\n";
			return 1;
		}

		processor.GenerateRarityScores(config.frequencyFilePath,
			config.listType, config.eBirdApiKey,
			config.countryFilter.front(), config.stateFilter.empty() ? UString::String() : config.stateFilter.front(),
			config.countyFilter.empty() ? UString::String() : config.countyFilter.front());
	}
	else if (config.findMaxNeedsLocations)
	{
		EBirdInterface ebi(config.eBirdApiKey);
		const auto regionCodes(ebi.GetRegionCodes(config.countryFilter,
			config.stateFilter, config.countyFilter));
		if (!processor.FindBestLocationsForNeededSpecies(
			config.frequencyFilePath, config.kmlLibraryPath, config.eBirdApiKey, regionCodes,
			config.highDetailCountries, config.cleanupKMLLocationNames, config.geoJSONPrecision))
			return 1;
	}
    else if (config.generateTargetCalendar)
	{
		if (config.countryFilter.size() > 1)
		{
			Cerr << "Calendar generation for multiple regions is not supported\n";
			return 1;
		}

		processor.GenerateTargetCalendar(config.topBirdCount,
			config.outputFileName, config.frequencyFilePath,
			config.countryFilter.front(), config.stateFilter.empty() ? UString::String() : config.stateFilter.front(),
			config.countyFilter.empty() ? UString::String() : config.countyFilter.front(),
			config.recentObservationPeriod,
			config.targetInfoFileName, config.homeLocation,
			config.googleMapsAPIKey, config.eBirdApiKey);
	}
	else if (config.doComparison)
		processor.DoListComparison();
	else
	{
		processor.SortData(config.primarySort, config.secondarySort);

		const UString::String list(processor.GenerateList(config.listType,
			config.showOnlyPhotoNeeds, config.showOnlyAudioNeeds));
		Cout << list << std::endl;

		if (!config.outputFileName.empty())
		{
			UString::OFStream outFile(config.outputFileName.c_str());
			if (!outFile.is_open() || !outFile.good())
			{
				Cerr << "Failed to open '" << config.outputFileName << "' for output\n";
				return 1;
			}

			outFile << list << std::endl;
		}
	}

	return 0;
}
