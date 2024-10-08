// File:  eBirdDatasetInterface.cpp
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

// Local headers
#include "eBirdDatasetInterface.h"
#include "bestObservationTimeEstimator.h"
#include "utilities/memoryMappedFile.h"
#include "stringUtilities.h"
#include "sunCalculator.h"

// POSIX headers
#include <sys/types.h>
#include <sys/stat.h>

// Windows stuff
#ifdef _WIN32
#undef AddJob
#endif

// Standard C++ headers
#include <iostream>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <locale>
#include <cmath>
#include <chrono>
#if __GNUC_MINOR__ >= 8 || defined (WIN32)
#include <filesystem>
#else
#include <experimental/filesystem>
#endif

const UString::String EBirdDatasetInterface::nameIndexFileName(_T("nameIndexMap.csv"));

const unsigned int EBirdDatasetInterface::SpeciesData::Rarity::yearsToCheck(5);
const unsigned int EBirdDatasetInterface::SpeciesData::Rarity::minHitYears(4);// To not be considered a rarity
std::mutex EBirdDatasetInterface::SpeciesData::Rarity::referenceYearMutex;
unsigned int EBirdDatasetInterface::SpeciesData::Rarity::referenceYear = 0;

bool EBirdDatasetInterface::ExtractGlobalFrequencyData(const UString::String& fileName,
	const UString::String& regionDataOutputFileName)
{
	if (!DoDatasetParsing(fileName, &EBirdDatasetInterface::ProcessObservationDataFrequency,
		regionDataOutputFileName))
		return false;
	UpdateRarityAssessment();

	return true;
}

bool EBirdDatasetInterface::DoDatasetParsing(const UString::String& fileName,
	ProcessFunction processFunction, const UString::String& regionDataOutputFileName)
{
	assert(frequencyMap.empty());

	if (!std::experimental::filesystem::exists(fileName))
	{
		Cerr << "File '" << fileName << "' does not exist\n";
		return false;
	}

	const double fileSize(static_cast<double>(std::experimental::filesystem::file_size(fileName)));

	try
	{
		MemoryMappedFile dataset(fileName.c_str());
		if (!dataset.IsOpenAndGood())
		{
			Cerr << "Failed to open '" << fileName << "' for input\n";
			return false;
		}

		Cout << "Parsing observation data from '" << fileName << '\'' << std::endl;

		std::string line;
		if (!dataset.ReadNextLine(line))
		{
			Cerr << "Failed to read header line\n";
			return false;
		}

		const auto columnMap(BuildColumnMapFromHeaderLine(UString::ToStringType(line)));

		regionDataOutputFile.open(regionDataOutputFileName);
		if (regionDataOutputFile.is_open() && regionDataOutputFile.good())
			regionDataOutputFile << UString::ToStringType(line) << '\n';

		ThreadPool pool(std::thread::hardware_concurrency() * 2, 0);
		constexpr unsigned int maxQueueSize(1000000);
		constexpr unsigned int minQueueSize(5000);
		pool.SetQueueSizeControl(maxQueueSize, minQueueSize);
		uint64_t lineCount(0);
		while (dataset.ReadNextLine(line))
		{
			if (lineCount % 1000000 == 0)
				Cout << "  " << lineCount << " records read (" << static_cast<double>(dataset.GetCurrentOffset()) / fileSize * 100.0 << "%)" << std::endl;

			pool.AddJob(std::make_unique<LineProcessJobInfo>(UString::ToStringType(line), *this, processFunction, columnMap));
			++lineCount;
		}

		pool.WaitForAllJobsComplete();
		Cout << "Finished parsing " << lineCount << " lines from dataset" << std::endl;
	}
	catch (const std::exception& ex)
	{
		std::cerr << ex.what() << '\n';
	}

	return true;
}

bool EBirdDatasetInterface::ProcessLine(const UString::String& line, const ColumnMap& columnMap, ProcessFunction processFunction)
{
	Observation observation;
	if (!ParseLine(line, columnMap, observation))
	{
		Cerr << "Failure parsing data line\n";
		return false;
	}

	if (regionDataOutputFile.is_open())
	{
		if (RegionMatches(observation.regionCode))
		{
			std::lock_guard<std::mutex> lock(regionWriteMutex);
			regionDataOutputFile << line << '\n';
		}
	}

	(this->*processFunction)(observation);
	return true;
}

