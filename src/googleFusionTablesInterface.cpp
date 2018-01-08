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
#include <fstream>
#include <cassert>

const std::string GoogleFusionTablesInterface::apiRoot("https://www.googleapis.com/fusiontables/v2/");
const std::string GoogleFusionTablesInterface::tablesEndPoint("tables");
const std::string GoogleFusionTablesInterface::importEndPoint("import");
const std::string GoogleFusionTablesInterface::columnsEndPoint("/columns");
const std::string GoogleFusionTablesInterface::copyEndPoint("/copy");
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
const std::string GoogleFusionTablesInterface::errorKey("error");
const std::string GoogleFusionTablesInterface::codeKey("code");
const std::string GoogleFusionTablesInterface::messageKey("message");

const std::string GoogleFusionTablesInterface::typeStringText("STRING");
const std::string GoogleFusionTablesInterface::typeNumberText("NUMBER");
const std::string GoogleFusionTablesInterface::typeDateTimeText("DATETIME");
const std::string GoogleFusionTablesInterface::typeLocationText("LOCATION");

const std::string GoogleFusionTablesInterface::fusionTableRefreshTokenFileName("~ftToken");

GoogleFusionTablesInterface::GoogleFusionTablesInterface(
	const std::string& userAgent, const std::string& oAuthClientId,
	const std::string& oAuthClientSecret) : JSONInterface(userAgent)
{
	OAuth2Interface::Get().SetClientID(oAuthClientId);
	OAuth2Interface::Get().SetClientSecret(oAuthClientSecret);
	OAuth2Interface::Get().SetScope("https://www.googleapis.com/auth/fusiontables");
	OAuth2Interface::Get().SetAuthenticationURL("https://accounts.google.com/o/oauth2/auth");
	OAuth2Interface::Get().SetResponseType("code");
	OAuth2Interface::Get().SetRedirectURI("oob");
	OAuth2Interface::Get().SetGrantType("authorization_code");
	OAuth2Interface::Get().SetTokenURL("https://accounts.google.com/o/oauth2/token");

	{
		std::ifstream tokenInFile(fusionTableRefreshTokenFileName.c_str());
		if (tokenInFile.good() && tokenInFile.is_open())
		{
			std::string token;
			tokenInFile >> token;
			OAuth2Interface::Get().SetRefreshToken(token);
		}
	}

	if (OAuth2Interface::Get().GetRefreshToken().empty())
		OAuth2Interface::Get().SetRefreshToken("");// This is how a new refresh token is requested

	if (!OAuth2Interface::Get().GetRefreshToken().empty())
	{
		std::ofstream tokenOutFile(fusionTableRefreshTokenFileName.c_str());
		tokenOutFile << OAuth2Interface::Get().GetRefreshToken();
	}
}

bool GoogleFusionTablesInterface::CreateTable(TableInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint, BuildCreateTableData(info),
		response, AddAuthAndContentTypeToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to create table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse create table response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!ReadTable(root, info))
	{
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::ListTables(std::vector<TableInfo>& tables)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint, response, AddAuthToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to request table list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse table list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, tableListKindText))
	{
		std::cerr << "Recieved unexpected kind in response to request for table list\n";
		cJSON_Delete(root);
		return false;
	}

	// TODO:  Add support for nextPageToken in order to support long result lists

	tables.clear();
	cJSON* items(cJSON_GetObjectItem(root, itemsKey.c_str()));
	if (items)// May not be items, if no tables exist
	{
		tables.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& table : tables)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				std::cerr << "Failed to get info for table " << i << '\n';
				cJSON_Delete(root);
				return false;
			}

			if (!ReadTable(item, table))
			{
				cJSON_Delete(root);
				return false;
			}

			++i;
		}
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::DeleteTable(const std::string& tableId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId, response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to delete table\n";
		return false;
	}

	return false;
}

bool GoogleFusionTablesInterface::CopyTable(const std::string& tableId, TableInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + '/' + tableId + copyEndPoint, std::string(),
		response, AddAuthAndContentTypeToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to copy table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse copy table response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!ReadTable(root, info))
	{
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::Import(const std::string& tableId,
	const std::string& csvData)
{
	// TODO:  implement
	return false;
}

bool GoogleFusionTablesInterface::ListColumns(const std::string& tableId,
	std::vector<TableInfo::ColumnInfo>& columnInfo)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId + columnsEndPoint,
		response, AddAuthToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to request column list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse column list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, columnListKindText))
	{
		std::cerr << "Recieved unexpected kind in response to request for column list\n";
		cJSON_Delete(root);
		return false;
	}

	// TODO:  Add support for nextPageToken in order to support long result lists (does this apply here?)

	cJSON* items(cJSON_GetObjectItem(root, itemsKey.c_str()));
	if (items)// May not be items, if no tables exist
	{
		columnInfo.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& column : columnInfo)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				std::cerr << "Failed to get info for column " << i << '\n';
				cJSON_Delete(root);
				return false;
			}

			if (!ReadColumn(item, column))
			{
				cJSON_Delete(root);
				return false;
			}

			++i;
		}
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::ResponseHasError(cJSON* root)
{
	cJSON* error(cJSON_GetObjectItem(root, errorKey.c_str()));
	if (!error)
		return false;

	unsigned int errorCode;
	std::string errorMessage;
	if (ReadJSON(error, codeKey, errorCode) && ReadJSON(error, messageKey, errorMessage))
		std::cerr << "Response contains error " << errorCode << ":  " << errorMessage << '\n';
	else
		std::cerr << "Response contains unknown error\n";

	return true;
}

