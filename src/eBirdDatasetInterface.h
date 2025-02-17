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
#include "eBirdDataProcessor.h"
#include "kmlLibraryManager.h"

// Standard C++ headers
#include <unordered_map>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <mutex>

class EBirdDatasetInterface
{
public:
	EBirdDatasetInterface() = default;

	bool ExtractGlobalFrequencyData(const UString::String& fileName, const UString::String& regionDataOutputFileName);
	bool ExtractLocalFrequencyData(const UString::String& fileName, const unsigned int& month,
		const double& latitude, const double& longitude, const double& radius, const UString::String& outputFileName);
	bool WriteFrequencyFiles(const UString::String& frequencyDataPath) const;

	bool ExtractTimeOfDayInfo(const UString::String& fileName,
		const std::vector<UString::String>& commonNames,
		const UString::String& regionCode, const UString::String& regionDataOutputFileName);
	bool WriteTimeOfDayFiles(const UString::String& dataFileName) const;
	
	bool ExtractObservationsWithinGeometry(const UString::String& globalFileName, const UString::String& kmlFileName, const UString::String& outputFileName);

	bool ExtractSpeciesWithinTimePeriod(const unsigned int& startMonth, const unsigned int& startDay,
		const unsigned int& endMonth, const unsigned int& endDay, const unsigned int& timePeriodYears) const;
	
	struct MapInfo
	{
		double latitude;// [deg]
		double longitude;// [deg]
		UString::String locationName;
		
		struct ChecklistInfo
		{
			UString::String id;
			UString::String groupID;
			UString::String dateString;
			unsigned int speciesCount;
		};
		
		std::vector<ChecklistInfo> checklists;
	};
	
	std::vector<MapInfo> GetMapInfo() const;

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
		
		unsigned int GetDayNumber() const { return GetDayNumberFromDate(*this); }
		
		static unsigned int GetDayNumberFromDate(const Date& date);
		static Date GetDateFromDayNumber(const unsigned int& dayNumber);
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
			Rarity();

			bool mightBeRarity = true;
			void Update(const Date& date);

			static const unsigned int yearsToCheck;
			static const unsigned int minHitYears;

			unsigned int yearsObservedInLastNYears = 0;
			std::vector<unsigned int> recentObservationYears;

