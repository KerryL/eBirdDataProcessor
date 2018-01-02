// File:  ebdpConfig.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration structure for eBird Data Processor.

#ifndef EBDP_CONFIG_H_
#define EBDP_CONFIG_H_

struct EBDPConfig
{
	std::string dataFileName;
	std::string outputFileName;

	std::string countryFilter;
	std::string stateFilter;
	std::string countyFilter;
	std::string locationFilter;

	bool bulkFrequencyUpdate;

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

	bool generateTargetCalendar;
	bool harvestFrequencyData;
	unsigned int topBirdCount;
	unsigned int recentObservationPeriod;// [days]
	std::string frequencyFileName;
	std::string targetInfoFileName;

	bool showOnlyPhotoNeeds;
	std::string photoFileName;

	std::string googleMapsAPIKey;
	std::string homeLocation;
};

#endif// EBDP_CONFIG_H_
