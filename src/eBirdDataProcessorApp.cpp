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

#include "kmlLibraryManager.h"
int EBirdDataProcessorApp::Run(int argc, char *argv[])
{
	if (argc != 2)
	{
		Cout << "Usage:  " << argv[0] << " <config file name>" << std::endl;
		return 1;
	}

	const String configFileName(UString::ToStringType(argv[1]));
	EBDPConfigFile configFile;
	if (!configFile.ReadConfiguration(configFileName))
		return 1;

	/*KMLLibraryManager kml(configFile.GetConfig().kmlLibraryPath, configFile.GetConfig().eBirdApiKey, configFile.GetConfig().googleMapsAPIKey, Cout);
	String s;
	s = kml.GetKML(_T("Azerbaijan"), _T("Agcabädi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Salyan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Siyäzän"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Baki"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Länkäran"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Beyläqan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Däväçi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xizi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qax"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Lerik"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Naxçivan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Neftçala"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Masalli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Biläsuvar"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Samux"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yevlax"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Quba"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Haciqabul"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qusar"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Kürdämir"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qäbälä"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Säki"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Ismayilli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xaçmaz"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Samaxi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Abseron"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Imisli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Tovuz"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Balakän"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Sumqayit"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Goranboy"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Sabirabad"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agcabädi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agdas"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Astara"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qazax"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Gädäbäy"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yardimli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Kälbäcär"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Cälilabab"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Daskäsän"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agstafa"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xanlar"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qobustan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Mingäçevir"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Ucar"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agsu"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Susa"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xocali"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Gäncä"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Saatli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Göyçay"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Bärdä"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Zärdab"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agdam"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Cäbrayil"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Füzuli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Laçin"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Länkäran Municipality"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Naftalan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Oguz"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qubadli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Susa Municipality"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Sämkir"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Tärtär"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xankändi"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xocavänd"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yevlax Municipality"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Zaqatala"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Zängilan"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Äli Bayramli"), String());
	s = kml.GetKML(_T("Azerbaijan"), _T("??ki Municipality"), String());
	//kml.GetKML(_T("Finland"), _T("Pirkanmaa"), String());//*/

	if (!configFile.GetConfig().eBirdDatasetPath.empty())// Ignore all other options and generate global frequency data
	{
		EBirdDatasetInterface dataset;
		if (!dataset.ExtractGlobalFrequencyData(configFile.GetConfig().eBirdDatasetPath))
			return 1;
		if (dataset.WriteFrequencyFiles(configFile.GetConfig().frequencyFilePath))
			return 1;
		return 0;
	}

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

	if (!configFile.GetConfig().mediaListHTML.empty())
	{
		processor.GenerateMediaList(configFile.GetConfig().mediaListHTML, configFile.GetConfig().mediaFileName);
		return 0;
	}
	else if (!configFile.GetConfig().mediaFileName.empty())
		processor.ReadMediaList(configFile.GetConfig().mediaFileName);

	// TODO:  species count only?

	if (configFile.GetConfig().generateRarityScores)
		processor.GenerateRarityScores(configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().listType, configFile.GetConfig().eBirdApiKey,
			configFile.GetConfig().countryFilter, configFile.GetConfig().stateFilter,
			configFile.GetConfig().countyFilter);
	else if (configFile.GetConfig().findMaxNeedsLocations)
	{
		if (!processor.FindBestLocationsForNeededSpecies(
			configFile.GetConfig().frequencyFilePath, configFile.GetConfig().kmlLibraryPath,
			configFile.GetConfig().googleMapsAPIKey, configFile.GetConfig().eBirdApiKey,
			configFile.GetConfig().oAuthClientId, configFile.GetConfig().oAuthClientSecret))
			return 1;
	}
    else if (configFile.GetConfig().generateTargetCalendar)
	{
		processor.GenerateTargetCalendar(configFile.GetConfig().topBirdCount,
			configFile.GetConfig().outputFileName, configFile.GetConfig().frequencyFilePath,
			configFile.GetConfig().countryFilter, configFile.GetConfig().stateFilter, configFile.GetConfig().countyFilter,
			configFile.GetConfig().recentObservationPeriod,
			configFile.GetConfig().targetInfoFileName, configFile.GetConfig().homeLocation,
			configFile.GetConfig().googleMapsAPIKey, configFile.GetConfig().eBirdApiKey);
	}
	else if (configFile.GetConfig().doComparison)
		processor.DoListComparison();
	else
	{
		processor.SortData(configFile.GetConfig().primarySort, configFile.GetConfig().secondarySort);

		const String list(processor.GenerateList(configFile.GetConfig().listType,
			configFile.GetConfig().showOnlyPhotoNeeds, configFile.GetConfig().showOnlyAudioNeeds));
		Cout << list << std::endl;

		if (!configFile.GetConfig().outputFileName.empty())
		{
			OFStream outFile(configFile.GetConfig().outputFileName.c_str());
			if (!outFile.is_open() || !outFile.good())
			{
				Cerr << "Failed to open '" << configFile.GetConfig().outputFileName << "' for output\n";
				return 1;
			}

			outFile << list << std::endl;
		}
	}

	return 0;
}
