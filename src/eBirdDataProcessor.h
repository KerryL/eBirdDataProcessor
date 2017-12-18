// File:  eBirdDataProcessor.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_H_
#define EBIRD_DATA_PROCESSOR_H_

// Local headers
#include "eBirdInterface.h"
#include "ebdpConfig.h"

// Standard C++ headers
#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <array>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>
#include <cassert>
#include <numeric>

class EBirdDataProcessor
{
public:
	bool Parse(const std::string& dataFile);
	bool ReadPhotoList(const std::string& photoFileName);

	void FilterLocation(const std::string& location, const std::string& county,
		const std::string& state, const std::string& country);
	void FilterCounty(const std::string& county, const std::string& state,
		const std::string& country);
	void FilterState(const std::string& state, const std::string& country);
	void FilterCountry(const std::string& country);

	void FilterYear(const unsigned int& year);
	void FilterMonth(const unsigned int& month);
	void FilterWeek(const unsigned int& week);
	void FilterDay(const unsigned int& day);

	void FilterPartialIDs();

	void SortData(const EBDPConfig::SortBy& primarySort, const EBDPConfig::SortBy& secondarySort);

	void GenerateUniqueObservationsReport(const EBDPConfig::UniquenessType& type);
	void GenerateRarityScores(const std::string& frequencyFileName, const EBDPConfig::ListType& listType);
	std::string GenerateList(const EBDPConfig::ListType& type, const bool& withoutPhotosOnly) const;

	bool GenerateTargetCalendar(const unsigned int& topBirdCount,
		const std::string& outputFileName, const std::string& frequencyFileName,
		const std::string& country, const std::string& state, const std::string& county,
		const unsigned int& recentPeriod,
		const std::string& hotspotInfoFileName, const std::string& homeLocation,
		const std::string& mapApiKey) const;

private:
	static const std::string headerLine;

	struct Entry
	{
		std::string submissionID;
		std::string commonName;
		std::string scientificName;
		int taxonomicOrder;
		int count;
		std::string stateProvidence;
		std::string county;
		std::string location;
		double latitude;// [deg]
		double longitude;// [deg]
		std::tm dateTime;
		std::string protocol;
		int duration;// [min]
		bool allObsReported;
		double distanceTraveled;// [km]
		double areaCovered;// [ha]
		int numberOfObservers;
		std::string breedingCode;
		std::string speciesComments;
		std::string checklistComments;

		bool hasPhoto = false;
	};

	std::vector<Entry> data;

	std::vector<Entry> ConsolidateByLife() const;
	std::vector<Entry> ConsolidateByYear() const;
	std::vector<Entry> ConsolidateByMonth() const;
	std::vector<Entry> ConsolidateByWeek() const;
	std::vector<Entry> ConsolidateByDay() const;

	std::vector<Entry> DoConsolidation(const EBDPConfig::ListType& type) const;

	static bool ParseLine(const std::string& line, Entry& entry);

	static int DoComparison(const Entry& a, const Entry& b, const EBDPConfig::SortBy& sortBy);

	template<typename T>
	static bool ParseToken(std::istringstream& lineStream, const std::string& fieldName, T& target);
	template<typename T>
	static bool InterpretToken(std::istringstream& tokenStream, const std::string& fieldName, T& target);
	static bool InterpretToken(std::istringstream& tokenStream, const std::string& fieldName, std::string& target);
	static bool ParseCountToken(std::istringstream& lineStream, const std::string& fieldName, int& target);
	static bool ParseDateTimeToken(std::istringstream& lineStream, const std::string& fieldName,
		std::tm& target, const std::string& format);

	static std::string Sanitize(const std::string& line);
	static std::string Desanitize(const std::string& token);

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

	static bool CommonNamesMatch(std::string a, std::string b);
	static std::string StripParentheses(std::string s);
	static std::string Trim(std::string s);

	static const std::string commaPlaceholder;

	struct FrequencyInfo
	{
		std::string species;
		double frequency = 0.0;

		FrequencyInfo() = default;
		FrequencyInfo(const std::string& species, const double& frequency) : species(species), frequency(frequency) {}
	};
	typedef std::array<std::vector<FrequencyInfo>, 12> FrequencyDataYear;
	typedef std::array<double, 12> DoubleYear;

	static bool ParseFrequencyFile(const std::string& fileName,
		FrequencyDataYear& frequencyData, DoubleYear& checklistCounts);
	static bool ParseFrequencyHeaderLine(const std::string& line, DoubleYear& checklistCounts);
	static bool ParseFrequencyLine(const std::string& line, FrequencyDataYear& frequencyData);
	void EliminateObservedSpecies(FrequencyDataYear& frequencyData) const;
	std::vector<FrequencyInfo> GenerateYearlyFrequencyData(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	static void GuessChecklistCounts(const FrequencyDataYear& frequencyData, const DoubleYear& checklistCounts);

	void RecommendHotspots(const std::set<std::string>& consolidatedSpeciesList,
		const std::string& country, const std::string& state, const std::string& county, const unsigned int& recentPeriod,
		const std::string& hotspotInfoFileName, const std::string& homeLocation,
		const std::string& mapApiKey) const;
	void GenerateHotspotInfoFile(const std::vector<std::pair<std::vector<std::string>, EBirdInterface::HotspotInfo>>& hotspots,
		const std::string& hotspotInfoFileName, const std::string& homeLocation, const std::string& mapApiKey,
		const std::string& regionCode) const;

	struct HotspotInfoComparer
	{
		bool operator()(const EBirdInterface::HotspotInfo& a, const EBirdInterface::HotspotInfo& b) const;
	};
};

template<typename T>
bool EBirdDataProcessor::ParseToken(std::istringstream& lineStream, const std::string& fieldName, T& target)
{
	std::string token;
	std::istringstream tokenStream;

	if (!std::getline(lineStream, token, ','))
	{
		/*std::cerr << "Failed to read token for " << fieldName << '\n';
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

	tokenStream.str(Desanitize(token));
	return InterpretToken(tokenStream, fieldName, target);
}

template<typename T>
bool EBirdDataProcessor::InterpretToken(std::istringstream& tokenStream, const std::string& fieldName, T& target)
{
	if ((tokenStream >> target).fail())
	{
		std::cerr << "Failed to interpret token for " << fieldName << '\n';
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
