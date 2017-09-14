// File:  eBirdDataProcessor.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessor.h"
#include "eBirdInterface.h"

// Standard C++ headers
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <locale>
#include <cctype>
#include <functional>
#include <map>

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
#ifdef __GNUC__// Bug in g++ (still there in v5.4) cannot parse AM/PM - assume for now that we don't care if we're off by 12 hours?
	if (!ParseDateTimeToken(lineStream, "Time", entry.dateTime, "%I:%M"))
#else
	if (!ParseDateTimeToken(lineStream, "Time", entry.dateTime, "%I:%M %p"))
#endif
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

	// Make sure the data stored in the tm structure is consistent
	mktime(&entry.dateTime);
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

void EBirdDataProcessor::FilterPartialIDs()
{
	data.erase(std::remove_if(data.begin(), data.end(), [](const Entry& entry)
	{
		return (entry.commonName.substr(entry.commonName.length() - 4).compare(" sp.") == 0) ||// Eliminate Spuhs
			(entry.commonName.find('/') != std::string::npos);// Eliminate species1/species2 type entries
	}), data.end());
}

int EBirdDataProcessor::DoComparison(const Entry& a, const Entry& b, const SortBy& sortBy)
{
	if (sortBy == SortBy::None)
		return 0;

	if (sortBy == SortBy::Date)
	{
		std::tm aTime(a.dateTime);
		std::tm bTime(b.dateTime);
		return static_cast<int>(difftime(mktime(&aTime), mktime(&bTime)));
	}
	else if (sortBy == SortBy::CommonName)
		return a.commonName.compare(b.commonName);
	else if (sortBy == SortBy::ScientificName)
		return a.scientificName.compare(b.scientificName);
	else if (sortBy == SortBy::TaxonomicOrder)
		return a.taxonomicOrder - b.taxonomicOrder;

	assert(false);
	return 0;
}

void EBirdDataProcessor::SortData(const SortBy& primarySort, const SortBy& secondarySort)
{
	if (primarySort == SortBy::None && secondarySort == SortBy::None)
		return;

	std::sort(data.begin(), data.end(), [primarySort, secondarySort](const Entry& a, const Entry& b)
	{
		int result(DoComparison(a, b, primarySort));
		if (result == 0)
			return DoComparison(a, b, secondarySort) < 0;

		return result < 0;
	});
}

std::string EBirdDataProcessor::GenerateList(const ListType& type) const
{
	std::vector<Entry> consolidatedList;
	switch (type)
	{
	case ListType::Life:
		consolidatedList = ConsolidateByLife();
		break;

	case ListType::Year:
		consolidatedList = ConsolidateByYear();
		break;

	case ListType::Month:
		consolidatedList = ConsolidateByMonth();
		break;

	case ListType::Week:
		consolidatedList = ConsolidateByWeek();
		break;

	case ListType::Day:
		consolidatedList = ConsolidateByDay();
		break;

	default:
	case ListType::SeparateAllObservations:
		consolidatedList = data;
	}

	std::ostringstream ss;
	unsigned int count(1);
	for (const auto& entry : consolidatedList)
		ss << count++ << ", " << std::put_time(&entry.dateTime, "%D") << ", "
		<< entry.commonName << ", '" << entry.location << "', " << entry.count << "\n";

	return ss.str();
}

bool EBirdDataProcessor::CommonNamesMatch(std::string a, std::string b)
{
	return Trim(StripParentheses(a)).compare(Trim(StripParentheses(b))) == 0;
}

std::string EBirdDataProcessor::StripParentheses(std::string s)
{
	std::string::size_type openParen;
	while (openParen = s.find('('), openParen != std::string::npos)
	{
		std::string::size_type closeParen(s.find(')', openParen));
		if (closeParen != std::string::npos)
			s.erase(s.begin() + openParen, s.begin() + closeParen + 1);
	}

	return s;
}

