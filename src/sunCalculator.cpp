// File:  sunCalculator.cpp
// Date:  11/25/2020
// Auth:  K. Loux
// Desc:  Class for calculating sunrise/sunset times based on location and time of year.

// Local headers
#include "sunCalculator.h"

#include <iostream>
const UString::String SunCalculator::requestURLBase(_T("http://api.geonames.org/timezoneJSON?"));
const UString::String SunCalculator::userName(_T("kerryl"));

bool SunCalculator::GetSunriseSunset(const double& latitude, const double& longitude,
	const Date& date, double& sunriseMinutes, double& sunsetMinutes) const
{
	std::string response;
	if (!DoCURLGet(BuildRequestURL(latitude, longitude, date), response))
		return false;
	
	cJSON *root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse returned string (GetHotspotsInRegion())\n";
		Cerr << response.c_str() << '\n';
		return false;
	}
	
	cJSON* datesArray(cJSON_GetObjectItem(root, "dates"));
	if (!datesArray)
	{
		Cerr << "Failed to read dates array from JSON response\n";
		cJSON_Delete(root);
		return false;
	}

	cJSON* datesItem(cJSON_GetArrayItem(datesArray, 0));
	if (!datesItem)
	{
		Cerr << "Failed to read dates item from JSON response\n";
		cJSON_Delete(root);
		return false;
	}

	std::tm sunrise{}, sunset{};
	if (!ReadJSON(datesItem, _T("sunrise"), sunrise) ||
		!ReadJSON(datesItem, _T("sunset"), sunset))
	{
		Cerr << "Failed to read sunrise/sunset times from JSON response\n";
		cJSON_Delete(root);
		return false;
	}
	
	sunriseMinutes = sunrise.tm_hour * 60 + sunrise.tm_min;
	sunsetMinutes = sunset.tm_hour * 60 + sunset.tm_min;
	
	cJSON_Delete(root);
	return true;
}

UString::String SunCalculator::BuildRequestURL(const double& latitude, const double& longitude, const Date& date)
{
	UString::OStringStream ss;
	ss << requestURLBase << "lat=" << latitude << "&lng=" << longitude << "&date=" << date.year << "-" << date.month << "-" << date.dayOfMonth << "&username=" << userName;
	return ss.str();
}
