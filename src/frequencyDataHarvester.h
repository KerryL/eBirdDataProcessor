// File:  frequencyDataHarvester.h
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

#ifndef FREQUENCY_DATA_HARVESTER_H_
#define FREQUENCY_DATA_HARVESTER_H_

// Local headers
#include "eBirdDataProcessor.h"

// Standard C++ headers
#include <string>
#include <vector>
#include <array>
#include <chrono>

// for cURL
typedef void CURL;

class FrequencyDataHarvester
{
public:
	FrequencyDataHarvester();
	~FrequencyDataHarvester();

	bool GenerateFrequencyFile(const std::string &country, const std::string &state,
		const std::string &county, const std::string &frequencyFileName);

	bool DoBulkFrequencyHarvest(const std::string &country, const std::string &state,
		const std::string& targetPath, const std::string& censusKey, const unsigned int& fipsStart);

	bool AuditFrequencyData(const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, const std::string& censusKey);

	static std::string GenerateFrequencyFileName(const std::string& state, const std::string& county);

private:
	static const std::string targetSpeciesURLBase;
	static const std::string eBirdLoginURL;
	static const std::string userAgent;
	static const bool verbose;
	static const std::string cookieFile;
	static const std::chrono::steady_clock::duration eBirdCrawlDelay;

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
	static std::string BuildRegionString(const std::string &country, const std::string &state,
		const unsigned int &county);
	static std::string BuildTargetSpeciesURL(const std::string& regionString,
		const unsigned int& beginMonth, const unsigned int& endMonth,
		const ListTimeFrame& timeFrame);
	static std::string ExtractCountyNameFromPage(const std::string& regionString, const std::string& htmlData);
	static std::string GetTimeFrameString(const ListTimeFrame& timeFrame);

	bool DoGeneralCurlConfiguration();
	bool DoEBirdLogin();

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

	bool PullFrequencyData(const std::string& regionString, std::array<FrequencyData, 12>& frequencyData);
	bool PostEBirdLoginInfo(const std::string& userName, const std::string& password, std::string& resultPage);
	static bool EBirdLoginSuccessful(const std::string& htmlData);
	static void GetUserNameAndPassword(std::string& userName, std::string& password);
	static std::string BuildEBirdLoginInfo(const std::string& userName, const std::string& password, const std::string& token);
	bool DoCURLGet(const std::string& url, std::string &response);
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static bool ExtractFrequencyData(const std::string& htmlData, FrequencyData& data);
	static bool WriteFrequencyDataToFile(const std::string& fileName, const std::array<FrequencyData, 12>& data);
	static bool ExtractTextBetweenTags(const std::string& htmlData, const std::string& startTag,
		const std::string& endTag, std::string& text, std::string::size_type& offset);
	static std::string ExtractTokenFromLoginPage(const std::string& htmlData);
	static bool CurrentDataMissingSpecies(const std::string& fileName, const std::array<FrequencyData, 12>& data);

	static std::string Clean(const std::string& s);
	static bool DataIsEmpty(const std::array<FrequencyData, 12>& frequencyData);

	static std::vector<std::string> GetStates(const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo);
	static std::vector<unsigned int> FindMissingCounties(const std::string& state,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo,
		const unsigned int& stateFIPSCode, const std::string& censusKey);
	static std::string ExtractStateFromFileName(const std::string& fileName);
	static std::string StripDirectory(const std::string& s);

	static const std::string endOfName;
};

#endif// FREQUENCY_DATA_HARVESTER_H_
