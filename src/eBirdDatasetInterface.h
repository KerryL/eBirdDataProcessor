// File:  eBirdDatasetInterface.h
// Date:  4/6/2018
// Auth:  K. Loux
// Desc:  Interface to huge eBird Reference Dataset file.

#ifndef EBIRD_DATASET_INTERFACE_H_
#define EBIRD_DATASET_INTERFACE_H_

// Local headers
#include "utilities/uString.h"

// Standard C++ headers
#include <unordered_map>
#include <array>
#include <vector>
#include <map>
#include <set>

class EBirdDatasetInterface
{
public:
	bool ExtractGlobalFrequencyData(const String& fileName);
	bool WriteFrequencyFiles(const String& frequencyDataPath) const;

private:
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
		std::set<String> checklistIDs;
		std::map<String, SpeciesData> speciesList;
	};

	typedef std::array<FrequencyData, 12> YearFrequencyData;
	std::unordered_map<String, YearFrequencyData> frequencyMap;// Key is fully qualified eBird region name

	struct Observation
	{
		String commonName;
		String checklistID;
		String regionCode;

		Date date;

		bool completeChecklist;
		bool approved;
	};

	void ProcessObservationData(const Observation& observation);
	void RemoveRarities();

	static bool ParseLine(const String& line, Observation& observation);
	static unsigned int GetMonthIndex(const Date& date);
	static bool HeaderMatchesExpectedFormat(const String& line);
	static Date ConvertStringToDate(const String& s);
	static bool IncludeInLikelihoodCalculation(const String& commonName);

	static String GetPath(const String& regionCode);
	static bool EnsureFolderExists(const String& dir);
	static bool FolderExists(const String& dir);
	static bool CreateFolder(const String& dir);

	template<typename T>
	static bool ParseInto(const String& s, T& value);
};

template<typename T>
static bool EBirdDatasetInterface::ParseInto(const String& s, T& value)
{
	IStringStream ss(s);
	return !(ss >> value).fail();
}

#endif// EBIRD_DATASET_INTERFACE_H_
