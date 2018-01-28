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
#include <sstream>

const std::string GoogleFusionTablesInterface::apiRoot("https://www.googleapis.com/fusiontables/v2/");
const std::string GoogleFusionTablesInterface::apiRootUpload("https://www.googleapis.com/upload/fusiontables/v2/");
const std::string GoogleFusionTablesInterface::tablesEndPoint("tables");
const std::string GoogleFusionTablesInterface::queryEndPoint("query");
const std::string GoogleFusionTablesInterface::importEndPoint("/import");
const std::string GoogleFusionTablesInterface::columnsEndPoint("/columns");
const std::string GoogleFusionTablesInterface::stylesEndPoint("/styles");
const std::string GoogleFusionTablesInterface::templatesEndPoint("/templates");
const std::string GoogleFusionTablesInterface::copyEndPoint("/copy");
const std::string GoogleFusionTablesInterface::tableListKindText("fusiontables#tableList");
const std::string GoogleFusionTablesInterface::tableKindText("fusiontables#table");
const std::string GoogleFusionTablesInterface::columnKindText("fusiontables#column");
const std::string GoogleFusionTablesInterface::importKindText("fusiontables#import");
const std::string GoogleFusionTablesInterface::queryResponseKindText("fusiontables#sqlresponse");
const std::string GoogleFusionTablesInterface::styleSettingListText("fusiontables#styleSettingList");
const std::string GoogleFusionTablesInterface::styleSettingKindText("fusiontables#styleSetting");
const std::string GoogleFusionTablesInterface::columnListKindText("fusiontables#columnList");
const std::string GoogleFusionTablesInterface::fromColumnKindText("fusiontables#fromColumn");
const std::string GoogleFusionTablesInterface::templateListKindText("fusiontables#templateList");
const std::string GoogleFusionTablesInterface::templateKindText("fusiontables#template");
const std::string GoogleFusionTablesInterface::itemsKey("items");
const std::string GoogleFusionTablesInterface::kindKey("kind");
const std::string GoogleFusionTablesInterface::tableIdKey("tableId");
const std::string GoogleFusionTablesInterface::styleIdKey("styleId");
const std::string GoogleFusionTablesInterface::nameKey("name");
const std::string GoogleFusionTablesInterface::columnIdKey("columnId");
const std::string GoogleFusionTablesInterface::columnsKey("columns");
const std::string GoogleFusionTablesInterface::typeKey("type");
const std::string GoogleFusionTablesInterface::descriptionKey("description");
const std::string GoogleFusionTablesInterface::isExportableKey("isExportable");
const std::string GoogleFusionTablesInterface::errorKey("error");
const std::string GoogleFusionTablesInterface::codeKey("code");
const std::string GoogleFusionTablesInterface::messageKey("message");
const std::string GoogleFusionTablesInterface::numberOfRowsImportedKey("numRowsReceived");
const std::string GoogleFusionTablesInterface::columnNameKey("columnName");
const std::string GoogleFusionTablesInterface::fillColorStylerKey("fillColorStyler");
const std::string GoogleFusionTablesInterface::templateIdKey("templateId");
const std::string GoogleFusionTablesInterface::bodyKey("body");

const std::string GoogleFusionTablesInterface::isDefaultKey("isDefaultForTable");
const std::string GoogleFusionTablesInterface::markerOptionsKey("markerOptions");
const std::string GoogleFusionTablesInterface::polylineOptionsKey("polylineOptions");
const std::string GoogleFusionTablesInterface::polygonOptionsKey("polygonOptions");

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
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
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

	return true;
}

