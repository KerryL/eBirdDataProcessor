// File:  ebdpConfig.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration structure for eBird Data Processor.

#ifndef EBDP_CONFIG_H_
#define EBDP_CONFIG_H_

struct EBDPConfig
{
	UString::String dataFileName;
	UString::String outputFileName;

	UString::String countryFilter;
	UString::String stateFilter;
	UString::String countyFilter;
	UString::String locationFilter;

	enum class ListType : int
	{
		Life = 0,
		Year,
		Month,
		Week,
		Day,
		SeparateAllObservations
	};

	ListType listType;

	bool generateRarityScores;
	bool speciesCountOnly;// Primarily for plotting
	bool includePartialIDs;
	UString::String timeOfDayOutputFile;
	std::vector<UString::String> timeOfDataCommonNames;
	UString::String splitRegionDataFile;

	unsigned int yearFilter;
	unsigned int monthFilter;
	unsigned int weekFilter;
	unsigned int dayFilter;

	enum class SortBy : int
	{
		None = 0,
		Date,
		CommonName,
		ScientificName,
		TaxonomicOrder
	};

	SortBy primarySort;
	SortBy secondarySort;

	enum class UniquenessType : int
	{
		None = 0,
		ByCountry,
		ByState,
		ByCounty
	};

	UniquenessType uniqueObservations;

	enum class TargetNeedArea : int
	{
		None = 0,
		Region,
		Subnational1,
		Country,
		World
	};

	bool doComparison;

	bool generateTargetCalendar;
	TargetNeedArea targetNeedArea;
	unsigned int topBirdCount;
	unsigned int recentObservationPeriod;// [days]
	UString::String frequencyFilePath;
	UString::String targetInfoFileName;
	bool cleanupKMLLocationNames;
	std::vector<UString::String> highDetailCountries;

	int showOnlyPhotoNeeds;
	int showOnlyAudioNeeds;
	UString::String mediaFileName;
	UString::String mediaListHTML;

	UString::String googleMapsAPIKey;
	UString::String homeLocation;
	UString::String eBirdApiKey;

	bool findMaxNeedsLocations;
	UString::String kmlLibraryPath;

	UString::String oAuthClientId;
	UString::String oAuthClientSecret;

	UString::String eBirdDatasetPath;
	
	UString::String commentGroupString;
};

#endif// EBDP_CONFIG_H_
