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
#include "kernelDensityEstimation.h"

// Standard C++ headers
#include <fstream>
#include <iostream>
#include <iomanip>
#include <locale>
#include <map>
#include <chrono>
#include <set>
#include <filesystem>

#ifdef _WIN32
	namespace fs = std::experimental::filesystem;
#else
	namespace fs = std::filesystem;
#endif// _WIN32

const UString::String EBirdDataProcessor::headerLine(_T("Submission ID,Common Name,Scientific Name,"
	"Taxonomic Order,Count,State/Province,County,Location ID,Location,Latitude,Longitude,Date,Time,"
	"Protocol,Duration (Min),All Obs Reported,Distance Traveled (km),Area Covered (ha),"
	"Number of Observers,Breeding Code,Observation Details,Checklist Comments,ML Catalog Numbers"));

bool EBirdDataProcessor::Parse()
{
	UString::IFStream file(appConfig.dataFileName.c_str());
	if (!file.is_open() || !file.good())
	{
		Cerr << "Failed to open '" << appConfig.dataFileName << "' for input\n";
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
	if (!ParseToken(lineStream, _T("Location ID"), entry.locationID))
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
	if (!ParseToken(lineStream, _T("Observation Details"), entry.speciesComments))
		return false;
	if (!ParseToken(lineStream, _T("Checklist Comments"), entry.checklistComments))
		return false;
	if (!ParseToken(lineStream, _T("ML Catalog Numbers"), entry.mlCatalogNumbers))
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

void EBirdDataProcessor::FilterByRadius(const double& latitude, const double& longitude, const double& radius)
{
	data.erase(std::remove_if(data.begin(), data.end(), [&latitude, &longitude, &radius](const Entry& entry)
	{
		return Utilities::ComputeWGS84Distance(latitude, longitude, entry.latitude, entry.longitude) > radius;
	}), data.end());
	
	std::set<UString::String> locations;
	for (const auto& entry : data)
		locations.insert(entry.location);
	
	Cout << "Locations within the specified circle include:\n";
	for (const auto& l : locations)
		Cout << l << '\n';
	Cout << std::endl;
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
		if ((!entry.photoRating.empty() && *std::max_element(entry.photoRating.begin(), entry.photoRating.end()) >= minPhotoScore && minPhotoScore >= 0) ||
			(!entry.audioRating.empty() && *std::max_element(entry.audioRating.begin(), entry.audioRating.end()) >= minAudioScore && minAudioScore >= 0))
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

		if (!entry.photoRating.empty())
			ss << " (photo rating = " << *std::max_element(entry.photoRating.begin(), entry.photoRating.end()) << ')';
		if (!entry.audioRating.empty())
			ss << " (audio rating = " << *std::max_element(entry.audioRating.begin(), entry.audioRating.end()) << ')';

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

bool EBirdDataProcessor::GenerateTargetCalendar(const CalendarParameters& calendarParameters,
	const UString::String& outputFileName, const UString::String& country,
	const UString::String& state, const UString::String& county) const
{
	FrequencyFileReader frequencyFileReader(appConfig.frequencyFilePath);
	EBirdInterface ebi(appConfig.eBirdApiKey);
	FrequencyDataYear frequencyData;
	UIntYear checklistCounts;
	unsigned int rarityYearRange;
	if (!frequencyFileReader.ReadRegionData(ebi.GetRegionCode(country, state, county), frequencyData, checklistCounts, rarityYearRange))
		return false;

	//GuessChecklistCounts(frequencyData, checklistCounts);

	EliminateObservedSpecies(frequencyData);

	for (auto& week : frequencyData)
	{
		std::sort(week.begin(), week.end(), [](const FrequencyInfo& a, const FrequencyInfo& b)
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

	outFile
		<< "January 1 (" << checklistCounts[0] << " checklists),"
		<< "January 8 (" << checklistCounts[1] << " checklists),"
		<< "January 15 (" << checklistCounts[2] << " checklists),"
		<< "January 22 (" << checklistCounts[3] << " checklists),"
		<< "February 1 (" << checklistCounts[4] << " checklists),"
		<< "February 8 (" << checklistCounts[5] << " checklists),"
		<< "February 15 (" << checklistCounts[6] << " checklists),"
		<< "February 22 (" << checklistCounts[7] << " checklists),"
		<< "March 1 (" << checklistCounts[8] << " checklists),"
		<< "March 8 (" << checklistCounts[9] << " checklists),"
		<< "March 15 (" << checklistCounts[10] << " checklists),"
		<< "March 22 (" << checklistCounts[11] << " checklists),"
		<< "April 1 (" << checklistCounts[12] << " checklists),"
		<< "April 8 (" << checklistCounts[13] << " checklists),"
		<< "April 15 (" << checklistCounts[14] << " checklists),"
		<< "April 22 (" << checklistCounts[15] << " checklists),"
		<< "May 1 (" << checklistCounts[16] << " checklists),"
		<< "May 8 (" << checklistCounts[17] << " checklists),"
		<< "May 15 (" << checklistCounts[18] << " checklists),"
		<< "May 22 (" << checklistCounts[19] << " checklists),"
		<< "June 1 (" << checklistCounts[20] << " checklists),"
		<< "June 8 (" << checklistCounts[21] << " checklists),"
		<< "June 15 (" << checklistCounts[22] << " checklists),"
		<< "June 22 (" << checklistCounts[23] << " checklists),"
		<< "July 1 (" << checklistCounts[24] << " checklists),"
		<< "July 8 (" << checklistCounts[25] << " checklists),"
		<< "July 15 (" << checklistCounts[26] << " checklists),"
		<< "July 22 (" << checklistCounts[27] << " checklists),"
		<< "August 1 (" << checklistCounts[28] << " checklists),"
		<< "August 8 (" << checklistCounts[29] << " checklists),"
		<< "August 15 (" << checklistCounts[30] << " checklists),"
		<< "August 22 (" << checklistCounts[31] << " checklists),"
		<< "September 1 (" << checklistCounts[32] << " checklists),"
		<< "September 8 (" << checklistCounts[33] << " checklists),"
		<< "September 15 (" << checklistCounts[34] << " checklists),"
		<< "September 22 (" << checklistCounts[35] << " checklists),"
		<< "October 1 (" << checklistCounts[36] << " checklists),"
		<< "October 8 (" << checklistCounts[37] << " checklists),"
		<< "October 15 (" << checklistCounts[38] << " checklists),"
		<< "October 22 (" << checklistCounts[39] << " checklists),"
		<< "November 1 (" << checklistCounts[40] << " checklists),"
		<< "November 8 (" << checklistCounts[41] << " checklists),"
		<< "November 15 (" << checklistCounts[42] << " checklists),"
		<< "November 22 (" << checklistCounts[43] << " checklists),"
		<< "December 1 (" << checklistCounts[44] << " checklists),"
		<< "December 8 (" << checklistCounts[45] << " checklists),"
		<< "December 15 (" << checklistCounts[46] << " checklists),"
		<< "December 22 (" << checklistCounts[47] << " checklists),";
	outFile << std::endl;

	for (unsigned int i = 0; i < calendarParameters.topBirdCount; ++i)
	{
		for (const auto& week : frequencyData)
		{
			if (i < week.size())
			{
				outFile << week[i].species << " (" << week[i].frequency << " %)";
				if (week[i].isRarity)
					outFile << " (observed " << week[i].yearsObservedInLastNYears << " of last " << rarityYearRange << " years)";
			}

			outFile << ',';
		}
		outFile << std::endl;
	}

	std::set<UString::String> consolidatedSpeciesList;
	std::map<UString::String, double> speciesFrequencyMap;
	for (unsigned int i = 0; i < calendarParameters.topBirdCount; ++i)
	{
		for (const auto& week : frequencyData)
		{
			if (i >= week.size())
				continue;

			consolidatedSpeciesList.insert(week[i].species);
			if (speciesFrequencyMap.find(week[i].species) == speciesFrequencyMap.end())
				speciesFrequencyMap[week[i].species] = week[i].frequency;
			else
				speciesFrequencyMap[week[i].species] = std::max(week[i].frequency, speciesFrequencyMap[week[i].species]);
		}
	}

	double maxFrequency(0.0);
	for (const auto& s : speciesFrequencyMap)
	{
		if (s.second > maxFrequency)
			maxFrequency = s.second;
	}

	Cout << calendarParameters.topBirdCount << " most common species needed for each week of the year includes "
		<< consolidatedSpeciesList.size() << " species" << std::endl;

	std::array<std::pair<double, unsigned int>, 6> bracketCounts;
	if (maxFrequency < 10.0)
	{
		bracketCounts[0] = std::make_pair(8.0, 0U);
		bracketCounts[1] = std::make_pair(5.0, 0U);
		bracketCounts[2] = std::make_pair(3.0, 0U);
		bracketCounts[3] = std::make_pair(2.0, 0U);
		bracketCounts[4] = std::make_pair(1.0, 0U);
		bracketCounts[5] = std::make_pair(0.5, 0U);
	}
	else if (maxFrequency < 5.0)
	{
		bracketCounts[0] = std::make_pair(4.0, 0U);
		bracketCounts[1] = std::make_pair(3.0, 0U);
		bracketCounts[2] = std::make_pair(2.0, 0U);
		bracketCounts[3] = std::make_pair(1.0, 0U);
		bracketCounts[4] = std::make_pair(0.5, 0U);
		bracketCounts[5] = std::make_pair(0.2, 0U);
	}
	else if (maxFrequency < 2.0)
	{
		bracketCounts[0] = std::make_pair(1.0, 0U);
		bracketCounts[1] = std::make_pair(0.8, 0U);
		bracketCounts[2] = std::make_pair(0.5, 0U);
		bracketCounts[3] = std::make_pair(0.3, 0U);
		bracketCounts[4] = std::make_pair(0.2, 0U);
		bracketCounts[5] = std::make_pair(0.1, 0U);
	}
	else
	{
		bracketCounts[0] = std::make_pair(50.0, 0U);
		bracketCounts[1] = std::make_pair(40.0, 0U);
		bracketCounts[2] = std::make_pair(30.0, 0U);
		bracketCounts[3] = std::make_pair(20.0, 0U);
		bracketCounts[4] = std::make_pair(10.0, 0U);
		bracketCounts[5] = std::make_pair(5.0, 0U);
	}

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

	RecommendHotspots(consolidatedSpeciesList, country, state, county, calendarParameters);

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
	for (auto& week : frequencyData)
	{
		week.erase(std::remove_if(week.begin(), week.end(), [this](const FrequencyInfo& f)
		{
			const auto& speciesIterator(std::find_if(data.begin(), data.end(), [f](const Entry& e)
			{
				return f.compareString.compare(e.compareString) == 0;
			}));
			return speciesIterator != data.end();
		}), week.end());
	}
}

void EBirdDataProcessor::RecommendHotspots(const std::set<UString::String>& consolidatedSpeciesList,
	const UString::String& country, const UString::String& state, const UString::String& county,
	const CalendarParameters& calendarParameters) const
{
	Cout << "Checking eBird for recent sightings..." << std::endl;

	EBirdInterface e(appConfig.eBirdApiKey);
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

		const auto hotspots(e.GetHotspotsWithRecentObservationsOf(speciesCode, region, calendarParameters.recentObservationPeriod));
		for (const auto& spot : hotspots)
		{
			hotspotInfo[spot].push_back(species);
			recentSpecies.emplace(species);
		}
	}

	Cout << recentSpecies.size() << " needed species have been observed within the last " << calendarParameters.recentObservationPeriod << " days" << std::endl;

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

	if (!calendarParameters.targetInfoFileName.empty())
		GenerateHotspotInfoFile(sortedHotspots, calendarParameters, region);
}

void EBirdDataProcessor::GenerateHotspotInfoFile(
	const std::vector<std::pair<std::vector<UString::String>, EBirdInterface::LocationInfo>>& hotspots,
	const CalendarParameters& calendarParameters,
	const UString::String& regionCode) const
{
	Cout << "Writing hotspot information to file..." << std::endl;

	UString::OFStream infoFile(calendarParameters.targetInfoFileName.c_str());
	if (!infoFile.good() || !infoFile.is_open())
	{
		Cerr << "Failed to open '" << calendarParameters.targetInfoFileName << "' for output\n";
		return;
	}

	if (!calendarParameters.homeLocation.empty())
		infoFile << "Travel time and distance given from " << calendarParameters.homeLocation << '\n';

	std::map<UString::String, UString::String> speciesToObservationTimeMap;

	for (const auto& h : hotspots)
	{
		infoFile << '\n' << h.second.name;
		if (!calendarParameters.homeLocation.empty() && !appConfig.googleMapsAPIKey.empty())
		{
			GoogleMapsInterface gMaps(_T("eBirdDataProcessor"), appConfig.googleMapsAPIKey);
			UString::OStringStream ss;
			ss << h.second.latitude << ',' << h.second.longitude;
			GoogleMapsInterface::Directions travelInfo(gMaps.GetDirections(calendarParameters.homeLocation, ss.str()));

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
				EBirdInterface e(appConfig.eBirdApiKey);
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

void EBirdDataProcessor::GenerateRarityScores(const EBDPConfig::ListType& listType,
	const UString::String& country, const UString::String& state, const UString::String& county)
{
	EBirdInterface ebi(appConfig.eBirdApiKey);
	FrequencyFileReader reader(appConfig.frequencyFilePath);
	FrequencyDataYear weekFrequencyData;
	UIntYear checklistCounts;
	unsigned int rarityYearRange;
	if (!reader.ReadRegionData(ebi.GetRegionCode(country, state, county), weekFrequencyData, checklistCounts, rarityYearRange))
		return;

	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData(
		GenerateYearlyFrequencyData(weekFrequencyData, checklistCounts));

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
		Cout << std::left << std::setw(longestName + minSpace) << std::setfill(UString::Char(' ')) << entry.species << entry.frequency << "%";
	// Would be nice to add "observed in x of last n years" here, but that data is weekly
	Cout << std::endl;
}

std::vector<EBirdDataProcessor::FrequencyInfo> EBirdDataProcessor::GenerateYearlyFrequencyData(
	const FrequencyDataYear& frequencyData, const UIntYear& checklistCounts)
{
	std::vector<EBirdDataProcessor::FrequencyInfo> yearFrequencyData;
	double totalObservations(0.0);

	auto weekCountIterator(checklistCounts.begin());
	for (const auto& weekData : frequencyData)
	{
		for (const auto& species : weekData)
		{
			const double observations(*weekCountIterator * species.frequency);
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

		totalObservations += *weekCountIterator;
		++weekCountIterator;
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
	const UString::String resultStart(_T("<li>"));//_T("<div class=\"ResultsList-header\">"));
	const auto resultStartPosition(html.find(resultStart, position));
	if (resultStartPosition == std::string::npos)
		return false;

	UString::String mediaEntryString;
	if (!StringUtilities::ExtractTextContainedInTag(html.substr(position), resultStart, mediaEntryString))
		return false;

	position = resultStartPosition + mediaEntryString.length();

	const UString::String playButtonStart(_T("<span class=\"playButton\">"));
	const auto playButtonStartPosition(mediaEntryString.find(playButtonStart));

	if (playButtonStartPosition != std::string::npos)
		entry.type = MediaEntry::Type::Audio;
	else
		entry.type = MediaEntry::Type::Photo;

	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString, _T("<span class=\"Species-common"), entry.commonName))
		return false;
	entry.commonName = StringUtilities::Trim(entry.commonName);

	const UString::String ratingStart(_T("<div class=\"RatingStars-stars RatingStars-stars-"));
	const auto ratingStartPosition(mediaEntryString.find(ratingStart));
	if (ratingStartPosition != std::string::npos)
	{
		UString::IStringStream ss(mediaEntryString.substr(ratingStartPosition + ratingStart.length(), 1));
		if ((ss >> entry.rating).fail())
			return false;
	}
	else
		entry.rating = 0;

	const UString::String calendarLine(_T("<time"));
	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString, calendarLine, entry.date))
		entry.date = _T("Unknown");// Can happen for sensitive species

	const UString::String locationLine(_T("<use xlink:href=\"#Icon--locationGeneric\"></use></svg>"));
	const auto locationStart(mediaEntryString.find(locationLine));
	if (locationStart == std::string::npos)
		return false;
	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString.substr(locationStart), _T("<span"), entry.location))
		return false;

	const UString::String tagsDivStart(_T("<div class=\"ResultsList-tags\""));
	const auto tagsStartPosition(mediaEntryString.find(tagsDivStart));
	if (tagsStartPosition != std::string::npos)
	{
		auto nextTagPos(tagsStartPosition + tagsDivStart.length());
		UString::String tagsToEnd(mediaEntryString.substr(nextTagPos));
		const auto tagDataStart(_T("<span class=\"ResultsList-label"));
		UString::String tagName;
		while (StringUtilities::ExtractTextContainedInTag(tagsToEnd, tagDataStart, tagName))
		{
			UString::String temp;
			const UString::String secondTag(_T("<span"));
			if (!ExtractBetweenTagAfterTag(tagsToEnd, tagDataStart, secondTag, temp))
				return false;
				
			// TODO:  The data allows multiple specifications (i.e. 2 adult males + 1 immature unknown sex, etc.) but our data structure doens't support storing this info
			// Same for behaviors and sounds
			if (tagName == _T("Age and sex"))
			{
				if (temp.find(_T("Adult")) != std::string::npos)
					entry.age = MediaEntry::Age::Adult;
				else if (temp.find(_T("Immature")) != std::string::npos)
					entry.age = MediaEntry::Age::Immature;
				else if (temp.find(_T("Juvenile")) != std::string::npos)
					entry.age = MediaEntry::Age::Juvenile;
				else
					entry.age = MediaEntry::Age::Unknown;
					
				if (temp.find(_T("Male")) != std::string::npos)
					entry.sex = MediaEntry::Sex::Male;
				else if (temp.find(_T("Female")) != std::string::npos)
					entry.sex = MediaEntry::Sex::Female;
				else
					entry.sex = MediaEntry::Sex::Unknown;
			}
			//else if (tagName == _T("Behaviors")) {}
			else if (tagName == _T("Sounds"))
			{
				if (temp.find(_T("Song")) != std::string::npos)
					entry.sound = MediaEntry::Sound::Song;
				else if (temp.find(_T("Call")) != std::string::npos)
					entry.sound = MediaEntry::Sound::Call;
				else
					entry.sound = MediaEntry::Sound::Other;
			}
			const auto nextStartPos(tagsToEnd.find(temp));
			tagsToEnd = tagsToEnd.substr(nextStartPos);
		}
	}

	UString::String temp;
	/*if (GetDTDDValue(specimenExtra, _T("Sounds"), temp))
	{
		if (temp.compare(_T("Song")) == 0)
			entry.sound = MediaEntry::Sound::Song;
		else if (temp.compare(_T("Call")) == 0)
			entry.sound = MediaEntry::Sound::Call;
		else if (temp.compare(_T("Unknown")) == 0)
			entry.sound = MediaEntry::Sound::Unknown;
		else
			entry.sound = MediaEntry::Sound::Other;
	}

	if (GetDTDDValue(specimenExtra, _T("Age"), temp))
	{
		if (temp.compare(_T("Adult")) == 0)
			entry.age = MediaEntry::Age::Adult;
		else if (temp.compare(_T("Juvenile")) == 0)
			entry.age = MediaEntry::Age::Juvenile;
		else if (temp.compare(_T("Immature")) == 0)
			entry.age = MediaEntry::Age::Immature;
		else
			entry.age = MediaEntry::Age::Unknown;
	}

	if (GetDTDDValue(specimenExtra, _T("Sex"), temp))
	{
		if (temp.compare(_T("Male")) == 0)
			entry.sex = MediaEntry::Sex::Male;
		else if (temp.compare(_T("Female")) == 0)
			entry.sex = MediaEntry::Sex::Female;
		else
			entry.sex = MediaEntry::Sex::Unknown;
	}
	
	UString::String specimenLinks;
	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString, _T("<ul class=\"SpecimenLinks\""), specimenLinks))
		return false;*/

	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString, _T("<a href=\"https://ebird.org/checklist/"), temp))
		//return false;
		temp.clear();// This can happen for hidden checklists or sensitive species!  Don't fail!
	entry.checklistId = GetLastWord(temp);

	if (!StringUtilities::ExtractTextContainedInTag(mediaEntryString, _T("<a href=\"https://macaulaylibrary.org/asset/"), temp))
		return false;
	entry.macaulayId = GetLastWord(temp);

	return true;
}

UString::String EBirdDataProcessor::GetLastWord(const UString::String& s)
{
	const auto lastNotSpace(s.find_last_not_of(UString::String(_T(" \t\r\n"))));
	const auto lastSpace(s.substr(0,lastNotSpace).find_last_of(UString::String(_T(" \t\r\n"))));
	if (lastSpace == std::string::npos)
		return s;
	return s.substr(lastSpace + 1, lastNotSpace - lastSpace);
}

bool EBirdDataProcessor::GetValueFromLITag(const UString::String& html, const UString::String& svgString, UString::String& value)
{
	const auto svgStartLocation(html.find(svgString));
	if (svgStartLocation == std::string::npos)
		return false;

	const auto liEndLocation(html.find(_T("</li>"), svgStartLocation + svgString.length()));
	value = StringUtilities::Trim(html.substr(svgStartLocation + svgString.length(), liEndLocation - svgStartLocation - svgString.length()));

	return true;
}

bool EBirdDataProcessor::GetDTDDValue(const UString::String& html, const UString::String& label, UString::String& value)
{
	const auto searchString(_T("<dt>") + label + _T("</dt>"));
	const auto labelLocation(html.find(searchString));
	if (labelLocation == std::string::npos)
		return false;

	return StringUtilities::ExtractTextContainedInTag(html.substr(labelLocation), _T("<dd"), value);
}

// Example: <a stuff="something">b</a><c>d</c>
// This can be used to extract the "d" given that tag <c> follows tag <a>
bool EBirdDataProcessor::ExtractBetweenTagAfterTag(const UString::String& html,
	const UString::String& firstTag, const UString::String& secondTag, UString::String& value)
{
	const auto firstTagStart(html.find(firstTag));
	if (firstTagStart == std::string::npos)
		return false;
	
	return StringUtilities::ExtractTextContainedInTag(html.substr(firstTagStart + firstTag.length()), secondTag, value);
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
bool EBirdDataProcessor::GenerateMediaList(const UString::String& mediaListHTML)
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

	UString::OFStream mediaList(appConfig.mediaFileName.c_str());
	if (!mediaList.is_open() || !mediaList.good())
	{
		Cerr << "Failed to open '" << appConfig.mediaFileName << "' for output\n";
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

bool EBirdDataProcessor::ReadMediaList()
{
	UString::IFStream mediaFile(appConfig.mediaFileName.c_str());
	if (!mediaFile.is_open() || !mediaFile.good())
	{
		Cerr << "Failed to open '" << appConfig.mediaFileName << "' for input" << std::endl;
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
					entry.photoRating.push_back(m.rating);
				else// if (m.type == MediaEntry::Type::Audio)
					entry.audioRating.push_back(m.rating);
				//break;// Efficiency gain if we break, but if an entry has both audio and photo media, only one of them will be assigned.  In practice, efficiency gain here is not needed.
			}
		}
	}

	return true;
}

std::vector<UString::String> EBirdDataProcessor::ListFilesInDirectory(const UString::String& directory)
{
	if (!fs::exists(directory))
	{
		Cerr << "Directory '" << directory << "' does not exist\n";
		return std::vector<UString::String>();
	}

	std::vector<UString::String> fileNames;
	for (const auto& e : fs::recursive_directory_iterator(directory))
	{
		if (fs::is_regular_file(e.status()))
			fileNames.push_back(UString::ToStringType(e.path()));
	}

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

bool EBirdDataProcessor::GatherFrequencyData(const std::vector<UString::String>& targetParentRegionCodes,
	std::vector<YearFrequencyInfo>& frequencyInfo, unsigned int& rarityYearRange) const
{
	auto fileNames(ListFilesInDirectory(appConfig.frequencyFilePath));
	if (fileNames.size() == 0)
		return false;

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), IsNotBinFile), fileNames.end());
	RemoveHighLevelFiles(fileNames);

	frequencyInfo.resize(fileNames.size());
	FrequencyFileReader reader(appConfig.frequencyFilePath);

	unsigned int i(0);
	for (const auto& f : fileNames)
	{
		FrequencyDataYear occurrenceData;
		UIntYear checklistCounts;
		const auto regionCode(Utilities::StripExtension(Utilities::ExtractFileName(f)));
		if (!RegionCodeMatches(regionCode, targetParentRegionCodes))
			continue;

		if (!reader.ReadRegionData(regionCode, occurrenceData, checklistCounts, rarityYearRange))
			return false;

		frequencyInfo[i].frequencyInfo = std::move(occurrenceData);
		frequencyInfo[i].locationCode = regionCode;
		++i;
	}

	// In case of failures, frequencyInfo may not have been fully populated
	frequencyInfo.erase(frequencyInfo.begin() + i, frequencyInfo.end());

	return true;
}

