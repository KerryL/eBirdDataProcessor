// File:  googleFusionTablesInterface.h
// Date:  1/7/2018
// Auth:  K. Loux
// Desc:  Interface to Google's Fusion Tables web API.

// Local headers
#include "googleFusionTablesInterface.h"
#include "email/oAuth2Interface.h"
#include "email/curlUtilities.h"

// Standard C++ headers
#include <cassert>

const UString::String GoogleFusionTablesInterface::apiRoot(_T("https://www.googleapis.com/fusiontables/v2/"));
const UString::String GoogleFusionTablesInterface::apiRootUpload(_T("https://www.googleapis.com/upload/fusiontables/v2/"));
const UString::String GoogleFusionTablesInterface::tablesEndPoint(_T("tables"));
const UString::String GoogleFusionTablesInterface::queryEndPoint(_T("query"));
const UString::String GoogleFusionTablesInterface::importEndPoint(_T("/import"));
const UString::String GoogleFusionTablesInterface::columnsEndPoint(_T("/columns"));
const UString::String GoogleFusionTablesInterface::stylesEndPoint(_T("/styles"));
const UString::String GoogleFusionTablesInterface::templatesEndPoint(_T("/templates"));
const UString::String GoogleFusionTablesInterface::copyEndPoint(_T("/copy"));
const UString::String GoogleFusionTablesInterface::tableListKindText(_T("fusiontables#tableList"));
const UString::String GoogleFusionTablesInterface::tableKindText(_T("fusiontables#table"));
const UString::String GoogleFusionTablesInterface::columnKindText(_T("fusiontables#column"));
const UString::String GoogleFusionTablesInterface::importKindText(_T("fusiontables#import"));
const UString::String GoogleFusionTablesInterface::queryResponseKindText(_T("fusiontables#sqlresponse"));
const UString::String GoogleFusionTablesInterface::styleSettingListText(_T("fusiontables#styleSettingList"));
const UString::String GoogleFusionTablesInterface::styleSettingKindText(_T("fusiontables#styleSetting"));
const UString::String GoogleFusionTablesInterface::columnListKindText(_T("fusiontables#columnList"));
const UString::String GoogleFusionTablesInterface::fromColumnKindText(_T("fusiontables#fromColumn"));
const UString::String GoogleFusionTablesInterface::templateListKindText(_T("fusiontables#templateList"));
const UString::String GoogleFusionTablesInterface::templateKindText(_T("fusiontables#template"));
const UString::String GoogleFusionTablesInterface::itemsKey(_T("items"));
const UString::String GoogleFusionTablesInterface::kindKey(_T("kind"));
const UString::String GoogleFusionTablesInterface::tableIdKey(_T("tableId"));
const UString::String GoogleFusionTablesInterface::styleIdKey(_T("styleId"));
const UString::String GoogleFusionTablesInterface::nameKey(_T("name"));
const UString::String GoogleFusionTablesInterface::columnIdKey(_T("columnId"));
const UString::String GoogleFusionTablesInterface::columnsKey(_T("columns"));
const UString::String GoogleFusionTablesInterface::typeKey(_T("type"));
const UString::String GoogleFusionTablesInterface::descriptionKey(_T("description"));
const UString::String GoogleFusionTablesInterface::isExportableKey(_T("isExportable"));
const UString::String GoogleFusionTablesInterface::errorKey(_T("error"));
const UString::String GoogleFusionTablesInterface::codeKey(_T("code"));
const UString::String GoogleFusionTablesInterface::messageKey(_T("message"));
const UString::String GoogleFusionTablesInterface::numberOfRowsImportedKey(_T("numRowsReceived"));
const UString::String GoogleFusionTablesInterface::columnNameKey(_T("columnName"));
const UString::String GoogleFusionTablesInterface::fillColorStylerKey(_T("fillColorStyler"));
const UString::String GoogleFusionTablesInterface::templateIdKey(_T("templateId"));
const UString::String GoogleFusionTablesInterface::bodyKey(_T("body"));

