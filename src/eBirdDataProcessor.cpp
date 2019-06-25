// File:  eBirdDataProcessor.cpp
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

// Local headers
#include "eBirdDataProcessor.h"
#include "googleMapsInterface.h"
#include "bestObservationTimeEstimator.h"
#include "mapPageGenerator.h"
#include "frequencyFileReader.h"
#include "utilities.h"
#include "stringUtilities.h"

// System headers (added from https://github.com/tronkko/dirent/ for Windows)
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4505)
#endif
#include <dirent.h>
#ifdef _WIN32
#pragma warning(pop)
#endif
#include <sys/stat.h>

// Standard C++ headers
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <map>
#include <chrono>
#include <set>

const UString::String EBirdDataProcessor::headerLine(_T("Submission ID,Common Name,Scientific Name,"
	"Taxonomic Order,Count,State/Province,County,Location,Latitude,Longitude,Date,Time,"
	"Protocol,Duration (Min),All Obs Reported,Distance Traveled (km),Area Covered (ha),"
	"Number of Observers,Breeding Code,Species Comments,Checklist Comments"));

bool EBirdDataProcessor::Parse(const UString::String& dataFile)
{
	UString::IFStream file(dataFile.c_str());
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << dataFile << "' for input\n";
		return false;
	}

	UString::String line;
	unsigned int lineCount(0);
	while (std::getline(file, line))
	{
		if (lineCount == 0)
		{
			if (line.compare(headerLine) != 0)
			{
				Cerr << "Unexpected file format\n";
				return false;
			}

			++lineCount;
			continue;
		}

		Entry entry;
		if (!ParseLine(line, entry))
		{
			Cerr << "Failed to parse line " << lineCount << '\n';
			return false;
		}

		data.push_back(entry);
		++lineCount;
	}

	Cout << "Parsed " << data.size() << " entries" << std::endl;
	return true;
}

bool EBirdDataProcessor::ParseLine(const UString::String& line, Entry& entry)
{
	UString::IStringStream lineStream(line);

	if (!ParseToken(lineStream, _T("Submission ID"), entry.submissionID))
		return false;
	if (!ParseToken(lineStream, _T("Common Name"), entry.commonName))
		return false;
	if (!ParseToken(lineStream, _T("Scientific Name"), entry.scientificName))
		return false;
	if (!ParseToken(lineStream, _T("Taxonomic Order"), entry.taxonomicOrder))
		return false;
	if (!ParseCountToken(lineStream, _T("Count"), entry.count))
		return false;
	if (!ParseToken(lineStream, _T("State/Providence"), entry.stateProvidence))
		return false;
	if (!ParseToken(lineStream, _T("County"), entry.county))
		return false;
	if (!ParseToken(lineStream, _T("Location"), entry.location))
		return false;
	if (!ParseToken(lineStream, _T("Latitude"), entry.latitude))
		return false;
	if (!ParseToken(lineStream, _T("Longitude"), entry.longitude))
		return false;
	if (!ParseDateTimeToken(lineStream, _T("Date"), entry.dateTime, _T("%Y-%m-%d")))
		return false;
#ifdef __GNUC__// Bug in g++ (still there in v5.4) cannot parse AM/PM - assume for now that we don't care if we're off by 12 hours?
	if (!ParseDateTimeToken(lineStream, _T("Time"), entry.dateTime, _T("%I:%M")))
#else
	if (!ParseDateTimeToken(lineStream, _T("Time"), entry.dateTime, _T("%I:%M %p")))
#endif
		return false;
	if (!ParseToken(lineStream, _T("Protocol"), entry.protocol))
		return false;
	if (!ParseToken(lineStream, _T("Duration"), entry.duration))
		return false;
	if (!ParseToken(lineStream, _T("All Obs Reported"), entry.allObsReported))
		return false;
	if (!ParseToken(lineStream, _T("Distance Traveled"), entry.distanceTraveled))
		return false;
	if (!ParseToken(lineStream, _T("Area Covered"), entry.areaCovered))
		return false;
	if (!ParseToken(lineStream, _T("Number of Observers"), entry.numberOfObservers))
		return false;
	if (!ParseToken(lineStream, _T("Breeding Code"), entry.breedingCode))
		return false;
	if (!ParseToken(lineStream, _T("Species Comments"), entry.speciesComments))
		return false;
	if (!ParseToken(lineStream, _T("Checklist Comments"), entry.checklistComments))
		return false;

	// Make sure the data stored in the tm structure is consistent
	entry.dateTime.tm_sec = 0;
	entry.dateTime.tm_isdst = -1;// Let locale determine if DST is in effect
	mktime(&entry.dateTime);

	entry.compareString = PrepareForComparison(entry.commonName);

	return true;
}

bool EBirdDataProcessor::ParseCountToken(UString::IStringStream& lineStream, const UString::String& fieldName, int& target)
{
	if (lineStream.peek() == 'X')
	{
		target = 1;
		UString::String token;
		return std::getline(lineStream, token, UString::Char(',')).good();// This is just to advance the stream pointer
	}

	return ParseToken(lineStream, fieldName, target);
}

bool EBirdDataProcessor::ParseDateTimeToken(UString::IStringStream& lineStream, const UString::String& fieldName,
	std::tm& target, const UString::String& format)
{
	UString::String token;
	if (!std::getline(lineStream, token, UString::Char(',')))// This advances the stream pointer
	{
		Cerr << "Failed to read token for " << fieldName << '\n';
		return false;
	}

	UString::IStringStream ss(token);
	if ((ss >> std::get_time(&target, format.c_str())).fail())
	{
		Cerr << "Failed to interpret token for " << fieldName << '\n';
		return false;
	}

	return true;
}

bool EBirdDataProcessor::InterpretToken(UString::IStringStream& tokenStream,
	const UString::String& /*fieldName*/, UString::String& target)
{
	target = tokenStream.str();
	return true;
}

void EBirdDataProcessor::FilterLocation(const std::vector<UString::String>& locations, const std::vector<UString::String>& counties,
	const std::vector<UString::String>& states, const std::vector<UString::String>& countries)
{
	if (!counties.empty() || !states.empty() || !countries.empty())
		FilterCounty(counties, states, countries);

	data.erase(std::remove_if(data.begin(), data.end(), [&locations](const Entry& entry)
	{
		for (const auto& location : locations)
		{
			if (std::regex_search(entry.location, UString::RegEx(location)))
				return false;
		}
		return true;
	}), data.end());
}

