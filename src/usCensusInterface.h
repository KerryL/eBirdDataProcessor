// File:  usCensusInterface.h
// Date:  1/3/2018
// Auth:  K. Loux
// Desc:  Interface to US Census Data API.  Very stripped down - designed only
//        for retrieving state and county FIPS codes.

#ifndef US_CENSUS_INTERFACE_H_
#define US_CENSUS_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"

// Standard C++ headers
#include <array>

class USCensusInterface : public JSONInterface
{
public:
	explicit USCensusInterface(const std::string& apiKey);

	struct FIPSNamePair
	{
		FIPSNamePair(const unsigned int& fipsCode, const std::string& name) : fipsCode(fipsCode), name(name) {}
		const unsigned int fipsCode;
		const std::string name;
	};

	std::vector<FIPSNamePair> GetStateCodes() const;
	std::vector<FIPSNamePair> GetCountyCodesInState(const unsigned int& stateCode) const;
	static bool GetStateFIPSCode(const std::string& state, unsigned int& fipsCode);

private:
	static const std::string apiRoot;
	static const std::string housing2016URL;

	struct StateFIPSEntry
	{
		StateFIPSEntry(const std::string& name, const unsigned int& fipsCode, const std::string abbreviation)
			: name(name), fipsCode(fipsCode), abbreviation(abbreviation) {}

		std::string name;
		unsigned int fipsCode;
		std::string abbreviation;
	};

	static const std::array<const StateFIPSEntry, 51> stateFIPSCodes;

	const std::string apiKey;

	std::vector<FIPSNamePair> DoRequest(const std::string& request) const;

	static std::string BuildRequestForStateFIPSCodes(const unsigned int& dateCode,
		const std::string& key);
	static std::string BuildRequestForCountyFIPSCodes(const unsigned int& stateCode,
		const unsigned int& dateCode, const std::string& key);

	static bool ParseResponse(const std::string& response, std::vector<FIPSNamePair>& codes);
};

#endif// US_CENSUS_INTERFACE_H_