void EBirdDatasetInterface::RemoveRarities()
{
	for (auto& entry : frequencyMap)
	{
		for (auto& week : entry.second)
		{
			for (auto it = week.speciesList.begin(); it != week.speciesList.end(); )
			{
				if (it->second.rarityGuess.mightBeRarity)
					it = week.speciesList.erase(it);
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
		Cerr << "Failed to create target directory\n";
		return false;
	}

	UString::OFStream file(frequencyDataPath + nameIndexFileName);
	file.imbue(std::locale());
	for (const auto& pair : nameIndexMap)
		file << pair.first << ',' << pair.second << '\n';

	return true;
}

bool EBirdDatasetInterface::SerializeWeekData(std::ofstream& file, const FrequencyData& data)
{
	if (!Write(file, static_cast<uint16_t>(data.checklistIDs.size())))
		return false;
	if (!Write(file, static_cast<uint16_t>(data.speciesList.size())))
		return false;
	if (!Write(file, static_cast<uint8_t>(SpeciesData::Rarity::yearsToCheck)))
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

		if (!Write(file, species.second.rarityGuess.mightBeRarity))
			return false;

		if (species.second.rarityGuess.mightBeRarity)// For rarities, append the number of years observed within last n years
		{
			if (!Write(file, static_cast<uint8_t>(species.second.rarityGuess.yearsObservedInLastNYears)))
				return false;
		}
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

		for (const auto& week : entry.second)
		{
			if (!SerializeWeekData(file, week))
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
	return std::experimental::filesystem::create_directory(UString::ToNarrowString(dir));
}

bool EBirdDatasetInterface::FolderExists(const UString::String& dir)
{
	return std::experimental::filesystem::is_directory(dir);
}

UString::String EBirdDatasetInterface::GetPath(const UString::String& regionCode)
{
	const auto firstDash(regionCode.find(UString::Char('-')));// Path based on country code only
#ifdef _WIN32
	const UString::Char slash('\\');
#else
	const UString::Char slash('/');
#endif// _WIN32

	if (firstDash != std::string::npos)
		regionCode + slash;
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

EBirdDatasetInterface::ColumnMap EBirdDatasetInterface::BuildColumnMapFromHeaderLine(const UString::String& headerLine)
{
	UString::String token;
	UString::IStringStream ss(headerLine);
	unsigned int column(0);
	ColumnMap columnMap;
	std::fill(columnMap.begin(), columnMap.end(), std::numeric_limits<size_t>::max());

	while (std::getline(ss, token, UString::Char('\t')))
	{
		if (token == _T("GLOBAL UNIQUE IDENTIFIER"))
			columnMap[static_cast<size_t>(Columns::GlobalUniqueId)] = column;
		else if (token == _T("COMMON NAME"))
			columnMap[static_cast<size_t>(Columns::CommonName)] = column;
		else if (token == _T("OBSERVATION COUNT"))
			columnMap[static_cast<size_t>(Columns::Count)] = column;
		else if (token == _T("COUNTRY CODE"))
			columnMap[static_cast<size_t>(Columns::CountryCode)] = column;
		else if (token == _T("STATE CODE"))
			columnMap[static_cast<size_t>(Columns::StateCode)] = column;
		else if (token == _T("COUNTY CODE"))
			columnMap[static_cast<size_t>(Columns::RegionCode)] = column;
		else if (token == _T("LOCALITY"))
			columnMap[static_cast<size_t>(Columns::LocationName)] = column;
		else if (token == _T("LATITUDE"))
			columnMap[static_cast<size_t>(Columns::Latitude)] = column;
		else if (token == _T("LONGITUDE"))
			columnMap[static_cast<size_t>(Columns::Longitude)] = column;
		else if (token == _T("OBSERVATION DATE"))
			columnMap[static_cast<size_t>(Columns::Date)] = column;
		else if (token == _T("TIME OBSERVATIONS STARTED"))
			columnMap[static_cast<size_t>(Columns::Time)] = column;
		else if (token == _T("SAMPLING EVENT IDENTIFIER"))
			columnMap[static_cast<size_t>(Columns::ChecklistId)] = column;
		else if (token == _T("DURATION MINUTES"))
			columnMap[static_cast<size_t>(Columns::Duration)] = column;
		else if (token == _T("EFFORT DISTANCE KM"))
			columnMap[static_cast<size_t>(Columns::Distance)] = column;
		else if (token == _T("ALL SPECIES REPORTED"))
			columnMap[static_cast<size_t>(Columns::CompleteChecklist)] = column;
		else if (token == _T("GROUP IDENTIFIER"))
			columnMap[static_cast<size_t>(Columns::GroupId)] = column;
		else if (token == _T("APPROVED"))
			columnMap[static_cast<size_t>(Columns::Approved)] = column;
		++column;
	}

	for (const auto& c : columnMap)
	{
		if (c == std::numeric_limits<size_t>::max())
		{
			Cerr << "Failed to map column\n";
			assert(false);
		}
	}

	return columnMap;
}

// TODO:  The below method is where the bulk of the time is spent - need to improve this somehow
bool EBirdDatasetInterface::ParseLine(const UString::String& line, const ColumnMap& columnMap, Observation& observation)
{
	unsigned int column(0);
	UString::String countryCode, stateCode;

	UString::String token;
	UString::IStringStream ss(line);
	while (std::getline(ss, token, UString::Char('\t')))
	{
		if (column == columnMap[static_cast<size_t>(Columns::GlobalUniqueId)])
			observation.uniqueID = token;
		else if (column == columnMap[static_cast<size_t>(Columns::CommonName)])
			observation.commonName = token;
		else if (column == columnMap[static_cast<size_t>(Columns::Count)])
		{
			observation.includesCount = token.compare(_T("X")) != 0;
			if (observation.includesCount && !ParseInto(token, observation.count))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::CountryCode)])
			countryCode = token;
		else if (column == columnMap[static_cast<size_t>(Columns::StateCode)])
			stateCode = token;
		else if (column == columnMap[static_cast<size_t>(Columns::RegionCode)])
			observation.regionCode = token;
		else if (column == columnMap[static_cast<size_t>(Columns::LocationName)])
			observation.locationName = token;
		else if (column == columnMap[static_cast<size_t>(Columns::Latitude)])
		{
			if (!ParseInto(token, observation.latitude))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::Longitude)])
		{
			if (!ParseInto(token, observation.longitude))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::Date)])
			observation.date = ConvertStringToDate(token);
		else if (column == columnMap[static_cast<size_t>(Columns::Time)])
		{
			observation.includesTime = !token.empty();
			if (observation.includesTime && !ParseInto(token, observation.time))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::ChecklistId)])
			observation.checklistID = token;
		else if (column == columnMap[static_cast<size_t>(Columns::Duration)])
		{
			observation.includesDuration = !token.empty();
			if (observation.includesDuration && !ParseInto(token, observation.duration))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::Distance)])
		{
			observation.includesDistance = !token.empty();
			if (observation.includesDistance && !ParseInto(token, observation.distance))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::CompleteChecklist)])
		{
			if (!ParseInto(token, observation.completeChecklist))
				return false;
		}
		else if (column == columnMap[static_cast<size_t>(Columns::GroupId)])
			observation.groupID = token;
		else if (column == columnMap[static_cast<size_t>(Columns::Approved)])
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
	const std::vector<UString::String>& commonNames, const UString::String& regionCode,
	const UString::String& regionDataOutputFileName)
{
	assert(!regionCode.empty());
	assert(!commonNames.empty());

	speciesNamesTimeOfDay = commonNames;
	regionCodeTimeOfDay = regionCode;

	if (!DoDatasetParsing(fileName, &EBirdDatasetInterface::ProcessObservationDataTimeOfDay,
		regionDataOutputFileName))
		return false;

	return true;
}

