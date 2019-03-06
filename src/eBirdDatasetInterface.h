// File:  eBirdDatasetInterface.h
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

#ifndef EBIRD_DATASET_INTERFACE_H_
#define EBIRD_DATASET_INTERFACE_H_

// Local headers
#include "utilities/uString.h"
#include "threadPool.h"
#include "eBirdInterface.h"

// Standard C++ headers
#include <unordered_map>
#include <array>
#include <vector>
#include <map>
#include <set>

class EBirdDatasetInterface
{
public:
	bool ExtractGlobalFrequencyData(const UString::String& fileName);
	bool WriteFrequencyFiles(const UString::String& frequencyDataPath) const;

	enum class TimeOfDayPeriod
	{
		Year,
		Month,
		Week
	};

	bool ExtractTimeOfDayInfo(const UString::String& fileName,
		const std::vector<UString::String>& commonNames,
		const UString::String& regionCode);
	bool WriteTimeOfDayFiles(const UString::String& dataFileName, const TimeOfDayPeriod& period) const;

private:
	static const UString::String nameIndexFileName;

	struct Date
	{
		unsigned int year;
		unsigned int month;
		unsigned int day;

		static Date GetMin();
		static Date GetMax();

		bool operator==(const Date& d) const;
		bool operator<(const Date& d) const;
		bool operator>(const Date& d) const;
		int operator-(const Date& d) const;// Returns delta in approx. # of days
		Date operator+(const int& days) const;
	};

	struct Time
	{
		unsigned int hour;// [0-23]
		unsigned int minute;// [0-59]
	};

	struct SpeciesData
	{
		uint32_t occurrenceCount = 0;

		struct Rarity
		{
		public:
			bool mightBeRarity = true;
			void Update(const Date& date);

		private:
			Date earliestObservationDate = Date::GetMax();
			Date latestObservationDate = Date::GetMin();

			bool ObservationsIndicateRarity() const;
		};

		Rarity rarityGuess;
	};

	struct FrequencyData
	{
		std::set<UString::String> checklistIDs;
		std::map<uint16_t, SpeciesData> speciesList;
	};

	std::unordered_map<UString::String, uint16_t> nameIndexMap;

	typedef std::array<FrequencyData, 12> YearFrequencyData;
	std::unordered_map<UString::String, YearFrequencyData> frequencyMap;// Key is fully qualified eBird region name

	struct Observation
	{
		UString::String commonName;
		UString::String checklistID;
		UString::String regionCode;

		Date date;

		bool includesTime;
		Time time;

		bool includesCount;
		unsigned int count;

		bool includesDistance;
		double distance = 0.0;// [km]

		bool includesDuration;
		unsigned int duration = 0;// [min]

		bool completeChecklist;
		bool approved;
	};

	std::vector<UString::String> speciesNamesTimeOfDay;
	UString::String regionCodeTimeOfDay;
	std::unordered_map<UString::String, std::vector<Observation>> timeOfDayObservationMap;// Key is species common name
	bool RegionMatches(const UString::String& regionCode) const;

	static UString::String GenerateMonthHeaderRow(const UString::String& species);
	static UString::String GenerateWeekHeaderRow(const UString::String& species);

	void ProcessObservationDataFrequency(const Observation& observation);
	void ProcessObservationDataTimeOfDay(const Observation& observation);
	typedef void (EBirdDatasetInterface::*ProcessFunction)(const Observation& observation);
	void RemoveRarities();

	static bool ParseLine(const UString::String& line, Observation& observation);
	static unsigned int GetMonthIndex(const Date& date);
	static bool HeaderMatchesExpectedFormat(const UString::String& line);
	static Date ConvertStringToDate(const UString::String& s);
	static bool IncludeInLikelihoodCalculation(const UString::String& commonName);

	bool WriteNameIndexFile(const UString::String& frequencyDataPath) const;

	static bool SerializeMonthData(std::ofstream& file, const FrequencyData& data);
	template<typename T>
	static bool Write(std::ofstream& file, const T& data);

	static UString::String GetPath(const UString::String& regionCode);
	static bool EnsureFolderExists(const UString::String& dir);
	static bool FolderExists(const UString::String& dir);
	static bool CreateFolder(const UString::String& dir);

	template<typename T>
	static bool ParseInto(const UString::String& s, T& value);
	static bool ParseInto(const UString::String& s, Time& value);

	struct LineProcessJobInfo : public ThreadPool::JobInfoBase
	{
		LineProcessJobInfo(const UString::String& line, EBirdDatasetInterface &ebdi,
			ProcessFunction processFunction) : line(line), ebdi(ebdi), processFunction(processFunction) {}

		const UString::String line;
		EBirdDatasetInterface& ebdi;
		ProcessFunction processFunction;

		void DoJob() override
		{
			ebdi.ProcessLine(line, processFunction);
		}
	};

	bool DoDatasetParsing(const UString::String& fileName, ProcessFunction processFunction);

	bool ProcessLine(const UString::String& line, ProcessFunction processFunction);

	std::mutex mutex;

	static std::vector<EBirdInterface::ObservationInfo> GetObservationsWithinDateRange(
		const std::vector<Observation>& observations, const Date& beginRange, const Date& endRange);// inclusive of endpoints
};

template<typename T>
bool EBirdDatasetInterface::Write(std::ofstream& file, const T& data)
{
	// TODO:  Make more robust (handle endianess?)
	file.write(reinterpret_cast<const char*>(&data), sizeof(data));
	return true;
}

template<typename T>
bool EBirdDatasetInterface::ParseInto(const UString::String& s, T& value)
{
	UString::IStringStream ss(s);
	return !(ss >> value).fail();
}

#endif// EBIRD_DATASET_INTERFACE_H_