void EBirdDataProcessor::FilterCounty(const std::vector<UString::String>& counties,
	const std::vector<UString::String>& states, const std::vector<UString::String>& countries)
{
	if (!states.empty() || !countries.empty())
		FilterState(states, countries);

	data.erase(std::remove_if(data.begin(), data.end(), [&counties](const Entry& entry)
	{
		return !Utilities::ItemIsInVector(entry.county, counties);
	}), data.end());
}

void EBirdDataProcessor::FilterState(const std::vector<UString::String>& states, const std::vector<UString::String>& countries)
{
	if (!countries.empty())
		FilterCountry(countries);

	data.erase(std::remove_if(data.begin(), data.end(), [&states](const Entry& entry)
	{
		return !Utilities::ItemIsInVector(entry.stateProvidence.substr(3), states);
	}), data.end());
}

void EBirdDataProcessor::FilterCountry(const std::vector<UString::String>& countries)
{
	data.erase(std::remove_if(data.begin(), data.end(), [&countries](const Entry& entry)
	{
		return !Utilities::ItemIsInVector(entry.stateProvidence.substr(0, 2), countries);
	}), data.end());
}

void EBirdDataProcessor::FilterYear(const unsigned int& year)
{
	FilterYear(year, data);
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
		UString::StringStream ss;
		ss << std::put_time(&entry.dateTime, _T("%U"));
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

void EBirdDataProcessor::FilterCommentString(const UString::String& commentString)
{
	data.erase(std::remove_if(data.begin(), data.end(), [&commentString](const Entry& entry)
	{
		return entry.checklistComments.find(commentString) == UString::String::npos;
	}), data.end());
}

void EBirdDataProcessor::FilterPartialIDs()
{
	data.erase(std::remove_if(data.begin(), data.end(), [](const Entry& entry)
	{
		return entry.commonName.find(_T(" sp.")) != std::string::npos ||// Eliminate Spuhs
			entry.commonName.find(UString::Char('/')) != std::string::npos ||// Eliminate species1/species2 type entries
			entry.commonName.find(_T("hybrid")) != std::string::npos ||// Eliminate hybrids
			entry.commonName.find(_T("Domestic")) != std::string::npos;// Eliminate domestic birds
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
	{
		if (a.taxonomicOrder == b.taxonomicOrder)
			return 0;
		else if (a.taxonomicOrder < b.taxonomicOrder)
			return -1;
		return 1;
	}

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

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::RemoveHighMediaScores(
	const int& minPhotoScore, const int& minAudioScore, const std::vector<Entry>& data)
{
	std::vector<Entry> sublist(data);
	std::set<UString::String> haveMediaSet;
	std::for_each(sublist.begin(), sublist.end(), [&minPhotoScore, &minAudioScore, &haveMediaSet](const Entry& entry)
	{
		if (entry.photoRating >= minPhotoScore && entry.audioRating >= minAudioScore)
			haveMediaSet.insert(entry.compareString);
	});
	sublist.erase(std::remove_if(sublist.begin(), sublist.end(), [&haveMediaSet](const Entry& entry)
	{
		return haveMediaSet.find(entry.compareString) != haveMediaSet.end();
	}), sublist.end());

	return sublist;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::DoConsolidation(const EBDPConfig::ListType& type, const std::vector<Entry>& data)
{
	std::vector<Entry> consolidatedList;
	switch (type)
	{
	case EBDPConfig::ListType::Life:
		return ConsolidateByLife(data);

	case EBDPConfig::ListType::Year:
		return ConsolidateByYear(data);

	case EBDPConfig::ListType::Month:
		return ConsolidateByMonth(data);

	case EBDPConfig::ListType::Week:
		return ConsolidateByWeek(data);

	case EBDPConfig::ListType::Day:
		return ConsolidateByDay(data);

	default:
	case EBDPConfig::ListType::SeparateAllObservations:
		break;
	}

	return data;
}

UString::String EBirdDataProcessor::GenerateList(const EBDPConfig::ListType& type,
	const int& minPhotoScore, const int& minAudioScore) const
{
	std::vector<Entry> consolidatedList;
	if (minPhotoScore >= 0 || minAudioScore >= 0)
		consolidatedList = RemoveHighMediaScores(minPhotoScore, minAudioScore, data);
	else
		consolidatedList = data;
	consolidatedList = DoConsolidation(type, consolidatedList);

	if (minPhotoScore >= 0)
		Cout << "Showing only species which do not have photo rated " << minPhotoScore << " or higher\n";
	if (minAudioScore >= 0)
		Cout << "Showing only species which do not have audio rated " << minAudioScore << " or higher\n";

	UString::OStringStream ss;
	unsigned int count(1);
	for (const auto& entry : consolidatedList)
	{
		ss << count++ << ", " << std::put_time(&entry.dateTime, _T("%D")) << ", "
			<< entry.commonName << ", '" << entry.location << "', " << entry.count;

		if (entry.photoRating >= 0)
			ss << " (photo rating = " << entry.photoRating << ')';
		if (entry.audioRating >= 0)
			ss << " (audio rating = " << entry.audioRating << ')';

		ss << '\n';
	}

	return ss.str();
}

bool EBirdDataProcessor::CommonNamesMatch(UString::String a, UString::String b)
{
	return PrepareForComparison(a).compare(PrepareForComparison(b)) == 0;
}

UString::String EBirdDataProcessor::StripParentheses(UString::String s)
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

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByLife(const std::vector<Entry>& data)
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		return CommonNamesMatch(a.commonName, b.commonName);
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);
	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByYear(const std::vector<Entry>& data)
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

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByMonth(const std::vector<Entry>& data)
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

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByWeek(const std::vector<Entry>& data)
{
	auto equivalencePredicate([](const Entry& a, const Entry& b)
	{
		unsigned int aWeek, bWeek;
		UString::StringStream ss;
		ss << std::put_time(&a.dateTime, _T("%U"));
		ss >> aWeek;
		ss.clear();
		ss.str(UString::String());
		ss << std::put_time(&b.dateTime, _T("%U"));
		ss >> bWeek;

		return CommonNamesMatch(a.commonName, b.commonName) &&
			a.dateTime.tm_year == b.dateTime.tm_year &&
			aWeek == bWeek;
	});

	std::vector<Entry> consolidatedList(data);
	StableRemoveDuplicates(consolidatedList, equivalencePredicate);

	return consolidatedList;
}

std::vector<EBirdDataProcessor::Entry> EBirdDataProcessor::ConsolidateByDay(const std::vector<Entry>& data)
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
	const UString::String& outputFileName, const UString::String& frequencyFilePath,
	const UString::String& country, const UString::String& state, const UString::String& county,
	const unsigned int& recentPeriod,
	const UString::String& hotspotInfoFileName, const UString::String& homeLocation,
	const UString::String& mapApiKey, const UString::String& eBirdApiKey) const
{
	FrequencyFileReader frequencyFileReader(frequencyFilePath);
	EBirdInterface ebi(eBirdApiKey);
	FrequencyDataYear frequencyData;
	DoubleYear checklistCounts;
	if (!frequencyFileReader.ReadRegionData(ebi.GetRegionCode(country, state, county), frequencyData, checklistCounts))
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

	Cout << "Writing calendar data to " << outputFileName.c_str() << std::endl;

	UString::OFStream outFile(outputFileName.c_str());
	if (!outFile.good() || !outFile.is_open())
	{
		Cerr << "Failed to open '" << outputFileName << "' for output\n";
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

	std::set<UString::String> consolidatedSpeciesList;
	std::map<UString::String, double> speciesFrequencyMap;
	for (i = 0; i < topBirdCount; ++i)
	{
		for (const auto& month : frequencyData)
		{
			if (i >= month.size())
				continue;

			consolidatedSpeciesList.insert(month[i].species);
			if (speciesFrequencyMap.find(month[i].species) == speciesFrequencyMap.end())
				speciesFrequencyMap[month[i].species] = month[i].frequency;
			else
				speciesFrequencyMap[month[i].species] = std::max(month[i].frequency, speciesFrequencyMap[month[i].species]);
		}
	}

	Cout << topBirdCount << " most common species needed for each month of the year includes "
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
			Cout << count.second << " species with frequency > " << count.first << '%' << std::endl;
	}
	Cout << std::endl;

	RecommendHotspots(consolidatedSpeciesList, country, state, county, recentPeriod,
		hotspotInfoFileName, homeLocation, mapApiKey, eBirdApiKey);

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

	Cout << "Estimated\tActual\n";
	for (i = 0; i < checklistCounts.size(); ++i)
		Cout << static_cast<int>(guessedCounts[i] + 0.5) << "\t\t" << checklistCounts[i] << '\n';

	Cout << std::endl;
}

void EBirdDataProcessor::EliminateObservedSpecies(FrequencyDataYear& frequencyData) const
{
	for (auto& month : frequencyData)
	{
		month.erase(std::remove_if(month.begin(), month.end(), [this](const FrequencyInfo& f)
		{
			const auto& speciesIterator(std::find_if(data.begin(), data.end(), [f](const Entry& e)
			{
				return f.compareString.compare(e.compareString) == 0;
			}));
			return speciesIterator != data.end();
		}), month.end());
	}
}

void EBirdDataProcessor::RecommendHotspots(const std::set<UString::String>& consolidatedSpeciesList,
	const UString::String& country, const UString::String& state, const UString::String& county, const unsigned int& recentPeriod,
	const UString::String& hotspotInfoFileName, const UString::String& homeLocation,
	const UString::String& mapApiKey, const UString::String& eBirdApiKey) const
{
	Cout << "Checking eBird for recent sightings..." << std::endl;

	EBirdInterface e(eBirdApiKey);
	const UString::String region(e.GetRegionCode(country, state, county));
	std::set<UString::String> recentSpecies;

	typedef std::vector<UString::String> SpeciesList;
	std::map<EBirdInterface::LocationInfo, SpeciesList, HotspotInfoComparer> hotspotInfo;
	for (const auto& species : consolidatedSpeciesList)
	{
		const UString::String speciesCode(e.GetSpeciesCodeFromCommonName(species));
		if (speciesCode.empty())
		{
			Cout << "Warning:  Species code for " << species << " is blank.  Are your frequency/name data up-to-date?" << std::endl;
			continue;
		}

		const auto hotspots(e.GetHotspotsWithRecentObservationsOf(speciesCode, region, recentPeriod));
		for (const auto& spot : hotspots)
		{
			hotspotInfo[spot].push_back(species);
			recentSpecies.emplace(species);
		}
	}

	Cout << recentSpecies.size() << " needed species have been observed within the last " << recentPeriod << " days" << std::endl;

	typedef std::pair<SpeciesList, EBirdInterface::LocationInfo> SpeciesHotspotPair;
	std::vector<SpeciesHotspotPair> sortedHotspots;
	for (const auto& h : hotspotInfo)
		sortedHotspots.push_back(std::make_pair(h.second, h.first));
	std::sort(sortedHotspots.begin(), sortedHotspots.end(), [](const SpeciesHotspotPair& a, const SpeciesHotspotPair& b)
	{
		if (a.first.size() > b.first.size())
			return true;
		return false;
	});

	Cout << "\nRecommended hotspots for observing needed species:\n";
	const unsigned int minimumHotspotCount(10);
	unsigned int hotspotCount(0);
	size_t lastHotspotSpeciesCount(0);
	for (const auto& hotspot : sortedHotspots)
	{
		if (hotspotCount >= minimumHotspotCount && hotspot.first.size() < lastHotspotSpeciesCount)
			break;

		Cout << "  " << hotspot.second.name << " (" << hotspot.first.size() << " species)\n";
		++hotspotCount;
		lastHotspotSpeciesCount = hotspot.first.size();
	}
	Cout << std::endl;

	if (!hotspotInfoFileName.empty())
		GenerateHotspotInfoFile(sortedHotspots, hotspotInfoFileName, homeLocation, mapApiKey, region, eBirdApiKey);
}

void EBirdDataProcessor::GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<UString::String>,
	EBirdInterface::LocationInfo>>& hotspots, const UString::String& hotspotInfoFileName,
	const UString::String& homeLocation, const UString::String& mapApiKey,
	const UString::String& regionCode, const UString::String& eBirdApiKey) const
{
	Cout << "Writing hotspot information to file..." << std::endl;

	UString::OFStream infoFile(hotspotInfoFileName.c_str());
	if (!infoFile.good() || !infoFile.is_open())
	{
		Cerr << "Failed to open '" << hotspotInfoFileName << "' for output\n";
		return;
	}

	if (!homeLocation.empty())
		infoFile << "Travel time and distance given from " << homeLocation << '\n';

	std::map<UString::String, UString::String> speciesToObservationTimeMap;

	for (const auto& h : hotspots)
	{
		infoFile << '\n' << h.second.name;
		if (!homeLocation.empty())
		{
			GoogleMapsInterface gMaps(_T("eBirdDataProcessor"), mapApiKey);
			UString::OStringStream ss;
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
			if (speciesToObservationTimeMap.find(s) == speciesToObservationTimeMap.end() ||
				speciesToObservationTimeMap[s].empty())
			{
				EBirdInterface e(eBirdApiKey);
				const unsigned int recentPeriod(30);
				const bool includeProvisional(true);
				const bool hotspotsOnly(false);
				std::vector<EBirdInterface::ObservationInfo> observationInfo(e.GetRecentObservationsOfSpeciesInRegion(
					e.GetSpeciesCodeFromCommonName(s), /*h.second.hotspotID*/regionCode, recentPeriod, includeProvisional, hotspotsOnly));// If we use hotspot ID, we get only the most recent sighting

				// Remove entires that don't include time data
				observationInfo.erase(std::remove_if(observationInfo.begin(), observationInfo.end(), [](const EBirdInterface::ObservationInfo& o)
				{
					return !o.dateIncludesTimeInfo;
				}), observationInfo.end());

				UString::String bestObservationTime;
				if (observationInfo.size() > 0)
					bestObservationTime = BestObservationTimeEstimator::EstimateBestObservationTime(observationInfo);

				speciesToObservationTimeMap[s] = bestObservationTime;
			}

			infoFile << "  " << s;

			const UString::String observationString(speciesToObservationTimeMap[s]);
			if (!observationString.empty())
				infoFile << " (observed " << observationString << ")\n";
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
				const UString::String aCountry(a.stateProvidence.substr(0, 2));
				const UString::String bCountry(b.stateProvidence.substr(0, 2));
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

	Cout << "\nUnique observations by ";
	if (type == EBDPConfig::UniquenessType::ByCountry)
		Cout << "Country:\n";
	else if (type == EBDPConfig::UniquenessType::ByState)
		Cout << "State:\n";
	else// if (type == EBDPConfig::UniquenessType::ByCounty)
		Cout << "County:\n";
}

UString::String EBirdDataProcessor::PrepareForComparison(const UString::String& commonName)
{
	return StringUtilities::Trim(StripParentheses(commonName));
}

void EBirdDataProcessor::GenerateRarityScores(const UString::String& frequencyFilePath,
	const EBDPConfig::ListType& listType, const UString::String& eBirdAPIKey,
	const UString::String& country, const UString::String& state, const UString::String& county)
{
	EBirdInterface ebi(eBirdAPIKey);
	FrequencyFileReader reader(frequencyFilePath);
	FrequencyDataYear monthFrequencyData;
	DoubleYear checklistCounts;
	if (!reader.ReadRegionData(ebi.GetRegionCode(country, state, county), monthFrequencyData, checklistCounts))
		return;

	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData(
		GenerateYearlyFrequencyData(monthFrequencyData, checklistCounts));

	const auto consolidatedData(DoConsolidation(listType, data));
	std::vector<EBirdDataProcessor::FrequencyInfo> rarityScoreData(consolidatedData.size());
	unsigned int i;
	for (i = 0; i < rarityScoreData.size(); ++i)
	{
		rarityScoreData[i].species = consolidatedData[i].commonName;
		rarityScoreData[i].frequency = 0.0;
		for (const auto& species : yearFrequencyData)
		{
			if (CommonNamesMatch(rarityScoreData[i].species, species.species))
			{
				rarityScoreData[i].frequency = species.frequency;
				break;
			}
		}
	}

	std::sort(rarityScoreData.begin(), rarityScoreData.end(), [](const FrequencyInfo& a, const FrequencyInfo& b)
	{
		return a.frequency < b.frequency;
	});

	Cout << std::endl;
	const unsigned int minSpace(4);
	size_t longestName(0);
	for (const auto& entry : rarityScoreData)
	{
		if (entry.species.length() > longestName)
			longestName = entry.species.length();
	}

	for (const auto& entry : rarityScoreData)
		Cout << std::left << std::setw(longestName + minSpace) << std::setfill(UString::Char(' ')) << entry.species << entry.frequency << "%\n";
	Cout << std::endl;
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

bool EBirdDataProcessor::HotspotInfoComparer::operator()(const EBirdInterface::LocationInfo& a,
	const EBirdInterface::LocationInfo& b) const
{
	return a.name < b.name;
}

bool EBirdDataProcessor::ExtractNextMediaEntry(const UString::String& html, std::string::size_type& position, MediaEntry& entry)
{
	const UString::String resultStart(_T("<div class=\"ResultsList-cell\">"));
	const auto resultStartPosition(html.find(resultStart, position));
	if (resultStartPosition == std::string::npos)
		return false;

	const UString::String playButtonStart(_T("<div class=\"Button--play\">"));
	const auto playButtonStartPosition(html.find(playButtonStart, resultStartPosition));

	const UString::String commonNameHeader(_T("<h3 class=\"SpecimenHeader-commonName\">"));
	const auto commonNameHeaderPosition(html.find(commonNameHeader, resultStartPosition));
	if (commonNameHeaderPosition == std::string::npos)
		return false;

	if (playButtonStartPosition < commonNameHeaderPosition)
		entry.type = MediaEntry::Type::Audio;
	else
		entry.type = MediaEntry::Type::Photo;

	const UString::String endOfLinkStartTag(_T("\">"));
	const auto endOfLinkPosition(html.find(endOfLinkStartTag, commonNameHeaderPosition + commonNameHeader.length()));

	const UString::String endOfCommonNameBlock(_T("</h3>"));
	const auto endOfCommonNamePosition(html.find(endOfCommonNameBlock, commonNameHeaderPosition + commonNameHeader.length()));
	if (endOfCommonNamePosition == std::string::npos)
		return false;

	if (endOfCommonNamePosition < endOfLinkPosition)// Can happend when there is no link around the common name (like for sputs)
	{
		const UString::String endOfLineTag(_T("\n"));
		const auto commonNameEndPosition(html.find(endOfLineTag, commonNameHeaderPosition + commonNameHeader.length() + 1));
		if (commonNameEndPosition == std::string::npos)
			return false;

		entry.commonName = StringUtilities::Trim(html.substr(commonNameHeaderPosition + commonNameHeader.length() + 1,
			commonNameEndPosition - commonNameHeaderPosition - commonNameHeader.length() - 1));
	}
	else
	{
		const UString::String endOfLinkTag(_T("</a>"));
		const auto commonNameEndPosition(html.find(endOfLinkTag, endOfLinkPosition));
		if (commonNameEndPosition == std::string::npos)
			return false;

		entry.commonName = html.substr(endOfLinkPosition + endOfLinkStartTag.length(), commonNameEndPosition - endOfLinkPosition - endOfLinkStartTag.length());
	}

	const UString::String ratingStart(_T("<div class=\"RatingStars RatingStars-"));
	const auto ratingStartPosition(html.find(ratingStart, endOfCommonNamePosition));
	if (ratingStartPosition != std::string::npos)
	{
		UString::IStringStream ss(html.substr(ratingStartPosition + ratingStart.length(), 1));
		if ((ss >> entry.rating).fail())
			return false;
	}
	else
		entry.rating = 0;

	const UString::String calendarLine(_T("<svg class=\"Icon Icon-calendar\" role=\"img\"><use xlink:href=\"#Icon--date\"></use></svg>"));
	const auto calendarLinePosition(html.find(calendarLine, endOfCommonNamePosition));
	if (calendarLinePosition == std::string::npos)
		return false;
	UString::IStringStream ss(StringUtilities::Trim(html.substr(calendarLinePosition + calendarLine.length() + 5)));
	if (!std::getline(ss, entry.date))
		return false;

	const UString::String locationLine(_T("<svg class=\"Icon Icon-location\" role=\"img\"><use xlink:href=\"#Icon--locationGeneric\"></use></svg>"));
	const auto locationLinePosition(html.find(locationLine, calendarLinePosition));
	if (locationLinePosition == std::string::npos)
		return false;
	ss.clear();
	ss.str(StringUtilities::Trim(html.substr(locationLinePosition + locationLine.length() + 2)));
	if (!std::getline(ss, entry.location))
		return false;
	entry.location = Utilities::Unsanitize(entry.location);

	const UString::String soundStart(_T("<dt>Sounds</dt>"));
	const auto soundStartPosition(html.find(soundStart, locationLinePosition));

	const UString::String ageStart(_T("<dt>Age</dt>"));
	const auto ageStartPosition(html.find(ageStart, locationLinePosition));

	const UString::String sexStart(_T("<dt>Sex</dt>"));
	const auto sexStartPosition(html.find(sexStart, locationLinePosition));

	const UString::String checklistIdStart(_T("\">eBird Checklist "));
	const auto checklistIdPosition(html.find(checklistIdStart, locationLinePosition));
	if (checklistIdPosition == std::string::npos)
		return false;

	const UString::String beginTag(_T("<dd>"));
	const UString::String endTag(_T("</dd>"));
	if (soundStartPosition < checklistIdPosition)
	{
		const auto startPosition(html.find(beginTag, soundStartPosition));
		if (startPosition != std::string::npos)
		{
			const auto endPosition(html.find(endTag, startPosition));
			if (endPosition != std::string::npos)
			{
				const UString::String s(StringUtilities::Trim(html.substr(startPosition + beginTag.length(), endPosition - startPosition - beginTag.length())));
				if (s.compare(_T("Song")) == 0)
					entry.sound = MediaEntry::Sound::Song;
				else if (s.compare(_T("Call")) == 0)
					entry.sound = MediaEntry::Sound::Call;
				else if (s.compare(_T("Unknown")) == 0)
					entry.sound = MediaEntry::Sound::Unknown;
				else
					entry.sound = MediaEntry::Sound::Other;
			}
		}
	}

	if (ageStartPosition < checklistIdPosition)
	{
		const auto startPosition(html.find(beginTag, ageStartPosition));
		if (startPosition != std::string::npos)
		{
			const auto endPosition(html.find(endTag, startPosition));
			if (endPosition != std::string::npos)
			{
				const UString::String s(StringUtilities::Trim(html.substr(startPosition + beginTag.length(), endPosition - startPosition - beginTag.length())));
				if (s.compare(_T("Adult")) == 0)
					entry.age = MediaEntry::Age::Adult;
				else if (s.compare(_T("Juvenile")) == 0)
					entry.age = MediaEntry::Age::Juvenile;
				else if (s.compare(_T("Immature")) == 0)
					entry.age = MediaEntry::Age::Immature;
				else
					entry.age = MediaEntry::Age::Unknown;
			}
		}
	}

	if (sexStartPosition < checklistIdPosition)
	{
		const auto startPosition(html.find(beginTag, sexStartPosition));
		if (startPosition != std::string::npos)
		{
			const auto endPosition(html.find(endTag, startPosition));
			if (endPosition != std::string::npos)
			{
				const UString::String s(StringUtilities::Trim(html.substr(startPosition + beginTag.length(), endPosition - startPosition - beginTag.length())));
				if (s.compare(_T("Male")) == 0)
					entry.sex = MediaEntry::Sex::Male;
				else if (s.compare(_T("Female")) == 0)
					entry.sex = MediaEntry::Sex::Female;
				else
					entry.sex = MediaEntry::Sex::Unknown;
			}
		}
	}

	const UString::String endOfLinkTag(_T("</a>"));
	const auto checklistIdEndPosition(html.find(endOfLinkTag, checklistIdPosition));
	if (checklistIdEndPosition == std::string::npos)
		return false;

	entry.checklistId = html.substr(checklistIdPosition + checklistIdStart.length(), checklistIdEndPosition - checklistIdPosition - checklistIdStart.length());

	const UString::String macaulayIdStart(_T("\">Macaulay Library "));
	const auto macaulayIdPosition(html.find(macaulayIdStart, checklistIdEndPosition));
	if (macaulayIdPosition == std::string::npos)
		return false;

	const auto macaulayIdEndPosition(html.find(endOfLinkTag, macaulayIdPosition));
	if (macaulayIdEndPosition == std::string::npos)
		return false;

	entry.macaulayId = html.substr(macaulayIdPosition + macaulayIdStart.length(), macaulayIdEndPosition - macaulayIdPosition - macaulayIdStart.length());

	position = macaulayIdEndPosition;
	return true;
}

UString::String EBirdDataProcessor::GetMediaTypeString(const MediaEntry::Type& type)
{
	if (type == MediaEntry::Type::Photo)
		return _T("Photo");
	//else
		return _T("Audio");
}

UString::String EBirdDataProcessor::GetMediaAgeString(const MediaEntry::Age& age)
{
	if (age == MediaEntry::Age::Juvenile)
		return _T("Juvenile");
	else if (age == MediaEntry::Age::Immature)
		return _T("Immature");
	else if (age == MediaEntry::Age::Adult)
		return _T("Adult");
	//else
		return _T("Unknown");
}

UString::String EBirdDataProcessor::GetMediaSexString(const MediaEntry::Sex& sex)
{
	if (sex == MediaEntry::Sex::Male)
		return _T("Male");
	else if (sex == MediaEntry::Sex::Female)
		return _T("Female");
	//else
		return _T("Unknown");
}

UString::String EBirdDataProcessor::GetMediaSoundString(const MediaEntry::Sound& sound)
{
	if (sound == MediaEntry::Sound::Song)
		return _T("Song");
	else if (sound == MediaEntry::Sound::Call)
		return _T("Call");
	else if (sound == MediaEntry::Sound::Unknown)
		return _T("Unknown");
	//else
		return _T("Other");
}

void EBirdDataProcessor::WriteNextMediaEntry(UString::OFStream& file, const MediaEntry& entry)
{
	file << entry.macaulayId << ','
		<< entry.commonName << ','
		<< GetMediaTypeString(entry.type) << ','
		<< entry.rating << ','
		<< entry.date << ','
		<< Utilities::SanitizeCommas(entry.location) << ','
		<< GetMediaAgeString(entry.age) << ','
		<< GetMediaSexString(entry.sex) << ','
		<< GetMediaSoundString(entry.sound) << ','
		<< entry.checklistId << '\n';
}

// Directions for getting media list from Chrome:
// 1.  Go to eBird profile page
// 2.  At bottom, choose "View All" next to list of recent photos
// 3.  At top of following page, remove filters for location and media type (i.e. "Photo")
// 4.  At bottom of page, click "Show More" until all available media is shown
// 5.  Right-click and choose "Inspect"
// 6.  In pane that appears, expand "<body>" tag down to "<div class="ResultsList js-ResultsContainer">" level
// 7.  Right-click on that element and choose Copy->Copy Element
// 8.  Paste into media list html file and save
bool EBirdDataProcessor::GenerateMediaList(const UString::String& mediaListHTML, const UString::String& mediaFileName)
{
	std::ifstream htmlFile(mediaListHTML.c_str(), std::ios::binary | std::ios::ate);
	if (!htmlFile.is_open() || !htmlFile.good())
	{
		Cerr << "Failed to open '" << mediaListHTML << "' for input\n";
		return false;
	}
	const unsigned int fileSize(static_cast<unsigned int>(htmlFile.tellg()));
	htmlFile.seekg(0, std::ios::beg);

	std::vector<char> buffer(fileSize);
	if (!htmlFile.read(buffer.data(), fileSize))
	{
		Cerr << "Failed to read html data from file\n";
		return false;
	}
	const UString::String html(UString::ToStringType(std::string(buffer.data(), fileSize)));

	UString::OFStream mediaList(mediaFileName.c_str());
	if (!mediaList.is_open() || !mediaList.good())
	{
		Cerr << "Failed to open '" << mediaFileName << "' for output\n";
		return false;
	}

	mediaList << "Macaulay Library ID,Common Name,Media Type,Rating,Date,Location,Age,Sex,Extra,eBird Checklist ID\n";
	std::string::size_type position(0);
	while (true)
	{
		MediaEntry entry;
		if (!ExtractNextMediaEntry(html, position, entry))
			break;
		WriteNextMediaEntry(mediaList, entry);
	}

	return true;
}

bool EBirdDataProcessor::ParseMediaEntry(const UString::String& line, MediaEntry& entry)
{
	UString::IStringStream lineStream(line);

	if (!ParseToken(lineStream, _T("Macaulay Library ID"), entry.macaulayId))
		return false;
	if (!ParseToken(lineStream, _T("Common Name"), entry.commonName))
		return false;
	UString::String temp;
	if (!ParseToken(lineStream, _T("Media Type"), temp))
		return false;

	if (temp.compare(_T("Photo")) == 0)
		entry.type = MediaEntry::Type::Photo;
	else
		entry.type = MediaEntry::Type::Audio;

	if (!ParseToken(lineStream, _T("Rating"), entry.rating))
		return false;
	if (!ParseToken(lineStream, _T("Date"), entry.date))
		return false;
	if (!ParseToken(lineStream, _T("Location"), entry.location))
		return false;
	entry.location = Utilities::Unsanitize(entry.location);
	if (!ParseToken(lineStream, _T("Age"), temp))
		return false;

	if (temp.compare(_T("Adult")) == 0)
		entry.age = MediaEntry::Age::Adult;
	else if (temp.compare(_T("Juvenile")) == 0)
		entry.age = MediaEntry::Age::Juvenile;
	else if (temp.compare(_T("Immature")) == 0)
		entry.age = MediaEntry::Age::Immature;
	else
		entry.age = MediaEntry::Age::Unknown;

	if (!ParseToken(lineStream, _T("Sex"), temp))
		return false;
	if (!ParseToken(lineStream, _T("Sound"), temp))
		return false;

	if (temp.compare(_T("Song")) == 0)
		entry.sound = MediaEntry::Sound::Song;
	else if (temp.compare(_T("Call")) == 0)
		entry.sound = MediaEntry::Sound::Call;
	else if (temp.compare(_T("Other")) == 0)
		entry.sound = MediaEntry::Sound::Other;
	else
		entry.sound = MediaEntry::Sound::Unknown;

	if (!ParseToken(lineStream, _T("eBird Checklist ID"), entry.checklistId))
		return false;

	return true;
}

bool EBirdDataProcessor::ReadMediaList(const UString::String& mediaFileName)
{
	UString::IFStream mediaFile(mediaFileName.c_str());
	if (!mediaFile.is_open() || !mediaFile.good())
	{
		Cerr << "Failed to open '" << mediaFileName << "' for input" << std::endl;
		return false;
	}

	UString::String line;
	if (!std::getline(mediaFile, line))// Discard header line
	{
		Cerr << "Media file is empty\n";
		return false;
	}

	std::vector<MediaEntry> mediaList;
	while (std::getline(mediaFile, line))
	{
		MediaEntry entry;
		if (!ParseMediaEntry(line, entry))
			return false;
		mediaList.push_back(entry);
	}

	for (auto& entry : data)
	{
		for (auto& m : mediaList)
		{
			if (m.checklistId.compare(entry.submissionID) == 0 &&
				CommonNamesMatch(entry.commonName, m.commonName))
			{
				if (m.type == MediaEntry::Type::Photo)
					entry.photoRating = m.rating;
				else// if (m.type == MediaEntry::Type::Audio)
					entry.audioRating = m.rating;
				//break;// Efficiency gain if we break, but if an entry has both audio and photo media, only one of them will be assigned.  In practice, efficiency gain here is not needed.
			}
		}
	}

	return true;
}

std::vector<UString::String> EBirdDataProcessor::ListFilesInDirectory(const UString::String& directory)
{
	DIR *dir(opendir(UString::ToNarrowString(directory).c_str()));
	if (!dir)
	{
		Cerr << "Failed to open directory '" << directory << "'\n";
		return std::vector<UString::String>();
	}

	std::vector<UString::String> fileNames;
	struct dirent *ent;
	while (ent = readdir(dir), ent)
	{
		if (std::string(ent->d_name).compare(".") == 0 ||
			std::string(ent->d_name).compare("..") == 0)
			continue;

		assert(ent->d_type != DT_UNKNOWN && "Cannot properly iterate through this filesystem");
		if (ent->d_type == DT_DIR)
		{
			const UString::String folder(UString::ToStringType(ent->d_name) + _T("/"));
			const UString::String subPath(directory + folder);
			auto subDirFiles(ListFilesInDirectory(subPath));
			/*std::for_each(subDirFiles.begin(), subDirFiles.end(), [&folder](UString::String& s)
			{
				s = folder + s;
			});*/
			fileNames.insert(fileNames.end(), subDirFiles.begin(), subDirFiles.end());
		}
		else
			fileNames.push_back(UString::ToStringType(ent->d_name));
	}
	closedir(dir);

	return fileNames;
}

bool EBirdDataProcessor::IsNotBinFile(const UString::String& fileName)
{
	const UString::String desiredExtension(_T(".bin"));
		if (fileName.length() < desiredExtension.length())
			return true;

	return fileName.substr(fileName.length() - 4).compare(desiredExtension) != 0;
}

void EBirdDataProcessor::RemoveHighLevelFiles(std::vector<UString::String>& fileNames)
{
	std::set<UString::String> removeList;
	for (const auto& f : fileNames)
	{
		const auto firstDash(f.find(UString::Char('-')));
		if (firstDash == std::string::npos)
			continue;// Nothing to remove - this is the highest level file we have

		const auto secondDash(f.find(UString::Char('-'), firstDash + 1));
		if (secondDash == std::string::npos)
		{
			if (f.substr(firstDash).compare(_T("-.bin")) == 0)// Nothing to remove - this is the highest level file we have
				continue;
		}
		else
		{
			const UString::String stateFileName(f.substr(0, secondDash) + _T(".bin"));
			removeList.insert(stateFileName);
		}

		const UString::String countryFileName(f.substr(0, firstDash) + _T("-.bin"));
		removeList.insert(countryFileName);
	}

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), [&removeList](const UString::String& f)
	{
		return std::find(removeList.begin(), removeList.end(),  f) != removeList.end();
	}), fileNames.end());
}

UString::String EBirdDataProcessor::RemoveTrailingDash(const UString::String& s)
{
	if (s.back() != UString::Char('-'))
		return s;
	return s.substr(0, s.length() - 1);
}

bool EBirdDataProcessor::RegionCodeMatches(const UString::String& regionCode, const std::vector<UString::String>& codeList)
{
	for (const auto& c : codeList)
	{
		if (regionCode.length() < c.length())
			continue;
		else if (regionCode.substr(0, c.length()) == c)
			return true;
	}

	return false;
}

bool EBirdDataProcessor::FindBestLocationsForNeededSpecies(const UString::String& frequencyFilePath,
	const UString::String& kmlLibraryPath, const UString::String& eBirdAPIKey, const std::vector<UString::String>& targetRegionCodes,
	const std::vector<UString::String>& highDetailCountries, const bool& cleanUpLocationNames, const int& geoJSONPrecision, const double& kmlReductionLimit) const
{
	auto fileNames(ListFilesInDirectory(frequencyFilePath));
	if (fileNames.size() == 0)
		return false;

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), IsNotBinFile), fileNames.end());
	RemoveHighLevelFiles(fileNames);

	std::vector<YearFrequencyInfo> newSightingProbability(fileNames.size());// frequency is probability of seeing new species and species is file name of frequency data file
	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	FrequencyFileReader reader(frequencyFilePath);

	std::unordered_map<UString::String, ConsolidationData> consolidatationData;

	auto probEntryIt(newSightingProbability.begin());
	for (const auto& f : fileNames)
	{
		FrequencyDataYear occurrenceData;
		DoubleYear checklistCounts;
		const auto regionCode(Utilities::StripExtension(f));
		if (!RegionCodeMatches(regionCode, targetRegionCodes))
			continue;

		if (!reader.ReadRegionData(regionCode, occurrenceData, checklistCounts))
			return false;

		const auto countryCode(Utilities::ExtractCountryFromRegionCode(regionCode));
		const bool useHighDetail(std::find(highDetailCountries.begin(), highDetailCountries.end(), countryCode) != highDetailCountries.end());

		if (useHighDetail)
		{
			probEntryIt->locationCode = RemoveTrailingDash(regionCode);
			pool.AddJob(std::make_unique<CalculateProbabilityJob>(*probEntryIt, std::move(occurrenceData), std::move(checklistCounts), *this));
			++probEntryIt;
		}
		else
		{
			if (consolidatationData.find(countryCode) == consolidatationData.end())
			{
				for (auto& c : consolidatationData[countryCode].checklistCounts)
					c = 0.0;
			}
			AddConsolidationData(consolidatationData[countryCode], std::move(occurrenceData), std::move(checklistCounts));
		}
	}

	pool.WaitForAllJobsComplete();

	for (auto& f : consolidatationData)
	{
		probEntryIt->locationCode = f.first;
		pool.AddJob(std::make_unique<CalculateProbabilityJob>(*probEntryIt, std::move(f.second.occurrenceData), std::move(f.second.checklistCounts), *this));
		++probEntryIt;
	}

	pool.WaitForAllJobsComplete();
	newSightingProbability.erase(probEntryIt, newSightingProbability.end());// Remove extra entries (only needed if every country were done in high detail)

	const UString::String fileName(_T("."));
	if (!WriteBestLocationsViewerPage(fileName, kmlLibraryPath, eBirdAPIKey,
		newSightingProbability, highDetailCountries, cleanUpLocationNames, geoJSONPrecision, kmlReductionLimit))
	{
		Cerr << "Faild to create best locations page\n";
	}

	return true;
}