bool EBirdDataProcessor::GatherFrequencyData(const std::vector<UString::String>& targetRegionCodes,
	const std::vector<UString::String>& highDetailCountries,
	const unsigned int& minObservationCount, std::vector<YearFrequencyInfo>& frequencyInfo) const
{
	auto fileNames(ListFilesInDirectory(appConfig.frequencyFilePath));
	if (fileNames.size() == 0)
		return false;

	fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), IsNotBinFile), fileNames.end());
	RemoveHighLevelFiles(fileNames);

	frequencyInfo.resize(fileNames.size());
	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	FrequencyFileReader reader(appConfig.frequencyFilePath);

	std::unordered_map<UString::String, ConsolidationData> consolidatationData;

	auto probEntryIt(frequencyInfo.begin());
	for (const auto& f : fileNames)
	{
		FrequencyDataYear occurrenceData;
		UIntYear checklistCounts;
		const auto regionCode(Utilities::StripExtension(Utilities::ExtractFileName(f)));
		if (!RegionCodeMatches(regionCode, targetRegionCodes))
			continue;

		unsigned int rarityYearRange;
		if (!reader.ReadRegionData(regionCode, occurrenceData, checklistCounts, rarityYearRange))
			return false;

		const auto countryCode(Utilities::ExtractCountryFromRegionCode(regionCode));
		const bool useHighDetail(std::find(highDetailCountries.begin(),
			highDetailCountries.end(), countryCode) != highDetailCountries.end());

		if (useHighDetail)
		{
			probEntryIt->locationCode = RemoveTrailingDash(regionCode);
			pool.AddJob(std::make_unique<CalculateProbabilityJob>(*probEntryIt, std::move(occurrenceData),
				std::move(checklistCounts), minObservationCount, *this));
			++probEntryIt;
		}
		else
		{
			if (consolidatationData.find(countryCode) == consolidatationData.end())
			{
				for (auto& c : consolidatationData[countryCode].checklistCounts)
					c = 0;
			}
			AddConsolidationData(consolidatationData[countryCode], std::move(occurrenceData), std::move(checklistCounts));
		}
	}

	pool.WaitForAllJobsComplete();

	for (auto& f : consolidatationData)
	{
		probEntryIt->locationCode = f.first;
		pool.AddJob(std::make_unique<CalculateProbabilityJob>(*probEntryIt, std::move(f.second.occurrenceData),
			std::move(f.second.checklistCounts), minObservationCount, *this));
		++probEntryIt;
	}

	pool.WaitForAllJobsComplete();
	frequencyInfo.erase(probEntryIt, frequencyInfo.end());// Remove extra entries (only needed if every country were done in high detail)

	return true;
}

