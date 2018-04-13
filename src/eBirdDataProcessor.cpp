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

const String EBirdDataProcessor::headerLine(_T("Submission ID,Common Name,Scientific Name,"
	"Taxonomic Order,Count,State/Province,County,Location,Latitude,Longitude,Date,Time,"
	"Protocol,Duration (Min),All Obs Reported,Distance Traveled (km),Area Covered (ha),"
	"Number of Observers,Breeding Code,Species Comments,Checklist Comments"));

bool EBirdDataProcessor::Parse(const String& dataFile)
{
	IFStream file(dataFile.c_str());
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << dataFile << "' for input\n";
		return false;
	}

	String line;
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

bool EBirdDataProcessor::ParseLine(const String& line, Entry& entry)
{
	IStringStream lineStream(line);

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
	if (!ParseDateTimeToken(lineStream, _T("Date"), entry.dateTime, _T("%m-%d-%Y")))
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

bool EBirdDataProcessor::ParseCountToken(IStringStream& lineStream, const String& fieldName, int& target)
{
	if (lineStream.peek() == 'X')
	{
		target = 1;
		String token;
		return std::getline(lineStream, token, Char(',')).good();// This is just to advance the stream pointer
	}

	return ParseToken(lineStream, fieldName, target);
}

bool EBirdDataProcessor::ParseDateTimeToken(IStringStream& lineStream, const String& fieldName,
	std::tm& target, const String& format)
{
	String token;
	if (!std::getline(lineStream, token, Char(',')))// This advances the stream pointer
	{
		Cerr << "Failed to read token for " << fieldName << '\n';
		return false;
	}

	IStringStream ss(token);
	if ((ss >> std::get_time(&target, format.c_str())).fail())
	{
		Cerr << "Failed to interpret token for " << fieldName << '\n';
		return false;
	}

	return true;
}

bool EBirdDataProcessor::InterpretToken(IStringStream& tokenStream,
	const String& /*fieldName*/, String& target)
{
	target = tokenStream.str();
	return true;
}

void EBirdDataProcessor::FilterLocation(const String& location, const String& county,
	const String& state, const String& country)
{
	if (!county.empty() || !state.empty() || !country.empty())
		FilterCounty(county, state, country);

	data.erase(std::remove_if(data.begin(), data.end(), [location](const Entry& entry)
	{
		return !std::regex_search(entry.location, RegEx(location));
	}), data.end());
}

void EBirdDataProcessor::FilterCounty(const String& county,
	const String& state, const String& country)
{
	if (!state.empty() || !country.empty())
		FilterState(state, country);

	data.erase(std::remove_if(data.begin(), data.end(), [county](const Entry& entry)
	{
		return entry.county.compare(county) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterState(const String& state, const String& country)
{
	if (!country.empty())
		FilterCountry(country);

	data.erase(std::remove_if(data.begin(), data.end(), [state](const Entry& entry)
	{
		return entry.stateProvidence.substr(3).compare(state) != 0;
	}), data.end());
}

void EBirdDataProcessor::FilterCountry(const String& country)
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
		StringStream ss;
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

void EBirdDataProcessor::FilterPartialIDs()
{
	data.erase(std::remove_if(data.begin(), data.end(), [](const Entry& entry)
	{
		return entry.commonName.find(_T(" sp.")) != std::string::npos ||// Eliminate Spuhs
			entry.commonName.find(Char('/')) != std::string::npos ||// Eliminate species1/species2 type entries
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

String EBirdDataProcessor::GenerateList(const EBDPConfig::ListType& type, const bool& withoutPhotosOnly) const
{
	std::vector<Entry> consolidatedList(DoConsolidation(type));

	if (withoutPhotosOnly)
		Cout << "Showing only species which have not been photographed:\n";

	OStringStream ss;
	unsigned int count(1);
	for (const auto& entry : consolidatedList)
	{
		if (!withoutPhotosOnly || !entry.hasPhoto)
		{
			ss << count++ << ", " << std::put_time(&entry.dateTime, _T("%D")) << ", "
				<< entry.commonName << ", '" << entry.location << "', " << entry.count;

			if (entry.hasPhoto)
				ss << " (photo)";

			ss << '\n';
		}
	}

	return ss.str();
}

bool EBirdDataProcessor::CommonNamesMatch(String a, String b)
{
	return PrepareForComparison(a).compare(PrepareForComparison(b)) == 0;
}

String EBirdDataProcessor::StripParentheses(String s)
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
		StringStream ss;
		ss << std::put_time(&a.dateTime, _T("%U"));
		ss >> aWeek;
		ss.clear();
		ss.str(String());
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
	const String& outputFileName, const String& frequencyFilePath,
	const String& country, const String& state, const String& county,
	const unsigned int& recentPeriod,
	const String& hotspotInfoFileName, const String& homeLocation,
	const String& mapApiKey, const String& eBirdApiKey) const
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

	OFStream outFile(outputFileName.c_str());
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

	std::set<String> consolidatedSpeciesList;
	std::map<String, double> speciesFrequencyMap;
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

void EBirdDataProcessor::RecommendHotspots(const std::set<String>& consolidatedSpeciesList,
	const String& country, const String& state, const String& county, const unsigned int& recentPeriod,
	const String& hotspotInfoFileName, const String& homeLocation,
	const String& mapApiKey, const String& eBirdApiKey) const
{
	Cout << "Checking eBird for recent sightings..." << std::endl;

	EBirdInterface e(eBirdApiKey);
	const String region(e.GetRegionCode(country, state, county));
	std::set<String> recentSpecies;

	typedef std::vector<String> SpeciesList;
	std::map<EBirdInterface::LocationInfo, SpeciesList, HotspotInfoComparer> hotspotInfo;
	for (const auto& species : consolidatedSpeciesList)
	{
		const String speciesCode(e.GetSpeciesCodeFromCommonName(species));
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
	unsigned int lastHotspotSpeciesCount(0);
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

void EBirdDataProcessor::GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<String>,
	EBirdInterface::LocationInfo>>& hotspots, const String& hotspotInfoFileName,
	const String& homeLocation, const String& mapApiKey,
	const String& regionCode, const String& eBirdApiKey) const
{
	Cout << "Writing hotspot information to file..." << std::endl;

	OFStream infoFile(hotspotInfoFileName.c_str());
	if (!infoFile.good() || !infoFile.is_open())
	{
		Cerr << "Failed to open '" << hotspotInfoFileName << "' for output\n";
		return;
	}

	if (!homeLocation.empty())
		infoFile << "Travel time and distance given from " << homeLocation << '\n';

	std::map<String, String> speciesToObservationTimeMap;

	for (const auto& h : hotspots)
	{
		infoFile << '\n' << h.second.name;
		if (!homeLocation.empty())
		{
			GoogleMapsInterface gMaps(_T("eBirdDataProcessor"), mapApiKey);
			OStringStream ss;
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

				String bestObservationTime;
				if (observationInfo.size() > 0)
					bestObservationTime = BestObservationTimeEstimator::EstimateBestObservationTime(observationInfo);

				speciesToObservationTimeMap[s] = bestObservationTime;
			}

			infoFile << "  " << s;

			const String observationString(speciesToObservationTimeMap[s]);
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
				const String aCountry(a.stateProvidence.substr(0, 2));
				const String bCountry(b.stateProvidence.substr(0, 2));
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

String EBirdDataProcessor::PrepareForComparison(const String& commonName)
{
	return StringUtilities::Trim(StripParentheses(commonName));
}

void EBirdDataProcessor::GenerateRarityScores(const String& frequencyFilePath,
	const EBDPConfig::ListType& listType, const String& eBirdAPIKey,
	const String& country, const String& state, const String& county)
{
	EBirdInterface ebi(eBirdAPIKey);
	FrequencyFileReader reader(frequencyFilePath);
	FrequencyDataYear monthFrequencyData;
	DoubleYear checklistCounts;
	if (!reader.ReadRegionData(ebi.GetRegionCode(country, state, county), monthFrequencyData, checklistCounts))
		return;

	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData(
		GenerateYearlyFrequencyData(monthFrequencyData, checklistCounts));

	const auto consolidatedData(DoConsolidation(listType));
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
	unsigned int longestName(0);
	for (const auto& entry : rarityScoreData)
	{
		if (entry.species.length() > longestName)
			longestName = entry.species.length();
	}

	for (const auto& entry : rarityScoreData)
		Cout << std::left << std::setw(longestName + minSpace) << std::setfill(Char(' ')) << entry.species << entry.frequency << "%\n";
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

bool EBirdDataProcessor::ReadPhotoList(const String& photoFileName)
{
	IFStream photoFile(photoFileName.c_str());
	if (!photoFile.is_open() || !photoFile.good())
	{
		Cerr << "Failed to open '" << photoFileName << "' for input" << std::endl;
		return false;
	}

	struct PhotoEntry
	{
		explicit PhotoEntry(const String& commonName) : commonName(commonName) {}

		String commonName;
		bool matchedOnce = false;
	};
	std::vector<PhotoEntry> photoList;

	String line;
	while (std::getline(photoFile, line))
		photoList.push_back(PhotoEntry(line));

	for (auto& entry : data)
	{
		for (auto& p : photoList)
		{
			if (CommonNamesMatch(entry.commonName, p.commonName))
			{
				p.matchedOnce = true;
				entry.hasPhoto = true;
				break;
			}
		}
	}

	for (const auto& p : photoList)
	{
		if (!p.matchedOnce)
			Cout << "Warning:  Failed to match species in photo list '" << p.commonName << "' to any observation\n";
	}

	return true;
}

std::vector<String> EBirdDataProcessor::ListFilesInDirectory(const String& directory)
{
	DIR *dir(opendir(UString::ToNarrowString(directory).c_str()));
	if (!dir)
	{
		Cerr << "Failed to open directory '" << directory << "'\n";
		return std::vector<String>();
	}

	std::vector<String> fileNames;
	struct dirent *ent;
	while (ent = readdir(dir), ent)
	{
		if (std::string(ent->d_name).compare(".") == 0 ||
			std::string(ent->d_name).compare("..") == 0)
			continue;

		assert(ent->d_type != DT_UNKNOWN && "Cannot properly iterate through this filesystem");
		if (ent->d_type == DT_DIR)
		{
			const String folder(UString::ToStringType(ent->d_name) + _T("/"));
			const String subPath(directory + folder);
			auto subDirFiles(ListFilesInDirectory(subPath));
			/*std::for_each(subDirFiles.begin(), subDirFiles.end(), [&folder](String& s)
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

bool EBirdDataProcessor::IsNotBinFile(const String& fileName)
{
	const String desiredExtension(_T(".bin"));
		if (fileName.length() < desiredExtension.length())
			return true;

	return fileName.substr(fileName.length() - 4).compare(desiredExtension) != 0;
}

void EBirdDataProcessor::RemoveHighLevelFiles(std::vector<String>& fileNames)
{
	std::set<String> removeList;
	for (const auto& f : fileNames)
	{
		const auto firstDash(f.find(Char('-')));
		if (firstDash == std::string::npos)
			continue;// Nothing to remove - this is the highest level file we have

		const auto secondDash(f.find(Char('-'), firstDash + 1));
		if (secondDash == std::string::npos)
		{
			if (f.substr(firstDash).compare(_T("-.bin")) == 0)// Nothing to remove - this is the highest level file we have
				continue;
		}
		else
		{
			const String stateFileName(f.substr(0, secondDash) + _T(".bin"));
			removeList.insert(stateFileName);
		}

		const String countryFileName(f.substr(0, firstDash) + _T("-.bin"));
		removeList.insert(countryFileName);
	}

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), [&removeList](const String& f)
	{
		return std::find(removeList.begin(), removeList.end(),  f) != removeList.end();
	}), fileNames.end());
}

String EBirdDataProcessor::RemoveTrailingDash(const String& s)
{
	if (s.back() != Char('-'))
		return s;
	return s.substr(0, s.length() - 1);
}

bool EBirdDataProcessor::FindBestLocationsForNeededSpecies(const String& frequencyFilePath,
	const String& kmlLibraryPath, const String& googleMapsKey, const String& eBirdAPIKey,
	const String& clientId, const String& clientSecret) const
{
	auto fileNames(ListFilesInDirectory(frequencyFilePath));
	if (fileNames.size() == 0)
		return false;

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), IsNotBinFile), fileNames.end());
	RemoveHighLevelFiles(fileNames);

	std::vector<YearFrequencyInfo> newSightingProbability(fileNames.size());// frequency is probability of seeing new species and species is file name of frequency data file
	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	FrequencyFileReader reader(frequencyFilePath);

	auto probEntryIt(newSightingProbability.begin());
	for (const auto& f : fileNames)
	{
		FrequencyDataYear occurrenceData;
		DoubleYear checklistCounts;
		const auto regionCode(Utilities::StripExtension(f));
		if (!reader.ReadRegionData(regionCode, occurrenceData, checklistCounts))
			return false;

		probEntryIt->locationCode = RemoveTrailingDash(regionCode);
		pool.AddJob(std::make_unique<CalculateProbabilityJob>(*probEntryIt, std::move(occurrenceData), std::move(checklistCounts), *this));
		++probEntryIt;
	}

	pool.WaitForAllJobsComplete();

	if (!googleMapsKey.empty())
	{
		const String fileName(_T("bestLocations.html"));
		if (!WriteBestLocationsViewerPage(fileName, kmlLibraryPath, googleMapsKey, eBirdAPIKey, newSightingProbability, clientId, clientSecret))
		{
			Cerr << "Faild to create Google Maps best locations page\n";
		}
	}

	return true;
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

		const double thresholdFrequency(2.0);// This could be tuned, which means maybe it shouldn't be hard-coded
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

bool EBirdDataProcessor::WriteBestLocationsViewerPage(const String& htmlFileName,
	const String& kmlLibraryPath, const String& googleMapsKey, const String& eBirdAPIKey,
	const std::vector<YearFrequencyInfo>& observationProbabilities,
	const String& clientId, const String& clientSecret)
{
	MapPageGenerator generator(kmlLibraryPath, eBirdAPIKey);
	return generator.WriteBestLocationsViewerPage(htmlFileName,
		googleMapsKey, eBirdAPIKey, observationProbabilities, clientId, clientSecret);
}