void EBirdDataProcessor::ConvertProbabilityToCounts(FrequencyDataYear& data, const std::array<double, 12>& counts)
{
	for (unsigned int i = 0; i < counts.size(); ++i)
	{
		for (auto& entry : data[i])
			entry.frequency = static_cast<int>(entry.frequency * counts[i] + 0.5);
	}
}

void EBirdDataProcessor::ConvertCountsToProbability(FrequencyDataYear& data, const std::array<double, 12>& counts)
{
	for (unsigned int i = 0; i < counts.size(); ++i)
	{
		for (auto& entry : data[i])
			entry.frequency /= counts[i];
	}
}

void EBirdDataProcessor::AddConsolidationData(ConsolidationData& existingData,
	FrequencyDataYear&& newData, std::array<double, 12>&& newCounts)
{
	ConvertProbabilityToCounts(existingData.occurrenceData, existingData.checklistCounts);
	ConvertProbabilityToCounts(newData, newCounts);

	for (unsigned int i = 0; i < newCounts.size(); ++i)
	{
		existingData.checklistCounts[i] += newCounts[i];

		for (const auto& newEntry : newData[i])
		{
			bool found(false);
			
			for (auto& existingEntry : existingData.occurrenceData[i])
			{
				if (newEntry.compareString.compare(existingEntry.compareString) == 0)
				{
					existingEntry.frequency += newEntry.frequency;
					found = true;
					break;
				}
			}

			if (!found)
				existingData.occurrenceData[i].push_back(newEntry);
		}
	}

	ConvertCountsToProbability(existingData.occurrenceData, existingData.checklistCounts);
}