const UString::String GoogleFusionTablesInterface::isDefaultKey(_T("isDefaultForTable"));
const UString::String GoogleFusionTablesInterface::markerOptionsKey(_T("markerOptions"));
const UString::String GoogleFusionTablesInterface::polylineOptionsKey(_T("polylineOptions"));
const UString::String GoogleFusionTablesInterface::polygonOptionsKey(_T("polygonOptions"));

const UString::String GoogleFusionTablesInterface::typeStringText(_T("STRING"));
const UString::String GoogleFusionTablesInterface::typeNumberText(_T("NUMBER"));
const UString::String GoogleFusionTablesInterface::typeDateTimeText(_T("DATETIME"));
const UString::String GoogleFusionTablesInterface::typeLocationText(_T("LOCATION"));

const UString::String GoogleFusionTablesInterface::fusionTableRefreshTokenFileName(_T("~ftToken"));

const unsigned int GoogleFusionTablesInterface::writeRequestRateLimit(25);// [requests per minute] (actual limit is 30/min)

GoogleFusionTablesInterface::GoogleFusionTablesInterface(
	const UString::String& userAgent, const UString::String& oAuthClientId,
	const UString::String& oAuthClientSecret) : JSONInterface(userAgent)
{
	OAuth2Interface::Get().SetClientID(oAuthClientId);
	OAuth2Interface::Get().SetClientSecret(oAuthClientSecret);
	OAuth2Interface::Get().SetScope(_T("https://www.googleapis.com/auth/fusiontables"));
	OAuth2Interface::Get().SetAuthenticationURL(_T("https://accounts.google.com/o/oauth2/auth"));
	OAuth2Interface::Get().SetResponseType(_T("code"));
	OAuth2Interface::Get().SetRedirectURI(_T("oob"));
	OAuth2Interface::Get().SetGrantType(_T("authorization_code"));
	OAuth2Interface::Get().SetTokenURL(_T("https://accounts.google.com/o/oauth2/token"));

	{
		UString::IFStream tokenInFile(fusionTableRefreshTokenFileName.c_str());
		if (tokenInFile.good() && tokenInFile.is_open())
		{
			UString::String token;
			tokenInFile >> token;
			OAuth2Interface::Get().SetRefreshToken(token);
		}
	}

	if (OAuth2Interface::Get().GetRefreshToken().empty())
		OAuth2Interface::Get().SetRefreshToken(UString::String());// This is how a new refresh token is requested

	if (!OAuth2Interface::Get().GetRefreshToken().empty())
	{
		UString::OFStream tokenOutFile(fusionTableRefreshTokenFileName.c_str());
		tokenOutFile << OAuth2Interface::Get().GetRefreshToken();
	}
}

bool GoogleFusionTablesInterface::CreateTable(TableInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint, UString::ToNarrowString(BuildCreateTableData(info)),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to create table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse create table response\n";
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
		Cerr << "Failed to request table list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse table list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, tableListKindText))
	{
		Cerr << "Recieved unexpected kind in response to request for table list\n";
		cJSON_Delete(root);
		return false;
	}

	// TODO:  Add support for nextPageToken in order to support long result lists

	tables.clear();
	cJSON* items(cJSON_GetObjectItem(root, UString::ToNarrowString(itemsKey).c_str()));
	if (items)// May not be items, if no tables exist
	{
		tables.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& table : tables)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				Cerr << "Failed to get info for table " << i << '\n';
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

bool GoogleFusionTablesInterface::DeleteTable(const UString::String& tableId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId, response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to delete table\n";
		return false;
	}

	return true;
}

