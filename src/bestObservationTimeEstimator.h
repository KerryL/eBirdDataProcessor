// File:  bestObservationTimeEstimator.h
// Date:  12/4/2017
// Auth:  K. Loux
// Desc:  Object for estimating best observation times based on observation history.

#ifndef BEST_OBSERVATION_TIME_ESTIMATOR_H_
#define BEST_OBSERVATION_TIME_ESTIMATOR_H_

// Local headers
#include "eBirdInterface.h"

class BestObservationTimeEstimator
{
public:
	static std::vector<std::tm> EstimateBestObservationTime(const std::vector<EBirdInterface::ObservationInfo>& observationInfo);

private:
};

#endif// BEST_OBSERVATION_TIME_ESTIMATOR_H_
