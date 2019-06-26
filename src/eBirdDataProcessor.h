// File:  eBirdDataProcessor.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_H_
#define EBIRD_DATA_PROCESSOR_H_

// Local headers
#include "eBirdInterface.h"
#include "ebdpConfig.h"
#include "threadPool.h"
#include "stringUtilities.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <vector>
#include <ctime>
#include <sstream>
#include <array>
#include <algorithm>
#include <set>
#include <utility>
#include <cassert>
#include <numeric>

// Local forward declarations
class FrequencyFileReader;

class EBirdDataProcessor
{
public:
	bool Parse(const UString::String& dataFile);
	bool ReadMediaList(const UString::String& mediaFileName);
	bool GenerateMediaList(const UString::String& mediaListHTML, const UString::String& mediaFileName);

	void FilterLocation(const std::vector<UString::String>& locations, const std::vector<UString::String>& counties,
		const std::vector<UString::String>& states, const std::vector<UString::String>& countries);
	void FilterCounty(const std::vector<UString::String>& counties, const std::vector<UString::String>& states,
		const std::vector<UString::String>& countries);
	void FilterState(const std::vector<UString::String>& states, const std::vector<UString::String>& countries);
	void FilterCountry(const std::vector<UString::String>& countries);

	void FilterYear(const unsigned int& year);
	void FilterMonth(const unsigned int& month);
	void FilterWeek(const unsigned int& week);
	void FilterDay(const unsigned int& day);

	void FilterPartialIDs();
	
	void FilterCommentString(const UString::String& commentString);

	void SortData(const EBDPConfig::SortBy& primarySort, const EBDPConfig::SortBy& secondarySort);

	void GenerateUniqueObservationsReport(const EBDPConfig::UniquenessType& type);
	void GenerateRarityScores(const UString::String& frequencyFilePath,
		const EBDPConfig::ListType& listType, const UString::String& eBirdAPIKey,
		const UString::String& country, const UString::String& state, const UString::String& county);
	UString::String GenerateList(const EBDPConfig::ListType& type, const int& minPhotoScore, const int& minAudioScore) const;

	bool GenerateTargetCalendar(const CalendarParameters& calendarParameters,
		const UString::String& outputFileName, const UString::String& frequencyFilePath,
		const UString::String& country, const UString::String& state, const UString::String& county,
		const UString::String& eBirdApiKey) const;

	bool FindBestLocationsForNeededSpecies(const UString::String& frequencyFilePath,
		const LocationFindingParameters& locationFindingParameters, const std::vector<UString::String>& highDetailCountries,
		const UString::String& eBirdAPIKey, const std::vector<UString::String>& targetRegionCodes) const;

	bool FindBestTripLocations(const UString::String& frequencyFilePath,
		const BestTripParameters& bestTripParameters, const std::vector<UString::String>& highDetailCountries,
		const std::vector<UString::String>& targetRegionCodes, const UString::String& outputFileName, const UString::String& eBirdApiKey) const;
		
	static UString::String PrepareForComparison(const UString::String& commonName);

	void DoListComparison() const;

	struct FrequencyInfo
	{
		UString::String species;
		double frequency = 0.0;
		UString::String compareString;// Huge boost in efficiency if we pre-compute this

		FrequencyInfo() = default;
		FrequencyInfo(const UString::String& species, const double& frequency) : species(species), frequency(frequency), compareString(PrepareForComparison(species)) {}
	};

	struct YearFrequencyInfo
	{
		YearFrequencyInfo() = default;
		YearFrequencyInfo(const UString::String& locationCode,
			const std::array<double, 12>& probabilities) : locationCode(locationCode), probabilities(probabilities) {}

		UString::String locationCode;
		std::array<double, 12> probabilities;
		std::array<std::vector<FrequencyInfo>, 12> frequencyInfo;
	};

	template<typename T>
	static bool ParseToken(UString::IStringStream& lineStream, const UString::String& fieldName, T& target);
	
	typedef std::array<std::vector<FrequencyInfo>, 12> FrequencyDataYear;
	typedef std::array<double, 12> DoubleYear;

private:
	static const UString::String headerLine;

	struct Entry
	{
		UString::String submissionID;
		UString::String commonName;
		UString::String scientificName;
		double taxonomicOrder;
		int count;
		UString::String stateProvidence;
		UString::String county;
		UString::String location;
		double latitude;// [deg]
		double longitude;// [deg]
		std::tm dateTime;
		UString::String protocol;
		int duration;// [min]
		bool allObsReported;
		double distanceTraveled;// [km]
		double areaCovered;// [ha]
		int numberOfObservers;
		UString::String breedingCode;
		UString::String speciesComments;
		UString::String checklistComments;

