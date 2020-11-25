// File:  sunCalculator.cpp
// Date:  11/25/2020
// Auth:  K. Loux
// Desc:  Class for calculating sunrise/sunset times based on location and time of year.
//        Adapted from:  https://www.esrl.noaa.gov/gmd/grad/solcalc/main.js

#ifndef SUN_CALCULATOR_H_
#define SUN_CALCULATOR_H_

class SunCalculator
{
public:
	enum class Timezone
	{
		a// TODO:  Remove?
	};
	
	struct Date
	{
		unsigned short month;// [1 - 12]
		unsigned short dayOfMonth;// [1 - 31]
		unsigned short year;// 4-digit year
	};
	
	bool GetSunriseSunset(const double& latitude, const double& longitude,
		const Date& date, const Timezone& timezone, double& sunriseMinutes, double& sunsetMinutes) const;
	
private:
	static unsigned short GetJulianDay(Date date);
	static double GetRefraction(const double& elevation);
	static double GetHourAngleSunrise(const double& latitude, const double& solarDeclination);
	static double CalcEquationOfTime(const double& time);
	static double GetSunDeclination(const double& time);
	static double GetGeomMeanLongSun(const double& time);
	static double GetGeomMeanAnomalySun(const double& time);
	static double GetEccentricityEarthOrbit(const double& time);
	static double GetSunEqOfCenter(const double& time);
	static double GetSunTrueLong(const double& time);
	static double GetSunTrueAnomaly(const double& time);
	static double GetSunRadVector(const double& time);
	static double GetSunApparentLong(const double& time);
	static double GetMeanObliquityOfEcliptic(const double& time);
	static double GetObliquityCorrection(const double& time);
	static double GetSunRtAscension(const double& time);
	static Date GetDateFromJD(const double& julianDay);
	static double GetTimeJulianCent(const double& julianDay);
	static bool IsLeapYear(const unsigned short& year);
	static double GetDayOfYearFromJD(const double& julianDay);
	
	static void GetSunriseSunsetUTC(const unsigned short& julianDay, const double& latitude, const double& longitude, double& sunrise, double& sunset);
};

#endif// SUN_CALCULATOR_H_