bool EBirdDatasetInterface::WriteTimeOfDayFiles(const UString::String& dataFileName) const
{
	UString::OFStream dataFile(dataFileName);
	if (!dataFile.is_open() || !dataFile.good())
	{
		Cerr << "Failed to open '" << dataFileName << "' for output\n";
		return false;
	}
	
	// We base sunrise/sunset times on an average location for all observations in the set.
	// We assume that there is no variation in these times year-to-year (not exactly true, but very close).
	// Instead of getting a sunrise/sunset time for each day, we only request sunrise/sunset times every
	// 2 weeks (approx.), then we interpolate based on the exact date (to avoid an unnecessary high volume of
	// API calls which doesn't significantly improve the accuracy of our calculation).
	
	double averageLatitude, averageLongitude;
	GetAverageLocation(averageLatitude, averageLongitude);
	SunTimeArray sunriseTimes, sunsetTimes;// The first and 15th of each month
	SunCalculator sunCalculator;
	for (unsigned int i = 0; i < sunsetTimes.size(); ++i)
	{
		SunCalculator::Date date;
		date.year = 2020;// Doesn't really matter
		date.month = static_cast<unsigned short>(i * 12.0 / sunriseTimes.size() + 1.0);
		date.dayOfMonth = static_cast<unsigned short>(12.0 / sunriseTimes.size() * 30.0 * fmod(i, sunriseTimes.size() / 12.0) + 1);
		if (!sunCalculator.GetSunriseSunset(averageLatitude, averageLongitude, date, sunriseTimes[i], sunsetTimes[i]))
		{
			Cerr << "Failed to get sunrise/sunset time\n";// TODO:  Handle this case more gracefully
			return false;
		}
	}

	// For each observation, scale the time such that 0 = midnight, 6 = sunrise, 18 = sunset and 24 = midnight (again)
	// Find PDF for scaled observation times (so each species gets a single PDF)

	// Each row corresponds to a time of day
	// Each column corresponds to a species (each column is a PDF)

	BestObservationTimeEstimator::PDFArray dummy;
	UString::OStringStream headerRow;
	std::vector<UString::OStringStream> rows(dummy.size());
	const double increment(1.0 / dummy.size());
	double t(0.0);
	headerRow << "Time (-),";
	for (auto& r : rows)
	{
		r << t << ',';
		t += increment;
	}

	std::vector<EBirdInterface::ObservationInfo> allObsVector(allObservationsInRegion.size());
	auto it(allObservationsInRegion.begin());
	for (unsigned int i = 0; i < allObsVector.size(); ++i)
	{
		Observation o(it->second);
		ScaleTime(sunriseTimes, sunsetTimes, o);
		allObsVector[i] = ConvertToObservationInfo(o);
		++it;
	}
	
	allObsVector.erase(std::remove_if(allObsVector.begin(), allObsVector.end(), [](const EBirdInterface::ObservationInfo& o)
	{
		return !o.dateIncludesTimeInfo;
	}), allObsVector.end());

	auto allObsPDF(BestObservationTimeEstimator::EstimateBestObservationTimePDF(allObsVector));
	headerRow << "All Observations,";
	for (unsigned int i = 0; i < allObsPDF.size(); ++i)
		rows[i] << allObsPDF[i] << ',';
	
	const double excludeFactor(0.1);
	const double excludeLimit(excludeFactor * *std::max_element(allObsPDF.begin(), allObsPDF.end()));

	for (const auto& species : timeOfDayObservationMap)
	{
		headerRow << species.first << ',';
		auto observations(GetObservationsOfSpecies(species.first, allObsVector));

		auto pdf(BestObservationTimeEstimator::EstimateBestObservationTimePDF(observations));
		for (unsigned int i = 0; i < pdf.size(); ++i)
		{
			// If we do this blindly, times when there are very few checklists submitted can
			// have an overwhelming scale effect on the PDFs, so we automatically exclude
			// times where there are very few observations
			if (allObsPDF[i] < excludeLimit)
				pdf[i] = 0.0;
			else
				pdf[i] /= allObsPDF[i];// Normalize based on total number of checklists per time period
		}

		const double sum(std::accumulate(pdf.begin(), pdf.end(), 0.0));
		if (sum > 0.0)
		{
			for (unsigned int i = 0; i < pdf.size(); ++i)
				rows[i] << pdf[i] / sum / (24.0 / pdf.size()) << ',';
		}
		else
		{
			for (auto& r : rows)
				r << "0,";
		}
	}

	dataFile << headerRow.str() << '\n';
	for (auto& r : rows)
		dataFile << r.str() << '\n';

	return true;
}

