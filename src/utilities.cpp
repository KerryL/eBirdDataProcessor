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

UString::String ExtractCountryFromRegionCode(const UString::String& regionCode)
{
	return regionCode.substr(0, 2);
}

UString::String ExtractStateFromRegionCode(const UString::String& regionCode)
{
	// For US, states abbreviations are all 2 characters, but this isn't universal.  Need to find the hyphen.
	// eBird does guarantee that country abbreviations are two characters, however.
	const std::string::size_type start(3);
	if (regionCode.length() < start)
		return UString::String();

	const std::string::size_type length(regionCode.find('-', start) - start);
	assert(length != std::string::npos);
	return regionCode.substr(start, length);
}

UString::String StripExtension(const UString::String& fileName)
{
	const auto extStart(fileName.find_last_of(_T(".")));
	if (extStart == std::string::npos)
		return fileName;
	return fileName.substr(0, extStart);
}

UString::String SanitizeCommas(const UString::String& s)
{
	UString::String clean(s);
	ReplaceAll(_T(","), _T("&#44;"), clean);
	return clean;
}

UString::String Unsanitize(const UString::String& s)
{
	UString::String dirty(s);
	ReplaceAll(_T("&#44;"), _T(","), dirty);
	ReplaceAll(_T("&#47;"), _T("/"), dirty);
	ReplaceAll(_T("&#39;"), _T("'"), dirty);
	ReplaceAll(_T("&#x27;"), _T("'"), dirty);// This is what appears in the html file, but I think it *should* be #39?
	return dirty;
}

void ReplaceAll(const UString::String& pattern, const UString::String& replaceWith, UString::String& s)
{
	std::string::size_type position(0);
	while (position = s.find(pattern, position), position != std::string::npos)
	{
		s.replace(position, pattern.length(), replaceWith);
		position += replaceWith.length();
	}
}

UString::String BuildRegionCode(const UString::String& country, const UString::String& state, const UString::String& county)
{
	UString::String code(country);
	if (!state.empty())
	{
		code.append(UString::String(_T("-")) + state);
		if (!county.empty())
			code.append(UString::String(_T("-")) + county);
	}

	return code;
}

}// namespace Utilities
