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

class EBirdDataProcessor
{
public:
	bool Parse(const String& dataFile);
	bool ReadPhotoList(const String& photoFileName);

	void FilterLocation(const String& location, const String& county,
		const String& state, const String& country);
	void FilterCounty(const String& county, const String& state,
		const String& country);
	void FilterState(const String& state, const String& country);
	void FilterCountry(const String& country);

	void FilterYear(const unsigned int& year);
	void FilterMonth(const unsigned int& month);
	void FilterWeek(const unsigned int& week);
	void FilterDay(const unsigned int& day);

	void FilterPartialIDs();

	void SortData(const EBDPConfig::SortBy& primarySort, const EBDPConfig::SortBy& secondarySort);

	void GenerateUniqueObservationsReport(const EBDPConfig::UniquenessType& type);
	void GenerateRarityScores(const String& frequencyFilePath,
		const EBDPConfig::ListType& listType, const String& eBirdAPIKey,
		const String& country, const String& state, const String& county);
	String GenerateList(const EBDPConfig::ListType& type, const bool& withoutPhotosOnly) const;

	bool GenerateTargetCalendar(const unsigned int& topBirdCount,
		const String& outputFileName, const String& frequencyFilePath,
		const String& country, const String& state, const String& county,
		const unsigned int& recentPeriod,
		const String& hotspotInfoFileName, const String& homeLocation,
		const String& mapApiKey, const String& eBirdApiKey) const;

	bool FindBestLocationsForNeededSpecies(const String& frequencyFilePath,
		const String& googleMapsKey, const String& eBirdAPIKey,
		const String& clientId, const String& clientSecret) const;

	struct FrequencyInfo
	{
		String species;
		double frequency = 0.0;
		String compareString;// Huge boost in efficiency if we pre-compute this

		FrequencyInfo() = default;
		FrequencyInfo(const String& species, const double& frequency) : species(species), frequency(frequency), compareString(StringUtilities::Trim(StripParentheses(species))) {}
	};

	struct YearFrequencyInfo
	{
		YearFrequencyInfo() = default;
		YearFrequencyInfo(const String& locationCode,
			const std::array<double, 12>& probabilities) : locationCode(locationCode), probabilities(probabilities) {}

		String locationCode;
		std::array<double, 12> probabilities;
		std::array<std::vector<FrequencyInfo>, 12> frequencyInfo;
	};

	static bool AuditFrequencyData(const String& freqFileDirectory, const String& eBirdApiKey);

	template<typename T>
	static bool ParseToken(IStringStream& lineStream, const String& fieldName, T& target);

private:
	static const String headerLine;

	struct Entry
	{
		String submissionID;
		String commonName;
		String scientificName;
		int taxonomicOrder;
		int count;
		String stateProvidence;
		String county;
		String location;
		double latitude;// [deg]
		double longitude;// [deg]
		std::tm dateTime;
		String protocol;
		int duration;// [min]
		bool allObsReported;
		double distanceTraveled;// [km]
		double areaCovered;// [ha]
		int numberOfObservers;
		String breedingCode;
		String speciesComments;
		String checklistComments;

		bool hasPhoto = false;

		String compareString;// Huge boost in efficiency if we pre-compute this
	};

	std::vector<Entry> data;

	std::vector<Entry> ConsolidateByLife() const;
	std::vector<Entry> ConsolidateByYear() const;
	std::vector<Entry> ConsolidateByMonth() const;
	std::vector<Entry> ConsolidateByWeek() const;
	std::vector<Entry> ConsolidateByDay() const;

	std::vector<Entry> DoConsolidation(const EBDPConfig::ListType& type) const;

	static bool ParseLine(const String& line, Entry& entry);

	static int DoComparison(const Entry& a, const Entry& b, const EBDPConfig::SortBy& sortBy);

	template<typename T>
	static bool InterpretToken(IStringStream& tokenStream, const String& fieldName, T& target);
	static bool InterpretToken(IStringStream& tokenStream, const String& fieldName, String& target);
	static bool ParseCountToken(IStringStream& lineStream, const String& fieldName, int& target);
	static bool ParseDateTimeToken(IStringStream& lineStream, const String& fieldName,
		std::tm& target, const String& format);

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