bool EBirdDataProcessor::ComputeNewSpeciesProbability(FrequencyDataYear&& frequencyData,
	DoubleYear&& checklistCounts, std::array<double, 12>& probabilities,
	std::array<std::vector<FrequencyInfo>, 12>& species) const
{
	EliminateObservedSpecies(frequencyData);

	unsigned int i(0);
	for (auto& p : probabilities)
	{
		const unsigned int thresholdObservationCount(30);
		if (checklistCounts[i] < thresholdObservationCount)// Ignore counties which have very few observations (due to insufficient data)
		{
			p = 0.0;
			continue;
		}

		const double thresholdFrequency(1.0);// This could be tuned, which means maybe it shouldn't be hard-coded
		double product(1.0);
		for (const auto& entry : frequencyData[i])
		{
			if (entry.frequency < thresholdFrequency)// Ignore rarities
				continue;
			product *= (1.0 - entry.frequency / 100.0);
			species[i].push_back(FrequencyInfo(entry.species, entry.frequency));
		}

		p = 1.0 - product;
		++i;
	}

	return true;
}

bool EBirdDataProcessor::WriteBestLocationsViewerPage(const UString::String& htmlFileName,
	const UString::String& kmlLibraryPath, const UString::String& eBirdAPIKey,
	const std::vector<YearFrequencyInfo>& observationProbabilities,
	const std::vector<UString::String>& highDetailCountries,
	const bool& cleanUpLocationNames, const int& geoJSONPrecision, const double& kmlReductionLimit)
{
	MapPageGenerator generator(kmlLibraryPath, eBirdAPIKey, highDetailCountries, cleanUpLocationNames, geoJSONPrecision, kmlReductionLimit);
	return generator.WriteBestLocationsViewerPage(htmlFileName, observationProbabilities);
}

