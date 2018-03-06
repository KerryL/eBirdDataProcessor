// File:  frequencyDataHarvester.h
// Date:  10/30/2017
// Auth:  K. Loux
// Desc:  Class for pulling frequency data for a specific region.

#ifndef FREQUENCY_DATA_HARVESTER_H_
#define FREQUENCY_DATA_HARVESTER_H_

// Local headers
#include "eBirdDataProcessor.h"
#include "throttledSection.h"
#include "utilities/uString.h"

// Standard C++ headers
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

	bool GenerateFrequencyFile(const String &country, const String &state,
		const String &county, const String &frequencyFilePath, const String& eBirdApiKey);

	bool DoBulkFrequencyHarvest(const String &country, const String &state,
		const String& targetPath, const String& firstSubRegion, const String& eBirdApiKey);

	bool AuditFrequencyData(const String& frequencyFilePath,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, const String& eBirdApiKey);

private:
	static const String targetSpeciesURLBase;
	static const String eBirdLoginURL;
	static const String userAgent;
	static const bool verbose;
	static const String cookieFile;
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

	static String BuildTargetSpeciesURL(const String& regionString,
		const unsigned int& beginMonth, const unsigned int& endMonth,
		const ListTimeFrame& timeFrame);
	static String ExtractCountyNameFromPage(const String& regionString, const String& htmlData);
	static String GetTimeFrameString(const ListTimeFrame& timeFrame);

	bool DoGeneralCurlConfiguration();
	bool DoEBirdLogin();

	struct FrequencyData
	{
		unsigned int checklistCount;
		std::vector<EBirdDataProcessor::FrequencyInfo> frequencies;
	};

	bool PullFrequencyData(const String& regionString, std::array<FrequencyData, 12>& frequencyData);
	bool HarvestMonthData(const String& regionString, const unsigned int& month, FrequencyData& frequencyData);
	bool PostEBirdLoginInfo(const String& userName, const String& password, String& resultPage);
	static bool EBirdLoginSuccessful(const String& htmlData);
	static void GetUserNameAndPassword(String& userName, String& password);
	static String BuildEBirdLoginInfo(const String& userName, const String& password, const String& token);
	bool DoCURLGet(const String& url, String &response);
	static size_t CURLWriteCallback(char *ptr, size_t size, size_t nmemb, void *userData);

	static bool ExtractFrequencyData(const String& htmlData, FrequencyData& data);
	static bool WriteFrequencyDataToFile(const String& fileName, const std::array<FrequencyData, 12>& data);
	static bool ExtractTextBetweenTags(const String& htmlData, const String& startTag,
		const String& endTag, String& text, std::string::size_type& offset);
	static String ExtractTokenFromLoginPage(const String& htmlData);
	static bool CurrentDataMissingSpecies(const String& fileName, const std::array<FrequencyData, 12>& data);

	static String Clean(const String& s);
	static bool DataIsEmpty(const std::array<FrequencyData, 12>& frequencyData);

	struct StateCountryCode
	{
		String state;
		String country;

		bool operator==(const StateCountryCode& other) const
		{
			return state.compare(other.state) == 0 && country.compare(other.country) == 0;
		}

		bool operator<(const StateCountryCode& other) const
		{
			if (country < other.country)
				return true;
			else if (country.compare(other.country) == 0)
				return state < other.state;
			return false;
		}
	};

	static std::vector<StateCountryCode> GetCountriesAndStates(const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo);
	static std::vector<EBirdInterface::RegionInfo> FindMissingCounties(const String& stateCode,
		const std::vector<EBirdDataProcessor::YearFrequencyInfo>& freqInfo, EBirdInterface& ebi);
	static String ExtractCountryFromFileName(const String& fileName);
	static String ExtractStateFromFileName(const String& fileName);
};

#endif// FREQUENCY_DATA_HARVESTER_H_