bool GoogleFusionTablesInterface::CopyTable(const UString::String& tableId, TableInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + UString::Char('/') + tableId + copyEndPoint, std::string(),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to copy table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse copy table response\n";
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

bool GoogleFusionTablesInterface::Import(const UString::String& tableId,
	const UString::String& csvData)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRootUpload + tablesEndPoint + UString::Char('/') + tableId + importEndPoint + _T("?uploadType=media"), UString::ToNarrowString(csvData),
		response, AddAuthAndOctetContentTypeToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to import data to table\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse copy import response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, importKindText))
	{
		Cerr << "Received unexpected kind in import response\n";
		cJSON_Delete(root);
		return false;
	}

	UString::String rowsString;
	if (!ReadJSON(root, numberOfRowsImportedKey, rowsString))
	{
		Cerr << "Failed to check number of imported rows\n";
		cJSON_Delete(root);
		return false;
	}

	Cout << "Successfully imported " << rowsString << " new rows into table " << tableId << std::endl;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::ListColumns(const UString::String& tableId,
	std::vector<TableInfo::ColumnInfo>& columnInfo)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId + columnsEndPoint,
		response, AddAuthToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to request column list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse column list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, columnListKindText))
	{
		Cerr << "Recieved unexpected kind in response to request for column list\n";
		cJSON_Delete(root);
		return false;
	}

	// TODO:  Add support for nextPageToken in order to support long result lists (does this apply here?)

	cJSON* items(cJSON_GetObjectItem(root, UString::ToNarrowString(itemsKey).c_str()));
	if (items)// May not be items, if no tables exist
	{
		columnInfo.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& column : columnInfo)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				Cerr << "Failed to get info for column " << i << '\n';
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
	cJSON* error(cJSON_GetObjectItem(root, UString::ToNarrowString(errorKey).c_str()));
	if (!error)
		return false;

	unsigned int errorCode;
	UString::String errorMessage;
	if (ReadJSON(error, codeKey, errorCode) && ReadJSON(error, messageKey, errorMessage))
		Cerr << "Response contains error " << errorCode << ":  " << errorMessage << '\n';
	else
		Cerr << "Response contains unknown error\n";

	return true;
}

bool GoogleFusionTablesInterface::ResponseTooLarge(cJSON* root)
{
	cJSON* error(cJSON_GetObjectItem(root, UString::ToNarrowString(errorKey).c_str()));
	if (!error)
		return false;

	unsigned int errorCode;
	UString::String errorMessage;
	if (ReadJSON(error, codeKey, errorCode) && ReadJSON(error, messageKey, errorMessage))
	{
		if (errorCode == 503 && errorMessage.find(_T("Please use media download.")) != std::string::npos)
			return true;
	}

	return false;
}

bool GoogleFusionTablesInterface::KindMatches(cJSON* root, const UString::String& kind)
{
	UString::String kindString;
	if (!ReadJSON(root, kindKey, kindString))
		return false;

	return kindString.compare(kind) == 0;
}

