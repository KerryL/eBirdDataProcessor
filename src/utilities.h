// File:  utilities.h
// Date:  4/10/2018
// Auth:  K. Loux
// Desc:  Collection of utility methods.

#ifndef UTILITES_H_
#define UTILITES_H_

// Local headers
#include "utilities/uString.h"

namespace Utilities
{
String ExtractCountryFromRegionCode(const String& regionCode);
String ExtractStateFromRegionCode(const String& regionCode);
String StripExtension(const String& fileName);
}

#endif// UTILITES_H_
