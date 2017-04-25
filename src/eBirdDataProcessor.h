// File:  eBirdDataProcessor.h
// Date:  4/20/2017
// Auth:  K. Loux
// Desc:  Main class for eBird Data Processor.

#ifndef EBIRD_DATA_PROCESSOR_H_

// Standard C++ headers
#include <string>
#include <vector>
#include <ctime>
#include <sstream>

class EBirdDataProcessor
{
public:
	bool Parse(const std::string& dataFile);

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

	enum class SortBy : int
	{
		None = 0,
		Date,
		CommonName,
		ScientificName,
		TaxonomicOrder
	};

	void SortData(const SortBy& primarySort, const SortBy& secondarySort);

	enum class ListType : int
	{
		Life = 0,
		Year,
		Month,
		Week,
		Day,
		SeparateAllObservations
	};

	std::string GenerateList(const ListType& type) const;

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
	};

	std::vector<Entry> data;

	std::vector<Entry> ConsolidateByLife() const;
	std::vector<Entry> ConsolidateByYear() const;
	std::vector<Entry> ConsolidateByMonth() const;
	std::vector<Entry> ConsolidateByWeek() const;
	std::vector<Entry> ConsolidateByDay() const;

	static bool ParseLine(const std::string& line, Entry& entry);

	static int DoComparison(const Entry& a, const Entry& b, const SortBy& sortBy);

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

	template<typename BinaryPredicate>
	static std::vector<Entry>::iterator UnsortedUnique(std::vector<Entry>::iterator first,
		std::vector<Entry>::iterator last, BinaryPredicate p);
	static bool CommonNamesMatch(std::string a, std::string b);
	static std::string StripParentheses(std::string s);
	static std::string Trim(std::string s);

	static const std::string commaPlaceholder;
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

// Uniqueness if achieved by:
// 1.  Moving unique elements to the front of the vector
// 2.  Returning an iterator pointing to the new end of the vector
// So elements occurring between (and including) the returned iterator and the
// end of the original vector can safely be erased.
template<typename BinaryPredicate>
std::vector<EBirdDataProcessor::Entry>::iterator EBirdDataProcessor::UnsortedUnique(
	std::vector<Entry>::iterator first, std::vector<Entry>::iterator last, BinaryPredicate p)
{
	std::vector<Entry>::iterator newLast(last);
	while (first != newLast)
	{
		std::vector<Entry>::iterator next(std::next(first));
		while (next != newLast)
		{
			if (p(*first, *next))
			{
				--newLast;
				std::rotate(next, next + 1, last);
			}
			else
				++next;
		}
		++first;
	}

	return newLast;
}

#endif// EBIRD_DATA_PROCESSOR_H_