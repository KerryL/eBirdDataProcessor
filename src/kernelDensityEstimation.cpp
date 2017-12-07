// File:  kernelDensityEstimation.cpp
// Date:  8/9/2016
// Auth:  K. Loux
// Desc:  Class for performing kernel density estimation.

// Standard C++ headers
#include <algorithm>
#include <cmath>

// Local headers
#include "kernelDensityEstimation.h"

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		KernelDensityEstimation
//
// Description:		Constructor for KernelDensityEstimation class.
//
// Input Arguments:
//		None
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
KernelDensityEstimation::KernelDensityEstimation()
{
	// Assign defaults
	SetKernelType(KernelEpanechnikov);
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		SetKernelType
//
// Description:		Assigns the specified kernel function.
//
// Input Arguments:
//		type	= const KernelTyep&
//
// Output Arguments:
//		None
//
// Return Value:
//		None
//
//==========================================================================
void KernelDensityEstimation::SetKernelType(const KernelType& type)
{
	switch (type)
	{
	case KernelUniform:
		kernel = UniformKernel;
		break;

	case KernelTriangular:
		kernel = TriangularKernel;
		break;

	default:
	case KernelEpanechnikov:
		kernel = EpanechnikovKernel;
		break;

	case KernelQuartic:
		kernel = QuarticKernel;
		break;

	case KernelTriweight:
		kernel = TriweightKernel;
		break;

	case KernelTricube:
		kernel = TricubeKernel;
		break;

	case KernelGaussian:
		kernel = GaussianKernel;
		break;

	case KernelCosine:
		kernel = CosineKernel;
		break;

	case KernelLogistic:
		kernel = LogisticKernel;
		break;

	case KernelSigmoid:
		kernel = SigmoidKernel;
		break;

	case KernelSilverman:
		kernel = SilvermanKernel;
	}
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		EstimateOptimalBandwidth
//
// Description:		Uses "rule of thumb" to estimate an optimal bandwidth.
//
// Input Arguments:
//		values		= std::vector<double>&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::vector<double>
//
//==========================================================================
double KernelDensityEstimation::EstimateOptimalBandwidth(
	const std::vector<double>& values)
{
	const double standardDeviation(ComputeStandardDeviation(values));
	if (standardDeviation == 0.0)
		return 0.0;

	return pow(4.0 / 3.0 * pow(standardDeviation, 5.0)
		/ static_cast<double>(values.size()), 0.2);
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		GetUniqueValues
//
// Description:		Uses Kernel Density Estimation to find the significantly
//					unique values in the specified vector.
//
// Input Arguments:
//		values		= std::vector<double>&
//		pdfRange	= std::vector<double>&, values at which PDF should be computed
//		bandwidth	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		std::vector<double>
//
//==========================================================================
std::vector<double> KernelDensityEstimation::ComputePDF(
	const std::vector<double>& values, const std::vector<double>& pdfRange,
	const double& bandwidth) const
{
	// Estimate the probability distribution over the range of input data
	std::vector<double> pdfEstimate(pdfRange.size());
	const double factor(1.0 / static_cast<double>(values.size()) / bandwidth);
	unsigned int i, j;
	for (i = 0; i < pdfEstimate.size(); i++)
	{
		double kernelSum(0.0);
		for (j = 0; j < values.size(); j++)
			kernelSum += kernel((pdfRange[i] - values[j]) / bandwidth);
		pdfEstimate[i] = factor * kernelSum;
	}

	return pdfEstimate;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		UniformKernel
//
// Description:		Uniform kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::UniformKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 0.5;
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		TriangularKernel
//
// Description:		Triangular kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::TriangularKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return (1.0 - fabs(v));
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		EpanechnikovKernel
//
// Description:		Epanechnikov kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::EpanechnikovKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 0.75 * (1.0 - v * v);
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		QuarticKernel
//
// Description:		Quartic (biweight) kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::QuarticKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 0.9375 * (1.0 - v * v) * (1.0 - v * v);
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		TriweightKernel
//
// Description:		Triweight kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::TriweightKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 1.09375 * pow(1.0 - v * v, 3);
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		TricubeKernel
//
// Description:		Tricube kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::TricubeKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 70.0 / 81.0 * pow(1.0 - fabs(v * v * v), 3);
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		GaussianKernel
//
// Description:		Gaussian kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::GaussianKernel(const double& v)
{
	return 1.0 / std::sqrt(2.0 * M_PI) * exp(-0.5 * v * v);
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		CosineKernel
//
// Description:		Cosine kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::CosineKernel(const double& v)
{
	if (fabs(v) < 1.0)
		return 0.25 * M_PI * cos(0.5 * M_PI * v);
	return 0.0;
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		LogisticKernel
//
// Description:		Logistic kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::LogisticKernel(const double& v)
{
	return 1.0 / (exp(v) + 2.0 + exp(-v));
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		SigmoidKernel
//
// Description:		Sigmoid kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::SigmoidKernel(const double& v)
{
	return 2.0 / M_PI / (exp(v) - exp(-v));
}

//==========================================================================
// Class:			KernelDensityEstimation
// Function:		SilvermanKernel
//
// Description:		Silverman kernel function.
//
// Input Arguments:
//		v	= const double&
//
// Output Arguments:
//		None
//
// Return Value:
//		double
//
//==========================================================================
double KernelDensityEstimation::SilvermanKernel(const double& v)
{
	return 0.5 * exp(-fabs(v) / std::sqrt(2.0))
		* sin(fabs(v) / std::sqrt(2.0) + 0.25 * M_PI);
}
