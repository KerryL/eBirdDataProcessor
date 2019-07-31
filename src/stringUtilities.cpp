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
#include <stack>

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

bool StringUtilities::ExtractTextContainedInTag(const UString::String& htmlData,
	const UString::String& startTag, UString::String& text)
{
	text.clear();
	const auto startLocation(htmlData.find(startTag));
	if (startLocation == std::string::npos)
	{
		Cerr << "Failed to find tag '" << startTag << "' in page\n";
		return false;
	}

	const auto endOfPureTag(htmlData.find_first_of(_T(" >"), startLocation));
	if (endOfPureTag == std::string::npos)
	{
		Cerr << "Failed to determine tag string\n";
		return false;
	}

	const auto endOfFullTag(htmlData.find_first_of(_T(">"), startLocation));
	if (endOfFullTag == std::string::npos)
	{
		Cerr << "Failed to determine tag string length\n";
		return false;
	}

	const auto trimmedStartTag(htmlData.substr(startLocation, endOfPureTag - startLocation));
	const auto endTag(UString::String(1, trimmedStartTag.front()) + _T("/") + trimmedStartTag.substr(1));
	std::stack<std::string::size_type> tagStartLocations;
	std::string::size_type position(endOfFullTag);
	while (position < htmlData.length())
	{
		const auto nextStartPosition(htmlData.find(trimmedStartTag, position));
		const auto nextEndPosition(htmlData.find(endTag, position));

		if (nextEndPosition == std::string::npos)
		{
			Cerr << "Failed to find next tag set\n";
			return false;
		}

		if (nextStartPosition < nextEndPosition)
		{
			tagStartLocations.push(nextStartPosition);
			position = nextStartPosition + 1;
		}
		else if (tagStartLocations.empty())
		{
			text = htmlData.substr(endOfFullTag + 1, nextEndPosition - endOfFullTag - 1);
			break;
		}
		else
		{
			tagStartLocations.pop();
			position = nextEndPosition + 1;
		}
	}

	if (text.empty())
	{
		Cerr << "Failed to find matching tag\n";
		return false;
	}

	return true;
}