		int photoRating = -1;
		int audioRating = -1;

		UString::String compareString;// Huge boost in efficiency if we pre-compute this
	};

	std::vector<Entry> data;

	void FilterYear(const unsigned int& year, std::vector<Entry>& dataToFilter) const;

	static std::vector<Entry> ConsolidateByLife(const std::vector<Entry>& data);
	static std::vector<Entry> ConsolidateByYear(const std::vector<Entry>& data);
	static std::vector<Entry> ConsolidateByMonth(const std::vector<Entry>& data);
	static std::vector<Entry> ConsolidateByWeek(const std::vector<Entry>& data);
	static std::vector<Entry> ConsolidateByDay(const std::vector<Entry>& data);
	static std::vector<Entry> RemoveHighMediaScores(const int& minPhotoScore, const int& minAudioScore, const std::vector<Entry>& data);

	static std::vector<Entry> DoConsolidation(const EBDPConfig::ListType& type, const std::vector<Entry>& data);

	static bool ParseLine(const UString::String& line, Entry& entry);

	static int DoComparison(const Entry& a, const Entry& b, const EBDPConfig::SortBy& sortBy);

	template<typename T>
	static bool InterpretToken(UString::IStringStream& tokenStream, const UString::String& fieldName, T& target);
	static bool InterpretToken(UString::IStringStream& tokenStream, const UString::String& fieldName, UString::String& target);
	static bool ParseCountToken(UString::IStringStream& lineStream, const UString::String& fieldName, int& target);
	static bool ParseDateTimeToken(UString::IStringStream& lineStream, const UString::String& fieldName,
		std::tm& target, const UString::String& format);

	template<typename T1, typename T2>
	static std::vector<std::pair<T1, T2>> Zip(const std::vector<T1>& v1, const std::vector<T2>& v2);
	template<typename T>
	static std::vector<std::pair<uint64_t, T>> Zip(const std::vector<T>& v);
	template<typename T1, typename T2>
	static void Unzip(const std::vector<std::pair<T1, T2>>& z, std::vector<T1>* v1, std::vector<T2>* v2);
	template<typename T, typename SortPredicate, typename EquivalencePredicate>
	static void StableRemoveDuplicates(std::vector<T>& v, SortPredicate sortPredicate, EquivalencePredicate equivalencePredicate);
	template<typename EquivalencePredicate>
	static void StableRemoveDuplicates(std::vector<Entry>& v, EquivalencePredicate equivalencePredicate);

	static bool CommonNamesMatch(UString::String a, UString::String b);
	static UString::String StripParentheses(UString::String s);

