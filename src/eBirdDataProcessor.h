// File:  eBirdDataProcessor.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_H_

// Standard C++ headers
#include <string>
#include <vector>
#include <ctime>

class EBirdDataProcessor
{
public:
	bool Parse(const std::string& dataFile);

private:
	static const std::string headerLine;

	struct Entry
	{
		std::string submissionID;
		std::string commonName;
		std::string scientificName;
		int taxonomicOrder;
		int count;
		std::string stateProvidence;
		std::string county;
		std::string location;
		double latitude;
		double longitude;
		std::time_t dateTime;
		std::string protocol;
		int duration;// [min]
		bool completeChecklist;
		double distance;// [km]
		double areaCovered;// [ha]
		int numberOfObservers;
		std::string breedingCode;
		std::string speciesComments;
		std::string checklistComments;
	};

	std::vector<Entry> data;

	bool ParseLine(const std::string& line, Entry& entry);
};

#endif// EBIRD_DATA_PROCESSOR_H_