bool EBirdDataProcessor::FindBestLocationsForNeededSpecies(const LocationFindingParameters& locationFindingParameters,
	const std::vector<UString::String>& highDetailCountries, const std::vector<UString::String>& targetRegionCodes) const
{
	std::vector<YearFrequencyInfo> newSightingProbability;
	const unsigned int minimumObservationCount(30);// TODO:  Don't hardcode
	if (!GatherFrequencyData(targetRegionCodes, highDetailCountries, minimumObservationCount, newSightingProbability))
		return false;

	if (!WriteBestLocationsViewerPage(locationFindingParameters, highDetailCountries, appConfig.eBirdApiKey, appConfig.kmlLibraryPath, newSightingProbability))
	{
		Cerr << "Faild to create best locations page\n";
		return false;
	}

	return true;
}

bool EBirdDataProcessor::BigYear(const std::vector<UString::String>& region) const
{
	std::vector<YearFrequencyInfo> sightingProbability;
	unsigned int rarityYearRange;
	if (!GatherFrequencyData(region, sightingProbability, rarityYearRange))
		return false;

	std::map<UString::String, double> speciesMaxProbability;// maximum for any county for each species
	std::cout << "Location count = " << sightingProbability.size() << std::endl;
	for (const auto& y : sightingProbability)
	{
		// Exclude AK and HI
		const auto state(Utilities::ExtractStateFromRegionCode(y.locationCode));
		if (state == _T("AK") || state == _T("HI"))
			continue;

		for (const auto& wk : y.frequencyInfo)
		{
			for (const auto& species : wk)
			{
				if (species.isRarity && species.yearsObservedInLastNYears < rarityYearRange)
					continue;

				auto it = speciesMaxProbability.find(species.compareString);
				if (it == speciesMaxProbability.end())
					speciesMaxProbability[species.compareString] = species.frequency;
				else if (species.frequency > it->second)
					it->second = species.frequency;
			}
		}
	}

	std::cout << "Total number of non-rare species = " << speciesMaxProbability.size() << std::endl;

	const std::array<double, 7> thresholds = { 25.0, 20.0, 15.0, 10.0, 7.0, 5.0, 3.0 };// [%] must be listed in descending order
	std::array<unsigned int, 7> counts;
	for (auto& c : counts)
		c = 0;

	for (const auto& s : speciesMaxProbability)
	{
		for (size_t i = 0; i < thresholds.size(); ++i)
		{
			if (s.second >= thresholds[i])
			{
				++counts[i];
				break;
			}
		}
	}

	unsigned int runningTotal(0);
	for (size_t i = 0; i < thresholds.size(); ++i)
	{
		runningTotal += counts[i];
		std::cout << "Count with max county frequency > " << thresholds[i] << "% = " << runningTotal << std::endl;
	}

	return true;
}