// Assume we're comparing lists based on year
void EBirdDataProcessor::DoListComparison() const
{
	std::set<unsigned int> years;
	for (const auto& e : data)
		years.insert(e.dateTime.tm_year + 1900);
	Cout << "Data for selected location spans " << years.size() << " years\n" << std::endl;

	std::vector<std::vector<Entry>> yearLists(years.size());

	auto it(years.begin());
	for (unsigned int i = 0; i < years.size(); ++i)
	{
		auto temp(data);
		FilterYear(*it, temp);
		++it;
		yearLists[i] = ConsolidateByYear(temp);
	}

	std::sort(yearLists.begin(), yearLists.end(), [](const std::vector<Entry>& a, const std::vector<Entry>& b)
	{
		return a.front().dateTime.tm_year < b.front().dateTime.tm_year;
	});
	
	PrintListComparison(yearLists);
}

void EBirdDataProcessor::PrintListComparison(const std::vector<std::vector<Entry>>& lists)
{
	std::vector<unsigned int> indexList(lists.size(), 0);
	std::vector<std::vector<UString::String>> listData(lists.size() + 1);// first index is column, second is row

	listData.front().push_back(_T("Species"));
	for (unsigned int i = 0; i < lists.size(); ++i)
	{
		UString::OStringStream ss;
		ss << lists[i].front().dateTime.tm_year + 1900;
		listData[i + 1].push_back(ss.str());
	}

	while (IndicesAreValid(indexList, lists))
	{
		double minTaxonomicOrder(std::numeric_limits<double>::max());
		unsigned int minIndex(0);
		UString::String compareString;
		for (unsigned int i = 0; i < lists.size(); ++i)
		{
			if (indexList[i] < lists[i].size() && lists[i][indexList[i]].taxonomicOrder < minTaxonomicOrder)
			{
				minTaxonomicOrder = lists[i][indexList[i]].taxonomicOrder;
				minIndex = i;
				compareString = lists[i][indexList[i]].compareString;
			}
		}

		listData[0].push_back(lists[minIndex][indexList[minIndex]].commonName);
		for (unsigned int i = 0; i < lists.size(); ++i)
		{
			if (indexList[i] < lists[i].size() &&
				lists[i][indexList[i]].compareString.compare(compareString) == 0)
			{
				listData[i + 1].push_back(_T("X"));
				++indexList[i];
			}
			else
				listData[i + 1].push_back(UString::String());
		}
	}

	for (auto& col : listData)
		col.push_back(UString::String());

	listData.front().push_back(_T("Total"));
	for (unsigned int i = 0; i < lists.size(); ++i)
	{
		UString::OStringStream ss;
		ss << lists[i].size();
		listData[i + 1].push_back(ss.str());
	}

	Cout << PrintInColumns(listData, 2) << std::endl;
}

