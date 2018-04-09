// File:  eBirdDatasetInterface.cpp
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

// Local headers
#include "eBirdDatasetInterface.h"

// POSIX headers
#include <sys/types.h>
#include <sys/stat.h>

// Windows headers
#ifdef _WIN32
#include <direct.h>

#pragma warning(push)
#pragma warning(disable:4505)
#endif

#include <dirent.h>

#ifdef _WIN32
#pragma warning(pop)
#endif

// Standard C++ headers
#include <iostream>
#include <cassert>
#include <algorithm>
#include <numeric>

bool EBirdDatasetInterface::ExtractGlobalFrequencyData(const String& fileName)
{
	assert(frequencyMap.empty());

	IFStream dataset(fileName.c_str());
	if (!dataset.good() || !dataset.is_open())
	{
		Cerr << "Failed to open '" << fileName << "' for input\n";
		return false;
	}

	Cout << "Parsing observation data from '" << fileName << '\'' << std::endl;

	String line;
	if (!std::getline(dataset, line))
	{
		Cerr << "Failed to read header line\n";
		return false;
	}

	if (!HeaderMatchesExpectedFormat(line))
	{
		Cerr << "Header line has unexpected format\n";
		return false;
	}

	uint64_t lineCount(0);
	std::streampos lastReport(0);// TODO:  Remove
	while (std::getline(dataset, line))
	{
		if (dataset.tellg() - lastReport > 1024 * 1024 * 1024)
		{
			Cout << "Read position = " << dataset.tellg() / 1024.0 / 1024 / 1024 << " GB" << std::endl;// TODO:  Remove
			lastReport = dataset.tellg();
			break;// TODO:  Remove
		}
		Observation observation;
		if (!ParseLine(line, observation))
		{
			Cerr << "Failure parsing line " << lineCount << '\n';
			return false;
		}

		ProcessObservationData(observation);
		++lineCount;
	}

	Cout << "Finished parsing " << lineCount << " lines from dataset" << std::endl;
	RemoveRarities();

	return true;
}

void EBirdDatasetInterface::RemoveRarities()
{
	for (auto& entry : frequencyMap)
	{
		for (auto& month : entry.second)
		{
			for (auto it = month.speciesList.begin(); it != month.speciesList.end(); )
			{
				if (it->second.rarityGuess.mightBeRarity)
					month.speciesList.erase(it);
				else
					++it;
			}
		}
	}
}

bool EBirdDatasetInterface::WriteFrequencyFiles(const String& frequencyDataPath) const
{
	for (const auto& entry : frequencyMap)
	{
		const String path(frequencyDataPath + GetPath(entry.first));
		if (!EnsureFolderExists(path))
		{
			Cerr << "Failed to create target direcotry\n";
			return false;
		}

		const String fullFileName(path + entry.first + _T(".csv"));
		OFStream file(fullFileName.c_str());
		if (!file.is_open() || !file.good())
		{
			Cerr << "Failed to open '" << fullFileName << "' for output\n";
			return false;
		}

		file << "January," << entry.second[0].checklistIDs.size() <<
			",February," << entry.second[1].checklistIDs.size() <<
			",March," << entry.second[2].checklistIDs.size() <<
			",April," << entry.second[3].checklistIDs.size() <<
			",May," << entry.second[4].checklistIDs.size() <<
			",June," << entry.second[5].checklistIDs.size() <<
			",July," << entry.second[6].checklistIDs.size() <<
			",August," << entry.second[7].checklistIDs.size() <<
			",September," << entry.second[8].checklistIDs.size() <<
			",October," << entry.second[9].checklistIDs.size() <<
			",November," << entry.second[10].checklistIDs.size() <<
			",December," << entry.second[11].checklistIDs.size() << ",\n";

		unsigned int i;
		for (i = 0; i < entry.second.size(); ++i)
			file << "Species,Frequency,";
		file << '\n';

		const unsigned int maxSpecies([&entry]()
		{
			unsigned int s(0);
			for (const auto& d : entry.second)
			{
				if (d.speciesList.size() > s)
					s = d.speciesList.size();
			}
			return s;
		}());

		std::array<std::map<String, SpeciesData>::const_iterator, 12> iterators;
		for (i = 0; i < iterators.size(); ++i)
			iterators[i] = entry.second[i].speciesList.begin();

		for (i = 0; i < maxSpecies; ++i)
		{
			for (const auto& m : entry.second)
			{
				if (iterators[i] != m.speciesList.end())
				{
					file << iterators[i]->first << ',' << iterators[i]->second.occurrenceCount * 100.0 / m.checklistIDs.size() << ',';
					++iterators[i];
				}
				else
					file << ",,";
			}

			file << '\n';
		}
	}

	return true;
}

