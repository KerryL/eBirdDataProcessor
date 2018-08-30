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
	static UString::String EstimateBestObservationTime(const std::vector<EBirdInterface::ObservationInfo>& observationInfo);

private:
	struct Sequence
	{
	public:
		Sequence(const double& minimum, const double& step)
			: minimum(minimum), step(step) { i = 0; }

		double operator()() { return minimum + ++i * step; }

	private:
		const double minimum;
		const double step;
		unsigned int i;
	};

	static bool IsNocturnal(const std::vector<double>& pdf);
	static bool HasFlatPDF(const std::vector<double>& pdf);

	struct TimeProbability
	{
		double time;

		enum Type
		{
			Peak,
			RangeStart,
			RangeEnd
		};

		Type type;

		TimeProbability(const double &time, const Type& type) : time(time), type(type) {}
	};

	static std::vector<TimeProbability> FindPeaks(const std::vector<double>& pdf);
};

#endif// BEST_OBSERVATION_TIME_ESTIMATOR_H_