bool GoogleFusionTablesInterface::KindMatches(cJSON* root, const std::string& kind)
{
	std::string kindString;
	if (!ReadJSON(root, kindKey, kindString))
		return false;

	return kindString.compare(kind) == 0;
}

bool GoogleFusionTablesInterface::AddAuthToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer " + static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer " + static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"), "Failed to set request type to DELETE"))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndContentTypeToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer " + static_cast<const AuthTokenData*>(data)->authToken).c_str());
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
}

bool GoogleFusionTablesInterface::ReadTable(cJSON* root, TableInfo& info)
{
	if (!KindMatches(root, tableKindText))
	{
		std::cerr << "Kind of element is not table\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		std::cerr << "Failed to read table name\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		std::cerr << "Failed to read table id\n";
		return false;
	}

	if (!ReadJSON(root, descriptionKey, info.description))
	{
		std::cerr << "Failed to read table description\n";
		return false;
	}

	if (!ReadJSON(root, isExportableKey, info.isExportable))
	{
		std::cerr << "Failed to read 'is exportable' flag\n";
		return false;
	}

	cJSON* columnArray(cJSON_GetObjectItem(root, columnsKey.c_str()));
	if (!columnArray)
	{
		std::cerr << "Failed to read columns array\n";
		return false;
	}

	info.columns.resize(cJSON_GetArraySize(columnArray));
	unsigned int i(0);
	for (auto& column : info.columns)
	{
		cJSON* item(cJSON_GetArrayItem(columnArray, i));
		if (!item)
		{
			std::cerr << "Failed to get column " << i << '\n';
			return false;
		}

		if (!ReadColumn(item, column))
			return false;

		++i;
	}

	return true;
}

bool GoogleFusionTablesInterface::ReadColumn(cJSON* root, TableInfo::ColumnInfo& info)
{
	if (!KindMatches(root, columnKindText))
	{
		std::cerr << "Kind of element is not column\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		std::cerr << "Failed to read column name\n";
		return false;
	}

	std::string typeString;
	if (!ReadJSON(root, typeKey, typeString))
	{
		std::cerr << "Failed to read column type\n";
		return false;
	}
	info.type = GetColumnTypeFromString(typeString);

	if (!ReadJSON(root, columnIdKey, info.columnId))
	{
		std::cerr << "Failed to read column name\n";
		return false;
	}

	return true;
}

std::string GoogleFusionTablesInterface::BuildCreateTableData(const TableInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		std::cerr << "Failed to create table data\n";
		return std::string();
	}

	cJSON_AddStringToObject(root, nameKey.c_str(), info.name.c_str());
	if (info.isExportable)
		cJSON_AddTrueToObject(root, isExportableKey.c_str());
	else
		cJSON_AddFalseToObject(root, isExportableKey.c_str());

	if (!info.description.empty())
		cJSON_AddStringToObject(root, descriptionKey.c_str(), info.description.c_str());

	cJSON* columns(cJSON_CreateArray());
	if (!columns)
	{
		std::cerr << "Failed to create column array\n";
		cJSON_Delete(root);
		return std::string();
	}

	cJSON_AddItemToObject(root, columnsKey.c_str(), columns);

	for (const auto& column : info.columns)
	{
		cJSON* columnItem(BuildColumnItem(column));
		if (!columnItem)
		{
			std::cerr << "Failed to create column information\n";
			cJSON_Delete(root);
			return std::string();
		}
		cJSON_AddItemToArray(columns, columnItem);
	}

	const std::string data(cJSON_Print(root));
	cJSON_Delete(root);
	return data;
}

cJSON* GoogleFusionTablesInterface::BuildColumnItem(const TableInfo::ColumnInfo& columnInfo)
{
	cJSON* columnItem(cJSON_CreateObject());
	if (!columnItem)
		return nullptr;

	cJSON_AddStringToObject(columnItem, nameKey.c_str(), columnInfo.name.c_str());
	cJSON_AddStringToObject(columnItem, typeKey.c_str(), GetColumnTypeString(columnInfo.type).c_str());

	return columnItem;
}

std::string GoogleFusionTablesInterface::GetColumnTypeString(
	const TableInfo::ColumnInfo::ColumnType& type)
{
	switch (type)
	{
	case TableInfo::ColumnInfo::ColumnType::String:
		return typeStringText;

	case TableInfo::ColumnInfo::ColumnType::Number:
		return typeNumberText;

	case TableInfo::ColumnInfo::ColumnType::DateTime:
		return typeDateTimeText;

	case TableInfo::ColumnInfo::ColumnType::Location:
		return typeLocationText;

	default:
		assert(false);
	}

	return std::string();
}

GoogleFusionTablesInterface::TableInfo::ColumnInfo::ColumnType
	GoogleFusionTablesInterface::GetColumnTypeFromString(const std::string& s)
{
	if (s.compare(typeStringText) == 0)
		return TableInfo::ColumnInfo::ColumnType::String;
	else if (s.compare(typeNumberText) == 0)
		return TableInfo::ColumnInfo::ColumnType::Number;
	else if (s.compare(typeLocationText) == 0)
		return TableInfo::ColumnInfo::ColumnType::Location;
	else if (s.compare(typeDateTimeText) == 0)
		return TableInfo::ColumnInfo::ColumnType::DateTime;

	assert(false);
	return TableInfo::ColumnInfo::ColumnType::String;
}