std::string EBirdDataProcessor::Trim(std::string s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));

    s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

	return s;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByLife() const
{
	std::vector<Entry> consolidatedList(data);
	consolidatedList.erase(UnsortedUnique(consolidatedList.begin(), consolidatedList.end(), [](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName);
	}), consolidatedList.end());

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByYear() const
{
	std::vector<Entry> consolidatedList(data);
	consolidatedList.erase(UnsortedUnique(consolidatedList.begin(), consolidatedList.end(), [](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year;
	}), consolidatedList.end());

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByMonth() const
{
	std::vector<Entry> consolidatedList(data);
	consolidatedList.erase(UnsortedUnique(consolidatedList.begin(), consolidatedList.end(), [](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			a.dateTime.tm_mon == b.dateTime.tm_mon;
	}), consolidatedList.end());

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByWeek() const
{
	std::vector<Entry> consolidatedList(data);
	consolidatedList.erase(UnsortedUnique(consolidatedList.begin(), consolidatedList.end(), [](const Entry& a, const Entry& b)
	{
		unsigned int aWeek, bWeek;
		std::stringstream ss;
		ss << std::put_time(&a.dateTime, "%U");
		ss >> aWeek;
		ss.clear();
		ss.str("");
		ss << std::put_time(&b.dateTime, "%U");
		ss >> bWeek;

		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			aWeek == bWeek;
	}), consolidatedList.end());

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByDay() const
{
	std::vector<Entry> consolidatedList(data);
	consolidatedList.erase(UnsortedUnique(consolidatedList.begin(), consolidatedList.end(), [](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			a.dateTime.tm_mon == b.dateTime.tm_mon &&
			a.dateTime.tm_mday == b.dateTime.tm_mday;
	}), consolidatedList.end());

	return consolidatedList;
}

bool EBirdDataProcessor::GenerateTargetCalendar(const unsigned int& topBirdCount,
	const std::string& outputFileName, const std::string& frequencyFileName,
	const std::string& country, const std::string& state, const std::string& county) const
{
	FrequencyDataYear frequencyData;
	DoubleYear checklistCounts;
	if (!ParseFrequencyFile(frequencyFileName, frequencyData, checklistCounts))
		return false;

	//GuessChecklistCounts(frequencyData, checklistCounts);

	EliminateObservedSpecies(frequencyData);

	for (auto& month : frequencyData)
	{
		std::sort(month.begin(), month.end(), [](const FrequencyInfo& a, const FrequencyInfo& b)
		{
			if (a.frequency > b.frequency)// Most frequent birds first
				return true;
			return false;
		});
	}

	std::cout << "Writing calendar data to " << outputFileName.c_str() << std::endl;

	std::ofstream outFile(outputFileName.c_str());
	if (!outFile.good() || !outFile.is_open())
	{
		std::cerr << "Failed to open '" << outputFileName << "' for output\n";
		return false;
	}

	outFile << "January (" << checklistCounts[0] << " checklists),"
		<< "February (" << checklistCounts[1] << " checklists),"
		<< "March (" << checklistCounts[2] << " checklists),"
		<< "April (" << checklistCounts[3] << " checklists),"
		<< "May (" << checklistCounts[4] << " checklists),"
		<< "June (" << checklistCounts[5] << " checklists),"
		<< "July (" << checklistCounts[6] << " checklists),"
		<< "August (" << checklistCounts[7] << " checklists),"
		<< "September (" << checklistCounts[8] << " checklists),"
		<< "October (" << checklistCounts[9] << " checklists),"
		<< "November (" << checklistCounts[10] << " checklists),"
		<< "December (" << checklistCounts[11] << " checklists),";
	outFile << std::endl;

	unsigned int i;
	for (i = 0; i < topBirdCount; ++i)
	{
		for (const auto& month : frequencyData)
		{
			if (i < month.size())
				outFile << month[i].species << " (" << month[i].frequency << " %)";

			outFile << ',';
		}
		outFile << std::endl;
	}

	RecommendHotspots(frequencyData, topBirdCount, country, state, county);

	return true;
}

void EBirdDataProcessor::GuessChecklistCounts(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts)
{
	DoubleYear guessedCounts;
	unsigned int i;
	for (i = 0; i < frequencyData.size(); ++i)
	{
		guessedCounts[i] = [frequencyData, i]()
		{
			auto it(frequencyData[i].crbegin());
			for (; it != frequencyData[i].crend(); ++it)
			{
				if (it->frequency > 0.0)
					return 100.0 / it->frequency;
			}
			return 0.0;
		}();
	}

	for (i = 0; i < frequencyData.size(); ++i)
	{
		std::vector<double> deltas;
		deltas.resize(frequencyData[i].size() - 1);
		unsigned int j;
		for (j = 1; j < frequencyData[i].size(); ++j)
			deltas[j - 1] = frequencyData[i][j - 1].frequency - frequencyData[i][j].frequency;

		std::sort(deltas.begin(), deltas.end());
		for (const auto & d : deltas)
		{
			if (d > 0.0)
			{
				if (d < guessedCounts[i])
					guessedCounts[i] = deltas.front();
				break;
			}
		}
	}

	std::cout << "Estimated\tActual\n";
	for (i = 0; i < checklistCounts.size(); ++i)
		std::cout << static_cast<int>(guessedCounts[i] + 0.5) << "\t\t" << checklistCounts[i] << '\n';

	std::cout << std::endl;
}

void EBirdDataProcessor::EliminateObservedSpecies(FrequencyDataYear& frequencyData) const
{
	for (auto& month : frequencyData)
	{
		month.erase(std::remove_if(month.begin(), month.end(), [this](const FrequencyInfo& f)
		{
			const auto& speciesIterator(std::find_if(data.begin(), data.end(), [f](const Entry& e)
			{
				return Trim(StripParentheses(f.species)).compare(Trim(StripParentheses(e.commonName))) == 0;
			}));
			return speciesIterator != data.end();
		}), month.end());
	}
}

bool EBirdDataProcessor::ParseFrequencyFile(const std::string& fileName,
	FrequencyDataYear& frequencyData, DoubleYear& checklistCounts)
{
	std::cout << "Readying frequency information from '" << fileName << "'.\n";
	std::ifstream frequencyFile(fileName.c_str());
	if (!frequencyFile.good() || !frequencyFile.is_open())
	{
		std::cerr << "Failed to open '" << fileName << "' for input.\n";
		return false;
	}

	std::string line;
	if (!std::getline(frequencyFile, line))
	{
		std::cerr << "Failed to read header line\n";
		return false;
	}

	if (!ParseFrequencyHeaderLine(line, checklistCounts))
		return false;

	if (!std::getline(frequencyFile, line))
	{
		std::cerr << "Failed to read second header line\n";
		return false;
	}

	while (std::getline(frequencyFile, line))
	{
		if (!ParseFrequencyLine(line, frequencyData))
			return false;
	}

	return true;
}

bool EBirdDataProcessor::ParseFrequencyHeaderLine(const std::string& line, DoubleYear& checklistCounts)
{
	std::istringstream ss(line);
	for (auto& count : checklistCounts)
	{
		std::string monthUnused;
		if (!ParseToken(ss, "Checklist Month", monthUnused))
			return false;
		if (!ParseToken(ss, "Checklist Count", count))
			return false;
	}

	return true;
}

bool EBirdDataProcessor::ParseFrequencyLine(const std::string& line, FrequencyDataYear& frequencyData)
{
	std::istringstream ss(line);
	for (auto& month : frequencyData)
	{
		std::string species;
		double frequency;
		if (!ParseToken(ss, "Species", species))
			return false;
		if (!ParseToken(ss, "Frequency", frequency))
			return false;
		if (!species.empty())
		{
			if (frequency < 0.0)
			{
				std::cerr << "Unexpected frequency data\n";
				return false;
			}

			month.push_back(FrequencyInfo(species, frequency));
		}
	}

	return true;
}

void EBirdDataProcessor::RecommendHotspots(const FrequencyDataYear& frequencyData,const unsigned int& topCount,
	const std::string& country, const std::string& state, const std::string& county) const
{
	std::cout << "Check eBird for recent sightings..." << std::endl;

	EBirdInterface e;
	const std::string region(e.GetRegionCode(country, state, county));

	std::map<std::string, unsigned int> hotspotScores;
	unsigned int i;
	for (i = 0; i < topCount; ++i)
	{
		for (const auto& month : frequencyData)
		{
			if (i < month.size())
			{
				const std::string scientificName(e.GetScientificNameFromCommonName(month[i].species));
				const std::vector<std::string> hotspots(e.GetHotspotsWithRecentObservationsOf(scientificName, region));
				for (const auto& spot : hotspots)
				{
					if (hotspotScores.find(spot) == hotspotScores.end())
						hotspotScores[spot] = 1;
					else
						++hotspotScores[spot];
				}
			}
		}
	}

	std::vector<std::pair<unsigned int, std::string>> sortedHotspots;
	for (const auto& h : hotspotScores)
		sortedHotspots.push_back(std::make_pair(h.second, h.first));
	std::sort(sortedHotspots.begin(), sortedHotspots.end(), [](const std::pair<unsigned int, std::string>& a, const std::pair<unsigned int, std::string>& b)
	{
		if (a.first > b.first)
			return true;
		return false;
	});

	std::cout << "\nRecommended hotspots for observing needed species:\n";
	for (i = 0; i < topCount; ++i)
		std::cout << "  " << sortedHotspots[i].second << std::endl;
}