void EBirdDataProcessor::ConvertProbabilityToCounts(FrequencyDataYear& data, UIntYear& counts)
{
	for (unsigned int i = 0; i < counts.size(); ++i)
	{
		for (auto& entry : data[i])
			entry.frequency = static_cast<int>(entry.frequency * counts[i] + 0.5);
	}
}

void EBirdDataProcessor::ConvertCountsToProbability(FrequencyDataYear& data, UIntYear& counts)
{
	for (unsigned int i = 0; i < counts.size(); ++i)
	{
		for (auto& entry : data[i])
			entry.frequency /= counts[i];
	}
}

void EBirdDataProcessor::AddConsolidationData(ConsolidationData& existingData,
	FrequencyDataYear&& newData, UIntYear&& newCounts)
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
	UIntYear&& checklistCounts, const unsigned int& thresholdObservationCount,
	std::array<double, 48>& probabilities, std::array<std::vector<FrequencyInfo>, 48>& species) const
{
	EliminateObservedSpecies(frequencyData);

	for (unsigned int i = 0; i < probabilities.size(); ++i)
	{
		if (checklistCounts[i] < thresholdObservationCount)// Ignore counties which have very few observations (due to insufficient data) (TODO)
		{
			probabilities[i] = 0.0;
			continue;
		}

		double product(1.0);
		for (const auto& entry : frequencyData[i])
		{
			if (entry.isRarity)
				continue;

			product *= (1.0 - entry.frequency / 100.0);
			species[i].push_back(FrequencyInfo(entry.species, entry.frequency));
		}

		probabilities[i] = 1.0 - product;
	}

	return true;
}

