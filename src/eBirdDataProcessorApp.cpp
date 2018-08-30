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

	const UString::String configFileName(UString::ToStringType(argv[1]));
	EBDPConfigFile configFile;
	if (!configFile.ReadConfiguration(configFileName))
		return 1;

	/*KMLLibraryManager kml(configFile.GetConfig().kmlLibraryPath, configFile.GetConfig().eBirdApiKey, configFile.GetConfig().googleMapsAPIKey, Cout);
	String s;
	s = kml.GetKML(_T("Azerbaijan"), _T("Agcab�di"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Salyan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Siy�z�n"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Baki"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("L�nk�ran"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Beyl�qan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("D�v��i"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xizi"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qax"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Lerik"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Nax�ivan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Neft�ala"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Masalli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Bil�suvar"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Samux"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yevlax"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Quba"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Haciqabul"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qusar"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("K�rd�mir"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Q�b�l�"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("S�ki"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Ismayilli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xa�maz"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Samaxi"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Abseron"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Imisli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Tovuz"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Balak�n"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Sumqayit"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Goranboy"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Sabirabad"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agcab�di"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agdas"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Astara"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qazax"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("G�d�b�y"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yardimli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("K�lb�c�r"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("C�lilabab"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Dask�s�n"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agstafa"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xanlar"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qobustan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Ming��evir"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Ucar"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agsu"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Susa"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xocali"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("G�nc�"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Saatli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("G�y�ay"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("B�rd�"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Z�rdab"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Agdam"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("C�brayil"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("F�zuli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("La�in"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("L�nk�ran Municipality"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Naftalan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Oguz"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Qubadli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Susa Municipality"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("S�mkir"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("T�rt�r"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xank�ndi"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Xocav�nd"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Yevlax Municipality"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Zaqatala"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("Z�ngilan"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("�li Bayramli"), UString::String());
	s = kml.GetKML(_T("Azerbaijan"), _T("??ki Municipality"), UString::String());
	//kml.GetKML(_T("Finland"), _T("Pirkanmaa"), UString::String());//*/

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

		const UString::String list(processor.GenerateList(configFile.GetConfig().listType,
			configFile.GetConfig().showOnlyPhotoNeeds, configFile.GetConfig().showOnlyAudioNeeds));
		Cout << list << std::endl;

		if (!configFile.GetConfig().outputFileName.empty())
		{
			UString::OFStream outFile(configFile.GetConfig().outputFileName.c_str());
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
