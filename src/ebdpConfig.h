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
	
	double radius;
	double latitude;
	double longitude;
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

struct TimeOfYearParameters
{
	UString::String outputFile;
	std::vector<UString::String> commonNames;
	double maxProbability;
};

struct LocationFindingParameters
{
	bool cleanupKMLLocationNames;
	double kmlReductionLimit;
	int geoJSONPrecision;
	UString::String baseOutputFileName;
};

struct CalendarParameters
{
	unsigned int topBirdCount;
	unsigned int recentObservationPeriod;// [days]
	UString::String targetInfoFileName;

	UString::String homeLocation;
};

struct BestTripParameters
{
	unsigned int topLocationCount;
	unsigned int minimumObservationCount;
};

struct SpeciesHunt
{
	UString::String commonName;
	double latitude;// [deg]
	double longitude;// [deg]
	double radius;// [km]
};

struct ApplicationConfiguration
{
	UString::String dataFileName;
	UString::String mediaFileName;
	UString::String frequencyFilePath;
	UString::String eBirdApiKey;
	UString::String kmlLibraryPath;
	UString::String googleMapsAPIKey;
};

struct RegionDetails
{
	unsigned int timePeriodYears;
	unsigned int startMonth;
	unsigned int startDay;
	unsigned int endMonth;
	unsigned int endDay;
};

struct EBDPConfig
{
	ApplicationConfiguration appConfig;
	UString::String eBirdDatasetPath;

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
	TimeOfYearParameters timeOfYearParameters;

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

	int showOnlyPhotoNeeds;
	int showOnlyAudioNeeds;
	UString::String mediaListHTML;
	bool showGaps;

	bool findMaxNeedsLocations;
	LocationFindingParameters locationFindingParameters;
	std::vector<UString::String> highDetailCountries;

	bool findBestTripLocations;
	BestTripParameters bestTripParameters;

	UString::String birdingSpotBubbleDataFileName;

	UString::String kmlFilterFileName;
	UString::String kmlFilteredOutputFileName;
	UString::String observationMapFileName;
	
	UString::String commentGroupString;

	SpeciesHunt speciesHunt;

	RegionDetails regionDetails;
	
	bool buildChecklistLinks;
	UString::String jsDataFileName;

	std::vector<UString::String> bigYear;
};

#endif// EBDP_CONFIG_H_