bool EBirdDataProcessor::IndicesAreValid(const std::vector<unsigned int>& indices, const std::vector<std::vector<Entry>>& lists)
{
	assert(indices.size() == lists.size());
	for (unsigned int i = 0; i < indices.size(); ++i)
	{
		if (indices[i] < lists[i].size())
			return true;
	}

	return false;
}

UString::String EBirdDataProcessor::PrintInColumns(const std::vector<std::vector<UString::String>>& cells, const unsigned int& columnSpacing)
{
	std::vector<size_t> widths(cells.size(), 0);
	for (unsigned int i = 0; i < cells.size(); ++i)
	{
		for (const auto& row : cells[i])
		{
			if (widths[i] < row.length())
				widths[i] = row.length();
		}
	}

	for (auto& w : widths)
		w += columnSpacing;

	UString::OStringStream ss;
	ss << std::setfill(_T(' '));
	for (unsigned int i = 0; i < cells.front().size(); ++i)
	{
		for (unsigned int j = 0; j < cells.size(); ++j)
			ss << std::setw(widths[j]) << std::left << cells[j][i];
		ss << '\n';
	}

	return ss.str();
}

void EBirdDataProcessor::FilterYear(const unsigned int& year, std::vector<Entry>& dataToFilter) const
{
	dataToFilter.erase(std::remove_if(dataToFilter.begin(), dataToFilter.end(), [year](const Entry& entry)
	{
		return static_cast<unsigned int>(entry.dateTime.tm_year) + 1900U != year;
	}), dataToFilter.end());
}