EBirdInterface::ObservationInfo EBirdDatasetInterface::ConvertToObservationInfo(const Observation& o)
{
	EBirdInterface::ObservationInfo oi;
	oi.commonName = o.commonName;

	oi.dateIncludesTimeInfo = o.includesTime;
	oi.observationDate.tm_hour = o.time.hour;
	oi.observationDate.tm_min = o.time.hour;
	oi.observationDate.tm_sec = 0;
	oi.observationDate.tm_year = o.date.year;
	oi.observationDate.tm_mon = o.date.month;
	oi.observationDate.tm_mday = o.date.day;

	if (o.includesCount)
		oi.count = o.count;
	else
		oi.count = 0;

	if (o.includesDistance)
		oi.distance = o.distance;
	else
		oi.distance = 0.0;

	if (o.includesDuration)
		oi.duration = o.duration;
	else
		oi.duration = 0;

	oi.observationValid = o.approved;
	
	oi.latitude = o.latitude;
	oi.longitude = o.longitude;
	
	return oi;
}

void EBirdDatasetInterface::GetAverageLocation(double& averageLatitude, double& averageLongitude) const
{
	// We don't want to average by observation, because the result will be skewed toward more popular birding spots
	double minLat(std::numeric_limits<double>::max());
	double maxLat(std::numeric_limits<double>::min());
	double minLon(std::numeric_limits<double>::max());
	double maxLon(std::numeric_limits<double>::min());
	for (const auto& o : allObservationsInRegion)
	{
		if (o.second.latitude < minLat)
			minLat = o.second.latitude;
		if (o.second.latitude > maxLat)
			maxLat = o.second.latitude;
		if (o.second.longitude < minLon)
			minLon = o.second.longitude;
		if (o.second.longitude > maxLon)
			maxLon = o.second.longitude;
	}
	
	averageLatitude = (maxLat - minLat) * 0.5;
	averageLongitude = (maxLon - minLon) * 0.5;
	
	// Handle wrap-around for locations near longitude = +/- 180 deg
	if (fabs(maxLon - 180.0 - averageLongitude) < fabs(maxLon - averageLongitude) &&
		fabs(minLon + 180.0 - averageLongitude) < fabs(minLon - averageLongitude))
		averageLongitude += 180.0;
	if (averageLongitude > 180.0)
		averageLongitude -= 360.0;
	// Can't wind up with < -180, so no need to check
}

