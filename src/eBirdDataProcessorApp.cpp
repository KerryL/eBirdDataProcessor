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
#include "mediaHTMLExtractor.h"
#include "observationMapBuilder.h"

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
		if (!config.kmlFilterFileName.empty())
		{
			dataset.ExtractObservationsWithinGeometry(config.eBirdDatasetPath, config.kmlFilterFileName, config.kmlFilteredOutputFileName);
			
			if (!config.observationMapFileName.empty())
			{
				ObservationMapBuilder mapBuilder;
				if (!mapBuilder.Build(config.observationMapFileName, config.kmlFilterFileName, dataset.GetMapInfo()))
					return 1;
			}
		}
		else if (!config.timeOfDayParameters.outputFile.empty())
		{
			if (config.locationFilters.country.size() > 1)
			{
				Cerr << "Time-of-day analysis for multiple regions is not supported\n";
				return 1;
			}

			EBirdInterface ebi(config.eBirdApiKey);
			const auto regionCode(ebi.GetRegionCode(config.locationFilters.country.front(),
				config.locationFilters.state.empty() ? UString::String() : config.locationFilters.state.front(),
				config.locationFilters.county.empty() ? UString::String() : config.locationFilters.county.front()));
			if (!dataset.ExtractTimeOfDayInfo(config.eBirdDatasetPath,
				config.timeOfDayParameters.commonNames, regionCode, config.timeOfDayParameters.splitRegionDataFile))
				return 1;
			if (!dataset.WriteTimeOfDayFiles(config.timeOfDayParameters.outputFile))
				return 1;
		}
		else// Ignore all other options and generate global frequency data
		{
			if (!dataset.ExtractGlobalFrequencyData(config.eBirdDatasetPath, config.timeOfDayParameters.splitRegionDataFile))
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
	if (!config.locationFilters.location.empty() && config.targetNeedArea == EBDPConfig::TargetNeedArea::None)
		processor.FilterLocation(config.locationFilters.location, config.locationFilters.county,
			config.locationFilters.state, config.locationFilters.country);
	else if (!config.locationFilters.county.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Region)
		processor.FilterCounty(config.locationFilters.county, config.locationFilters.state,
			config.locationFilters.country);
	else if (!config.locationFilters.state.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Subnational1)
		processor.FilterState(config.locationFilters.state, config.locationFilters.country);
	else if (!config.locationFilters.country.empty() && config.targetNeedArea <= EBDPConfig::TargetNeedArea::Country)
		processor.FilterCountry(config.locationFilters.country);

	// Remove entries that don't fall within the specified dates
	if (config.timeFilters.year > 0)
		processor.FilterYear(config.timeFilters.year);

	if (config.timeFilters.month > 0)
		processor.FilterMonth(config.timeFilters.month);

	if (config.timeFilters.week > 0)
		processor.FilterWeek(config.timeFilters.week);

	if (config.timeFilters.day > 0)
		processor.FilterDay(config.timeFilters.day);
		
	// Remove entries that don't contain the specified comment string
	if (!config.commentGroupString.empty())
		processor.FilterCommentString(config.commentGroupString);

	// Other filter criteria
	if (!config.includePartialIDs)
		processor.FilterPartialIDs();

	if (!config.mediaListHTML.empty())
	{
		MediaHTMLExtractor htmlExtractor;
		if (!htmlExtractor.ExtractMediaHTML(config.mediaListHTML))
			Cout << "Failed to download current media list HTML; continuing with existing HTML file" << std::endl;
		processor.GenerateMediaList(config.mediaListHTML, config.mediaFileName);
		return 0;
	}
	else if (!config.mediaFileName.empty())
		processor.ReadMediaList(config.mediaFileName);

	// TODO:  species count only?

	if (config.generateRarityScores)
	{
		if (config.locationFilters.country.size() > 1)
		{
			Cerr << "Rarity analysis for multiple regions is not supported\n";
			return 1;
		}

		processor.GenerateRarityScores(config.frequencyFilePath,
			config.listType, config.eBirdApiKey,
			config.locationFilters.country.front(), config.locationFilters.state.empty() ? UString::String() : config.locationFilters.state.front(),
			config.locationFilters.county.empty() ? UString::String() : config.locationFilters.county.front());
	}
	else if (config.findMaxNeedsLocations)
	{
		EBirdInterface ebi(config.eBirdApiKey);
		const auto regionCodes(ebi.GetRegionCodes(config.locationFilters.country,
			config.locationFilters.state, config.locationFilters.county));
		if (!processor.FindBestLocationsForNeededSpecies(
			config.frequencyFilePath, config.locationFindingParameters, config.highDetailCountries, config.eBirdApiKey, regionCodes))
			return 1;
	}
    else if (config.generateTargetCalendar)
	{
		if (config.locationFilters.country.size() > 1)
		{
			Cerr << "Calendar generation for multiple regions is not supported\n";
			return 1;
		}

		processor.GenerateTargetCalendar(config.calendarParameters,
			config.outputFileName, config.frequencyFilePath,
			config.locationFilters.country.front(), config.locationFilters.state.empty() ? UString::String() : config.locationFilters.state.front(),
			config.locationFilters.county.empty() ? UString::String() : config.locationFilters.county.front(), config.eBirdApiKey);
	}
	else if (config.findBestTripLocations)
	{
		EBirdInterface ebi(config.eBirdApiKey);
		const auto regionCodes(ebi.GetRegionCodes(config.locationFilters.country,
			config.locationFilters.state, config.locationFilters.county));
		if (!processor.FindBestTripLocations(config.frequencyFilePath, config.bestTripParameters,
			config.highDetailCountries, regionCodes, config.outputFileName, config.eBirdApiKey))
			return 1;
	}
	else if (!config.speciesHunt.commonName.empty())
	{
		if (!processor.HuntSpecies(config.speciesHunt, config.eBirdApiKey))
			return 1;
	}
	else if (!config.timeOfYearParameters.outputFile.empty())
	{
		EBirdInterface ebi(config.eBirdApiKey);
		const auto regionCodes(ebi.GetRegionCodes(config.locationFilters.country,
			config.locationFilters.state, config.locationFilters.county));
		if (!processor.GenerateTimeOfYearData(config.timeOfYearParameters, config.frequencyFilePath, regionCodes))
			return 1;
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
