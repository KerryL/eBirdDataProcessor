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

const String EBirdDatasetInterface::nameIndexFileName(_T("nameIndexMap.csv"));

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
	while (std::getline(dataset, line))
	{
		if (lineCount % 1000000 == 0)
			Cout << "  " << lineCount << " records parsed" << std::endl;

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
					it = month.speciesList.erase(it);
				else
					++it;
			}
		}
	}
}

bool EBirdDatasetInterface::WriteNameIndexFile(const String& frequencyDataPath) const
{
	if (!EnsureFolderExists(frequencyDataPath))
	{
		Cerr << "Failed to create target direcotry\n";
		return false;
	}

	OFStream file(frequencyDataPath + nameIndexFileName);
	for (const auto& pair : nameIndexMap)
		file << pair.first << ',' << pair.second << '\n';

	return true;
}

bool EBirdDatasetInterface::SerializeMonthData(std::ofstream& file, const FrequencyData& data)
{
	if (!Write(file, static_cast<uint16_t>(data.checklistIDs.size())))
		return false;
	if (!Write(file, static_cast<uint16_t>(data.speciesList.size())))
		return false;

	for (const auto& species : data.speciesList)
	{
		if (!Write(file, species.first))
			return false;

		const double frequency([&data, &species]()
		{
			if (data.checklistIDs.size() == 0)
				return 0.0;
			return 100.0 * species.second.occurrenceCount / data.checklistIDs.size();
		}());

		if (!Write(file, frequency))
			return false;
	}

	return true;
}

bool EBirdDatasetInterface::WriteFrequencyFiles(const String& frequencyDataPath) const
{
	if (!WriteNameIndexFile(frequencyDataPath))
		return false;

	for (const auto& entry : frequencyMap)
	{
		const String path(frequencyDataPath + GetPath(entry.first));
		if (!EnsureFolderExists(path))
		{
			Cerr << "Failed to create target direcotry\n";
			return false;
		}

		const String fullFileName(path + entry.first + _T(".bin"));
		std::ofstream file(fullFileName.c_str(), std::ios::binary);
		if (!file.is_open() || !file.good())
		{
			Cerr << "Failed to open '" << fullFileName << "' for output\n";
			return false;
		}

		for (const auto& month : entry.second)
		{
			if (!SerializeMonthData(file, month))
				return false;
		}
	}

	return true;
}

bool EBirdDatasetInterface::IncludeInLikelihoodCalculation(const String& commonName)
{
	return commonName.find(_T(" sp.")) == std::string::npos &&// Eliminate Spuhs
		commonName.find(Char('/')) == std::string::npos &&// Eliminate species1/species2 type entries
		commonName.find(_T("hybrid")) == std::string::npos &&// Eliminate hybrids
		commonName.find(_T("Domestic")) == std::string::npos;// Eliminate domestic birds
}

bool EBirdDatasetInterface::EnsureFolderExists(const String& dir)// Creates each level of a directory as needed to generate the full path
{
#ifdef _WIN32
	std::string::size_type nextSlash(std::min(dir.find(Char('/')), dir.find(Char('\\'))));
#else
	std::string::size_type nextSlash(dir.find(Char('/')));
#endif// _WIN32
	while (nextSlash != std::string::npos)
	{
		const auto partialPath(dir.substr(0, nextSlash));
		if (!FolderExists(partialPath))
		{
			if (!CreateFolder(partialPath))
				return false;
		}

#ifdef _WIN32
		nextSlash = std::min(dir.find(Char('/'), nextSlash + 1), dir.find(Char('\\'), nextSlash + 1));
#else
		nextSlash = dir.find(Char('/'), nextSlash + 1);
#endif// _WIN32
	}

	if (!FolderExists(dir))
	{
		if (!CreateFolder(dir))
			return false;
	}

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
	else if (!IncludeInLikelihoodCalculation(observation.commonName))
		return;

	auto& entry(frequencyMap[observation.regionCode]);
	const auto monthIndex(GetMonthIndex(observation.date));
	nameIndexMap.insert(std::make_pair(observation.commonName, static_cast<uint16_t>(nameIndexMap.size())));
	auto& speciesInfo(entry[monthIndex].speciesList[nameIndexMap[observation.commonName]]);
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
	const int minimumTimeDelta(365);
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
