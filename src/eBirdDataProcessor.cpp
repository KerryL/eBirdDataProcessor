// File:  eBirdDataProcessor.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessor.h"
#include "googleMapsInterface.h"
#include "bestObservationTimeEstimator.h"

// Standard C++ headers
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <cctype>
#include <functional>
#include <map>
#include <regex>

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
		return !std::regex_search(entry.location, std::regex(location));
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
		return (entry.commonName.find(" sp.") != std::string::npos) ||// Eliminate Spuhs
			(entry.commonName.find('/') != std::string::npos);// Eliminate species1/species2 type entries
	}), data.end());
}

int EBirdDataProcessor::DoComparison(const Entry& a, const Entry& b, const EBDPConfig::SortBy& sortBy)
{
	if (sortBy == EBDPConfig::SortBy::None)
		return 0;

	if (sortBy == EBDPConfig::SortBy::Date)
	{
		std::tm aTime(a.dateTime);
		std::tm bTime(b.dateTime);
		return static_cast<int>(difftime(mktime(&aTime), mktime(&bTime)));
	}
	else if (sortBy == EBDPConfig::SortBy::CommonName)
		return a.commonName.compare(b.commonName);
	else if (sortBy == EBDPConfig::SortBy::ScientificName)
		return a.scientificName.compare(b.scientificName);
	else if (sortBy == EBDPConfig::SortBy::TaxonomicOrder)
		return a.taxonomicOrder - b.taxonomicOrder;

	assert(false);
	return 0;
}

void EBirdDataProcessor::SortData(const EBDPConfig::SortBy& primarySort, const EBDPConfig::SortBy& secondarySort)
{
	if (primarySort == EBDPConfig::SortBy::None && secondarySort == EBDPConfig::SortBy::None)
		return;

	std::sort(data.begin(), data.end(), [primarySort, secondarySort](const Entry& a, const Entry& b)
	{
		int result(DoComparison(a, b, primarySort));
		if (result == 0)
			return DoComparison(a, b, secondarySort) < 0;

		return result < 0;
	});
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::DoConsolidation(const EBDPConfig::ListType& type) const
{
	std::vector<Entry> consolidatedList;
	switch (type)
	{
	case EBDPConfig::ListType::Life:
		return ConsolidateByLife();

	case EBDPConfig::ListType::Year:
		return ConsolidateByYear();

	case EBDPConfig::ListType::Month:
		return ConsolidateByMonth();

	case EBDPConfig::ListType::Week:
		return ConsolidateByWeek();

	case EBDPConfig::ListType::Day:
		return ConsolidateByDay();

	default:
	case EBDPConfig::ListType::SeparateAllObservations:
		break;
	}

	return data;
}

std::string EBirdDataProcessor::GenerateList(const EBDPConfig::ListType& type) const
{
	std::vector<Entry> consolidatedList(DoConsolidation(type));

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
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName);
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);
	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByYear() const
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year;
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByMonth() const
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			a.dateTime.tm_mon == b.dateTime.tm_mon;
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByWeek() const
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
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
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByDay() const
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			a.dateTime.tm_mon == b.dateTime.tm_mon &&
			a.dateTime.tm_mday == b.dateTime.tm_mday;
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);

	return consolidatedList;
}

