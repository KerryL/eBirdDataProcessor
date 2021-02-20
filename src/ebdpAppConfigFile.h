// File:  ebdpAppConfigFile.h
// Date:  9/27/2017
// Auth:  K. Loux
// Desc:  Configuration file object for configuration options that are independent of the analysis to be conducted.

#ifndef EBDP_APP_CONFIG_FILE_H_
#define EBDP_APP_CONFIG_FILE_H_

// Local headers
#include "utilities/configFile.h"
#include "utilities/uString.h"
#include "ebdpConfig.h"

class EBDPAppConfigFile : public ConfigFile
{
public:
	EBDPAppConfigFile(UString::OStream &outStream = Cout)
		: ConfigFile(outStream) {}

	ApplicationConfiguration& GetConfig() { return config; }

private:
	ApplicationConfiguration config;

	void BuildConfigItems() override;
	void AssignDefaults() override;
	bool ConfigIsOK() override;
};

#endif// EBDP_APP_CONFIG_FILE_H_
