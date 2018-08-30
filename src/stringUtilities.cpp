// File:  UString::Utilities.cpp
// Date:  2/6/2018
// Auth:  K. Loux
// Desc: Colleciton of utilities for working with UString::s.

// Local headers
#include "stringUtilities.h"

// Standard C++ headers
#include <algorithm>
#include <cctype>
#include <functional>

UString::String StringUtilities::Trim(UString::String s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));

    s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

	return s;
}

UString::String StringUtilities::ToLower(const UString::String& s)
{
	UString::String lower(s);
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	return lower;
}