	void EliminateObservedSpecies(FrequencyDataYear& frequencyData) const;
	std::vector<FrequencyInfo> GenerateYearlyFrequencyData(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	static bool RegionCodeMatches(const UString::String& regionCode, const std::vector<UString::String>& codeList);
	static void GuessChecklistCounts(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	void RecommendHotspots(const std::set<UString::String>& consolidatedSpeciesList,
		const UString::String& country, const UString::String& state, const UString::String& county,
		const CalendarParameters& calendarParameters, const UString::String& eBirdApiKey) const;
	void GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<UString::String>, EBirdInterface::LocationInfo>>& hotspots,
		const CalendarParameters& calendarParameters, const UString::String& regionCode, const UString::String& eBirdApiKey) const;

	struct HotspotInfoComparer
	{
		bool operator()(const EBirdInterface::LocationInfo& a, const EBirdInterface::LocationInfo& b) const;
	};

	bool ComputeNewSpeciesProbability(FrequencyDataYear&& frequencyData, DoubleYear&& checklistCounts,
		const double& thresholdFrequency, const unsigned int& thresholdObservationCount,
		std::array<double, 12>& probabilities, std::array<std::vector<FrequencyInfo>, 12>& species) const;

	static bool WriteBestLocationsViewerPage(const LocationFindingParameters& locationFindingParameters,
		const std::vector<UString::String>& highDetailCountries,
		const UString::String& eBirdAPIKey, const std::vector<YearFrequencyInfo>& observationProbabilities);

	class CalculateProbabilityJob : public ThreadPool::JobInfoBase
	{
	public:
		CalculateProbabilityJob(YearFrequencyInfo& frequencyInfo, FrequencyDataYear&& occurrenceData,
			DoubleYear&& checklistCounts, const double& thresholdFrequency,
			const unsigned int& thresholdObservationCount,const EBirdDataProcessor& ebdp) : frequencyInfo(frequencyInfo),
			occurrenceData(occurrenceData), checklistCounts(checklistCounts),
			thresholdFrequency(thresholdFrequency), thresholdObservationCount(thresholdObservationCount), ebdp(ebdp) {}

		YearFrequencyInfo& frequencyInfo;
		FrequencyDataYear occurrenceData;
		DoubleYear checklistCounts;

		const double thresholdFrequency;
		const unsigned int thresholdObservationCount;

		const EBirdDataProcessor& ebdp;

		void DoJob() override
		{
			ebdp.ComputeNewSpeciesProbability(std::move(occurrenceData), std::move(checklistCounts),
				thresholdFrequency, thresholdObservationCount,
				frequencyInfo.probabilities, frequencyInfo.frequencyInfo);
		}
	};

	static std::vector<UString::String> ListFilesInDirectory(const UString::String& directory);
	static bool IsNotBinFile(const UString::String& fileName);
	static void RemoveHighLevelFiles(std::vector<UString::String>& fileNames);
	static UString::String RemoveTrailingDash(const UString::String& s);

	static void PrintListComparison(const std::vector<std::vector<Entry>>& lists);
	static bool IndicesAreValid(const std::vector<unsigned int>& indices, const std::vector<std::vector<Entry>>& lists);
	static UString::String PrintInColumns(const std::vector<std::vector<UString::String>>& cells, const unsigned int& columnSpacing);

	struct MediaEntry
	{
		UString::String macaulayId;
		UString::String commonName;

		enum class Type
		{
			Photo,
			Audio
		};

		Type type;

		int rating;
		UString::String date;
		UString::String location;

		enum class Age
		{
			Unknown,
			Juvenile,
			Immature,
			Adult
		};

		Age age = Age::Unknown;

		enum class Sex
		{
			Unknown,
			Male,
			Female
		};

		Sex sex = Sex::Unknown;

		enum class Sound
		{
			Unknown,
			Song,
			Call,
			Other
		};

		Sound sound = Sound::Unknown;

		UString::String checklistId;
	};

	static bool ExtractNextMediaEntry(const UString::String& html, std::string::size_type& position, MediaEntry& entry);
	static void WriteNextMediaEntry(UString::OFStream& file, const MediaEntry& entry);
	static UString::String GetMediaTypeString(const MediaEntry::Type& type);
	static UString::String GetMediaAgeString(const MediaEntry::Age& age);
	static UString::String GetMediaSexString(const MediaEntry::Sex& sex);
	static UString::String GetMediaSoundString(const MediaEntry::Sound& sound);
	static bool ParseMediaEntry(const UString::String& line, MediaEntry& entry);

	struct ConsolidationData
	{
		FrequencyDataYear occurrenceData;
		std::array<double, 12> checklistCounts;
	};
	static void AddConsolidationData(ConsolidationData& existingData, FrequencyDataYear&& newData, std::array<double, 12>&& newCounts);
	static void ConvertProbabilityToCounts(FrequencyDataYear& data, const std::array<double, 12>& counts);
	static void ConvertCountsToProbability(FrequencyDataYear& data, const std::array<double, 12>& counts);

	bool GatherFrequencyData(const UString::String& frequencyFilePath,
		const std::vector<UString::String>& targetRegionCodes, const std::vector<UString::String>& highDetailCountries,
		const double& minLikilihood, const unsigned int& minObservationCount, std::vector<YearFrequencyInfo>& frequencyInfo) const;
};

template<typename T>
bool EBirdDataProcessor::ParseToken(UString::IStringStream& lineStream, const UString::String& fieldName, T& target)
{
	UString::String token;
	UString::IStringStream tokenStream;

	if (lineStream.peek() == static_cast<int>('"'))// target UString::String may contain commas
	{
		lineStream.ignore();// Skip the first quote

		do// In a loop, so we can properly handle escaped double quotes.
		{
			UString::String tempToken;
			const UString::Char quote('"');
			if (!std::getline(lineStream, tempToken, quote))
				return false;
			token.append(tempToken);
			if (lineStream.peek() == static_cast<int>(quote))
			{
				token.append(_T("\""));
				lineStream.ignore();
			}
		} while (lineStream.peek() == -1 && lineStream.peek() == static_cast<int>(','));

		lineStream.ignore();// Skip the next comma
	}
	else if (!std::getline(lineStream, token, UString::Char(',')))
	{
		/*Cerr << "Failed to read token for " << fieldName << '\n';
		return false;*/
		// Data file drops trailing empty fields, so if there's no checklist comment, this would return false
		target = T{};
		return true;
	}

	if (token.empty())
	{
		target = T{};
		return true;
	}

	tokenStream.str(token);
	return InterpretToken(tokenStream, fieldName, target);
}

template<typename T>
bool EBirdDataProcessor::InterpretToken(UString::IStringStream& tokenStream, const UString::String& fieldName, T& target)
{
	if ((tokenStream >> target).fail())
	{
		Cerr << "Failed to interpret token for " << fieldName << '\n';
		return false;
	}

	return true;
}

template<typename T1, typename T2>
std::vector<std::pair<T1, T2>> EBirdDataProcessor::Zip(const std::vector<T1>& v1, const std::vector<T2>& v2)
{
	assert(v1.size() == v1.size());
	std::vector<std::pair<T1, T2>> z(v1.size());
	auto v1Iterator(v1.cbegin());
	auto v2Iterator(v2.cbegin());
	for (auto& i : z)
	{
		i.first = *v1Iterator;
		i.second = *v2Iterator;
		++v1Iterator;
		++v2Iterator;
	}

	return z;
}

template<typename T>
std::vector<std::pair<uint64_t, T>> EBirdDataProcessor::Zip(const std::vector<T>& v)
{
	std::vector<uint64_t> indexVector(v.size());
	std::iota(indexVector.begin(), indexVector.end(), 0);
	return Zip(indexVector, v);
}

template<typename T1, typename T2>
void EBirdDataProcessor::Unzip(const std::vector<std::pair<T1, T2>>& z, std::vector<T1>* v1, std::vector<T2>* v2)
{
	assert(v1 || v2);
	typename std::vector<T1>::iterator v1Iterator;
	typename std::vector<T2>::iterator v2Iterator;

	if (v1)
	{
		v1->resize(z.size());
		v1Iterator = v1->begin();
	}

	if (v2)
	{
		v2->resize(z.size());
		v2Iterator = v2->begin();
	}

	for (const auto& i : z)
	{
		if (v1)
		{
			*v1Iterator = i.first;
			++v1Iterator;
		}

		if (v2)
		{
			*v2Iterator = i.second;
			++v2Iterator;
		}
	}
}

// Uniqueness is achieved by:
// 1.  Moving unique elements to the front of the vector
// 2.  Returning an iterator pointing to the new end of the vector
// So elements occurring between (and including) the returned iterator and the
// end of the original vector can safely be erased.
template<typename T, typename SortPredicate, typename EquivalencePredicate>
void EBirdDataProcessor::StableRemoveDuplicates(std::vector<T>& v, SortPredicate sortPredicate, EquivalencePredicate equivalencePredicate)
{
	auto z(Zip(v));
	std::stable_sort(z.begin(), z.end(), [sortPredicate](const std::pair<uint64_t, T>& a, const std::pair<uint64_t, T>& b)
	{
		return sortPredicate(a.second, b.second);
	});

	z.erase(std::unique(z.begin(), z.end(), [equivalencePredicate](const std::pair<uint64_t, T>& a, const std::pair<uint64_t, T>& b)
	{
		return equivalencePredicate(a.second, b.second);
	}), z.end());

	std::sort(z.begin(), z.end(), [](const std::pair<uint64_t, T>& a, const std::pair<uint64_t, T>& b)
	{
		return a.first < b.first;
	});

	Unzip(z, static_cast<std::vector<uint64_t>*>(nullptr), &v);
}

template<typename EquivalencePredicate>
void EBirdDataProcessor::StableRemoveDuplicates(std::vector<Entry>& v, EquivalencePredicate equivalencePredicate)
{
	auto sortPredicate([equivalencePredicate](const Entry& a, const Entry& b)
	{
		if (equivalencePredicate(a, b))
			return false;
		return a.commonName.compare(b.commonName) < 0;
	});

	StableRemoveDuplicates(v, sortPredicate, equivalencePredicate);
}

#endif// EBIRD_DATA_PROCESSOR_H_
