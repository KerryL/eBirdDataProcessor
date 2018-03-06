// File:  stringUtilities.h
// Date:  2/6/2018
// Auth:  K. Loux
// Desc: Colleciton of utilities for working with strings.

#ifndef STRING_UTILITIES_H_
#define STRING_UTILITIES_H_

// Local headers
#include "utilities/uString.h"

namespace StringUtilities
{
String Trim(String s);
String ToLower(const String& s);
}// namespace StringUtilities

#endif// STRING_UTILITIES_H_
