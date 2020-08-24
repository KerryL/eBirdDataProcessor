// File:  frequencyFileReader.cpp
// Date:  4/10/2018
// Auth:  K. Loux
// Desc:  Class for reading binary frequency data files.

// Local headers
#include "frequencyFileReader.h"
#include "utilities.h"

// Standard C++ headers
#include <locale>

const UString::String FrequencyFileReader::nameIndexFileName(_T("nameIndexMap.csv"));

FrequencyFileReader::FrequencyFileReader(const UString::String& rootPath) : rootPath(rootPath)
{
}

UString::String FrequencyFileReader::GenerateFileName(const UString::String& regionCode)
{
	const UString::String countryCode(Utilities::ExtractCountryFromRegionCode(regionCode));
	return rootPath + countryCode + _T("/") + regionCode + _T(".bin");
}
	
bool FrequencyFileReader::ReadRegionData(const UString::String& regionCode,
	EBirdDataProcessor::FrequencyDataYear& frequencyData, EBirdDataProcessor::DoubleYear& checklistCounts)
{
	if (indexToNameMap.empty())
	{
		if (!ReadNameIndexData())
			return false;
	}
	
	const UString::String fileName(GenerateFileName(regionCode));
	std::ifstream file(fileName, std::ios::binary);
	if (!file.good() || !file.is_open())
	{
		Cerr << "Failed to open '" << fileName << "' for input\n";
		return false;
	}
	
	unsigned int i;
	for (i = 0; i < frequencyData.size(); ++i)
	{
		if (!DeserializeWeekData(file, frequencyData[i], checklistCounts[i]))
			return false;
	}
	
	return true;
}

bool FrequencyFileReader::ReadNameIndexData()
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!indexToNameMap.empty())// In case of data race
		return true;

	const UString::String fileName(rootPath + nameIndexFileName);
	UString::IFStream file(fileName);
	if (!file.good() || !file.is_open())
	{
		Cerr << "Failed to open '" << fileName << "' for input\n";
		return false;
	}

	file.imbue(std::locale());
	
	UString::String line;
	while (std::getline(file, line))
	{
		UString::IStringStream ss(line);
		UString::String commonName;
		if (!std::getline(ss, commonName, UString::Char(',')))
		{
			Cerr << "Failed to parse name\n";
			return false;
		}
		
		uint16_t index;
		if ((ss >> index).fail())
		{
			Cerr << "Failed to parse index\n";
			return false;
		}
		
		indexToNameMap[index] = commonName;
	}
	
	return true;
}

bool FrequencyFileReader::DeserializeWeekData(std::ifstream& file,
	std::vector<EBirdDataProcessor::FrequencyInfo>& weekData, double& checklistCount) const
{
	uint16_t temp;
	if (!Read(file, temp))
		return false;
	checklistCount = temp;

	if (!Read(file, temp))
		return false;
	weekData.resize(temp);
	
	for (auto& species : weekData)
	{
		if (!Read(file, temp))
			return false;

		const auto mapping(indexToNameMap.find(temp));
		assert(mapping != indexToNameMap.end());
		species.species = mapping->second;
		species.compareString = EBirdDataProcessor::PrepareForComparison(species.species);
		
		if (!Read(file, species.frequency))
			return false;
		
		assert(!species.species.empty());
		assert(species.frequency >= 0.0);
	}
	
	return true;
}
