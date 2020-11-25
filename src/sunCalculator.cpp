// File:  sunCalculator.cpp
// Date:  11/25/2020
// Auth:  K. Loux
// Desc:  Class for calculating sunrise/sunset times based on location and time of year.
//        Adapted from:  https://www.esrl.noaa.gov/gmd/grad/solcalc/main.js

// Local headers
#include "sunCalculator.h"

// Standard C++ headers
#include <cmath>

double SunCalculator::GetTimeJulianCent(const double& julianDay)
{
	return (julianDay - 2451545.0) / 36525.0;
}

bool SunCalculator::IsLeapYear(const unsigned short& year)
{
	return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

SunCalculator::Date SunCalculator::GetDateFromJD(const double& julianDay)
{
	const unsigned int z(floor(julianDay + 0.5));
	const double f((julianDay + 0.5) - z);
	unsigned int A;
	if (z < 2299161)
		A = z;
	else
	{
		const double alpha(floor((z - 1867216.25) / 36524.25));
		A = z + 1 + alpha - floor(0.25 * alpha);
	}
	const unsigned int B(A + 1524);
	const unsigned int C(floor((B - 122.1)/365.25));
	const unsigned int D(floor(365.25 * C));
	const unsigned int E(floor((B - D)/30.6001));
	
	Date date;
	date.dayOfMonth = B - D - floor(30.6001 * E) + f;
	date.month = (E < 14) ? E - 1 : E - 13;
	date.year = (date.month > 2) ? C - 4716 : C - 4715;
	return date;
}

double SunCalculator::GetDayOfYearFromJD(const double& julianDay)
{
	const auto date(GetDateFromJD(julianDay));

	const unsigned short k(IsLeapYear(date.year) ? 1 : 2);
	return floor((275 * date.month) / 9) - k * floor((date.month + 9)/12) + date.dayOfMonth - 30;
}

double SunCalculator::GetGeomMeanLongSun(const double& time)
{
	double L0(280.46646 + time * (36000.76983 + time * 0.0003032));
	while(L0 > 360.0)
		L0 -= 360.0;
	while(L0 < 0.0)
		L0 += 360.0;
	return L0;// [deg]
}

double SunCalculator::GetGeomMeanAnomalySun(const double& time)
{
	return 357.52911 + time * (35999.05029 - 0.0001537 * time);// [deg]
}

double SunCalculator::GetEccentricityEarthOrbit(const double& time)
{
	return 0.016708634 - time * (0.000042037 + 0.0000001267 * time);// [-]
}

double SunCalculator::GetSunEqOfCenter(const double& time)
{
	const double mrad(GetGeomMeanAnomalySun(time) * M_PI / 180.0);
	const double sinm(sin(mrad));
	const double sin2m(sin(2 * mrad));
	const double sin3m(sin(3 * mrad));
	return sinm * (1.914602 - time * (0.004817 + 0.000014 * time))
		+ sin2m * (0.019993 - 0.000101 * time) + sin3m * 0.000289;// [deg]
}

double SunCalculator::GetSunTrueLong(const double& time)
{
	const double l0(GetGeomMeanLongSun(time));
	const double c(GetSunEqOfCenter(time));
	return l0 + c;// [deg]
}

double SunCalculator::GetSunTrueAnomaly(const double& time)
{
	const double m(GetGeomMeanAnomalySun(time));
	const double c(GetSunEqOfCenter(time));
	return m + c;// [deg]
}

double SunCalculator::GetSunRadVector(const double& time)
{
	const double v(GetSunTrueAnomaly(time));
	const double e(GetEccentricityEarthOrbit(time));
	return (1.000001018 * (1 - e * e)) / (1 + e * cos(v * M_PI / 180.0));// [AU]
}

double SunCalculator::GetSunApparentLong(const double& time)
{
	const double o(GetSunTrueLong(time));
	const double omega(125.04 - 1934.136 * time);
	return o - 0.00569 - 0.00478 * sin(omega * M_PI / 180.0);// [deg]
}

double SunCalculator::GetMeanObliquityOfEcliptic(const double& time)
{
	const double seconds(21.448 - time * (46.8150 + time * (0.00059 - time * 0.001813)));
	return 23.0 + (26.0 + seconds / 60.0) / 60.0;// [deg]
}

double SunCalculator::GetObliquityCorrection(const double& time)
{
	const double e0(GetMeanObliquityOfEcliptic(time));
	const double omega(125.04 - 1934.136 * time);
	return e0 + 0.00256 * cos(omega * M_PI / 180.0);// [deg]
}

double SunCalculator::GetSunRtAscension(const double& time)
{
	const double e(GetObliquityCorrection(time));
	const double lambda(GetSunApparentLong(time) * M_PI / 180.0);
	const double tananum(cos(e * M_PI / 180.0) * sin(lambda));
	const double tanadenom(cos(lambda));
	return atan2(tananum, tanadenom) * 180.0 / M_PI;// [deg]
}

double SunCalculator::GetSunDeclination(const double& time)
{
	const double e(GetObliquityCorrection(time));
	const double lambda(GetSunApparentLong(time));
	const double sint(sin(e * M_PI / 180.0) * sin(lambda * M_PI / 180.0));
	return asin(sint) * 180.0 / M_PI;// [deg]
}

double SunCalculator::CalcEquationOfTime(const double& time)
{
	const double epsilon(GetObliquityCorrection(time));
	const double l0(GetGeomMeanLongSun(time));
	const double e(GetEccentricityEarthOrbit(time));
	const double m(GetGeomMeanAnomalySun(time));

	double y(tan(epsilon * 0.5 * M_PI / 180.0));
	y *= y;

	const double sin2l0(sin(2.0 * M_PI / 180.0 * l0));
	const double sinm(sin(M_PI / 180.0 * m));
	const double cos2l0(cos(2.0 * M_PI / 180.0 * l0));
	const double sin4l0(sin(4.0 * M_PI / 180.0 * l0));
	const double sin2m(sin(2.0 * M_PI / 180.0 * m));

	const double eTime(y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0
		- 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m);
	return eTime * 4.0 * 180.0 / M_PI;// [min]
}

double SunCalculator::GetHourAngleSunrise(const double& latitude, const double& solarDeclination)
{
	const double latitudeRadians(latitude * M_PI / 180.0);
	const double solarDeclinationRadians(solarDeclination * M_PI / 180.0);
	const double arg(cos(M_PI / 180.0 * 90.833) / (cos(latitudeRadians) * cos(solarDeclinationRadians))
		- tan(latitudeRadians) * tan(solarDeclinationRadians));
	return acos(arg);// [rad] (for sunset, use opposite sign)
}

unsigned short SunCalculator::GetJulianDay(Date date)
{
	if (date.month <= 2)
	{
		date.year -= 1;
		date.month += 12;
	}

	const unsigned short a(date.year / 100);
	const unsigned short b(2 - a + a / 4);
	return floor(floor(365.25 * (date.year + 4716)) + floor(30.6001 * (date.month + 1)) + date.dayOfMonth + b - 1524.5);
}

double SunCalculator::GetRefraction(const double& elevation)
{
	if (elevation > 85.0)
		return 0.0;

	const double tanElevation(tan(elevation * M_PI / 180.0));
	double correction;
	if (elevation > 5.0)
		correction = 58.1 / tanElevation - 0.07 / pow(tanElevation, 3) + 0.000086 / pow(tanElevation, 5);
	else if (elevation > -0.575)
		correction = 1735.0 + elevation * (-518.2 + elevation * (103.4 + elevation * (-12.79 + elevation * 0.711)));
	else
		correction = -20.774 / tanElevation;

	return correction / 3600.0;
}

/*double SunCalculator::GetSolarNoon(const unsigned short& julianDay, const double& longitude, const Timezone& timezone)
{
	const double noon(GetTimeJulianCent(julianDay - longitude / 360.0));
	double eqTime(CalcEquationOfTime(noon));
	const double solarNoonOffset(720.0 - (longitude * 4) - eqTime);// [min]
	const double newTime(GetTimeJulianCent(julianDay + solarNoonOffset / 1440.0));
	eqTime = CalcEquationOfTime(newTime);
	
	double solarNoonLocal(720 - (longitude * 4) - eqTime + (timezone * 60.0);// [min]
	while (solarNoonLocal < 0.0)
		solarNoonLocal += 1440.0;
	while (solarNoonLocal >= 1440.0)
		solarNoonLocal -= 1440.0;

	return solarNoonLocal;
}*/

void SunCalculator::GetSunriseSunsetUTC(const unsigned short& julianDay, const double& latitude, const double& longitude, double& sunrise, double& sunset)
{
	const double time(GetTimeJulianCent(julianDay));
	const double eqTime(CalcEquationOfTime(time));
	const double hourAngle(GetHourAngleSunrise(latitude, GetSunDeclination(time)));
	const double riseDelta(longitude + hourAngle * 180.0 / M_PI);
	const double setDelta(longitude - hourAngle * 180.0 / M_PI);
	
	sunrise = 720 - (4.0 * riseDelta) - eqTime;
	sunset = 720 - (4.0 * setDelta) - eqTime;
}

bool SunCalculator::GetSunriseSunset(const double& latitude, const double& longitude,
	const Date& date, const Timezone& timezone, double& sunriseMinutes, double& sunsetMinutes) const
{
	const unsigned short julianDay(GetJulianDay(date));
	double tempRise, tempSet, dummy;
	GetSunriseSunsetUTC(julianDay, latitude, longitude, tempRise, tempSet);
	GetSunriseSunsetUTC(julianDay + tempRise / 1440.0, latitude, longitude, tempRise, dummy);
	GetSunriseSunsetUTC(julianDay + tempSet / 1440.0, latitude, longitude, dummy, tempSet);

	if (std::isnan(tempRise) || std::isnan(tempSet) || std::isinf(tempRise) || std::isinf(tempSet))
		return false;

	sunriseMinutes = tempRise;
	sunsetMinutes = tempSet;
		/*double timeLocal(newTimeUTC + (timezone * 60.0));
		double riseTime(GetTimeJulianCent(julianDay + newTimeUTC / 1440.0));
		unsigned short tempDay(julianDay);
		if ((timeLocal < 0.0) || (timeLocal >= 1440.0))
		{
			const unsigned short increment((timeLocal < 0) ? 1 : -1);
			while ((timeLocal < 0.0) || (timeLocal >= 1440.0))
			{
				timeLocal += increment * 1440.0;
				jday -= increment;
			}
		}*/
	
	return true;
}