void EBirdDatasetInterface::ScaleTime(const SunTimeArray& sunriseTimes, const SunTimeArray& sunsetTimes, Observation& o)
{
	// Interpolate to find sunrise/sunset times for the date in question
	Date jan1;
	jan1.year = o.date.year;
	jan1.month = 1;
	jan1.day = 1;
	const unsigned int dayOfYear(o.date.GetDayNumber() - Date::GetDayNumberFromDate(jan1));
	const unsigned int daysPerPeriod(static_cast<unsigned int>(365 / sunriseTimes.size()));
	const unsigned int startIndex(std::min(dayOfYear / daysPerPeriod, static_cast<unsigned int>(sunriseTimes.size() - 1)));
	const unsigned int startDayOfYear(startIndex * daysPerPeriod);
	
	const unsigned int endIndex([&startIndex, &sunsetTimes]()
	{
		if (startIndex == sunsetTimes.size() - 1)
			return 0U;
		return startIndex + 1U;
	}());
	const double startRiseTime(sunriseTimes[startIndex]);
	const double endRiseTime(sunriseTimes[endIndex]);
	const double startSetTime(sunsetTimes[startIndex]);
	const double endSetTime(sunsetTimes[endIndex]);
	
	const double fraction((dayOfYear - startDayOfYear) / static_cast<double>(daysPerPeriod));
	const double sunrise(fraction * (endRiseTime - startRiseTime) + startRiseTime);
	const double sunset(fraction * (endSetTime - startSetTime) + startSetTime);

	const double observationMinutes(o.time.hour * 60 + o.time.minute);
	const double halfDayMinutes(12.0 * 60.0);

	double interpolatedTime;
	
	// When we scale the time, we make sunrise = 6 AM and sunset = 6 PM
	if (o.time.hour < sunrise)// Nighttime interpolation
	{
		const double sunsetToMidnight(24.0 * 60.0 - sunset);
		const double minutesFromRef(observationMinutes + sunsetToMidnight);
		const double nightTimeLength(1440 - sunset + sunrise);
		interpolatedTime = minutesFromRef / nightTimeLength * halfDayMinutes - 0.5 * halfDayMinutes;
	}
	else if (o.time.hour > sunset)// Nighttime interpolation
	{
		const double minutesFromRef(observationMinutes - sunset);
		const double nightTimeLength(1440 - sunset + sunrise);
		interpolatedTime = minutesFromRef / nightTimeLength * halfDayMinutes + 1.5 * halfDayMinutes;
	}
	else// Daytime interpolation
	{
		const double minutesFromRef(observationMinutes - sunrise);
		const double dayTimeLength(sunset - sunrise);
		interpolatedTime = minutesFromRef / dayTimeLength * halfDayMinutes + 0.5 * halfDayMinutes;
	}

	if (interpolatedTime < 0.0)
		interpolatedTime += 1440.0;
	
	o.time.hour = static_cast<unsigned int>(floor(interpolatedTime / 60.0));
	o.time.minute = static_cast<unsigned int>(fmod(interpolatedTime, 60.0));
}

