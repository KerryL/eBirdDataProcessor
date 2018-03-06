// File:  stringUtilities.cpp
// Date:  2/6/2018
// Auth:  K. Loux
// Desc: Colleciton of utilities for working with strings.

// Local headers
#include "stringUtilities.h"

// Standard C++ headers
#include <algorithm>
#include <cctype>
#include <functional>

String StringUtilities::Trim(String s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
		std::not1(std::ptr_fun<int, int>(std::isspace))));

    s.erase(std::find_if(s.rbegin(), s.rend(),
		std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

	return s;
}

String StringUtilities::ToLower(const String& s)
{
	String lower(s);
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	return lower;
}
