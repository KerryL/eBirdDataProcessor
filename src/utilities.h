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
UString::String ExtractCountryFromRegionCode(const UString::String& regionCode);
UString::String ExtractStateFromRegionCode(const UString::String& regionCode);
UString::String StripExtension(const UString::String& fileName);
UString::String SanitizeCommas(const UString::String& s);
UString::String Unsanitize(const UString::String& s);
UString::String BuildRegionCode(const UString::String& country, const UString::String& state, const UString::String& county);
void ReplaceAll(const UString::String& pattern, const UString::String& replaceWith, UString::String& s);
}

#endif// UTILITES_H_
