// File:  utilities.h
// Date:  4/10/2018
// Auth:  K. Loux
// Desc:  Collection of utility methods.

#ifndef UTILITES_H_
#define UTILITES_H_

// Local headers
#include "utilities/uString.h"

// Standard C++ headers
#include <vector>

namespace Utilities
{
UString::String ExtractCountryFromRegionCode(const UString::String& regionCode);
UString::String ExtractStateFromRegionCode(const UString::String& regionCode);
UString::String ExtractFileName(const UString::String& path);
UString::String StripExtension(const UString::String& fileName);
UString::String SanitizeCommas(const UString::String& s);
UString::String Unsanitize(const UString::String& s);
void ReplaceAll(const UString::String& pattern, const UString::String& replaceWith, UString::String& s);
bool ItemIsInVector(const UString::String& s, const std::vector<UString::String>& v);
double ComputeWGS84Distance(const double& latitude1, const double& longitude1, const double& latitude2, const double& longitude2);
}

#endif// UTILITES_H_
