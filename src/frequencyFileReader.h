// File:  frequencyFileReader.h
// Date:  4/10/2018
// Auth:  K. Loux
// Desc:  Class for reading binary frequency data files.

#ifndef FREQUENCY_FILE_READER_H_
#define FREQUENCY_FILE_READER_H_

// Local headers
#include "utilities/uString.h"
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <map>
#include <mutex>

class FrequencyFileReader
{
public:
	FrequencyFileReader(const UString::String& rootPath);
	
	bool ReadRegionData(const UString::String& regionCode,
		EBirdDataProcessor::FrequencyDataYear& frequencyData, EBirdDataProcessor::DoubleYear& checklistCounts);
	
private:
	static const UString::String nameIndexFileName;
	const UString::String rootPath;
	
	UString::String GenerateFileName(const UString::String& regionCode);
	
	std::mutex mutex;
	std::map<uint16_t, UString::String> indexToNameMap;
	bool ReadNameIndexData();
	
	bool DeserializeMonthData(std::ifstream& file, std::vector<EBirdDataProcessor::FrequencyInfo>& monthData, double& checklistCount) const;
	template<typename T>
	static bool Read(std::ifstream& file, T& data);
};

template<typename T>
bool FrequencyFileReader::Read(std::ifstream& file, T& data)
{
	// TODO:  Make more robust (handle endianess?)
	file.read(reinterpret_cast<char*>(&data), sizeof(data));
	return file.gcount() == sizeof(data);
}

#endif// FREQUENCY_FILE_READER_H_