bool GoogleFusionTablesInterface::CopyTable(const std::string& tableId, TableInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + '/' + tableId + copyEndPoint, std::string(),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
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
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRootUpload + tablesEndPoint + '/' + tableId + importEndPoint + "?uploadType=media", csvData,
		response, AddAuthAndOctetContentTypeToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to import data to table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse copy import response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, importKindText))
	{
		std::cerr << "Received unexpected kind in import response\n";
		cJSON_Delete(root);
		return false;
	}

	std::string rowsString;
	if (!ReadJSON(root, numberOfRowsImportedKey, rowsString))
	{
		std::cerr << "Failed to check number of imported rows\n";
		cJSON_Delete(root);
		return false;
	}

	std::cout << "Successfully imported " << rowsString << " new rows into table " << tableId << std::endl;

	cJSON_Delete(root);
	return true;
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
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer "
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer "
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_CUSTOMREQUEST, "DELETE"), "Failed to set request type to DELETE"))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndJSONContentTypeToCurlHeader(
	CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer "
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
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

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndOctetContentTypeToCurlHeader(
	CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, std::string("Authorization: Bearer "
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		std::cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	headerList = curl_slist_append(headerList, "Content-Type: application/octet-stream");
	if (!headerList)
	{
		std::cerr << "Failed to append content type to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), "Failed to set header"))
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

bool GoogleFusionTablesInterface::DeleteAllRows(const std::string& tableId)
{
	const std::string deleteCommand("DELETE FROM " + tableId);
	cJSON* root(nullptr);
	if (!SubmitQuery(deleteCommand, root))
		return false;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::DeleteRow(const std::string& tableId, const unsigned int& rowId)
{
	std::ostringstream ss;
	ss << rowId;
	const std::string deleteCommand("DELETE FROM " + tableId + " WHERE ROWID = " + ss.str());
	cJSON* root(nullptr);
	if (!SubmitQuery(deleteCommand, root))
		return false;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::SetTableAccess(const std::string& tableId, const TableAccess& access)
{
	// TODO:  implement
	std::cerr << "Setting table access permissions is not currently implemented.  You can do this"
		<< " manually by logging into Google Drive and modifying the 'Sharing' settings for the table.\n";
	return false;
}

bool GoogleFusionTablesInterface::SubmitQuery(const std::string& query, cJSON*& root)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + queryEndPoint + "?sql=" + URLEncode(query), std::string(),
		response, AddAuthToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to submit query\n";
		return false;
	}

	root = cJSON_Parse(response.c_str());
	if (!root)
	{
		std::cerr << "Failed to parse query response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, queryResponseKindText))
	{
		std::cerr << "Received unexpected kind in query response\n";
		cJSON_Delete(root);
		return false;
	}

	//cJSON_Delete(root);// Caller responsible for freeing root!
	return true;
}

bool GoogleFusionTablesInterface::CreateStyle(const std::string& tableId, StyleInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + '/' + tableId + stylesEndPoint, BuildCreateStyleData(info),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to create style\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse create style response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!ReadStyle(root, info))
	{
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::ListStyles(const std::string& tableId, std::vector<StyleInfo>& styles)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId + stylesEndPoint, response, AddAuthToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to request style list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse style list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, styleSettingListText))
	{
		std::cerr << "Recieved unexpected kind in response to request for style list\n";
		cJSON_Delete(root);
		return false;
	}

	styles.clear();
	cJSON* items(cJSON_GetObjectItem(root, itemsKey.c_str()));
	if (items)// May not be items, if no styles exist
	{
		styles.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& style : styles)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				std::cerr << "Failed to get info for style " << i << '\n';
				cJSON_Delete(root);
				return false;
			}

			if (!ReadStyle(item, style))
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

bool GoogleFusionTablesInterface::DeleteStyle(const std::string& tableId, const unsigned int& styleId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	std::ostringstream ss;
	ss << styleId;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId + stylesEndPoint + '/' + ss.str(), response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to delete style\n";
		return false;
	}

	return true;
}

std::string GoogleFusionTablesInterface::BuildCreateStyleData(const StyleInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		std::cerr << "Failed to create style data\n";
		return std::string();
	}

	cJSON_AddStringToObject(root, nameKey.c_str(), info.name.c_str());
	cJSON_AddStringToObject(root, tableIdKey.c_str(), info.tableId.c_str());
	if (info.isDefaultForTable)
		cJSON_AddTrueToObject(root, isDefaultKey.c_str());
	else
		cJSON_AddFalseToObject(root, isDefaultKey.c_str());

	if (info.hasMarkerOptions)
		AddMarkerOptions(root, info.markerOptions);

	if (info.hasPolylineOptions)
		AddPolylineOptions(root, info.polylineOptions);

	if (info.hasPolygonOptions)
		AddPolygonOptions(root, info.polygonOptions);

	const std::string data(cJSON_Print(root));
	cJSON_Delete(root);
	return data;
}

void GoogleFusionTablesInterface::AddMarkerOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, markerOptionsKey.c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddPolylineOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, polylineOptionsKey.c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddPolygonOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, polygonOptionsKey.c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	for (const auto& option : info)
	{
		if (option.type == StyleInfo::Options::Type::String)
			cJSON_AddStringToObject(root, option.key.c_str(), option.s.c_str());
		else if (option.type == StyleInfo::Options::Type::Number)
			cJSON_AddNumberToObject(root, option.key.c_str(), option.n);
		else if (option.type == StyleInfo::Options::Type::Bool)
		{
			if (option.b)
				cJSON_AddTrueToObject(root, option.key.c_str());
			else
				cJSON_AddFalseToObject(root, option.key.c_str());
		}
		else if (option.type == StyleInfo::Options::Type::Complex)
		{
			cJSON* item(cJSON_CreateObject());
			if (!item)
				return;

			cJSON_AddItemToObject(root, option.key.c_str(), item);
			AddOptions(item, option.c);
		}
		else
			assert(false);
	}
}

