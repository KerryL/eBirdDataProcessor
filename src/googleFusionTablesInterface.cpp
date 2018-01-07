// File:  googleFusionTablesInterface.h
// Date:  1/7/2018
// Auth:  K. Loux
// Desc:  Interface to Google's Fusion Tables web API.

// Local headers
#include "googleFusionTablesInterface.h"
#include "email/oAuth2Interface.h"
#include "email/curlUtilities.h"

// cURL headers
#include <curl/curl.h>

// Standard C++ headers
#include <iostream>

const std::string GoogleFusionTablesInterface::apiRoot("https://www.googleapis.com/fusiontables/v2/");
const std::string GoogleFusionTablesInterface::tablesEndPoint("tables");
const std::string GoogleFusionTablesInterface::importEndPoint("import");
const std::string GoogleFusionTablesInterface::columnsEndPoint("columns");
const std::string GoogleFusionTablesInterface::tableListKindText("fusiontables#tableList");
const std::string GoogleFusionTablesInterface::tableKindText("fusiontables#table");
const std::string GoogleFusionTablesInterface::columnKindText("fusiontables#column");
const std::string GoogleFusionTablesInterface::columnListKindText("");
const std::string GoogleFusionTablesInterface::itemsKey("items");
const std::string GoogleFusionTablesInterface::kindKey("kind");
const std::string GoogleFusionTablesInterface::tableIdKey("tableId");
const std::string GoogleFusionTablesInterface::nameKey("name");
const std::string GoogleFusionTablesInterface::columnIdKey("columnId");
const std::string GoogleFusionTablesInterface::columnsKey("columns");
const std::string GoogleFusionTablesInterface::typeKey("type");
const std::string GoogleFusionTablesInterface::descriptionKey("description");
const std::string GoogleFusionTablesInterface::isExportableKey("isExportable");

const std::string GoogleFusionTablesInterface::typeStringText("STRING");
const std::string GoogleFusionTablesInterface::typeNumberText("NUMBER");
const std::string GoogleFusionTablesInterface::typeDateTimeText("DATETIME");
const std::string GoogleFusionTablesInterface::locationText("LOCATION");

GoogleFusionTablesInterface::GoogleFusionTablesInterface(
	const std::string& userAgent, const std::string& oAuthClientId,
	const std::string& oAuthClientSecret) : JSONInterface(userAgent)
{
	OAuth2Interface::Get().SetClientID(oAuthClientId);
	OAuth2Interface::Get().SetClientSecret(oAuthClientSecret);
	OAuth2Interface::Get().SetScope("https://www.googleapis.com/auth/fusiontables");

	OAuth2Interface::Get().SetAuthenticationURL("https://accounts.google.com/o/oauth2/auth");
	OAuth2Interface::Get().SetResponseType("code");
	OAuth2Interface::Get().SetRedirectURI("urn:ietf:wg:oauth:2.0:oob");
	//OAuth2Interface::Get().SetLoginHint(loginInfo.localEmail);
	OAuth2Interface::Get().SetGrantType("authorization_code");
	OAuth2Interface::Get().SetTokenURL("https://accounts.google.com/o/oauth2/token");
}

bool GoogleFusionTablesInterface::CreateTable(TableInfo& info)
{
	return false;
}

bool GoogleFusionTablesInterface::ListTables(std::vector<TableInfo>& tables)
{
	const std::string authToken(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint, response, [authToken](CURL* curl)
		{
			curl_slist* headerList(curl_slist_append(headerList, std::string("Authorization: " + authToken).c_str()));
			if (!headerList)
			{
				std::cerr << "Failed to append auth token to header in ListTables\n";
				return false;
			}

			headerList = curl_slist_append(headerList, "Content-Type: application/json");
			if (!headerList)
			{
				std::cerr << "Failed to append content type to header in ListTables\n";
				return false;
			}

			if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
				return false;

			return true;
		}))
	{
		std::cerr << "Failed to request table list\n";
		return false;
	}

	std::cout << response << std::endl;

	return true;
}

bool GoogleFusionTablesInterface::DeleteTable(const std::string& tableId)
{
	return false;
}

bool GoogleFusionTablesInterface::CopyTable(const std::string& tableId, TableInfo& info)
{
	return false;
}

bool GoogleFusionTablesInterface::Import(const std::string& tableId,
	const std::string& csvData)
{
	return false;
}

bool GoogleFusionTablesInterface::ListColumns(const std::string& tableId,
	std::vector<TableInfo::ColumnInfo>& columnInfo)
{
	return false;
}
