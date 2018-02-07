// File:  frequencyDataHarvester.h
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

#ifndef FREQUENCY_DATA_HARVESTER_H_
#define FREQUENCY_DATA_HARVESTER_H_

// Local headers
#include "eBirdDataProcessor.h"
#include "throttledSection.h"

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
		const std::string &county, const std::string &frequencyFilePath, const std::string& eBirdApiKey);

	bool DoBulkFrequencyHarvest(const std::string &country, const std::string &state,
		const std::string& targetPath, const unsigned int& fipsStart, const std::string& eBirdApiKey);

	bool AuditFrequencyData(const std::string& frequencyFilePath,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, const std::string& eBirdApiKey);

private:
	static const std::string targetSpeciesURLBase;
	static const std::string eBirdLoginURL;
	static const std::string userAgent;
	static const bool verbose;
	static const std::string cookieFile;
	static const std::chrono::steady_clock::duration eBirdCrawlDelay;

	ThrottledSection rateLimiter;

	CURL* curl = nullptr;
	struct curl_slist* headerList = nullptr;

	enum class ListTimeFrame
	{
		Life,
		Year,
		Month,
		Day
	};

	static std::string BuildTargetSpeciesURL(const std::string& regionString,
		const unsigned int& beginMonth, const unsigned int& endMonth,
		const ListTimeFrame& timeFrame);
	static std::string ExtractCountyNameFromPage(const std::string& regionString, const std::string& htmlData);
	static std::string GetTimeFrameString(const ListTimeFrame& timeFrame);

	bool DoGeneralCurlConfiguration();
	bool DoEBirdLogin();

	struct FrequencyData
	{
		unsigned int checklistCount;
		std::vector<EBirdDataProcessor::FrequencyInfo> frequencies;
	};

	bool PullFrequencyData(const std::string& regionString, std::array<FrequencyData, 12>& frequencyData);
	bool HarvestMonthData(const std::string& regionString, const unsigned int& month, FrequencyData& frequencyData);
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
	static std::vector<EBirdInterface::RegionInfo> FindMissingCounties(const std::string& stateCode,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, EBirdInterface& ebi);
	static std::string ExtractStateFromFileName(const std::string& fileName);
};

#endif// FREQUENCY_DATA_HARVESTER_H_