bool GoogleFusionTablesInterface::ReadStyle(cJSON* root, StyleInfo& info)
{
	if (!KindMatches(root, styleSettingKindText))
	{
		std::cerr << "Kind of element is not style\n";
		return false;
	}

	if (!ReadJSON(root, isDefaultKey, info.isDefaultForTable))
		info.isDefaultForTable = false;// not a required element

	if (!ReadJSON(root, styleIdKey, info.styleId))
	{
		std::cerr << "Failed to read style id\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		std::cerr << "Failed to read table id\n";
		return false;
	}

	cJSON* markerOptions(cJSON_GetObjectItem(root, markerOptionsKey.c_str()));
	if (markerOptions)
	{
		info.hasMarkerOptions = true;
		if (!ReadOptions(markerOptions, info.markerOptions))
		{
			std::cerr << "Failed to read marker options\n";
			return false;
		}
	}
	else
		info.hasMarkerOptions = false;

	cJSON* polylineOptions(cJSON_GetObjectItem(root, polylineOptionsKey.c_str()));
	if (polylineOptions)
	{
		info.hasPolylineOptions = true;
		if (!ReadOptions(polylineOptions, info.polylineOptions))
		{
			std::cerr << "Failed to read polyline options\n";
			return false;
		}
	}
	else
		info.hasPolylineOptions = false;

	cJSON* polygonOptions(cJSON_GetObjectItem(root, polygonOptionsKey.c_str()));
	if (polygonOptions)
	{
		info.hasPolygonOptions = true;
		if (!ReadOptions(polygonOptions, info.polygonOptions))
		{
			std::cerr << "Failed to read polygon options\n";
			return false;
		}
	}
	else
		info.hasPolygonOptions = false;

	return true;
}

