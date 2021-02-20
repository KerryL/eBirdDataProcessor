// File:  ebdpConfigFile.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration file object.

#ifndef EBDP_CONFIG_FILE_H_
#define EBDP_CONFIG_FILE_H_

// Local headers
#include "utilities/configFile.h"
#include "utilities/uString.h"
#include "ebdpConfig.h"

class EBDPConfigFile : public ConfigFile
{
public:
	EBDPConfigFile(UString::OStream &outStream = Cout)
		: ConfigFile(outStream) {}

	EBDPConfig& GetConfig() { return config; }

private:
	UString::String appConfigFileName;
	EBDPConfig config;

	void BuildConfigItems() override;
	void AssignDefaults() override;
	bool ConfigIsOK() override;

	bool TimeOfDayConfigIsOK();
	bool TimeOfYearConfigIsOK();
	bool FrequencyHarvestConfigIsOK();
	bool TargetCalendarConfigIsOK();
	bool FindMaxNeedsConfigIsOK();
	bool GeneralConfigIsOK();
	bool RaritiesConfigIsOK();
	bool BestTripConfigIsOK();
	bool SpeciesHuntConfigIsOK();
};

#endif// EBDP_CONFIG_FILE_H_