bool EBirdDatasetInterface::EnsureFolderExists(const String& dir)// Creates each level of a directory as needed to generate the full path
{
#ifdef _WIN32
	std::string::size_type nextSlash(std::min(dir.find(Char('/')), dir.find(Char('\\'))));
	while (nextSlash != std::string::npos)
	{
		const auto partialPath(dir.substr(0, nextSlash));
		if (!FolderExists(partialPath))
		{
			if (!CreateFolder(partialPath))
				return false;
		}

		nextSlash = std::min(dir.find(Char('/'), nextSlash + 1), dir.find(Char('\\'), nextSlash + 1));
	}

	if (!FolderExists(dir))
	{
		if (!CreateFolder(dir))
			return false;
	}
#else
#error "not yet implemented"
#endif// _WIN32

	return true;
}

bool EBirdDatasetInterface::CreateFolder(const String& dir)// Can only create one more level deep at a time
{
#ifdef _WIN32
	const auto error(_mkdir(UString::ToNarrowString(dir).c_str()));
#else
	const mode_t mode(0733);
	const auto error(mkdir(UString::ToNarrowString(dir).c_str(), mode));
#endif// _WIN32
	return error == 0;
}

bool EBirdDatasetInterface::FolderExists(const String& dir)
{
	DIR* directory(opendir(UString::ToNarrowString(dir).c_str()));
	if (directory)
	{
		closedir(directory);
		return true;
	}

	// TODO:  Check to ensure ENOENT == errno; if not, opendir() failed for reasons other than directory doesn't exist

	return false;
}

String EBirdDatasetInterface::GetPath(const String& regionCode)
{
	const auto firstDash(regionCode.find(Char('-')));// Path based on country code only
	assert(firstDash != std::string::npos);
#ifdef _WIN32
	const Char slash('\\');
#else
	const Char slash('/');
#endif// _WIN32
	return regionCode.substr(0, firstDash) + slash;
}

bool EBirdDatasetInterface::ParseLine(const String& line, Observation& observation)
{
	unsigned int column(0);
	String token;
	String countryCode, stateCode, dateString;
	IStringStream ss(line);
	while (std::getline(ss, token, Char('\t')))
	{
		if (column == 4)
			observation.commonName = token;
		else if (column == 13)
			countryCode = token;
		else if (column == 15)
			stateCode = token;
		else if (column == 17)
			observation.regionCode = token;
		else if (column == 27)
			dateString = token;
		else if (column == 32)
			observation.checklistID = token;
		else if (column == 39)
		{
			if (!ParseInto(token, observation.completeChecklist))
				return false;
		}
		else if (column == 42)
		{
			if (!ParseInto(token, observation.approved))
				return false;
			break;
		}

		++column;
	}

	if (observation.regionCode.empty())
	{
		if (!stateCode.empty())
			observation.regionCode = stateCode;
		else
			observation.regionCode = countryCode;
	}

	observation.date = ConvertStringToDate(dateString);

	return true;
}