bool EBirdDataProcessor::WriteBestLocationsViewerPage(const LocationFindingParameters& locationFindingParameters,
	const std::vector<UString::String>& highDetailCountries, const UString::String& eBirdAPIKey, const UString::String& kmlLibraryPath,
	const std::vector<YearFrequencyInfo>& observationProbabilities)
{
	MapPageGenerator generator(locationFindingParameters, highDetailCountries, eBirdAPIKey, kmlLibraryPath);
	return generator.WriteBestLocationsViewerPage(locationFindingParameters.baseOutputFileName, observationProbabilities);
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

bool EBirdDataProcessor::FindBestTripLocations(const BestTripParameters& bestTripParameters, const std::vector<UString::String>& highDetailCountries,
	const std::vector<UString::String>& targetRegionCodes, const UString::String& outputFileName) const
{
	std::vector<YearFrequencyInfo> newSightingProbability;
	if (!GatherFrequencyData(targetRegionCodes, highDetailCountries,
		bestTripParameters.minimumObservationCount, newSightingProbability))
		return false;

	// Generate an index list for each week and sort it based on # of species for each location
	std::array<std::vector<unsigned int>, 48> indexList;
	for (unsigned int i = 0; i < indexList.size(); ++i)
	{
		indexList[i].resize(newSightingProbability.size());
		std::iota(indexList[i].begin(), indexList[i].end(), 0);

		std::sort(indexList[i].begin(), indexList[i].end(), [&newSightingProbability, i](const unsigned int& a, const unsigned int& b)
		{
			return newSightingProbability[a].frequencyInfo[i].size() > newSightingProbability[b].frequencyInfo[i].size();
		});
	}

	UString::OFStream outFile(UString::ToNarrowString(outputFileName));
	if (!outFile.good() || !outFile.is_open())
	{
		Cerr << "Failed to open '" << outputFileName << "' for output\n";
		return false;
	}

	outFile << "January 1,"
		<< "January 8,"
		<< "January 15,"
		<< "January 22,"
		<< "February 1,"
		<< "February 8,"
		<< "February 15,"
		<< "February 22,"
		<< "March 1,"
		<< "March 8,"
		<< "March 15,"
		<< "March 22,"
		<< "April 1,"
		<< "April 8,"
		<< "April 15,"
		<< "April 22,"
		<< "May 1,"
		<< "May 8,"
		<< "May 15,"
		<< "May 22,"
		<< "June 1,"
		<< "June 8,"
		<< "June 15,"
		<< "June 22,"
		<< "July 1,"
		<< "July 8,"
		<< "July 15,"
		<< "July 22,"
		<< "August 1,"
		<< "August 8,"
		<< "August 15,"
		<< "August 22,"
		<< "September 1,"
		<< "September 8,"
		<< "September 15,"
		<< "September 22,"
		<< "October 1,"
		<< "October 8,"
		<< "October 15,"
		<< "October 22,"
		<< "November 1,"
		<< "November 8,"
		<< "November 15,"
		<< "November 22,"
		<< "December 1,"
		<< "December 8,"
		<< "December 15,"
		<< "December 22,";
	outFile << std::endl;

	EBirdInterface ebi(appConfig.eBirdApiKey);

	for (unsigned int i = 0; i < bestTripParameters.topLocationCount; ++i)
	{
		for (unsigned int j = 0; j < indexList.size(); ++j)
			outFile << '"' << ebi.GetRegionName(newSightingProbability[indexList[j][i]].locationCode) << "\" (" << newSightingProbability[indexList[j][i]].frequencyInfo[j].size() << "),";
		outFile << '\n';
	}

	return true;
}

bool EBirdDataProcessor::HuntSpecies(const SpeciesHunt& speciesHunt) const
{
	Cout << "Checking for observations of " << speciesHunt.commonName << " within "
		<< speciesHunt.radius << " km of " << speciesHunt.latitude << ',' << speciesHunt.longitude << '.' << std::endl;

	/*unsigned int recentPeriod(30);// [days]
	EBirdInterface ebi(eBirdApiKey);
	const auto checklists(ebi.GetRecentObservationsNear(speciesHunt.latitude, speciesHunt.longitude, speciesHunt.radius, recentPeriod, true, false));
	if (observations.empty())
	{
		Cout << "No records found." << std::endl;
		return false;
	}
	else
		Cout << "Found " << observations.size() << " records from last " << recentPeriod << " days." << std::endl;

	// TODO:  Currently, it seems the recent observations api returns only one entry for each recently observed species.
	// What we want is recent checklists.  This data is available (GetChecklistFeed()),
	// but it looks like we would have to get the list of checklists, then download each checklist and check the observations
	// with products/checklist/view/{{subId}}, but there is a warning to not download a large amount of data this way or risk being banned.
	// No indication of how many lists constitutes a large amount of data.  If we get all checklists in a region (state/county?), this
	// includes lat/lon data, so we could then determine fraction km distance to limit the number of lists in which we're actually interested.
	time_t now;
	time(&now);
	struct tm firstSuccessfulObservationDate(*std::localtime(&now));
	struct tm latestSuccessfulObservationDate;
	latestSuccessfulObservationDate.tm_hour = 0;
	latestSuccessfulObservationDate.tm_min = 0;
	latestSuccessfulObservationDate.tm_sec = 0;
	latestSuccessfulObservationDate.tm_year = 0;
	latestSuccessfulObservationDate.tm_mday = 1;
	latestSuccessfulObservationDate.tm_mon = 0;
	struct tm latestAnyObservationDate(latestSuccessfulObservationDate);
	std::vector<EBirdInterface::ObservationInfo> observationsWithTarget;
	std::vector<EBirdInterface::ObservationInfo> onePerChecklist;
	for (const auto& o : observations)
	{
		struct tm copyTime(o.observationDate);
		if (o.commonName == speciesHunt.commonName)// TODO:  Need to allow for species/subspecies matches, too; best to do this by matching first two scientific names
		{
			observationsWithTarget.push_back(o);
			if (difftime(std::mktime(&copyTime), std::mktime(&firstSuccessfulObservationDate)) < 0)
				firstSuccessfulObservationDate = copyTime;
			if (difftime(std::mktime(&copyTime), std::mktime(&latestSuccessfulObservationDate)) > 0)
				latestSuccessfulObservationDate = copyTime;
		}
		
		if (difftime(std::mktime(&copyTime), std::mktime(&latestAnyObservationDate)) > 0)
				latestAnyObservationDate = copyTime;

		// No checklist ID in this data, so we'll have to guess.  We'll assume that if 
		// location ID and date/time are an exact match, it's the same checklist.
		// This seems like a pretty crumby way to do this, but if we're targeting a rarity
		// there's a good chance two checklists at the same place and time would either
		// both be successes or failures, so maybe not so bad.
		bool needToAdd(true);
		for (const auto& o2 : onePerChecklist)
		{
			if (o.locationID == o2.locationID && TimesMatch(o, o2))
			{
				needToAdd = false;
				break;
			}
		}

		if (needToAdd)
			onePerChecklist.push_back(o);
	}

	Cout << "\nDeduced that observation records come from " << onePerChecklist.size() << " unique checklists." << std::endl;
	Cout << observationsWithTarget.size() << " checklists include " << speciesHunt.commonName << " ("
		<< static_cast<double>(observationsWithTarget.size()) / onePerChecklist.size() * 100.0 << "%)." << std::endl;
	Cout << "\nBird first reported " << StringifyDateTime(firstSuccessfulObservationDate) << '.' << std::endl;
	Cout << "Bird last reported " << StringifyDateTime(latestSuccessfulObservationDate) << '.' << std::endl;

	unsigned int missCountSinceSeen(0);
	for (const auto& o : onePerChecklist)
	{
		bool isMiss(true);
		for (const auto& o2 : observationsWithTarget)
		{
			if (o.locationID == o2.locationID && TimesMatch(o, o2))
			{
				isMiss = false;
				break;
			}
		}

		if (!isMiss)
			continue;

		struct tm copyTime(o.observationDate);
		if (difftime(std::mktime(&copyTime), std::mktime(&latestSuccessfulObservationDate)) > 0)
			++missCountSinceSeen;
	}

	if (missCountSinceSeen > 0)
		Cout << missCountSinceSeen << " checklists submitted since last successful observation." << std::endl;
	else
		Cout << "No misses reported since last successful observation." << std::endl;*/

	// TODO:  ASCII time-of-day histogram

	return true;
}

bool EBirdDataProcessor::TimesMatch(const EBirdInterface::ObservationInfo& o1, const EBirdInterface::ObservationInfo& o2)
{
	if (o1.observationDate.tm_year != o2.observationDate.tm_year ||
		o1.observationDate.tm_mon != o2.observationDate.tm_mon ||
		o1.observationDate.tm_mday != o2.observationDate.tm_mday)
		return false;

	if (o1.dateIncludesTimeInfo != o2.dateIncludesTimeInfo)
		return false;
	else if (!o1.dateIncludesTimeInfo)
		return true;

	return o1.observationDate.tm_hour == o2.observationDate.tm_hour &&
		o1.observationDate.tm_min == o2.observationDate.tm_min;
}

UString::String EBirdDataProcessor::StringifyDateTime(struct tm& dateTime)
{
	UString::OStringStream ss;
	ss << dateTime.tm_mon + 1 << '/' << dateTime.tm_mday << '/' << dateTime.tm_year + 1900 << ' '
		<< dateTime.tm_hour << ':' << std::setw(2) << std::setfill(UString::Char('0')) << dateTime.tm_min;
	return ss.str();
}

unsigned int EBirdDataProcessor::CountConsecutiveLeadingQuotes(UString::IStringStream& ss)
{
	unsigned int count(0);
	while (ss.peek() == UString::Char('"'))
	{
		ss.ignore();
		++count;
	}

	return count;
}

bool EBirdDataProcessor::GenerateTimeOfYearData(const TimeOfYearParameters& toyParameters,
	const std::vector<UString::String>& regionCodes) const
{
	FrequencyFileReader ffReader(appConfig.frequencyFilePath);
	FrequencyDataYear frequencyData;
	UIntYear checklistCounts;
	for (auto& c : checklistCounts)
		c = 0;

	for (const auto& rc : regionCodes)
	{
		FrequencyDataYear tempFrequencyData;
		UIntYear tempChecklistCounts;
		unsigned int rarityYearRange;
		if (!ffReader.ReadRegionData(rc, tempFrequencyData, tempChecklistCounts, rarityYearRange))
			return false;

		for (unsigned int i = 0; i < frequencyData.size(); ++i)
		{
			for (const auto& tfd : tempFrequencyData[i])
			{
				bool found(false);
				for (auto& fd : frequencyData[i])
				{
					if (tfd.compareString == fd.compareString)
					{
						fd.frequency = floor(fd.frequency * checklistCounts[i] + tfd.frequency * 0.01 * tempChecklistCounts[i] + 0.5) / (checklistCounts[i] + tempChecklistCounts[i]);
						found = true;
					}
				}

				if (!found)
				{
					frequencyData[i].push_back(tfd);
					frequencyData[i].back().frequency = floor(frequencyData[i].back().frequency * 0.01 * tempChecklistCounts[i] + 0.5) / (checklistCounts[i] + tempChecklistCounts[i]);
				}
			}
			checklistCounts[i] += tempChecklistCounts[i];
		}
	}

	const unsigned int totalObservations(static_cast<unsigned int>(std::accumulate(checklistCounts.begin(), checklistCounts.end(), 0)));

	// Convert frequency data from list of species within each week to list of weeks within each species
	std::map<UString::String, DoubleYear> speciesObservationsByWeek;
	DoubleYear zeroYear;
	std::fill(zeroYear.begin(), zeroYear.end(), 0.0);
	for (unsigned int wk = 0; wk < frequencyData.size(); ++wk)
	{
		for (const auto& sp : frequencyData[wk])
		{
			if (speciesObservationsByWeek.find(sp.species) == speciesObservationsByWeek.end())
				speciesObservationsByWeek[sp.species] = zeroYear;

			speciesObservationsByWeek[sp.species][wk] += floor(checklistCounts[wk] * sp.frequency + 0.5);
		}
	}

	std::vector<UString::String> speciesList;
	if (toyParameters.maxProbability > 0.0)// Find all regularly occuring birds with total probability not to exceed maxProbability
	{
		const unsigned int minWeeksPerYear(3);// Used to define what constitutes a "rarity"
		for (const auto& sp : speciesObservationsByWeek)
		{
			unsigned int weekCount(0);
			unsigned int observationCount(0);

			for (const auto& o : sp.second)
			{
				if (o > 0.0)
				{
					++weekCount;
					observationCount += static_cast<unsigned int>(floor(o + 0.5));
				}
			}

			if (weekCount > minWeeksPerYear && 100.0 * observationCount / totalObservations < toyParameters.maxProbability)
				speciesList.push_back(sp.first);
		}

		Cout << "Found " << speciesList.size() << " species with overall probability < " << toyParameters.maxProbability << std::endl;
	}
	else// Only check specific species
		speciesList = toyParameters.commonNames;

	const auto pdfSize(frequencyData.size());
	std::vector<std::vector<double>> pdfs(speciesList.size(), std::vector<double>(pdfSize));
	for (unsigned int i = 0; i < speciesList.size(); ++i)
	{
		std::vector<double> values(frequencyData.size());
		std::copy(speciesObservationsByWeek[speciesList[i]].begin(), speciesObservationsByWeek[speciesList[i]].end(), values.begin());
		std::vector<double> range(pdfSize);
		double temp(1.0);
		const double step(12.0 / range.size());
		std::generate(range.begin(), range.end(), [&temp, &step]()
		{
			double t(temp);
			temp += step;
			return t;
		});

		auto FindStartIndex([](const std::vector<double>& values)
		{
			struct Segment
			{
				size_t start;
				size_t end;
				size_t length;
			};

			std::vector<Segment> zeroSegments;
			size_t next;
			if (values.front() == 0.0)
			{
				Segment s;
				for (s.start = values.size() - 1; s.start > 0; --s.start)
				{
					if (values[s.start - 1] > 0.0)
						break;
				}

				for (s.end = 0; s.end < values.size() - 1; ++s.end)
				{
					if (values[s.end + 1] > 0.0)
						break;
				}
				s.length = s.end + values.size() - s.start;
				zeroSegments.push_back(s);
				next = s.end + 1;
			}
			else
				next = 1;

			for (; next < values.size() - 2; ++next)
			{
				if (values[next + 1] == 0.0)
				{
					Segment s;
					s.start = next + 1;

					for (s.end = s.start + 1; s.end < values.size() - 2; ++s.end)
					{
						if (values[s.end] > 0.0)
							break;
					}

					if (zeroSegments.size() > 0 && s.start == zeroSegments.front().start)
						break;

					s.length = s.end - s.start;
					zeroSegments.push_back(s);
					next = s.end + 1;
				}
			}

			if (zeroSegments.empty())
				return 0ULL;

			Segment longest(zeroSegments.front());
			for (const auto& s : zeroSegments)
			{
				if (s.length > longest.length)
					longest = s;
			}

			return static_cast<unsigned long long>((longest.start + longest.length / 2) % values.size());
		});

		// Rotate input so the longest-duration 0-frequency portion gets split to become beginning/end of time frame, then rotate back after fitting PDF
		// This avoids odd-shaped PDFs for birds that are here only in the winter, for example
		const auto startIndex(FindStartIndex(values));
		std::rotate(values.begin(), values.begin() + startIndex, values.end());
		std::rotate(checklistCounts.begin(), checklistCounts.begin() + startIndex, checklistCounts.end());

		std::vector<std::pair<double, double>> kdeInput(values.size());
		double inputIntegral(0.0);
		const auto indexToMonthFactor(frequencyData.size() / 12.0);
		for (unsigned int j = 0; j < values.size(); ++j)
		{
			kdeInput[j].first = (j + indexToMonthFactor) / indexToMonthFactor;// convert index to floating point 1-based month value
			kdeInput[j].second = values[j] / checklistCounts[j] * 100.0;
			inputIntegral += kdeInput[j].second;
			pdfs[i][j] = kdeInput[j].second;
		}

		KernelDensityEstimation kde;
		pdfs[i] = kde.ComputePDF(kdeInput, range, 0.3);

		// Restore correct order
		std::rotate(pdfs[i].begin(), pdfs[i].begin() + pdfs[i].size() - startIndex, pdfs[i].end());
		assert(pdfs[i].size() == values.size());

		double outputIntegral(0.0);
		for (const auto& p : pdfs[i])
			outputIntegral += p;

		const auto scaleFactor(inputIntegral / kdeInput.size() / outputIntegral * pdfs[i].size());
		for (auto& p : pdfs[i])
			p *= scaleFactor;
	}

	UString::OFStream outFile(UString::ToNarrowString(toyParameters.outputFile));
	if (!outFile.good() || !outFile.is_open())
	{
		Cerr << "Failed to open '" << toyParameters.outputFile << "' for output\n";
		return false;
	}

	outFile << "Month,";
	for (const auto& sp : speciesList)
		outFile << sp << ',';
	outFile << '\n';

	const double multiplier(frequencyData.size() / 12.0);
	for (unsigned int i = 0; i < frequencyData.size(); ++i)
	{
		outFile << (i + multiplier) / multiplier << ',';// Shift to start at month 1.0
		for (const auto& p : pdfs)
			outFile << p[i] << ',';
		outFile << '\n';
	}

	return true;
}

void EBirdDataProcessor::BuildChecklistLinks() const
{
	std::set<UString::String> checklistIds;
	for (const auto& o : data)
		checklistIds.insert(o.submissionID);
		
	std::cout << "Generating URLs for " << checklistIds.size() << " checklists:\n";
	for (const auto& c : checklistIds)
		Cout << "https://ebird.org/checklist/" << c << "\n";
	Cout << std::endl;
}

// TODO:  Make parameter for gap size?
void EBirdDataProcessor::ShowGaps() const
{
	// Implementation only supports weekly gaps, for now
	constexpr unsigned int periodCount(48);
	std::map<UString::String, std::array<bool, periodCount>> hits;
	
	for (const auto& o : data)
	{
		auto it(hits.find(o.compareString));
		const auto i(GetWeekIndex(o.dateTime));
		if (it == hits.end())
		{
			hits[o.compareString] = {false};
			hits[o.compareString][i] = true;
		}
		else
			it->second[i] = true;
	}
	
	auto it(hits.begin());
	for (; it != hits.end(); ++it)
	{
		for (unsigned int i = 0; i < periodCount; ++i)
		{
			if (!it->second[i] && it->second[(i - 1) % periodCount] && it->second[(i + 1) % periodCount])
				Cout << "Gap for " << it->first << " in month " << i / 4 + 1 << ", week " << i % 4 + 1  << std::endl;
		}
	}
}

void EBirdDataProcessor::BuildJSData(const UString::String& fileName) const
{
	cJSON* root(cJSON_CreateArray());

	struct SpeciesOrder
	{
		UString::String commonName;
		UString::String compareString;
		unsigned int order;
		UString::String firstObservedDate;
		
		bool operator==(const UString::String& test) { return test == compareString; }
	};
	
	std::vector<SpeciesOrder> species;
	for (const auto& o : data)
	{
		if (std::find(species.begin(), species.end(), o.compareString) != species.end())
			continue;
			
		SpeciesOrder so;
		so.commonName = o.commonName;
		so.compareString = o.compareString;
		so.order = o.taxonomicOrder;
		
		UString::OStringStream ss;
		ss << 1900 + o.dateTime.tm_year << '-' << std::setw(2) << std::setfill(UString::Char('0')) << 1 + o.dateTime.tm_mon << '-' << std::setw(2) << std::setfill(UString::Char('0')) << o.dateTime.tm_mday;
		so.firstObservedDate = ss.str();
		species.push_back(so);
	}
	
	std::sort(species.begin(), species.end(), [](const SpeciesOrder& a, const SpeciesOrder& b)
	{
		return a.order < b.order;
	});
		
	for (const auto& s : species)
	{
		cJSON* item(cJSON_CreateObject());
		cJSON_AddItemToObject(item, "species", cJSON_CreateString(UString::ToNarrowString(s.commonName).c_str()));
		cJSON_AddItemToObject(item, "firstObservedDate", cJSON_CreateString(UString::ToNarrowString(s.firstObservedDate).c_str()));
		const auto frequency(ComputeFrequency(s.compareString));
		cJSON_AddItemToObject(item, "frequency", cJSON_CreateDoubleArray(frequency.data(), static_cast<int>(frequency.size())));
		
		cJSON_AddItemToArray(root, item);
	}
	 
	UString::OFStream file(fileName);
	file << "var yardBirds = " << cJSON_PrintUnformatted(root) << ";\n";
	 
	cJSON_free(root);
}
 
std::array<double, 48> EBirdDataProcessor::ComputeFrequency(const UString::String& compareString) const
{
	std::array<std::set<UString::String>, 48> checklists = {};
	std::array<int, 48> hits = {};
	
	for (const auto& o : data)
	{
		const auto i(GetWeekIndex(o.dateTime));
		if (o.allObsReported)
			checklists[i].insert(o.submissionID);
		
		if (o.compareString == compareString)
		{
			if (!o.allObsReported && hits[i] == 0)
				hits[i] = -1;
			else if (o.allObsReported && hits[i] == -1)
				hits[i] = 1;
			else if (o.allObsReported)
				++hits[i];
		}
	}
	
	std::array<double, 48> frequency;
	for (unsigned int i = 0; i < frequency.size(); ++i)
	{
		if (hits[i] >= 0)
			frequency[i] = static_cast<double>(hits[i]) / checklists[i].size();
		else
			frequency[i] = -1;
	}
	return frequency;
}

unsigned int EBirdDataProcessor::GetWeekIndex(const std::tm& date)
{
	unsigned int weekOfMonth;
	if (date.tm_mday < 8)
		weekOfMonth = 0;
	else if (date.tm_mday < 15)
		weekOfMonth = 1;
	else if (date.tm_mday < 22)
		weekOfMonth = 2;
	else
		weekOfMonth = 3;
	return date.tm_mon * 4 + weekOfMonth;
}
