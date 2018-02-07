// File:  stringUtilities.h
// Date:  2/6/2018
// Auth:  K. Loux
// Desc: Colleciton of utilities for working with strings.

#ifndef STRING_UTILITIES_H_
#define STRING_UTILITIES_H_

// Standard C++ headers
#include <string>

namespace StringUtilities
{
std::string Trim(std::string s);
std::string ToLower(const std::string& s);
}

#endif// STRING_UTILITIES_H_