bool EBirdDataProcessor::GenerateTargetCalendar(const unsigned int& topBirdCount,
	const std::string& outputFileName, const std::string& frequencyFileName,
	const std::string& country, const std::string& state, const std::string& county,
	const unsigned int& recentPeriod,
	const std::string& hotspotInfoFileName, const std::string& homeLocation,
	const std::string& mapApiKey) const
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

	std::set<std::string> consolidatedSpeciesList;
	std::map<std::string, double> speciesFrequencyMap;
	for (i = 0; i < topBirdCount; ++i)
	{
		for (const auto& month : frequencyData)
		{
			consolidatedSpeciesList.insert(month[i].species);
			if (speciesFrequencyMap.find(month[i].species) == speciesFrequencyMap.end())
				speciesFrequencyMap[month[i].species] = month[i].frequency;
			else
				speciesFrequencyMap[month[i].species] = std::max(month[i].frequency, speciesFrequencyMap[month[i].species]);
		}
	}

	std::cout << topBirdCount << " most common species needed for each month of the year includes "
		<< consolidatedSpeciesList.size() << " species" << std::endl;

	std::array<std::pair<double, unsigned int>, 6> bracketCounts;
	bracketCounts[0] = std::make_pair(50.0, 0U);
	bracketCounts[1] = std::make_pair(40.0, 0U);
	bracketCounts[2] = std::make_pair(30.0, 0U);
	bracketCounts[3] = std::make_pair(20.0, 0U);
	bracketCounts[4] = std::make_pair(10.0, 0U);
	bracketCounts[5] = std::make_pair(5.0, 0U);

	for (const auto& species : speciesFrequencyMap)
	{
		for (auto& count : bracketCounts)
		{
			if (species.second > count.first)
			{
				++count.second;
				break;
			}
		}
	}

	for (auto& count : bracketCounts)
	{
		if (count.second > 0)
			std::cout << count.second << " species with frequency > " << count.first << '%' << std::endl;
	}
	std::cout << std::endl;

	RecommendHotspots(consolidatedSpeciesList, country, state, county, recentPeriod, hotspotInfoFileName, homeLocation, mapApiKey);

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
	std::cout << "Reading frequency information from '" << fileName << "'.\n";
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

void EBirdDataProcessor::RecommendHotspots(const std::set<std::string>& consolidatedSpeciesList,
	const std::string& country, const std::string& state, const std::string& county, const unsigned int& recentPeriod,
	const std::string& hotspotInfoFileName, const std::string& homeLocation,
	const std::string& mapApiKey) const
{
	std::cout << "Checking eBird for recent sightings..." << std::endl;

	EBirdInterface e;
	const std::string region(e.GetRegionCode(country, state, county));

	typedef std::vector<std::string> SpeciesList;
	std::map<EBirdInterface::HotspotInfo, SpeciesList, HotspotInfoComparer> hotspotInfo;
	for (const auto& species : consolidatedSpeciesList)
	{
		const std::string scientificName(e.GetScientificNameFromCommonName(species));
		const std::vector<EBirdInterface::HotspotInfo> hotspots(e.GetHotspotsWithRecentObservationsOf(scientificName, region, recentPeriod));
		for (const auto& spot : hotspots)
		{
			hotspotInfo[spot].push_back(species);
		}
	}

	typedef std::pair<SpeciesList, EBirdInterface::HotspotInfo> SpeciesHotspotPair;
	std::vector<SpeciesHotspotPair> sortedHotspots;
	for (const auto& h : hotspotInfo)
		sortedHotspots.push_back(std::make_pair(h.second, h.first));
	std::sort(sortedHotspots.begin(), sortedHotspots.end(), [](const SpeciesHotspotPair& a, const SpeciesHotspotPair& b)
	{
		if (a.first.size() > b.first.size())
			return true;
		return false;
	});

	std::cout << "\nRecommended hotspots for observing needed species:\n";
	const unsigned int minimumHotspotCount(10);
	unsigned int hotspotCount(0);
	unsigned int lastHotspotSpeciesCount(0);
	for (const auto& hotspot : sortedHotspots)
	{
		if (hotspotCount >= minimumHotspotCount && hotspot.first.size() < lastHotspotSpeciesCount)
			break;

		std::cout << "  " << hotspot.second.hotspotName << " (" << hotspot.first.size() << " species)\n";
		++hotspotCount;
		lastHotspotSpeciesCount = hotspot.first.size();
	}
	std::cout << std::endl;

	if (!hotspotInfoFileName.empty())
		GenerateHotspotInfoFile(sortedHotspots, hotspotInfoFileName, homeLocation, mapApiKey, region);
}