bool GoogleFusionTablesInterface::AddAuthToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, UString::ToNarrowString(_T("Authorization: Bearer ")
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		Cerr << "Failed to append auth token to header in ListTables\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList,UString::ToNarrowString(_T("Authorization: Bearer ")
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		Cerr << "Failed to append auth token to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_CUSTOMREQUEST, "DELETE"), _T("Failed to set request type to DELETE")))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndJSONContentTypeToCurlHeader(
	CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, UString::ToNarrowString(_T("Authorization: Bearer ")
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		Cerr << "Failed to append auth token to header\n";
		return false;
	}

	headerList = curl_slist_append(headerList, "Content-Type: application/json");
	if (!headerList)
	{
		Cerr << "Failed to append content type to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::AddAuthAndOctetContentTypeToCurlHeader(
	CURL* curl, const ModificationData* data)
{
	curl_slist* headerList(nullptr);
	headerList = curl_slist_append(headerList, UString::ToNarrowString(_T("Authorization: Bearer ")
		+ static_cast<const AuthTokenData*>(data)->authToken).c_str());
	if (!headerList)
	{
		Cerr << "Failed to append auth token to header\n";
		return false;
	}

	headerList = curl_slist_append(headerList, "Content-Type: application/octet-stream");
	if (!headerList)
	{
		Cerr << "Failed to append content type to header\n";
		return false;
	}

	if (CURLUtilities::CURLCallHasError(curl_easy_setopt(curl,
		CURLOPT_HTTPHEADER, headerList), _T("Failed to set header")))
		return false;

	return true;
}

bool GoogleFusionTablesInterface::ReadTable(cJSON* root, TableInfo& info)
{
	if (!KindMatches(root, tableKindText))
	{
		Cerr << "Kind of element is not table\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		Cerr << "Failed to read table name\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		Cerr << "Failed to read table id\n";
		return false;
	}

	if (!ReadJSON(root, descriptionKey, info.description))
	{
		Cerr << "Failed to read table description\n";
		return false;
	}

	if (!ReadJSON(root, isExportableKey, info.isExportable))
	{
		Cerr << "Failed to read 'is exportable' flag\n";
		return false;
	}

	cJSON* columnArray(cJSON_GetObjectItem(root, UString::ToNarrowString(columnsKey).c_str()));
	if (!columnArray)
	{
		Cerr << "Failed to read columns array\n";
		return false;
	}

	info.columns.resize(cJSON_GetArraySize(columnArray));
	unsigned int i(0);
	for (auto& column : info.columns)
	{
		cJSON* item(cJSON_GetArrayItem(columnArray, i));
		if (!item)
		{
			Cerr << "Failed to get column " << i << '\n';
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
		Cerr << "Kind of element is not column\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		Cerr << "Failed to read column name\n";
		return false;
	}

	UString::String typeString;
	if (!ReadJSON(root, typeKey, typeString))
	{
		Cerr << "Failed to read column type\n";
		return false;
	}
	info.type = GetColumnTypeFromString(typeString);

	if (!ReadJSON(root, columnIdKey, info.columnId))
	{
		Cerr << "Failed to read column name\n";
		return false;
	}

	return true;
}

UString::String GoogleFusionTablesInterface::BuildCreateTableData(const TableInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		Cerr << "Failed to create table data\n";
		return UString::String();
	}

	cJSON_AddStringToObject(root, UString::ToNarrowString(nameKey).c_str(), UString::ToNarrowString(info.name).c_str());
	if (info.isExportable)
		cJSON_AddTrueToObject(root, UString::ToNarrowString(isExportableKey).c_str());
	else
		cJSON_AddFalseToObject(root, UString::ToNarrowString(isExportableKey).c_str());

	if (!info.description.empty())
		cJSON_AddStringToObject(root, UString::ToNarrowString(descriptionKey).c_str(), UString::ToNarrowString(info.description).c_str());

	cJSON* columns(cJSON_CreateArray());
	if (!columns)
	{
		Cerr << "Failed to create column array\n";
		cJSON_Delete(root);
		return UString::String();
	}

	cJSON_AddItemToObject(root, UString::ToNarrowString(columnsKey).c_str(), columns);

	for (const auto& column : info.columns)
	{
		cJSON* columnItem(BuildColumnItem(column));
		if (!columnItem)
		{
			Cerr << "Failed to create column information\n";
			cJSON_Delete(root);
			return UString::String();
		}
		cJSON_AddItemToArray(columns, columnItem);
	}

	const UString::String data(UString::ToStringType(cJSON_Print(root)));
	cJSON_Delete(root);
	return data;
}

cJSON* GoogleFusionTablesInterface::BuildColumnItem(const TableInfo::ColumnInfo& columnInfo)
{
	cJSON* columnItem(cJSON_CreateObject());
	if (!columnItem)
		return nullptr;

	cJSON_AddStringToObject(columnItem, UString::ToNarrowString(nameKey).c_str(), UString::ToNarrowString(columnInfo.name).c_str());
	cJSON_AddStringToObject(columnItem, UString::ToNarrowString(typeKey).c_str(), UString::ToNarrowString(GetColumnTypeString(columnInfo.type)).c_str());

	return columnItem;
}

UString::String GoogleFusionTablesInterface::GetColumnTypeString(
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

	return UString::String();
}

GoogleFusionTablesInterface::TableInfo::ColumnInfo::ColumnType
	GoogleFusionTablesInterface::GetColumnTypeFromString(const UString::String& s)
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

bool GoogleFusionTablesInterface::DeleteAllRows(const UString::String& tableId)
{
	const UString::String deleteCommand(_T("DELETE FROM ") + tableId);
	cJSON* root(nullptr);
	if (!SubmitQuery(deleteCommand, root))
		return false;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::DeleteRow(const UString::String& tableId, const unsigned int& rowId)
{
	UString::OStringStream ss;
	ss << rowId;
	const UString::String deleteCommand(_T("DELETE FROM ") + tableId + _T(" WHERE ROWID = ") + ss.str());
	cJSON* root(nullptr);
	if (!SubmitQuery(deleteCommand, root))
		return false;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::DeleteRows(const UString::String& tableId,
	const std::vector<unsigned int>& rowIds)
{
	UString::OStringStream ss;
	for (const auto& id : rowIds)
	{
		if (!ss.str().empty())
			ss << ',';
		ss << id;
	}

	const UString::String deleteCommand(_T("DELETE FROM ") + tableId + _T(" WHERE ROWID IN (") + ss.str() + _T(")"));
	cJSON* root(nullptr);
	if (!SubmitQuery(deleteCommand, root))
		return false;

	cJSON_Delete(root);
	return true;
}

bool GoogleFusionTablesInterface::SetTableAccess(const UString::String& tableId, const TableAccess& access)
{
	// TODO:  implement
	Cerr << "Setting table access permissions is not currently implemented.  You can do this"
		<< " manually by logging into Google Drive and modifying the 'Sharing' settings for the table.\n";
	return false;
}

bool GoogleFusionTablesInterface::SubmitQuery(const UString::String& query, cJSON*& root, UString::String* csvData)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + queryEndPoint + _T("?sql=") + URLEncode(query), std::string(),
		response, AddAuthToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to submit query\n";
		return false;
	}

	root = cJSON_Parse(response.c_str());
	if (!root)
	{
		Cerr << "Failed to parse query response\n";
		return false;
	}

	if (ResponseTooLarge(root) && csvData)
	{
		cJSON_Delete(root);
		root = nullptr;
		return SubmitQueryMediaDownload(query, *csvData);
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, queryResponseKindText))
	{
		Cerr << "Received unexpected kind in query response\n";
		cJSON_Delete(root);
		return false;
	}

	//cJSON_Delete(root);// Caller responsible for freeing root!
	return true;
}