	static bool CommonNamesMatch(String a, String b);
	static String StripParentheses(String s);

	typedef std::array<std::vector<FrequencyInfo>, 12> FrequencyDataYear;
	typedef std::array<double, 12> DoubleYear;

	static bool ParseFrequencyFile(const String& fileName,
		FrequencyDataYear& frequencyData, DoubleYear& checklistCounts);
	static bool ParseFrequencyHeaderLine(const String& line, DoubleYear& checklistCounts);
	static bool ParseFrequencyLine(const String& line, FrequencyDataYear& frequencyData);
	void EliminateObservedSpecies(FrequencyDataYear& frequencyData) const;
	std::vector<FrequencyInfo> GenerateYearlyFrequencyData(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	static void GuessChecklistCounts(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	void RecommendHotspots(const std::set<String>& consolidatedSpeciesList,
		const String& country, const String& state, const String& county, const unsigned int& recentPeriod,
		const String& hotspotInfoFileName, const String& homeLocation,
		const String& mapApiKey, const String& eBirdApiKey) const;
	void GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<String>, EBirdInterface::LocationInfo>>& hotspots,
		const String& hotspotInfoFileName, const String& homeLocation, const String& mapApiKey,
		const String& regionCode, const String& eBirdApiKey) const;

	struct HotspotInfoComparer
	{
		bool operator()(const EBirdInterface::LocationInfo& a, const EBirdInterface::LocationInfo& b) const;
	};

	bool ComputeNewSpeciesProbability(const String& fileName,
		std::array<double, 12>& probabilities, std::array<std::vector<FrequencyInfo>, 12>& species) const;

	static bool WriteBestLocationsViewerPage(const String& htmlFileName,
		const String& googleMapsKey, const String& eBirdAPIKey,
		const std::vector<YearFrequencyInfo>& observationProbabilities,
		const String& clientId, const String& clientSecret);

	static String StripExtension(const String& fileName);
	class FileReadAndCalculateJob : public ThreadPool::JobInfoBase
	{
	public:
		FileReadAndCalculateJob(YearFrequencyInfo& frequencyInfo, const String& path, const String& fileName,
			const EBirdDataProcessor& ebdp) : frequencyInfo(frequencyInfo), path(path), fileName(fileName), ebdp(ebdp) {}

		YearFrequencyInfo& frequencyInfo;
		const String path;
		const String fileName;
		const EBirdDataProcessor& ebdp;

		void DoJob() override
		{
			frequencyInfo.locationCode = StripExtension(fileName);
			ebdp.ComputeNewSpeciesProbability(path + fileName, frequencyInfo.probabilities, frequencyInfo.frequencyInfo);
		}
	};

	class FileReadJob : public ThreadPool::JobInfoBase
	{
	public:
		FileReadJob(YearFrequencyInfo& frequencyInfo, const String& path, const String& fileName)
			: frequencyInfo(frequencyInfo), path(path), fileName(fileName) {}

		YearFrequencyInfo& frequencyInfo;
		const String path;
		const String fileName;

		void DoJob() override
		{
			frequencyInfo.locationCode = StripExtension(fileName);
			ParseFrequencyFile(path + fileName, frequencyInfo.frequencyInfo, frequencyInfo.probabilities);
		}
	};

	static std::vector<String> ListFilesInDirectory(const String& directory);
};

template<typename T>
bool EBirdDataProcessor::ParseToken(IStringStream& lineStream, const String& fieldName, T& target)
{
	String token;
	IStringStream tokenStream;

	if (lineStream.peek() == static_cast<int>('"'))// target string may contain commas
	{
		lineStream.ignore();// Skip the first quote

		do// In a loop, so we can properly handle escaped double quotes.
		{
			String tempToken;
			const Char quote('"');
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
	else if (!std::getline(lineStream, token, Char(',')))
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
bool EBirdDataProcessor::InterpretToken(IStringStream& tokenStream, const String& fieldName, T& target)
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
