// File:  bestObservationTimeEstimator.cpp
// Date:  12/4/2017
// Auth:  K. Loux
// Desc:  Object for estimating best observation times based on observation history.

// Local headers
#include "bestObservationTimeEstimator.h"

// Standard C++ headers
#include <cassert>

std::vector<std::tm> BestObservationTimeEstimator::EstimateBestObservationTime(
	const std::vector<EBirdInterface::ObservationInfo>& observationInfo)
{
	std::vector<std::tm> estimates;

	// TODO:  Looks as though eBird currently only supports retrieval of the latest sighting for a species, so determining the "best" times from this data is not helpful
	assert(observationInfo.size() == 1);
	estimates.push_back(observationInfo.front().observationDate);

	return estimates;
}