bool GoogleFusionTablesInterface::SubmitQueryMediaDownload(const UString::String& query, UString::String& csvData)
{
	std::string rawResponse;
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	if (!DoCURLGet(apiRoot + queryEndPoint + _T("?alt=media&sql=") + URLEncode(query),
		rawResponse, AddAuthToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to submit query\n";
		return false;
	}

	csvData = UString::ToStringType(rawResponse);

	return true;
}

bool GoogleFusionTablesInterface::CreateStyle(const UString::String& tableId, StyleInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + UString::Char('/') + tableId + stylesEndPoint, UString::ToNarrowString(BuildCreateStyleData(info)),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to create style\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse create style response\n";
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

bool GoogleFusionTablesInterface::ListStyles(const UString::String& tableId, std::vector<StyleInfo>& styles)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId + stylesEndPoint, response, AddAuthToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to request style list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse style list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, styleSettingListText))
	{
		Cerr << "Recieved unexpected kind in response to request for style list\n";
		cJSON_Delete(root);
		return false;
	}

	styles.clear();
	cJSON* items(cJSON_GetObjectItem(root, UString::ToNarrowString(itemsKey).c_str()));
	if (items)// May not be items, if no styles exist
	{
		styles.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& style : styles)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				Cerr << "Failed to get info for style " << i << '\n';
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

bool GoogleFusionTablesInterface::DeleteStyle(const UString::String& tableId, const unsigned int& styleId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	UString::OStringStream ss;
	ss << styleId;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId + stylesEndPoint + UString::Char('/') + ss.str(), response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to delete style\n";
		return false;
	}

	return true;
}

UString::String GoogleFusionTablesInterface::BuildCreateStyleData(const StyleInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		Cerr << "Failed to create style data\n";
		return UString::String();
	}

	cJSON_AddStringToObject(root, UString::ToNarrowString(nameKey).c_str(), UString::ToNarrowString(info.name).c_str());
	cJSON_AddStringToObject(root, UString::ToNarrowString(tableIdKey).c_str(), UString::ToNarrowString(info.tableId).c_str());
	if (info.isDefaultForTable)
		cJSON_AddTrueToObject(root, UString::ToNarrowString(isDefaultKey).c_str());
	else
		cJSON_AddFalseToObject(root, UString::ToNarrowString(isDefaultKey).c_str());

	if (info.hasMarkerOptions)
		AddMarkerOptions(root, info.markerOptions);

	if (info.hasPolylineOptions)
		AddPolylineOptions(root, info.polylineOptions);

	if (info.hasPolygonOptions)
		AddPolygonOptions(root, info.polygonOptions);

	const UString::String data(UString::ToStringType(cJSON_Print(root)));
	cJSON_Delete(root);
	return data;
}

