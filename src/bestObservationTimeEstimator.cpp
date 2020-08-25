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

UString::String BestObservationTimeEstimator::EstimateBestObservationTime(
	const std::vector<EBirdInterface::ObservationInfo>& observationInfo)
{
	auto pdfEstimate(EstimateBestObservationTimePDF(observationInfo));
	if (observationInfo.size() < 3)
	{
		UString::OStringStream ss;
		ss << std::setfill(UString::Char('0'));
		unsigned int i(0);
		for (const auto& o : pdfEstimate)
		{
			++i;
			if (o == 0.0)
				continue;

			if (ss.str().empty())
				ss << "at ";
			else
				ss << " and ";

			const double increment(24.0 / pdfEstimate.size());
			const double hours((i - 1) * increment);
			ss << std::setw(2) << static_cast<int>(hours) % 24 << ':'
				<< std::setw(2) << static_cast<int>((hours - static_cast<int>(hours)) * 60.0);
		}

		return ss.str();
	}

	// Examine PDF to give insight into observation times.  There can be (multiple) obvious peaks
	// where we should list out certain hours, or PDF can be flat over several hours, in which
	// case we should report a range.
	if (HasFlatPDF(pdfEstimate))
		return _T("throughout the day and night");

	const bool isNocturnal(IsNocturnal(pdfEstimate));
	unsigned int shift(0);
	if (isNocturnal)
	{
		std::rotate(pdfEstimate.begin(), pdfEstimate.begin() + pdfEstimate.size() / 2, pdfEstimate.end());
		shift = 12;
	}

	auto peaks(FindPeaks(pdfEstimate));
	assert(peaks.size() > 0);

	UString::OStringStream ss;
	ss << std::setfill(UString::Char('0'));
	bool lastWasRangeStart(false);
	const auto lastPeakPtr([&peaks]() -> TimeProbability*
	{
		if (peaks.back().type == TimeProbability::Type::Peak)
			return &*peaks.rbegin();
		return &*(peaks.rbegin() + 1);
	}());

	for (const auto& peak : peaks)
	{
		const bool isLast(&peak == lastPeakPtr);
		if (!ss.str().empty() && !lastWasRangeStart)
		{
			if (!isLast)
				ss << ", ";
			else
				ss << " and ";
		}

		if (peak.type == TimeProbability::Type::Peak)
		{
			assert(!lastWasRangeStart);
			ss << "around ";
		}
		else if (peak.type == TimeProbability::Type::RangeStart)
		{
			assert(!lastWasRangeStart);
			ss << "from ";
		}
		else// if (peak.type == TimeProbability::Type::RangeEnd)
		{
			assert(lastWasRangeStart);
			ss << " to ";
		}

		ss << std::setw(2) << static_cast<int>(peak.time + shift) % 24 << ':'
			<< std::setw(2) << static_cast<int>((peak.time - static_cast<int>(peak.time)) * 60.0);

		lastWasRangeStart = peak.type == TimeProbability::Type::RangeStart;
	}

	return ss.str();
}

