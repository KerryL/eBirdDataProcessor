// File:  eBirdInterface.h
// Date:  8/22/2017
// Auth:  K. Loux
// Desc:  Interface to eBird web API.

#ifndef EBIRD_INTERFACE_H_

// Local headers
#include "jsonInterface.h"

// Standard C++ headers
#include <string>
#include <vector>

class EBirdInterface : public JSONInterface
{
public:
	std::vector<std::string> GetHotspotsWithRecentObservationsOf(
		const std::string& scientificName, const std::string& region);

	std::string GetScientificNameFromCommonName(const std::string& commonName);

private:
	static const std::string apiRoot;
	static const std::string recentObservationsOfSpeciesInRegionURL;
	static const std::string taxonomyLookupURL;

	static const std::string commonNameTag;
	static const std::string scientificNameTag;
};

#endif// EBIRD_INTERFACE_H_