void GoogleFusionTablesInterface::AddMarkerOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, UString::ToNarrowString(markerOptionsKey).c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddPolylineOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, UString::ToNarrowString(polylineOptionsKey).c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddPolygonOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	cJSON* options(cJSON_CreateObject());
	if (!options)
		return;

	cJSON_AddItemToObject(root, UString::ToNarrowString(polygonOptionsKey).c_str(), options);
	AddOptions(options, info);
}

void GoogleFusionTablesInterface::AddOptions(cJSON* root, const std::vector<StyleInfo::Options>& info)
{
	for (const auto& option : info)
	{
		if (option.type == StyleInfo::Options::Type::String)
			cJSON_AddStringToObject(root, UString::ToNarrowString(option.key).c_str(), UString::ToNarrowString(option.s).c_str());
		else if (option.type == StyleInfo::Options::Type::Number)
			cJSON_AddNumberToObject(root, UString::ToNarrowString(option.key).c_str(), option.n);
		else if (option.type == StyleInfo::Options::Type::Bool)
		{
			if (option.b)
				cJSON_AddTrueToObject(root, UString::ToNarrowString(option.key).c_str());
			else
				cJSON_AddFalseToObject(root, UString::ToNarrowString(option.key).c_str());
		}
		else if (option.type == StyleInfo::Options::Type::Complex)
		{
			cJSON* item(cJSON_CreateObject());
			if (!item)
				return;

			cJSON_AddItemToObject(root, UString::ToNarrowString(option.key).c_str(), item);
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
		Cerr << "Kind of element is not style\n";
		return false;
	}

	if (!ReadJSON(root, isDefaultKey, info.isDefaultForTable))
		info.isDefaultForTable = false;// not a required element

	if (!ReadJSON(root, styleIdKey, info.styleId))
	{
		Cerr << "Failed to read style id\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		Cerr << "Failed to read table id\n";
		return false;
	}

	cJSON* markerOptions(cJSON_GetObjectItem(root, UString::ToNarrowString(markerOptionsKey).c_str()));
	if (markerOptions)
	{
		info.hasMarkerOptions = true;
		if (!ReadOptions(markerOptions, info.markerOptions))
		{
			Cerr << "Failed to read marker options\n";
			return false;
		}
	}
	else
		info.hasMarkerOptions = false;

	cJSON* polylineOptions(cJSON_GetObjectItem(root, UString::ToNarrowString(polylineOptionsKey).c_str()));
	if (polylineOptions)
	{
		info.hasPolylineOptions = true;
		if (!ReadOptions(polylineOptions, info.polylineOptions))
		{
			Cerr << "Failed to read polyline options\n";
			return false;
		}
	}
	else
		info.hasPolylineOptions = false;

	cJSON* polygonOptions(cJSON_GetObjectItem(root, UString::ToNarrowString(polygonOptionsKey).c_str()));
	if (polygonOptions)
	{
		info.hasPolygonOptions = true;
		if (!ReadOptions(polygonOptions, info.polygonOptions))
		{
			Cerr << "Failed to read polygon options\n";
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
		const UString::String key(UString::ToStringType(next->string));

		if (next->type == cJSON_Number)
			info.push_back(StyleInfo::Options(key, next->valuedouble));
		else if (next->type == cJSON_String)
			info.push_back(StyleInfo::Options(key, UString::ToStringType(next->valuestring)));
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
				Cerr << "unsupported option type:  " << next->string << '\n';
				Cerr << cJSON_Print(next) << '\n';
				assert(false);
			}
		}
		else
		{
			Cerr << "unsupported option type:  " << next->string << '\n';
			Cerr << cJSON_Print(next) << '\n';
			assert(false);
		}

		next = next->next;
	}

	return true;
}