unsigned int EBirdDatasetInterface::Date::GetDayNumberFromDate(const Date& date)
{
	const unsigned int m((date.month + 9) % 12);
	const unsigned int y(date.year - m / 10);
	return 365 * y + y / 4 - y / 100 + y / 400 + (m * 306 + 5) / 10 + date.day - 1;
}

EBirdDatasetInterface::Date EBirdDatasetInterface::Date::GetDateFromDayNumber(const unsigned int& dayNumber)
{
	unsigned int y((10000 * dayNumber + 14780) / 3652425);
	int d(dayNumber - (365 * y / 4 - y / 100 + y / 400));
	if (d < 0)
	{
		y = y - 1;
		d = dayNumber - (365 * y + y / 4 - y / 100 + y / 400);
	}
	
	Date date;
	const unsigned int mi((100 * d + 52) / 3600);
	date.month = (mi + 2) % 12 + 1;
	date.year = y + (mi + 2) / 12;
	date.day = d - (mi * 306 + 5) / 10 + 1;
	
	return date;
}

std::vector<EBirdInterface::ObservationInfo> EBirdDatasetInterface::GetObservationsOfSpecies(const UString::String& speciesName, std::vector<EBirdInterface::ObservationInfo>& obsSet)
{
	auto relevantObservations(obsSet);
	relevantObservations.erase(std::remove_if(relevantObservations.begin(), relevantObservations.end(),
		[&speciesName](const EBirdInterface::ObservationInfo& o)
		{
			return o.commonName != speciesName;
		}), relevantObservations.end());

	return relevantObservations;
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

	const auto weekIndex(GetWeekIndex(observation.date));

	std::lock_guard<std::mutex> lock(mutex);// TODO:  If there were a way to eliminate this lock, there could potentially be a big speed improvement (would need to verify by profiling to ensure read isn't bottleneck)

	auto& entry(frequencyMap[observation.regionCode]);
	nameIndexMap.insert(std::make_pair(observation.commonName, static_cast<uint16_t>(nameIndexMap.size())));
	auto& speciesInfo(entry[weekIndex].speciesList[nameIndexMap[observation.commonName]]);
	speciesInfo.rarityGuess.Update(observation.date);

	if (observation.completeChecklist)
	{
		entry[weekIndex].checklistIDs.insert(observation.checklistID);
		++speciesInfo.occurrenceCount;
	}
}

void EBirdDatasetInterface::ProcessObservationDataTimeOfDay(const Observation& observation)
{
	if (!observation.approved)
		return;
	else if (!RegionMatches(observation.regionCode))
		return;

	{
		std::lock_guard<std::mutex> lock(mutex);
		allObservationsInRegion[observation.checklistID] = observation;
	}

	// TODO:  Also filter out groups?  Currently observations made by multiple people sharing a checklist may create some bias...

	if (std::find(speciesNamesTimeOfDay.begin(), speciesNamesTimeOfDay.end(), observation.commonName) == speciesNamesTimeOfDay.end())
		return;

	std::lock_guard<std::mutex> lock(mutex);
	timeOfDayObservationMap[observation.commonName].push_back(observation);
}

