// File:  observationMapBuilder.h
// Date:  11/29/2020
// Auth:  K. Loux
// Desc:  Object for building observation map HTML and JS files.

#ifndef OBSERVATION_MAP_BUILDER_H_
#define OBSERVATION_MAP_BUILDER_H_

// Local headers
#include "utilities/uString.h"
#include "eBirdDatasetInterface.h"

class ObservationMapBuilder
{
public:
	bool Build(const UString::String& outputFileName, const UString::String& kmlBoundaryFileName, const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo) const;

private:
	bool WriteDataFile(const UString::String& fileName, const UString::String& kml, const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo) const;
	bool WriteHTMLFile(const UString::String& fileName) const;
	
	static bool CreateJSONData(const UString::String& kml, const double& kmlReductionLimit, cJSON*& geoJSON);
	static bool CreateJSONData(const std::vector<EBirdDatasetInterface::MapInfo>& mapInfo, cJSON*& mapJSON);
	static bool BuildGeometryJSON(const UString::String& kml, const double& kmlReductionLimit, cJSON* json);
	static bool BuildLocationJSON(const EBirdDatasetInterface::MapInfo& mapInfo, cJSON* json);
};

#endif// OBSERVATION_MAP_BUILDER_H_
