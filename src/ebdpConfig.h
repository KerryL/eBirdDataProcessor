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

	unsigned int listType;// 0 - life, 1 - year, 2 - month, 3 - week, 4 - day, 5 - separate all observations

	bool speciesCountOnly;// Primarily for plotting
	bool includePartialIDs;

	unsigned int yearFilter;
	unsigned int monthFilter;
	unsigned int weekFilter;
	unsigned int dayFilter;

	unsigned int primarySort;// 0 - none, 1 - date, 2 - common name, 3 - scientific name, 4 - taxonomic order
	unsigned int secondarySort;// 0 - none, 1 - date, 2 - common name, 3 - scientific name, 4 - taxonomic order

	bool generateTargetCalendar;
	unsigned int topBirdCount;
	unsigned int recentObservationPeriod;// [days]
	std::string frequencyFileName;
	std::string targetInfoFileName;

	std::string googleMapsAPIKey;
	std::string homeLocation;
};

#endif// EBDP_CONFIG_H_
