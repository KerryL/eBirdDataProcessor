// File:  UString::Utilities.h
// Date:  2/6/2018
// Auth:  K. Loux
// Desc: Colleciton of utilities for working with UString::s.

#ifndef STRING_UTILITIES_H_
#define STRING_UTILITIES_H_

// Local headers
#include "utilities/uString.h"

namespace StringUtilities
{
UString::String Trim(UString::String s);
UString::String ToLower(const UString::String& s);
}// namespace UString::Utilities

#endif// UString::_UTILITIES_H_
