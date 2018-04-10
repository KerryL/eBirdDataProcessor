// File:  utilities.cpp
// Date:  4/10/2018
// Auth:  K. Loux
// Desc:  Collection of utility methods.

// Local headers
#include "utilities.h"

// Standard C++ headers
#include <cassert>

namespace Utilities
{

String ExtractCountryFromRegionCode(const String& regionCode)
{
	return regionCode.substr(0, 2);
}

String ExtractStateFromRegionCode(const String& regionCode)
{
	// For US, states abbreviations are all 2 characters, but this isn't universal.  Need to find the hyphen.
	// eBird does guarantee that country abbreviation are two characters, however.
	const std::string::size_type start(3);
	const std::string::size_type length(regionCode.find('-', start) - start);
	assert(length != std::string::npos);
	return regionCode.substr(start, length);
}

String StripExtension(const String& fileName)
{
	const String extension(_T(".csv"));
	return fileName.substr(0, fileName.length() - extension.length());
}

}// namespace Utilities