bool GoogleFusionTablesInterface::CreateTemplate(const UString::String& tableId, TemplateInfo& info)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLPost(apiRoot + tablesEndPoint + UString::Char('/') + tableId + templatesEndPoint, UString::ToNarrowString(BuildCreateTemplateData(info)),
		response, AddAuthAndJSONContentTypeToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to create template\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse create template response\n";
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

bool GoogleFusionTablesInterface::ListTemplates(const UString::String& tableId, std::vector<TemplateInfo>& templates)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId + templatesEndPoint, response, AddAuthToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to request template list\n";
		return false;
	}

	cJSON* root(cJSON_Parse(response.c_str()));
	if (!root)
	{
		Cerr << "Failed to parse template list response\n";
		return false;
	}

	if (ResponseHasError(root))
	{
		cJSON_Delete(root);
		return false;
	}

	if (!KindMatches(root, templateListKindText))
	{
		Cerr << "Recieved unexpected kind in response to request for template list\n";
		cJSON_Delete(root);
		return false;
	}

	templates.clear();
	cJSON* items(cJSON_GetObjectItem(root, UString::ToNarrowString(itemsKey).c_str()));
	if (items)// May not be items, if no templates exist
	{
		templates.resize(cJSON_GetArraySize(items));
		unsigned int i(0);
		for (auto& t : templates)
		{
			cJSON* item(cJSON_GetArrayItem(items, i));
			if (!item)
			{
				Cerr << "Failed to get info for template " << i << '\n';
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

bool GoogleFusionTablesInterface::DeleteTemplate(const UString::String& tableId, const unsigned int& templateId)
{
	const AuthTokenData authTokenData(OAuth2Interface::Get().GetAccessToken());
	std::string response;
	UString::OStringStream ss;
	ss << templateId;
	if (!DoCURLGet(apiRoot + tablesEndPoint + UString::Char('/') + tableId + templatesEndPoint + UString::Char('/') + ss.str(), response, AddAuthAndDeleteToCurlHeader, &authTokenData))
	{
		Cerr << "Failed to delete template\n";
		return false;
	}

	return true;
}

UString::String GoogleFusionTablesInterface::BuildCreateTemplateData(const TemplateInfo& info)
{
	cJSON* root(cJSON_CreateObject());
	if (!root)
	{
		Cerr << "Failed to create template data\n";
		return UString::String();
	}

	cJSON_AddStringToObject(root, UString::ToNarrowString(nameKey).c_str(), UString::ToNarrowString(info.name).c_str());
	cJSON_AddStringToObject(root, UString::ToNarrowString(tableIdKey).c_str(), UString::ToNarrowString(info.tableId).c_str());
	cJSON_AddStringToObject(root, UString::ToNarrowString(bodyKey).c_str(), UString::ToNarrowString(info.body).c_str());
	if (info.isDefaultForTable)
		cJSON_AddTrueToObject(root, UString::ToNarrowString(isDefaultKey).c_str());
	else
		cJSON_AddFalseToObject(root, UString::ToNarrowString(isDefaultKey).c_str());

	const UString::String data(UString::ToStringType(cJSON_Print(root)));
	cJSON_Delete(root);
	return data;
}

bool GoogleFusionTablesInterface::ReadTemplate(cJSON* root, TemplateInfo& info)
{
	if (!KindMatches(root, templateKindText))
	{
		Cerr << "Kind of element is not template\n";
		return false;
	}

	if (!ReadJSON(root, isDefaultKey, info.isDefaultForTable))
		info.isDefaultForTable = false;// not a required element

	if (!ReadJSON(root, templateIdKey, info.templateId))
	{
		Cerr << "Failed to read template id\n";
		return false;
	}

	if (!ReadJSON(root, tableIdKey, info.tableId))
	{
		Cerr << "Failed to read table id\n";
		return false;
	}

	if (!ReadJSON(root, nameKey, info.name))
	{
		Cerr << "Failed to read template name\n";
		return false;
	}

	if (!ReadJSON(root, bodyKey, info.body))
	{
		Cerr << "Failed to read template body\n";
		return false;
	}

	return true;
}