bool EBirdDatasetInterface::RegionMatches(const UString::String& regionCode) const
{
	if (regionCode.length() < regionCodeTimeOfDay.length())
		return false;

	return regionCode.substr(0, regionCodeTimeOfDay.length()).compare(regionCodeTimeOfDay) == 0;
}

unsigned int EBirdDatasetInterface::GetWeekIndex(const Date& date)
{
	const unsigned int monthIndex(date.month - 1);
	const unsigned int monthWeekIndex((date.day - 1) / 7);
	return 4 * monthIndex + std::min(monthWeekIndex, 3U);
}

EBirdDatasetInterface::SpeciesData::Rarity::Rarity() : recentObservationYears(yearsToCheck, 0)
{
	assert(!recentObservationYears.empty());
	static_assert(yearsToCheck >= minHitYears, "yearsToCheck must be greater than or equal to minHitYears");
}

void EBirdDatasetInterface::SpeciesData::Rarity::Update(const Date& date)
{
	// The part below here will always work; when Rarity() is called, recentObservationYears is initialized to a vector of zeros with size = yearsToCheck.
	// Here, we get a pointer to the smallest year (i.e. the initial zero or the "longest ago" year).
	// Then we replace that minimum with date.year (unless date.year was already in the list).
	auto minYear(std::min_element(recentObservationYears.begin(), recentObservationYears.end()));
	if (date.year > *minYear)
	{
		bool add(true);
		for (auto &y : recentObservationYears)
		{
			if (y == date.year)
			{
				add = false;
				break;
			}
		}

		if (add)
			*minYear = date.year;
	}

	// We assume that there is enough data that we'll always have some observations (not of any particular species) on 12/31 if the dataset includes data for an entire year
	if (date.month == 12 && date.day == 31)
	{
		std::lock_guard<std::mutex> lock(referenceYearMutex);
		if (date.year > referenceYear)
			referenceYear = date.year;// dataset goes through at least this year
	}
}

