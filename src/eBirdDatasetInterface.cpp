// File:  eBirdDatasetInterface.cpp
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

// Local headers
#include "eBirdDatasetInterface.h"
#include "bestObservationTimeEstimator.h"

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
#include <locale>

const UString::String EBirdDatasetInterface::nameIndexFileName(_T("nameIndexMap.csv"));

bool EBirdDatasetInterface::ExtractGlobalFrequencyData(const UString::String& fileName)
{
	if (!DoDatasetParsing(fileName, &EBirdDatasetInterface::ProcessObservationDataFrequency))
		return false;
	RemoveRarities();

	return true;
}

bool EBirdDatasetInterface::DoDatasetParsing(const UString::String& fileName, ProcessFunction processFunction)
{
	assert(frequencyMap.empty());

	UString::IFStream dataset(fileName.c_str());
	if (!dataset.good() || !dataset.is_open())
	{
		Cerr << "Failed to open '" << fileName << "' for input\n";
		return false;
	}

	Cout << "Parsing observation data from '" << fileName << '\'' << std::endl;

	UString::String line;
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

	ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
	uint64_t lineCount(0);
	while (std::getline(dataset, line))
	{
		if (lineCount % 1000000 == 0)
			Cout << "  " << lineCount << " records read" << std::endl;

		pool.AddJob(std::make_unique<LineProcessJobInfo>(line, *this, processFunction));
		++lineCount;
	}

	pool.WaitForAllJobsComplete();
	Cout << "Finished parsing " << lineCount << " lines from dataset" << std::endl;

	return true;
}