			static unsigned int referenceYear;
			static std::mutex referenceYearMutex;
		};

		Rarity rarityGuess;
	};

	struct FrequencyData
	{
		std::set<UString::String> checklistIDs;
		std::map<uint16_t, SpeciesData> speciesList;
	};

	std::unordered_map<UString::String, uint16_t> nameIndexMap;

	typedef std::array<FrequencyData, 48> YearFrequencyData;
	std::unordered_map<UString::String, YearFrequencyData> frequencyMap;// Key is fully qualified eBird region name

	struct Observation
	{
		UString::String uniqueID;
		UString::String commonName;
		UString::String checklistID;
		UString::String groupID;
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
		
		double latitude;// [deg]
		double longitude;// [deg]
		UString::String locationName;
		UString::String locationID;
	};

	std::vector<UString::String> speciesNamesTimeOfDay;
	UString::String regionCodeTimeOfDay;
	UString::OFStream regionDataOutputFile;
	std::unordered_map<UString::String, std::vector<Observation>> timeOfDayObservationMap;// Key is species common name
	std::unordered_map<UString::String, Observation> allObservationsInRegion;// Key is checklist ID
	std::unordered_map<UString::String, std::vector<Observation>> allObservationsbyChecklist;// Key is checklist ID
	bool RegionMatches(const UString::String& regionCode) const;

	std::unique_ptr<KMLLibraryManager::GeometryInfo> kmlFilterGeometry;
	
	void ProcessObservationDataFrequency(const Observation& observation);
	void ProcessObservationDataTimeOfDay(const Observation& observation);
	void ProcessObservationKMLFilter(const Observation& observation);
	typedef void (EBirdDatasetInterface::*ProcessFunction)(const Observation& observation);
	void UpdateRarityAssessment();
	void RemoveRarities();
	void ProcessObservationDataTripPlanning(const Observation& observation);

	struct TripPlanningData
	{
		unsigned int month;
		double latitude;
		double longitude;
		double radius;
	} tripPlanningData;

	static double ComputeDistance(const double latitude1, const double longitude1, const double latitude2, const double longitude2);

	enum class Columns
	{
		GlobalUniqueId,
		CommonName,
		Count,
		CountryCode,
		StateCode,
		RegionCode,
		LocationName,
		Latitude,
		Longitude,
		LocationId,
		Date,
		Time,
		ChecklistId,
		Duration,
		Distance,
		CompleteChecklist,
		GroupId,
		Approved,
		NumberOfColumns
	};

	typedef std::array<size_t, static_cast<size_t>(Columns::NumberOfColumns)> ColumnMap;
	static ColumnMap BuildColumnMapFromHeaderLine(const UString::String& headerLine);

	static bool ParseLine(const UString::String& line, const ColumnMap& columnMap, Observation& observation);
	static unsigned int GetWeekIndex(const Date& date);
	static Date ConvertStringToDate(const UString::String& s);
	static bool IncludeInLikelihoodCalculation(const UString::String& commonName);

	bool WriteNameIndexFile(const UString::String& frequencyDataPath) const;

	static bool SerializeWeekData(std::ofstream& file, const FrequencyData& data);
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
			ProcessFunction processFunction, const ColumnMap& columnMap) : line(line), ebdi(ebdi), processFunction(processFunction), columnMap(columnMap) {}

		const UString::String line;
		EBirdDatasetInterface& ebdi;
		ProcessFunction processFunction;
		const ColumnMap& columnMap;

		void DoJob() override
		{
			ebdi.ProcessLine(line, columnMap, processFunction);
		}
	};

	bool DoDatasetParsing(const UString::String& fileName, ProcessFunction processFunction,
		const UString::String& regionDataOutputFileName);

	bool ProcessLine(const UString::String& line, const ColumnMap& columnMap, ProcessFunction processFunction);
	
	typedef std::array<double, 24> SunTimeArray;
	void GetAverageLocation(double& averageLatitude, double& averageLongitude) const;
	static void ScaleTime(const SunTimeArray& sunriseTimes, const SunTimeArray& sunsetTimes, Observation& o);

	std::mutex mutex;
	std::mutex regionWriteMutex;

	static std::vector<EBirdInterface::ObservationInfo> GetObservationsOfSpecies(const UString::String& speciesName, std::vector<EBirdInterface::ObservationInfo>& obsSet);
	static EBirdInterface::ObservationInfo ConvertToObservationInfo(const Observation& o);
	
	static void AddObservationToMapInfo(const Observation& o, MapInfo& m);

	struct LocationData
	{
		UString::String name;
		double latitude;
		double longitude;
		std::unordered_map<UString::String, unsigned int> speciesList;
		std::set<UString::String> completeChecklistIds;
	};

	static bool WriteSpeciesAtLocationJSON(const std::unordered_map<UString::String, LocationData>& locationData,
		const std::vector<std::pair<double, UString::String>>& sortedSpecies, const double& minLat, const double& minLon,
		const double& maxLat, const double& maxLon, const std::string& fileName);
	static bool CreateLocationJSONData(const std::unordered_map<UString::String, LocationData>& locationData,
		const std::vector<std::pair<double, UString::String>>& sortedSpecies, cJSON*& json);
	static bool CreateSpeciesJSONData(const std::unordered_map<UString::String, LocationData>& locationData,
		const std::vector<std::pair<double, UString::String>>& sortedSpecies, cJSON*& json);
	static bool BuildLocationRecord(const std::pair<UString::String, LocationData>& location,
		const std::vector<std::pair<double, UString::String>>& sortedSpecies, cJSON*& json);
	static bool BuildSpeciesRecord(const std::pair<double, UString::String>& species,
		const std::unordered_map<UString::String, LocationData>& locationData, cJSON*& json);
	static bool BuildSpeciesList(const std::unordered_map<UString::String, unsigned int>& speciesList,
		const std::vector<std::pair<double, UString::String>>& sortedSpecies, cJSON*& json);
	static bool BuildLocationsList(const std::unordered_map<UString::String, LocationData>& locationData,
		const UString::String& speciesName, cJSON*& json);
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