void EBirdDatasetInterface::UpdateRarityAssessment()
{
	for (auto& f : frequencyMap)
	{
		for (auto& week : f.second)
		{
			for (auto& species : week.speciesList)
			{
				unsigned int recentYearCount(0);
				for (auto& y : species.second.rarityGuess.recentObservationYears)
				{
					if (y > SpeciesData::Rarity::referenceYear - SpeciesData::Rarity::yearsToCheck)
						++recentYearCount;
				}

				species.second.rarityGuess.mightBeRarity = recentYearCount <= SpeciesData::Rarity::minHitYears;
				if (species.second.rarityGuess.mightBeRarity)
					species.second.rarityGuess.yearsObservedInLastNYears = recentYearCount;
			}
		}
	}
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

bool EBirdDatasetInterface::ExtractObservationsWithinGeometry(
	const UString::String& globalFileName, const UString::String& kmlFileName, const UString::String& outputFileName)
{
	kmlFilterGeometry = KMLLibraryManager::ReadKML(kmlFileName); 
	if (!kmlFilterGeometry)
		return false;
	return DoDatasetParsing(globalFileName, &EBirdDatasetInterface::ProcessObservationKMLFilter, outputFileName);
}

std::vector<EBirdDatasetInterface::MapInfo> EBirdDatasetInterface::GetMapInfo() const
{
	std::vector<EBirdDatasetInterface::MapInfo> mapInfo;// Each entry is a location, and includes a list of checklists at that location
	unsigned int a(0);
	for (const auto& o : allObservationsInRegion)
	{
		if (o.second.checklistID == _T("S73117658"))
			a++;
		bool found(false);
		for (auto& m : mapInfo)
		{
			if (m.latitude == o.second.latitude && m.longitude == o.second.longitude)
			{
				AddObservationToMapInfo(o.second, m);
				found = true;
				break;
			}
		}
		
		if (!found)
		{
			mapInfo.push_back(MapInfo());
			mapInfo.back().latitude = o.second.latitude;
			mapInfo.back().longitude = o.second.longitude;
			mapInfo.back().locationName = o.second.locationName;
			AddObservationToMapInfo(o.second, mapInfo.back());
		}
	}
	
	return mapInfo;
}

void EBirdDatasetInterface::AddObservationToMapInfo(const Observation& o, MapInfo& m)
{
	for (auto& c : m.checklists)
	{	
		if (o.checklistID == c.id)// Already have an entry for this checklist, just increment the species count
		{
			++c.speciesCount;
			return;
		}
		else if (!c.groupID.empty() && o.groupID == c.groupID)// Already included a checklist from this group; don't want to include any others
			return;
	}
	
	m.checklists.push_back(MapInfo::ChecklistInfo());
	m.checklists.back().id = o.checklistID;
	m.checklists.back().speciesCount = 1;
	m.checklists.back().groupID = o.groupID;
	
	UString::OStringStream ss;
	ss << o.date.month << "-" << o.date.day << "-" << o.date.year;
	m.checklists.back().dateString = ss.str();
}

void EBirdDatasetInterface::ProcessObservationKMLFilter(const Observation& observation)
{
	KMLLibraryManager::GeometryInfo::Point p(observation.longitude, observation.latitude);
	if (!KMLLibraryManager::PointIsWithinPolygons(p, *kmlFilterGeometry))
		return;

	std::lock_guard<std::mutex> lock(mutex);
	allObservationsInRegion[observation.uniqueID] = observation;// Don't use the checklist ID as the key for this case, or we'll end up with only one entry per checklist
}

bool EBirdDatasetInterface::ExtractSpeciesWithinTimePeriod(const unsigned int& startMonth, const unsigned int& startDay,
	const unsigned int& endMonth, const unsigned int& endDay, const unsigned int& timePeriodYears) const
{
	const auto now(std::chrono::system_clock::now());
	const unsigned int discardBeforeYear(1970 - timePeriodYears + static_cast<unsigned int>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() / 31537970UL));

	const auto DateIsBetween([](const unsigned int& startMonth, const unsigned int& startDay,
		const unsigned int& endMonth, const unsigned int& endDay, const unsigned int& month, const unsigned int& day)
	{
		if (startMonth == endMonth && month == startMonth)
		{
			if (startDay < endDay && day >= startDay && day <= endDay)
				return true;
			else if (startDay > endDay && (day <= endDay || day >= startDay))// Essentially, excluding a small range of dates
				return true;
		}
		else if (month == startMonth)
			return day >= startDay;
		else if (month == endMonth)
			return day <= endDay;
		else if (startMonth < endMonth && month >= startMonth && month <= endMonth)
			return true;
		else if (startMonth > endMonth && (month <= endMonth || month >= startMonth))
			return true;

		return false;
	});

	std::unordered_map<UString::String, unsigned int> observedSpecies;
	std::set<UString::String> checklistIDs;
	for (const auto& o : allObservationsInRegion)
	{
		if (o.second.date.year < discardBeforeYear ||
			!DateIsBetween(startMonth, startDay, endMonth, endDay, o.second.date.month, o.second.date.day))
			continue;

		if (o.second.completeChecklist)
			checklistIDs.insert(o.second.checklistID);

		const UString::String name(o.second.commonName.substr(0, o.second.commonName.find(_T(" ("))));
		if (name.find(UString::Char('/')) != std::string::npos || name.find(_T("sp.")) != std::string::npos)
			continue;
			
		if (observedSpecies.find(name) == observedSpecies.end())
			observedSpecies[name] = 1;
		else if (o.second.completeChecklist)
			++observedSpecies[name];
	}

	Cout << "\nObserved species (" << observedSpecies.size() << ") in the region (since " << discardBeforeYear << ") include:\n";
	const auto coutState(Cout.flags());
	Cout.precision(2);
	Cout << std::fixed;
	for (const auto& s : observedSpecies)
		Cout << s.first << ' ' << 100.0 * s.second / checklistIDs.size() << "%\n";
	Cout << "\nObservation frequency based on " << checklistIDs.size() << " checklists" << std::endl;
	Cout.flags(coutState);
	
	return true;
}