bool EBirdDatasetInterface::ProcessLine(const UString::String& line, ProcessFunction processFunction)
{
	Observation observation;
	if (!ParseLine(line, observation))
	{
		Cerr << "Failure parsing data line\n";
		return false;
	}

	(this->*processFunction)(observation);
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

bool EBirdDatasetInterface::WriteNameIndexFile(const UString::String& frequencyDataPath) const
{
	if (!EnsureFolderExists(frequencyDataPath))
	{
		Cerr << "Failed to create target direcotry\n";
		return false;
	}

	UString::OFStream file(frequencyDataPath + nameIndexFileName);
	file.imbue(std::locale());
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

bool EBirdDatasetInterface::WriteFrequencyFiles(const UString::String& frequencyDataPath) const
{
	if (!WriteNameIndexFile(frequencyDataPath))
		return false;

	bool success(true);
	for (const auto& entry : frequencyMap)
	{
		const UString::String path(frequencyDataPath + GetPath(entry.first));
		if (!EnsureFolderExists(path))
		{
			Cerr << "Failed to create directory '" << path << "'\n";
			success = false;
			continue;
		}

		const UString::String fullFileName(path + entry.first + _T(".bin"));
		std::ofstream file(fullFileName.c_str(), std::ios::binary);
		if (!file.is_open() || !file.good())
		{
			Cerr << "Failed to open '" << fullFileName << "' for output\n";
			success = false;
			continue;
		}

		for (const auto& month : entry.second)
		{
			if (!SerializeMonthData(file, month))
			{
				Cerr << "Failed to serialize data to '" << fullFileName << "'\n";
				success = false;
				break;
			}
		}
	}

	return success;
}

bool EBirdDatasetInterface::IncludeInLikelihoodCalculation(const UString::String& commonName)
{
	return commonName.find(_T(" sp.")) == std::string::npos &&// Eliminate Spuhs
		commonName.find(UString::Char('/')) == std::string::npos &&// Eliminate species1/species2 type entries
		commonName.find(_T("hybrid")) == std::string::npos &&// Eliminate hybrids
		commonName.find(_T("Domestic")) == std::string::npos;// Eliminate domestic birds
}

bool EBirdDatasetInterface::EnsureFolderExists(const UString::String& dir)// Creates each level of a directory as needed to generate the full path
{
#ifdef _WIN32
	std::string::size_type nextSlash(std::min(dir.find(UString::Char('/')), dir.find(UString::Char('\\'))));
#else
	std::string::size_type nextSlash(dir.find(UString::Char('/')));
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
		nextSlash = std::min(dir.find(UString::Char('/'), nextSlash + 1), dir.find(UString::Char('\\'), nextSlash + 1));
#else
		nextSlash = dir.find(UString::Char('/'), nextSlash + 1);
#endif// _WIN32
	}

	if (!FolderExists(dir))
	{
		if (!CreateFolder(dir))
			return false;
	}

	return true;
}

bool EBirdDatasetInterface::CreateFolder(const UString::String& dir)// Can only create one more level deep at a time
{
#ifdef _WIN32
	const auto error(_mkdir(UString::ToNarrowString(dir).c_str()));
#else
	const mode_t mode(0733);
	const auto error(mkdir(UString::ToNarrowString(dir).c_str(), mode));
#endif// _WIN32
	return error == 0;
}

bool EBirdDatasetInterface::FolderExists(const UString::String& dir)
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

UString::String EBirdDatasetInterface::GetPath(const UString::String& regionCode)
{
	const auto firstDash(regionCode.find(UString::Char('-')));// Path based on country code only
	assert(firstDash != std::string::npos);
#ifdef _WIN32
	const UString::Char slash('\\');
#else
	const UString::Char slash('/');
#endif// _WIN32
	return regionCode.substr(0, firstDash) + slash;
}

bool EBirdDatasetInterface::ParseInto(const UString::String& s, Time& value)
{
	UString::IStringStream ss(s);
	UString::String token;
	if (!std::getline(ss, token, UString::Char(':')))
		return false;

	if (!ParseInto(token, value.hour))
		return false;

	if (!std::getline(ss, token, UString::Char(':')))
		return false;

	if (!ParseInto(token, value.minute))
		return false;

	return true;
}

bool EBirdDatasetInterface::ParseLine(const UString::String& line, Observation& observation)
{
	unsigned int column(0);
	UString::String token;
	UString::String countryCode, stateCode;
	UString::IStringStream ss(line);
	while (std::getline(ss, token, UString::Char('\t')))
	{
		if (column == 4)
			observation.commonName = token;
		else if (column == 8)
		{
			observation.includesCount = token.compare(_T("X")) != 0;
			if (observation.includesCount && !ParseInto(token, observation.count))
				return false;
		}
		else if (column == 13)
			countryCode = token;
		else if (column == 15)
			stateCode = token;
		else if (column == 17)
			observation.regionCode = token;
		else if (column == 27)
			observation.date = ConvertStringToDate(token);
		else if (column == 28)
		{
			observation.includesTime = !token.empty();
			if (observation.includesTime && !ParseInto(token, observation.time))
				return false;
		}
		else if (column == 30)
			observation.checklistID = token;
		else if (column == 34)
		{
			observation.includesDuration = !token.empty();
			if (observation.includesDuration && !ParseInto(token, observation.duration))
				return false;
		}
		else if (column == 35)
		{
			observation.includesDistance = !token.empty();
			if (observation.includesDistance && !ParseInto(token, observation.distance))
				return false;
		}
		else if (column == 38)
		{
			if (!ParseInto(token, observation.completeChecklist))
				return false;
		}
		else if (column == 41)
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

	return true;
}

bool EBirdDatasetInterface::ExtractTimeOfDayInfo(const UString::String& fileName,
	const std::vector<UString::String>& commonNames, const UString::String& regionCode)
{
	assert(!regionCode.empty());
	assert(!commonNames.empty());

	speciesNamesTimeOfDay = commonNames;
	regionCodeTimeOfDay = regionCode;

	if (!DoDatasetParsing(fileName, &EBirdDatasetInterface::ProcessObservationDataTimeOfDay))
		return false;

	return true;
}

bool EBirdDatasetInterface::WriteTimeOfDayFiles(const UString::String& dataFileName, const TimeOfDayPeriod& period) const
{
	UString::OFStream dataFile(dataFileName);
	if (!dataFile.is_open() || !dataFile.good())
	{
		Cerr << "Failed to open '" << dataFileName << "' for output\n";
		return false;
	}

	// Each row corresponds to a time of day
	// Each column corresponds to a species-time of year (PDFs are columns)

	BestObservationTimeEstimator::PDFArray dummy;
	UString::OStringStream headerRow;
	std::vector<UString::OStringStream> rows(dummy.size());
	const double increment(24.0 / dummy.size());// [hr]
	double t(0.0);
	for (auto& r : rows)
	{
		r << t << ',';
		t += increment;
	}

	for (const auto& species : timeOfDayObservationMap)
	{
		// Group by time period, then for each time period generate PDF and write row to file
		std::vector<std::vector<EBirdInterface::ObservationInfo>> observations;
		Date beginDate;
		beginDate.day = 1;
		beginDate.month = 1;
		if (period == TimeOfDayPeriod::Year)
		{
			headerRow << species.first << ',';
			observations.resize(1);
			const Date endDate(beginDate + 365);
			observations[0] = GetObservationsWithinDateRange(species.second, beginDate, endDate);
		}
		else if (period == TimeOfDayPeriod::Month)
		{
			headerRow << GenerateMonthHeaderRow(species.first);
			observations.resize(12);
			for (auto& o : observations)
			{
				const Date endDate(beginDate + 30);
				o = GetObservationsWithinDateRange(species.second, beginDate, endDate);
				beginDate = endDate + 1;
			}
		}
		else if (period == TimeOfDayPeriod::Week)
		{
			headerRow << GenerateWeekHeaderRow(species.first);
			observations.resize(52);
			for (auto& o : observations)
			{
				const Date endDate(beginDate + 7);
				o = GetObservationsWithinDateRange(species.second, beginDate, endDate);
				beginDate = endDate + 1;
			}
		}

		for (const auto& o : observations)
		{
			const auto pdf(BestObservationTimeEstimator::EstimateBestObservationTimePDF(o));
			for (unsigned int i = 0; i < pdf.size(); ++i)
				rows[i] << pdf[i] << ',';
		}
	}

	dataFile << headerRow.str() << '\n';
	for (auto& r : rows)
		dataFile << r.str() << '\n';

	return true;
}

std::vector<EBirdInterface::ObservationInfo> EBirdDatasetInterface::GetObservationsWithinDateRange(
	const std::vector<Observation>& observations, const Date& beginRange, const Date& endRange)
{
	std::vector<EBirdInterface::ObservationInfo> relevantObservations;
	for (const auto& o : observations)
	{
		if (o.date.month < beginRange.month || (o.date.month == beginRange.month && o.date.day < beginRange.day))
			continue;
		else if (o.date.month > endRange.month || (o.date.month == endRange.month && o.date.day > endRange.day))
			continue;

		EBirdInterface::ObservationInfo observationToAdd;
		observationToAdd.commonName = o.commonName;
		observationToAdd.count = o.count;
		if (o.includesDistance)
			observationToAdd.distance = o.distance;
		if (o.includesDuration)
			observationToAdd.duration = o.duration;
		observationToAdd.dateIncludesTimeInfo = o.includesTime;
		observationToAdd.observationDate.tm_year = o.date.year - 1900;
		observationToAdd.observationDate.tm_mon = o.date.month - 1;
		observationToAdd.observationDate.tm_mday = o.date.day;
		if (o.includesTime)
		{
			observationToAdd.observationDate.tm_hour = o.time.hour;
			observationToAdd.observationDate.tm_min = o.time.minute;
			observationToAdd.observationDate.tm_sec = 0;
		}
		observationToAdd.observationValid = o.approved;
		relevantObservations.push_back(observationToAdd);
	}

	return relevantObservations;
}

UString::String EBirdDatasetInterface::GenerateMonthHeaderRow(const UString::String& species)
{
	UString::OStringStream ss;
	ss << "January (" << species <<
		"),February (" << species <<
		"),March (" << species <<
		"),April (" << species <<
		"),May (" << species <<
		"),June (" << species <<
		"),July (" << species <<
		"),August (" << species <<
		"),September (" << species <<
		"),October (" << species <<
		"),November (" << species <<
		"),December (" << species << "),";
	return ss.str();
}

UString::String EBirdDatasetInterface::GenerateWeekHeaderRow(const UString::String& species)
{
	UString::OStringStream ss;
	for (unsigned int i = 1; i <= 52; ++i)
		ss << "Week " << i << " (" << species << "),";
	return ss.str();
}

bool EBirdDatasetInterface::HeaderMatchesExpectedFormat(const UString::String& line)
{
	const UString::String expectedHeader(_T("GLOBAL UNIQUE IDENTIFIER	LAST EDITED DATE	TAXONOMIC ORDER	CATEGORY	COMMON NAME	SCIENTIFIC NAME	SUBSPECIES COMMON NAME	SUBSPECIES SCIENTIFIC NAME	OBSERVATION COUNT	BREEDING BIRD ATLAS CODE	BREEDING BIRD ATLAS CATEGORY	AGE/SEX	COUNTRY	COUNTRY CODE	STATE	STATE CODE	COUNTY	COUNTY CODE	IBA CODE	BCR CODE	USFWS CODE	ATLAS BLOCK	LOCALITY	LOCALITY ID	 LOCALITY TYPE	LATITUDE	LONGITUDE	OBSERVATION DATE	TIME OBSERVATIONS STARTED	OBSERVER ID	SAMPLING EVENT IDENTIFIER	PROTOCOL TYPE	PROTOCOL CODE	PROJECT CODE	DURATION MINUTES	EFFORT DISTANCE KM	EFFORT AREA HA	NUMBER OBSERVERS	ALL SPECIES REPORTED	GROUP IDENTIFIER	HAS MEDIA	APPROVED	REVIEWED	REASON	TRIP COMMENTS	SPECIES COMMENTS"));
	return line.compare(expectedHeader) == 0;
}

EBirdDatasetInterface::Date EBirdDatasetInterface::ConvertStringToDate(const UString::String& s)
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

void EBirdDatasetInterface::ProcessObservationDataFrequency(const Observation& observation)
{
	if (!observation.approved)
		return;
	else if (!IncludeInLikelihoodCalculation(observation.commonName))
		return;

	const auto monthIndex(GetMonthIndex(observation.date));

	std::lock_guard<std::mutex> lock(mutex);// TODO:  If there were a way to eliminate this lock, there could potentially be a big speed improvement (would need to verify by profiling to ensure read isn't bottleneck)

	auto& entry(frequencyMap[observation.regionCode]);
	nameIndexMap.insert(std::make_pair(observation.commonName, static_cast<uint16_t>(nameIndexMap.size())));
	auto& speciesInfo(entry[monthIndex].speciesList[nameIndexMap[observation.commonName]]);
	speciesInfo.rarityGuess.Update(observation.date);

	if (observation.completeChecklist)
	{
		entry[monthIndex].checklistIDs.insert(observation.checklistID);
		++speciesInfo.occurrenceCount;
	}
}

void EBirdDatasetInterface::ProcessObservationDataTimeOfDay(const Observation& observation)
{
	if (!observation.approved)
		return;
	else if (std::find(speciesNamesTimeOfDay.begin(), speciesNamesTimeOfDay.end(), observation.commonName) == speciesNamesTimeOfDay.end())
		return;
	else if (!RegionMatches(observation.regionCode))
		return;

	std::lock_guard<std::mutex> lock(mutex);
	timeOfDayObservationMap[observation.commonName].push_back(observation);
}

bool EBirdDatasetInterface::RegionMatches(const UString::String& regionCode) const
{
	if (regionCode.length() < regionCodeTimeOfDay.length())
		return false;

	return regionCodeTimeOfDay.substr(0, regionCode.length()).compare(regionCode) == 0;
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

EBirdDatasetInterface::Date EBirdDatasetInterface::Date::operator+(const int& days) const
{
	const time_t oneDay = 24 * 60 * 60;
	struct tm thisTime;
	thisTime.tm_hour = 12;
	thisTime.tm_min = 0;
	thisTime.tm_sec = 0;
	thisTime.tm_mon = month - 1;
	thisTime.tm_mday = day;
	thisTime.tm_year = year - 1900;

	time_t startDate(std::mktime(&thisTime) + days * oneDay);
	thisTime = *localtime(&startDate);

	Date newDate;
	newDate.day = thisTime.tm_mday;
	newDate.year = thisTime.tm_year + 1900;
	newDate.month = thisTime.tm_mon + 1;
	return newDate;
}
