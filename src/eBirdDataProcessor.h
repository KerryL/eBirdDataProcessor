// File:  eBirdDataProcessor.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_H_

// Standard C++ headers
#include <string>
#include <vector>
#include <ctime>
#include <sstream>

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
		double latitude;// [deg]
		double longitude;// [deg]
		std::time_t dateTime;
		std::string protocol;
		int duration;// [min]
		bool allObsReported;
		double distanceTraveled;// [km]
		double areaCovered;// [ha]
		int numberOfObservers;
		std::string breedingCode;
		std::string speciesComments;
		std::string checklistComments;
	};

	std::vector<Entry> data;

	static bool ParseLine(const std::string& line, Entry& entry);

	template<typename T>
	static bool ParseToken(std::istringstream& lineStream, const std::string& fieldName, T& target);
	static bool ParseCountToken(std::istringstream& lineStream, const std::string& fieldName, int& target);
	static std::string Sanitize(const std::string& line);
	static std::string Desanitize(const std::string& token);

	static const std::string commaPlaceholder;
};

template<typename T>
bool EBirdDataProcessor::ParseToken(std::istringstream& lineStream, const std::string& fieldName, T& target)
{
	std::string token;
	std::istringstream tokenStream;

	if (!std::getline(lineStream, token, ','))
	{
		/*std::cerr << "Failed to read token for " << fieldName << '\n';
		return false;*/
		// Data file drops trailing empty fields, so if there's no checklist comment, this would return false
		target = T{};
		return true;
	}

	if (token.empty())
	{
		target = T{};
		return true;
	}

	tokenStream.str(Desanitize(token));
	if ((tokenStream >> target).fail())
	{
		std::cerr << "Failed to interpret token for " << fieldName << '\n';
		return false;
	}

	return true;
}

#endif// EBIRD_DATA_PROCESSOR_H_