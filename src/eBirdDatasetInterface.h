// File:  eBirdDatasetInterface.h
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

#ifndef EBIRD_DATASET_INTERFACE_H_
#define EBIRD_DATASET_INTERFACE_H_

// Local headers
#include "utilities/uString.h"
#include "threadPool.h"

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

		bool completeChecklist;
		bool approved;
	};

	void ProcessObservationData(const Observation& observation);
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

	struct LineProcessJobInfo : public ThreadPool::JobInfoBase
	{
		LineProcessJobInfo(const UString::String& line, EBirdDatasetInterface &ebdi) : line(line), ebdi(ebdi) {}

		const UString::String line;
		EBirdDatasetInterface& ebdi;

		void DoJob() override
		{
			ebdi.ProcessLine(line);
		}
	};

	bool ProcessLine(const UString::String& line);

	std::mutex mutex;
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
