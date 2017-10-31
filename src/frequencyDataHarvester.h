// File:  frequencyDataHarvester.h
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

#ifndef FREQUENCY_DATA_HARVESTER_H_
#define FREQUENCY_DATA_HARVESTER_H_

// Standard C++ headers
#include <string>
#include <vector>
#include <array>

// for cURL
typedef void CURL;

class FrequencyDataHarvester
{
public:
	~FrequencyDataHarvester();

	bool GenerateFrequencyFile(const std::string &country, const std::string &state,
		const std::string &county, const std::string &frequencyFileName);

private:
	static const std::string targetSpeciesURLBase;
	static const std::string eBirdLoginURL;
	static const std::string userAgent;
	static const bool verbose;

	CURL* curl = nullptr;
	struct curl_slist* headerList = nullptr;

	enum class ListTimeFrame
	{
		Life,
		Year,
		Month,
		Day
	};

	static std::string BuildRegionString(const std::string &country, const std::string &state,
		const std::string &county);
	static std::string BuildTargetSpeciesURL(const std::string& regionString,
		const unsigned int& beginMonth, const unsigned int& endMonth,
		const ListTimeFrame& timeFrame);
	static std::string GetTimeFrameString(const ListTimeFrame& timeFrame);

	bool PostEBirdLoginInfo(const std::string& userName, const std::string& password);
	static std::string BuildEBirdLoginInfo(const std::string& userName, const std::string& password, const std::string& token);
	bool DoCURLGet(const std::string& url, std::string &response);
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	struct FrequencyData
	{
		struct SpeciesFrequency
		{
			std::string species;
			double frequency;
		};

		unsigned int checklistCount;
		std::vector<SpeciesFrequency> frequencies;
	};

	static bool ExtractFrequencyData(const std::string& htmlData, FrequencyData& data);
	static bool WriteFrequencyDataToFile(const std::string& fileName, const std::array<FrequencyData, 12>& data);
	static bool ExtractTextBetweenTags(const std::string& htmlData, const std::string& startTag,
		const std::string& endTag, std::string& text, std::string::size_type& offset);
	static std::string ExtractTokenFromLoginPage(const std::string& htmlData);
};

#endif// FREQUENCY_DATA_HARVESTER_H_
