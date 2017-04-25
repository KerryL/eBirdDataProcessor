// File:  eBirdDataProcessor.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cassert>

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

	std::cout << "Parsed " << data.size() << " entries" << std::endl;
	return true;
}

std::string EBirdDataProcessor::Sanitize(const std::string& line)
{
	std::string s(line);
	std::string::size_type nextQuote(0);
	while (nextQuote = s.find('"', nextQuote), nextQuote != std::string::npos)
	{
		s.erase(nextQuote, 1);
		const std::string::size_type endQuote(s.find('"', nextQuote));
		s.erase(endQuote, 1);
		if (endQuote != std::string::npos)
		{
			std::string::size_type nextComma(nextQuote);
			while (nextComma = s.find(',', nextComma + 1), nextComma != std::string::npos && nextComma < endQuote)
				s.replace(nextComma, 1, commaPlaceholder);
		}
		nextQuote = endQuote;
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
	if (!ParseDateTimeToken(lineStream, "Date", entry.dateTime, "%m-%d-%Y"))
		return false;
	if (!ParseDateTimeToken(lineStream, "Time", entry.dateTime, "%I:%M %p"))
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
		return std::getline(lineStream, token, ',').good();// This is just to advance the stream pointer
	}

	return ParseToken(lineStream, fieldName, target);
}

bool EBirdDataProcessor::ParseDateTimeToken(std::istringstream& lineStream, const std::string& fieldName,
	std::tm& target, const std::string& format)
{
	std::string token;
	if (!std::getline(lineStream, token, ','))// This advances the stream pointer
	{
		std::cerr << "Failed to read token for " << fieldName << '\n';
		return false;
	}

	std::istringstream ss(token);
	if ((ss >> std::get_time(&target, format.c_str())).fail())
	{
		std::cerr << "Failed to interpret token for " << fieldName << '\n';
		return false;
	}

	return true;
}

bool EBirdDataProcessor::InterpretToken(std::istringstream& tokenStream,
	const std::string& /*fieldName*/, std::string& target)
{
	target = tokenStream.str();
	return true;
}

void EBirdDataProcessor::FilterLocation(const std::string& location, const std::string& county,
	const std::string& state, const std::string& country)
{
	if (!county.empty() || !state.empty() || !country.empty())
		FilterCounty(county, state, country);

	data.erase(std::remove_if(data.begin(), data.end(), [location](const Entry& entry)
	{
		return entry.location.compare(location) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterCounty(const std::string& county,
	const std::string& state, const std::string& country)
{
	if (!state.empty() || !country.empty())
		FilterState(state, country);

	data.erase(std::remove_if(data.begin(), data.end(), [county](const Entry& entry)
	{
		return entry.county.compare(county) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterState(const std::string& state, const std::string& country)
{
	if (!country.empty())
		FilterCountry(country);

	data.erase(std::remove_if(data.begin(), data.end(), [state](const Entry& entry)
	{
		return entry.stateProvidence.substr(3).compare(state) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterCountry(const std::string& country)
{
	data.erase(std::remove_if(data.begin(), data.end(), [country](const Entry& entry)
	{
		return entry.stateProvidence.substr(0, 2).compare(country) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterYear(const unsigned int& year)
{
	data.erase(std::remove_if(data.begin(), data.end(), [year](const Entry& entry)
	{
		return static_cast<unsigned int>(entry.dateTime.tm_year) + 1900U != year;
	}), data.end());
}

void EBirdDataProcessor::FilterMonth(const unsigned int& month)
{
	data.erase(std::remove_if(data.begin(), data.end(), [month](const Entry& entry)
	{
		return static_cast<unsigned int>(entry.dateTime.tm_mon) + 1U != month;
	}), data.end());
}

void EBirdDataProcessor::FilterWeek(const unsigned int& week)
{
	data.erase(std::remove_if(data.begin(), data.end(), [week](const Entry& entry)
	{
		std::stringstream ss;
		ss << std::put_time(&entry.dateTime, "%U");
		unsigned int entryWeek;
		ss >> entryWeek;
		++entryWeek;
		return entryWeek != week;
	}), data.end());
}

void EBirdDataProcessor::FilterDay(const unsigned int& day)
{
	data.erase(std::remove_if(data.begin(), data.end(), [day](const Entry& entry)
	{
		return static_cast<unsigned int>(entry.dateTime.tm_mday) != day;
	}), data.end());
}

void EBirdDataProcessor::DoSort(const SortBy& sortBy)
{
	if (sortBy == SortBy::None)
		return;

	std::sort(data.begin(), data.end(), [sortBy](const Entry& a, const Entry& b)
	{
		switch (sortBy)
		{
		case SortBy::Date:
			return difftime(time_t(&a.dateTime), time_t(&b.dateTime)) < 0.0;

		case SortBy::CommonName:
			return a.commonName.compare(b.commonName) < 0;

		case SortBy::ScientificName:
			return a.scientificName.compare(b.scientificName) < 0;

		case SortBy::TaxonomicOrder:
			return a.taxonomicOrder < b.taxonomicOrder;
		}

		assert(false);
		return true;
	});
}

void EBirdDataProcessor::SortData(const SortBy& primarySort, const SortBy& secondarySort)
{
	DoSort(secondarySort);
	DoSort(primarySort);
}

std::string EBirdDataProcessor::GenerateList() const
{
	std::ostringstream ss;

	for (const auto& entry : data)
		ss << std::put_time(&entry.dateTime, "%D") << ", " << entry.commonName << ", '" << entry.location << "', " << entry.count << "\n";

	return ss.str();
}