void EBirdDataProcessor::GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<std::string>, EBirdInterface::HotspotInfo>>& hotspots,
	const std::string& hotspotInfoFileName, const std::string& homeLocation, const std::string& mapApiKey, const std::string& regionCode) const
{
	std::cout << "Writing hotspot information to file..." << std::endl;

	std::ofstream infoFile(hotspotInfoFileName.c_str());
	if (!infoFile.good() || !infoFile.is_open())
	{
		std::cerr << "Failed to open '" << hotspotInfoFileName << "' for output\n";
		return;
	}

	if (!homeLocation.empty())
		infoFile << "Travel time and distance given from " << homeLocation << '\n';

	for (const auto& h : hotspots)
	{
		infoFile << '\n' << h.second.hotspotName;
		if (!homeLocation.empty())
		{
			GoogleMapsInterface gMaps("eBirdDataProcessor", mapApiKey);
			std::ostringstream ss;
			ss << h.second.latitude << ',' << h.second.longitude;
			GoogleMapsInterface::Directions travelInfo(gMaps.GetDirections(homeLocation, ss.str()));

			if (travelInfo.legs.size() == 0)
				infoFile << " (failed to get directions)";
			else
			{
				double distance(0.0);
				double duration(0.0);
				for (const auto& leg : travelInfo.legs)
				{
					distance += leg.distance.value;
					duration += leg.duration.value;
				}

				infoFile.precision(1);
				infoFile << " (" << std::fixed << distance * 6.213712e-04;
				infoFile << " miles, " << static_cast<int>(duration / 60.0) << " minutes)";
			}
		}

		infoFile << "\nRecently observed target species (" << h.first.size() << "):\n";
		for (const auto& s : h.first)
		{
			EBirdInterface e;
			const unsigned int recentPeriod(30);
			const bool includeProvisional(true);
			const bool hotspotsOnly(false);
			std::vector<EBirdInterface::ObservationInfo> observationInfo(e.GetRecentObservationsOfSpeciesInRegion(
				e.GetScientificNameFromCommonName(s), /*h.second.hotspotID*/regionCode, recentPeriod, includeProvisional, hotspotsOnly));// If we use hotspot ID, we get only the most recent sighting

			std::string bestObservationTime;
			if (observationInfo.size() > 0)
				bestObservationTime = BestObservationTimeEstimator::EstimateBestObservationTime(observationInfo);

			infoFile << "  " << s;

			if (!bestObservationTime.empty())
				infoFile << " (observed " << bestObservationTime << ")\n";
			else
				infoFile << '\n';
		}
	}
}

void EBirdDataProcessor::GenerateUniqueObservationsReport(const EBDPConfig::UniquenessType& type)
{
	typedef bool (*EquivalenceFunction)(const Entry&, const Entry&);
	EquivalenceFunction equivalenceFunction([type]() -> EquivalenceFunction
	{
		if (type == EBDPConfig::UniquenessType::ByCountry)
		{
			return [](const Entry& a, const Entry& b)
			{
				const std::string aCountry(a.stateProvidence.substr(0, 2));
				const std::string bCountry(b.stateProvidence.substr(0, 2));
				return CommonNamesMatch(a.commonName, b.commonName) &&
					aCountry.compare(bCountry) == 0;
			};
		}
		else if (type == EBDPConfig::UniquenessType::ByState)
		{
			return [](const Entry& a, const Entry& b)
			{
				return CommonNamesMatch(a.commonName, b.commonName) &&
					a.stateProvidence.compare(b.stateProvidence) == 0;
			};
		}
		else// if (type == EBDPConfig::UniquenessType::ByCounty)
		{
			return [](const Entry& a, const Entry& b)
			{
				return CommonNamesMatch(a.commonName, b.commonName) &&
					a.stateProvidence.compare(b.stateProvidence) == 0 &&
					a.county.compare(b.county) == 0;
			};
		}
	}());

	if (type == EBDPConfig::UniquenessType::ByCounty)
	{
		std::sort(data.begin(), data.end(), [](const Entry& a, const Entry& b)
		{
			return a.county < b.county;
		});
	}

	std::stable_sort(data.begin(), data.end(), [](const Entry& a, const Entry& b)
	{
		return a.stateProvidence < b.stateProvidence;
	});

	std::stable_sort(data.begin(), data.end(), [](const Entry& a, const Entry& b)
	{
		if (CommonNamesMatch(a.commonName, b.commonName))
			return false;
		return a.commonName < b.commonName;
	});

	StableRemoveDuplicates(data, equivalenceFunction);

	auto endUniqueIt(data.end());
	auto it(data.begin());
	for (; it != endUniqueIt; ++it)
	{
		auto nextIt(it);
		++nextIt;
		if (nextIt == endUniqueIt)
			break;

		if (CommonNamesMatch(it->commonName, nextIt->commonName))
		{
			do
			{
				++nextIt;
			} while (nextIt != endUniqueIt &&
				CommonNamesMatch(it->commonName, nextIt->commonName));

			auto endShift(std::distance(nextIt, it));
			std::rotate(it, nextIt, endUniqueIt);
			std::advance(endUniqueIt, endShift);
			--it;
		}
	}

	data.erase(endUniqueIt, data.end());

	std::cout << "\nUnique observations by ";
	if (type == EBDPConfig::UniquenessType::ByCountry)
		std::cout << "Country:\n";
	else if (type == EBDPConfig::UniquenessType::ByState)
		std::cout << "State:\n";
	else// if (type == EBDPConfig::UniquenessType::ByCounty)
		std::cout << "County:\n";
}

