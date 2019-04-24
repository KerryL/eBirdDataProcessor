// File:  kernelDensityEstimation.h
// Date:  8/9/2016
// Auth:  K. Loux
// Desc:  Class for performing kernel density estimation.

#ifndef KERNEL_DENSITY_ESTIMATION_H_
#define KERNEL_DENSITY_ESTIMATION_H_

// Standard C++ headers
#include <vector>
#include <cmath>
#include <algorithm>

class KernelDensityEstimation
{
public:
	KernelDensityEstimation();

	enum class KernelType
	{
		KernelUniform,
		KernelTriangular,
		KernelEpanechnikov,
		KernelQuartic,
		KernelTriweight,
		KernelTricube,
		KernelGaussian,
		KernelCosine,
		KernelLogistic,
		KernelSigmoid,
		KernelSilverman
	};

	void SetKernelType(const KernelType& type);

	// For unweighted samples
	std::vector<double> ComputePDF(const std::vector<double>& values,
		const std::vector<double>& pdfRange,
		const double& bandwidth) const;
	static double EstimateOptimalBandwidth(const std::vector<double>& v);

	// For weighted samples
	std::vector<double> ComputePDF(const std::vector<std::pair<double, double>>& values,
		const std::vector<double>& pdfRange,
		const double& bandwidth) const;
	static double EstimateOptimalBandwidth(const std::vector<std::pair<double, double>>& v);

	template <typename T>
	static double ComputeStandardDeviation(const std::vector<T>& v);
	template <typename Iter>
	static double ComputeStandardDeviation(Iter start, Iter end);
	template <typename T>
	static double ComputeMean(const std::vector<T>& v);
	template <typename Iter>
	static double ComputeMean(Iter start, Iter end);

private:
	typedef double (*KernelFunction)(const double& v);
	KernelFunction kernel;

	// Kernel functions
	static double UniformKernel(const double& v);
	static double TriangularKernel(const double& v);
	static double EpanechnikovKernel(const double& v);
	static double QuarticKernel(const double& v);
	static double TriweightKernel(const double& v);
	static double TricubeKernel(const double& v);
	static double GaussianKernel(const double& v);
	static double CosineKernel(const double& v);
	static double LogisticKernel(const double& v);
	static double SigmoidKernel(const double& v);
	static double SilvermanKernel(const double& v);
};

// Standard C++ headers
#include <numeric>

//==========================================================================
// Namespace:		Utilities
// Function:		ComputeStandardDeviation
//
// Description:		Computes the standard deviation of the specified vector.
//
// Input Arguments:
//		v	= const std::vector<T>&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
template <typename T>
double KernelDensityEstimation::ComputeStandardDeviation(const std::vector<T>& v)
{
	return ComputeStandardDeviation(v.begin(), v.end());
}

//==========================================================================
// Namespace:		Utilities
// Function:		ComputeStandardDeviation
//
// Description:		Computes the standard deviation of the specified vector.
//
// Input Arguments:
//		start	= Iter
//		end		= Iter
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
template <typename Iter>
double KernelDensityEstimation::ComputeStandardDeviation(Iter start, Iter end)
{
	const double mean(ComputeMean(start, end));
	std::vector<double> diff(std::distance(start, end));
	std::transform(start, end, diff.begin(), [mean](double x) { return x - mean; });
	const double sq_sum(std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0));
	return std::sqrt(sq_sum / diff.size());
}

//==========================================================================
// Namespace:		Utilities
// Function:		ComputeMean
//
// Description:		Computes the mean of the specified vector.
//
// Input Arguments:
//		v	= const std::vector<T>&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
template <typename T>
double KernelDensityEstimation::ComputeMean(const std::vector<T>& v)
{
	return ComputeMean(v.begin(), v.end());
}

//==========================================================================
// Namespace:		Utilities
// Function:		ComputeMean
//
// Description:		Computes the mean of the specified vector.
//
// Input Arguments:
//		start	= Iter
//		end		= Iter
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
template <typename Iter>
double KernelDensityEstimation::ComputeMean(Iter start, Iter end)
{
	return std::accumulate(start, end, static_cast<typename std::iterator_traits<Iter>::value_type>(0))
		/ static_cast<double>(std::distance(start, end));
}

#endif// KERNEL_DENSITY_ESTIMATION_H_
