// File:  bestObservationTimeEstimator.cpp
// Date:  12/4/2017
// Auth:  K. Loux
// Desc:  Object for estimating best observation times based on observation history.

// Local headers
#include "bestObservationTimeEstimator.h"
#include "kernelDensityEstimation.h"

// Standard C++ headers
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>

std::string BestObservationTimeEstimator::EstimateBestObservationTime(
	const std::vector<EBirdInterface::ObservationInfo>& observationInfo)
{
	// For ease of processing, convert times to a double representation of time as fractional hours since midnight
	std::vector<double> inputTimes(observationInfo.size());
	auto inputIt(inputTimes.begin());
	for (const auto& o : observationInfo)
		*(inputIt++) = o.observationDate.tm_hour + o.observationDate.tm_min / 60.0;

	// Estimate the probability distribution by binning into 1-hour wide slots
	const double minimum(0.0);// hours since midnight
	const double maximum(23.0);// hours since midnight
	const unsigned int pdfPointCount(24);
	const double step(1.0);// hours
	std::vector<double> pdfRange(pdfPointCount);
	std::generate(pdfRange.begin(), pdfRange.end(), Sequence(minimum, step));

	KernelDensityEstimation kde;
	std::vector<double> pdfEstimate(kde.ComputePDF(inputTimes, pdfRange,
		KernelDensityEstimation::EstimateOptimalBandwidth(inputTimes)));

	// Examine PDF to give insight into observation times.  There can be (multiple) obvious peaks
	// where we should list out certain hours, or PDF can be flat over several hours, in which
	// case we should report a range.
	if (HasFlatPDF(pdfEstimate))
		return "throughout the day and night";

	const bool isNocturnal(IsNocturnal(pdfEstimate));
	if (isNocturnal)
		std::rotate(pdfEstimate.begin(), pdfEstimate.begin() + pdfPointCount / 2, pdfEstimate.end());

	auto peaks(FindPeaks(pdfEstimate, inputTimes));
	if (peaks.size() == 0)
	{
		const double maxProbability(*std::max_element(pdfEstimate.begin(), pdfEstimate.end()));
		// TODO:  Find start and stop times (adjust if nocturnal)
		return "between ";
	}

	std::ostringstream ss;
	ss << "around ";
	ss << std::setfill('0');
	int shift([isNocturnal]()
	{
		if (isNocturnal)
			return 12;
		return 0;
	}());
	bool first(true);
	for (const auto& t : peaks)
	{
		if (!first)
			ss << ", ";
		ss << std::setw(2) << static_cast<int>(t + shift) % 24 << ':' << std::setw(2) << static_cast<int>(fmod(t, 60.0));
		first = false;
	}

	return ss.str();
}

bool BestObservationTimeEstimator::IsNocturnal(const std::vector<double>& pdf)
{
	// Assume nocturnal if the middle 50% of the day has less liklihood of
	// observation than the first and last quarter combined
	double nighttimeProbability(0.0);
	double daytimeProbability(0.0);

	double hour(0.0);
	const double increment(24.0 / pdf.size());
	for (const auto& p : pdf)
	{
		if (hour < 6.0 || hour > 18.0)
			nighttimeProbability += p;
		else
			daytimeProbability += p;
		hour += increment;
	}

	return nighttimeProbability > daytimeProbability;
}

bool BestObservationTimeEstimator::HasFlatPDF(const std::vector<double>& pdf)
{
	const double uniformProbability(1.0 / pdf.size());
	const double allowedVariation(0.2);// [% of uniform]
	const unsigned int allowedOutliers(3);
	unsigned int outliers(0);
	for (const auto& p : pdf)
	{
		if (p < uniformProbability * (1.0 - allowedVariation) ||
			p > uniformProbability * (1.0 + allowedVariation))
		{
			++outliers;
			if (outliers > allowedOutliers)
				return false;
		}
	}

	return true;
}

std::vector<double> BestObservationTimeEstimator::FindPeaks(const std::vector<double>& pdf, const std::vector<double>& input)
{
	const double maxProbability(*std::max_element(pdf.begin(), pdf.end()));
	// Split the data at each valley - but because the valleys can be wide and flat,
	// we'll use half-way between adjacent peaks as the valley positions.
	unsigned int lastPeak(pdf.size()), newPeak;
	const double minimum(*std::min_element(input.begin(), input.end()));
	double classMinimum(minimum - 1.0);
	double classMaximum;
	std::vector<double> bins;
	unsigned int i;
	for (i = 0; i < pdf.size(); i++)
	{
		// Check to see if we're at a peak
		newPeak = 0;
		if (i == 0)
		{
			if (pdf[0] > pdf[1])
				lastPeak = 0;
		}
		else if (i == pdf.size() - 1)
		{
			if (pdf[i] > pdf[i - 1])
				newPeak = i;
		}
		else if ((pdf[i] > pdf[i - 1] && pdf[i] >= pdf[i + 1]))
			newPeak = i;

		if (newPeak > 0 && lastPeak < pdf.size())// Found a new peak that we can process
		{
			classMaximum = minimum + (newPeak + lastPeak) / 2 * (24.0 / pdf.size());

			std::vector<double> temp;
			for (const auto& t : input)
			{
				if (t <= classMaximum && t > classMinimum)
					temp.push_back(t);
			}

			// If the PDF isn't quite smooth enough, we may get some bogus bins
			// that don't contain any values.  We want to ignore these.
			if (temp.size() > 0)
				bins.push_back(KernelDensityEstimation::ComputeMean(temp));

			lastPeak = newPeak;
			classMinimum = classMaximum;
		}
		else if (newPeak > 0)// Found the first peak - store it, but don't do any processing yet
			lastPeak = newPeak;
		else if (i == pdf.size() - 1)// Reached the end of the range, and the last point is not a peak
		{
			std::vector<double> temp;
			for (const auto& t : input)
			{
				if (t > classMinimum)
					temp.push_back(t);
			}

			// If the PDF isn't quite smooth enough, we may get some bogus bins
			// that don't contain any values.  We want to ignore these.
			if (temp.size() > 0)
				bins.push_back(KernelDensityEstimation::ComputeMean(temp));
		}
	}

	return bins;
}
