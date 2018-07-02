// File:  ebdpConfig.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration structure for eBird Data Processor.

#ifndef EBDP_CONFIG_H_
#define EBDP_CONFIG_H_

struct EBDPConfig
{
	String dataFileName;
	String outputFileName;

	String countryFilter;
	String stateFilter;
	String countyFilter;
	String locationFilter;

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
	String frequencyFilePath;
	String targetInfoFileName;

	int showOnlyPhotoNeeds;
	int showOnlyAudioNeeds;
	String mediaFileName;
	String mediaListHTML;

	String googleMapsAPIKey;
	String homeLocation;
	String eBirdApiKey;

	bool findMaxNeedsLocations;
	String kmlLibraryPath;

	String oAuthClientId;
	String oAuthClientSecret;

	String eBirdDatasetPath;
};

#endif// EBDP_CONFIG_H_