void EBirdDataProcessor::GenerateRarityScores(const std::string& frequencyFileName, const EBDPConfig::ListType& listType)
{
	FrequencyDataYear monthFrequencyData;
	DoubleYear checklistCounts;
	if (!ParseFrequencyFile(frequencyFileName, monthFrequencyData, checklistCounts))
		return;

	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData(
		GenerateYearlyFrequencyData(monthFrequencyData, checklistCounts));

	const auto consolidatedData(DoConsolidation(listType));
	std::vector<EBirdDataProcessor::FrequencyInfo> rarityScoreData(consolidatedData.size());
	unsigned int i;
	for (i = 0; i < rarityScoreData.size(); ++i)
	{
		rarityScoreData[i].species = consolidatedData[i].commonName;
		bool found(false);
		for (const auto& species : yearFrequencyData)
		{
			if (CommonNamesMatch(rarityScoreData[i].species, species.species))
			{
				rarityScoreData[i].frequency = species.frequency;
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::cerr << "Failed to find a match for '" << rarityScoreData[i].species << "' in frequency data.  Try re-generating data on a day that you have not submitted any checklists.\n";
			return;
		}
	}

	std::sort(rarityScoreData.begin(), rarityScoreData.end(), [](const FrequencyInfo& a, const FrequencyInfo& b)
	{
		return a.frequency < b.frequency;
	});

	std::cout << std::endl;
	const unsigned int minSpace(4);
	unsigned int longestName(0);
	for (const auto& entry : rarityScoreData)
	{
		if (entry.species.length() > longestName)
			longestName = entry.species.length();
	}

	for (const auto& entry : rarityScoreData)
		std::cout << std::left << std::setw(longestName + minSpace) << std::setfill(' ') << entry.species << entry.frequency << "%\n";
	std::cout << std::endl;
}

std::vector<EBirdDataProcessor::FrequencyInfo> EBirdDataProcessor::GenerateYearlyFrequencyData(
	const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts)
{
	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData;
	double totalObservations(0.0);

	auto monthCountIterator(checklistCounts.begin());
	for (const auto& monthData : frequencyData)
	{
		for (const auto& species : monthData)
		{
			const double observations(*monthCountIterator * species.frequency);
			bool found(false);
			for (auto& countData : yearFrequencyData)
			{
				if (species.species.compare(countData.species) == 0)
				{
					countData.frequency += observations;
					found = true;
					break;
				}
			}

			if (!found)
				yearFrequencyData.push_back(FrequencyInfo(species.species, observations));
		}

		totalObservations += *monthCountIterator;
		++monthCountIterator;
	}

	for (auto& species : yearFrequencyData)
		species.frequency /= totalObservations;

	return yearFrequencyData;
}

bool EBirdDataProcessor::HotspotInfoComparer::operator()(const EBirdInterface::HotspotInfo& a, const EBirdInterface::HotspotInfo& b) const
{
	return a.hotspotName < b.hotspotName;
}