BestObservationTimeEstimator::PDFArray BestObservationTimeEstimator::EstimateBestObservationTimePDF(
	const std::vector<EBirdInterface::ObservationInfo>& observationInfo)
{
	if (observationInfo.size() < 3)
	{
		auto obsInfoSortable(observationInfo);
		std::sort(obsInfoSortable.begin(), obsInfoSortable.end(),
			[](const EBirdInterface::ObservationInfo& a, const EBirdInterface::ObservationInfo& b)
		{
			if (a.observationDate.tm_hour < b.observationDate.tm_hour)
				return true;
			else if (a.observationDate.tm_hour > b.observationDate.tm_hour)
				return false;

			return a.observationDate.tm_min < b.observationDate.tm_min;
		});

		PDFArray exactTimes;
		std::for_each(exactTimes.begin(), exactTimes.end(), [](double& a) { a = 0.0; });
		// TODO:  Modify this to make use of duration (if available) and to disregard observations that have no time data
		for (const auto& o : obsInfoSortable)
		{
			const double increment(24.0 / exactTimes.size());// hours
			exactTimes[std::min(static_cast<int>((o.observationDate.tm_hour + o.observationDate.tm_min / 60.0) / increment + 0.5),
				static_cast<int>(exactTimes.size()) - 1)] += 1.0 / observationInfo.size();
		}
		return exactTimes;
	}

	// For ease of processing, convert times to a double representation of time as fractional hours since midnight
	std::vector<std::pair<double, double>> inputTimes;// <time (hr), weight>
	for (const auto& o : observationInfo)
	{
		if (!o.dateIncludesTimeInfo)
			continue;

		const unsigned int increment(15);// [min]
		const double startTime(floor((o.observationDate.tm_hour * 60.0 + o.observationDate.tm_min + increment / 2.0) / increment) * increment / 60.0);// Rounded to nearest 15 minute increment
		if (o.duration > 0)
		{
			const unsigned int incrementsSpanned(o.duration / increment);
			for (unsigned int i = 0; i < incrementsSpanned; ++i)
				inputTimes.push_back(std::make_pair(startTime + i * increment / 60.0, 1.0 / incrementsSpanned));
		}
		else
			inputTimes.push_back(std::make_pair(startTime, 1.0));
	}

	// Estimate the probability distribution by binning into x-hour wide slots
	const double minimum(0.0);// hours since midnight
	PDFArray pdfArray;
	const double step(24.0 / pdfArray.size());// hours
	std::vector<double> pdfRange(pdfArray.size());
	std::generate(pdfRange.begin(), pdfRange.end(), Sequence(minimum, step));

	KernelDensityEstimation kde;
	std::vector<double> pdfEstimate(kde.ComputePDF(inputTimes, pdfRange,
		std::max(1.0, KernelDensityEstimation::EstimateOptimalBandwidth(inputTimes))));

	assert(pdfEstimate.size() == pdfArray.size());
	std::copy(pdfEstimate.begin(), pdfEstimate.end(), pdfArray.begin());

	return pdfArray;
}

bool BestObservationTimeEstimator::IsNocturnal(const PDFArray& pdf)
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

bool BestObservationTimeEstimator::HasFlatPDF(const PDFArray& pdf)
{
	const double uniformProbability(1.0 / pdf.size());
	const double allowedVariation(0.5);// [% of uniform]
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

std::vector<BestObservationTimeEstimator::TimeProbability> BestObservationTimeEstimator::FindPeaks(
	const PDFArray& pdf)
{
	// If there is a time where observation is twice as likely as another time, this 
	// is significant.  Less variation than that is not significant.
	const double significanceRatio(0.5);
	const double minimumOnProbability(*std::max_element(pdf.begin(), pdf.end()) * significanceRatio);

	std::vector<TimeProbability> peaks;
	auto pdfIt(pdf.begin());
	for (; pdfIt != pdf.end(); ++pdfIt)
	{
		if (*pdfIt < minimumOnProbability)
			continue;

		auto nextIt(pdfIt + 1);
		if (peaks.size() > 0 &&
			peaks.back().type == TimeProbability::Type::RangeStart &&
			*nextIt >= minimumOnProbability)// If this is in the middle of a wider range
			continue;

		TimeProbability::Type type;
		if (nextIt != pdf.end() && *nextIt > minimumOnProbability)
			type = TimeProbability::Type::RangeStart;
		else if (peaks.size() > 0 && peaks.back().type == TimeProbability::Type::RangeStart)
			type = TimeProbability::Type::RangeEnd;
		else
			type = TimeProbability::Type::Peak;

		const double step(24.0 / pdf.size());// hours; need to scale distance according to granularity of pdf
		peaks.push_back(TimeProbability(static_cast<double>(std::distance(pdf.begin(), pdfIt)) * step, type));
	}

	assert(peaks.back().type != TimeProbability::Type::RangeStart);

	return peaks;
}
