// File:  sunCalculator.cpp
// Date:  11/25/2020
// Auth:  K. Loux
// Desc:  Class for calculating sunrise/sunset times based on location and time of year.

#ifndef SUN_CALCULATOR_H_
#define SUN_CALCULATOR_H_

// Local headers
#include "email/jsonInterface.h"

class SunCalculator : public JSONInterface
{
public:
	SunCalculator() : JSONInterface("eBirdDataProcessor") {}
	
	struct Date
	{
		unsigned short month;// [1 - 12]
		unsigned short dayOfMonth;// [1 - 31]
		unsigned short year;// 4-digit year
	};
	
	// Returns sunrise and sunset time in minutes since midnight, local time
	bool GetSunriseSunset(const double& latitude, const double& longitude,
		const Date& date, double& sunriseMinutes, double& sunsetMinutes) const;
	
private:
	static const UString::String requestURLBase;
	static const UString::String userName;
	
	static UString::String BuildRequestURL(const double& latitude, const double& longitude, const Date& date);
};

#endif// SUN_CALCULATOR_H_