bool GoogleFusionTablesInterface::ReadOptions(cJSON* root, std::vector<StyleInfo::Options>& info)
{
	cJSON* next(root->child);
	while (next)
	{
		const std::string key(next->string);

		if (next->type == cJSON_Number)
			info.push_back(StyleInfo::Options(key, next->valuedouble));
		else if (next->type == cJSON_String)
			info.push_back(StyleInfo::Options(key, std::string(next->valuestring)));
		else if (next->type == cJSON_True || next->type == cJSON_False)
			info.push_back(StyleInfo::Options(key, next->valueint == 1));
		else if (next->type == cJSON_Object && key.compare(fillColorStylerKey) == 0)
		{
			if (KindMatches(next, fromColumnKindText))
			{
				std::vector<StyleInfo::Options> complexInfo;
				if (!ReadOptions(next, complexInfo))
					return false;
				info.push_back(StyleInfo::Options(key, complexInfo));
			}
			else
			{
				std::cerr << "unsupported option type:  " << next->string << '\n';
				std::cerr << cJSON_Print(next) << '\n';
				assert(false);
			}
		}
		else
		{
			std::cerr << "unsupported option type:  " << next->string << '\n';
			std::cerr << cJSON_Print(next) << '\n';
			assert(false);
		}

		next = next->next;
	}

	return true;
}

bool GoogleFusionTablesInterface::CreateTemplate(const std::string& tableId, TemplateInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + '/' + tableId + templatesEndPoint, BuildCreateTemplateData(info),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to create template\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse create template response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!ReadTemplate(root, info))
	{
		cJSON_Delete(root);
		return false;
	}

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::ListTemplates(const std::string& tableId, std::vector<TemplateInfo>& templates)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId + templatesEndPoint, response, AddAuthToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to request template list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		std::cerr << "Failed to parse template list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, templateListKindText))
	{
		std::cerr << "Recieved unexpected kind in response to request for template list\n";
		cJSON_Delete(root);
		return false;
	}

	templates.clear();
	cJSON* items(cJSON_GetObjectItem(root, itemsKey.c_str()));
	if (items)// May not be items, if no templates exist
	{
		templates.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& t : templates)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				std::cerr << "Failed to get info for template " << i << '\n';
				cJSON_Delete(root);
				return false;
			}

			if (!ReadTemplate(item, t))
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

bool GoogleFusionTablesInterface::DeleteTemplate(const std::string& tableId, const unsigned int& templateId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	std::ostringstream ss;
	ss << templateId;
	if (!DoCURLGet(apiRoot + tablesEndPoint + '/' + tableId + templatesEndPoint + '/' + ss.str(), response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		std::cerr << "Failed to delete template\n";
		return false;
	}

	return true;
}

std::string GoogleFusionTablesInterface::BuildCreateTemplateData(const TemplateInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		std::cerr << "Failed to create template data\n";
		return std::string();
	}

	cJSON_AddStringToObject(root, nameKey.c_str(), info.name.c_str());
	cJSON_AddStringToObject(root, tableIdKey.c_str(), info.tableId.c_str());
	cJSON_AddStringToObject(root, bodyKey.c_str(), info.body.c_str());
	if (info.isDefaultForTable)
		cJSON_AddTrueToObject(root, isDefaultKey.c_str());
	else
		cJSON_AddFalseToObject(root, isDefaultKey.c_str());

	const std::string data(cJSON_Print(root));
	cJSON_Delete(root);
	return data;
}

bool GoogleFusionTablesInterface::ReadTemplate(cJSON* root, TemplateInfo& info)
{
	if (!KindMatches(root, templateKindText))
	{
		std::cerr << "Kind of element is not template\n";
		return false;
	}

	if (!ReadJSON(root, isDefaultKey, info.isDefaultForTable))
		info.isDefaultForTable = false;// not a required element

	if (!ReadJSON(root, templateIdKey, info.templateId))
	{
		std::cerr << "Failed to read template id\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		std::cerr << "Failed to read table id\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		std::cerr << "Failed to read template name\n";
		return false;
	}

	if (!ReadJSON(root, bodyKey, info.body))
	{
		std::cerr << "Failed to read template body\n";
		return false;
	}

	return true;
}
