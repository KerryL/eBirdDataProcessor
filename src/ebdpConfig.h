// File:  ebdpConfig.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration structure for eBird Data Processor.

#ifndef EBDP_CONFIG_H_
#define EBDP_CONFIG_H_

struct LocationFilters
{
	std::vector<UString::String> country;
	std::vector<UString::String> state;
	std::vector<UString::String> county;
	std::vector<UString::String> location;
};

struct TimeFilters
{
	unsigned int year;
	unsigned int month;
	unsigned int week;
	unsigned int day;
};

struct TimeOfDayParameters
{
	UString::String outputFile;
	std::vector<UString::String> commonNames;
	UString::String splitRegionDataFile;
};

struct LocationFindingParameters
{
	bool cleanupKMLLocationNames;
	UString::String kmlLibraryPath;
	double kmlReductionLimit;
	int geoJSONPrecision;
};

struct CalendarParameters
{
	unsigned int topBirdCount;
	unsigned int recentObservationPeriod;// [days]
	UString::String targetInfoFileName;

	UString::String googleMapsAPIKey;
	UString::String homeLocation;
};

struct BestTripParameters
{
	unsigned int topLocationCount;
	double minimumLiklihood;
	unsigned int minimumObservationCount;
};

struct SpeciesHunt
{
	UString::String commonName;
	double latitude;// [deg]
	double longitude;// [deg]
	double radius;// [km]
};

struct EBDPConfig
{
	UString::String dataFileName;
	UString::String outputFileName;

	LocationFilters locationFilters;

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

	enum class TargetNeedArea : int
	{
		None = 0,
		Region,
		Subnational1,
		Country,
		World
	};

	TargetNeedArea targetNeedArea;

	bool generateRarityScores;
	bool speciesCountOnly;// Primarily for plotting
	bool includePartialIDs;

	TimeOfDayParameters timeOfDayParameters;

	TimeFilters timeFilters;

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

	bool doComparison;

	bool generateTargetCalendar;
	CalendarParameters calendarParameters;

	UString::String frequencyFilePath;

	int showOnlyPhotoNeeds;
	int showOnlyAudioNeeds;
	UString::String mediaFileName;
	UString::String mediaListHTML;

	UString::String eBirdApiKey;

	bool findMaxNeedsLocations;
	LocationFindingParameters locationFindingParameters;
	std::vector<UString::String> highDetailCountries;

	bool findBestTripLocations;
	BestTripParameters bestTripParameters;

	UString::String eBirdDatasetPath;
	
	UString::String commentGroupString;

	SpeciesHunt speciesHunt;
};

#endif// EBDP_CONFIG_H_
