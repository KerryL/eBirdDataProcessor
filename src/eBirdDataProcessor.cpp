// File:  eBirdDataProcessor.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <fstream>
#include <iostream>

const std::string EBirdDataProcessor::headerLine("Submission ID,Common Name,Scientific Name,"
	"Taxonomic Order,Count,State/Province,County,Location,Latitude,Longitude,Date,Time,"
	"Protocol,Duration (Min),All Obs Reported,Distance Traveled (km),Area Covered (ha),"
	"Number of Observers,Breeding Code,Species Comments,Checklist Comments");
const std::string EBirdDataProcessor::commaPlaceholder("%&!COMMA!&%");

bool EBirdDataProcessor::Parse(const std::string& dataFile)
{
	std::ifstream file(dataFile.c_str());
	if (!file.is_open() || !file.good())
	{
		std::cerr << "Failed to open '" << dataFile << "' for input\n";
		return false;
	}

	std::string line;
	unsigned int lineCount(0);
	while (std::getline(file, line))
	{
		if (lineCount == 0)
		{
			if (line.compare(headerLine) != 0)
			{
				std::cerr << "Unexpected file format\n";
				return false;
			}

			++lineCount;
			continue;
		}

		Entry entry;
		if (!ParseLine(line, entry))
		{
			std::cerr << "Failed to parse line " << lineCount << '\n';
			return false;
		}

		data.push_back(entry);
		++lineCount;
	}

	std::cout << "Parsed " << data.size() << " entries" << std::endl;// TODO:  Count seems to be too big?
	return true;
}

std::string EBirdDataProcessor::Sanitize(const std::string& line)
{
	std::string s(line);
	std::string::size_type nextQuote(0);
	while (nextQuote = s.find('"', nextQuote + 1), nextQuote != std::string::npos)
	{
		const std::string::size_type endQuote(s.find('"', nextQuote + 1));
		if (endQuote != std::string::npos)
		{
			std::string::size_type nextComma(nextQuote);
			while (nextComma = s.find(',', nextComma + 1), nextComma != std::string::npos && nextComma < endQuote)
				s.replace(nextComma, 1, commaPlaceholder);
		}
		nextQuote = endQuote;

		// TODO:  Could also remove quotation marks then
	}

	return s;
}

std::string EBirdDataProcessor::Desanitize(const std::string& token)
{
	std::string s(token);
	std::string::size_type nextPlaceholder(0);
	while (nextPlaceholder = s.find(commaPlaceholder, nextPlaceholder + 1), nextPlaceholder != std::string::npos)
		s.replace(nextPlaceholder, commaPlaceholder.length(), ",");

	return s;
}

bool EBirdDataProcessor::ParseLine(const std::string& line, Entry& entry)
{
	std::istringstream lineStream(Sanitize(line));

	if (!ParseToken(lineStream, "Submission ID", entry.submissionID))
		return false;
	if (!ParseToken(lineStream, "Common Name", entry.commonName))
		return false;
	if (!ParseToken(lineStream, "Scientific Name", entry.scientificName))
		return false;
	if (!ParseToken(lineStream, "Taxonomic Order", entry.taxonomicOrder))
		return false;
	if (!ParseCountToken(lineStream, "Count", entry.count))
		return false;
	if (!ParseToken(lineStream, "State/Providence", entry.stateProvidence))
		return false;
	if (!ParseToken(lineStream, "County", entry.county))
		return false;
	if (!ParseToken(lineStream, "Location", entry.location))
		return false;
	if (!ParseToken(lineStream, "Latitude", entry.latitude))
		return false;
	if (!ParseToken(lineStream, "Longitude", entry.longitude))
		return false;
	if (!ParseToken(lineStream, "Date", entry.dateTime))
		return false;
	if (!ParseToken(lineStream, "Time", entry.dateTime))
		return false;
	if (!ParseToken(lineStream, "Protocol", entry.protocol))
		return false;
	if (!ParseToken(lineStream, "Duration", entry.duration))
		return false;
	if (!ParseToken(lineStream, "All Obs Reported", entry.allObsReported))
		return false;
	if (!ParseToken(lineStream, "Distance Traveled", entry.distanceTraveled))
		return false;
	if (!ParseToken(lineStream, "Area Covered", entry.areaCovered))
		return false;
	if (!ParseToken(lineStream, "Number of Observers", entry.numberOfObservers))
		return false;
	if (!ParseToken(lineStream, "Breeding Code", entry.breedingCode))
		return false;
	if (!ParseToken(lineStream, "Species Comments", entry.speciesComments))
		return false;
	if (!ParseToken(lineStream, "Checklist Comments", entry.checklistComments))
		return false;

	return true;
}

bool EBirdDataProcessor::ParseCountToken(std::istringstream& lineStream, const std::string& fieldName, int& target)
{
	if (lineStream.peek() == 'X')
	{
		target = 1;
		std::string token;
		std::getline(lineStream, token, ',');// This is just to advance the stream pointer
		return true;
	}

	return ParseToken(lineStream, fieldName, target);
}