bool EBirdDatasetInterface::HeaderMatchesExpectedFormat(const String& line)
{
	const String expectedHeader(_T("GLOBAL UNIQUE IDENTIFIER	LAST EDITED DATE	TAXONOMIC ORDER	CATEGORY	COMMON NAME	SCIENTIFIC NAME	SUBSPECIES COMMON NAME	SUBSPECIES SCIENTIFIC NAME	OBSERVATION COUNT	BREEDING BIRD ATLAS CODE	BREEDING BIRD ATLAS CATEGORY	AGE/SEX	COUNTRY	COUNTRY CODE	STATE	STATE CODE	COUNTY	COUNTY CODE	IBA CODE	BCR CODE	USFWS CODE	ATLAS BLOCK	LOCALITY	LOCALITY ID	 LOCALITY TYPE	LATITUDE	LONGITUDE	OBSERVATION DATE	TIME OBSERVATIONS STARTED	OBSERVER ID	FIRST NAME	LAST NAME	SAMPLING EVENT IDENTIFIER	PROTOCOL TYPE	PROJECT CODE	DURATION MINUTES	EFFORT DISTANCE KM	EFFORT AREA HA	NUMBER OBSERVERS	ALL SPECIES REPORTED	GROUP IDENTIFIER	HAS MEDIA	APPROVED	REVIEWED	REASON	TRIP COMMENTS	SPECIES COMMENTS	"));
	return line.compare(expectedHeader) == 0;
}

EBirdDatasetInterface::Date EBirdDatasetInterface::ConvertStringToDate(const String& s)
{
	assert(s.length() == 10);
	Date date;
	if (!ParseInto(s.substr(0, 4), date.year) ||
		!ParseInto(s.substr(5, 2), date.month) ||
		!ParseInto(s.substr(8, 2), date.day))
	{
		// TODO:  Anything?
		assert(false);
	}

	return date;
}

EBirdDatasetInterface::Date EBirdDatasetInterface::Date::GetMin()
{
	Date d;
	d.year = 0;
	d.month = 0;
	d.day = 0;
	return d;
}

EBirdDatasetInterface::Date EBirdDatasetInterface::Date::GetMax()
{
	Date d;
	d.year = std::numeric_limits<unsigned int>::max();
	d.month = std::numeric_limits<unsigned int>::max();
	d.day = std::numeric_limits<unsigned int>::max();
	return d;
}

bool EBirdDatasetInterface::Date::operator==(const Date& d) const
{
	return d.year == year &&
		d.month == month &&
		d.day == day;
}

bool EBirdDatasetInterface::Date::operator<(const Date& d) const
{
	if (year < d.year)
		return true;
	else if (year > d.year)
		return false;

	if (month < d.month)
		return true;
	else if (month > d.month)
		return false;

	return day < d.day;
}

bool EBirdDatasetInterface::Date::operator>(const Date& d) const
{
	return d < *this;
}

void EBirdDatasetInterface::ProcessObservationData(const Observation& observation)
{
	if (!observation.approved)
		return;

	auto entry(frequencyMap[observation.regionCode]);
	const auto monthIndex(GetMonthIndex(observation.date));
	auto speciesInfo(entry[monthIndex].speciesList[observation.commonName]);
	speciesInfo.rarityGuess.Update(observation.date);

	if (observation.completeChecklist)
	{
		entry[monthIndex].checklistIDs.insert(observation.checklistID);
		++speciesInfo.occurrenceCount;
	}
}

unsigned int EBirdDatasetInterface::GetMonthIndex(const Date& date)
{
	return date.month - 1;
}

void EBirdDatasetInterface::SpeciesData::Rarity::Update(const Date& date)
{
	if (!mightBeRarity)// No need to continue with updates if we've already determined that it's not a rarity
		return;

	if (date < earliestObservationDate)
		earliestObservationDate = date;
	if (date > latestObservationDate)
		latestObservationDate = date;

	if (!ObservationsIndicateRarity())
		mightBeRarity = false;
}

bool EBirdDatasetInterface::SpeciesData::Rarity::ObservationsIndicateRarity() const
{
	const unsigned int minimumTimeDelta(365);
	return latestObservationDate - earliestObservationDate < minimumTimeDelta;
}

// Returns delta in approx. # of days
int EBirdDatasetInterface::Date::operator-(const Date& d) const
{
	// Assumes each month has 31 days, each year has 365 days
	return (static_cast<int>(year) - static_cast<int>(d.year)) * 365
		+ (static_cast<int>(month) - static_cast<int>(d.month)) * 31
		+ (static_cast<int>(day) - static_cast<int>(d.day));
}
