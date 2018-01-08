// File:  googleFusionTablesInterface.h
// Date:  1/7/2018
// Auth:  K. Loux
// Desc:  Interface to Google's Fusion Tables web API.

#ifndef GOOGLE_FUSION_TABLES_INTERFACE_H_
#define GOOGLE_FUSION_TABLES_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"

class GoogleFusionTablesInterface : public JSONInterface
{
public:
	GoogleFusionTablesInterface(const std::string& userAgent,
		const std::string& oAuthClientId, const std::string& oAuthClientSecret);

	struct TableInfo
	{
		std::string description;
		std::string name;
		std::string tableId;
		bool isExportable;

		struct ColumnInfo
		{
			unsigned int columnId;
			std::string name;

			enum class ColumnType
			{
				String,
				Number,
				DateTime,
				Location
			};

			ColumnType type;
		};

		std::vector<ColumnInfo> columns;
	};

	bool CreateTable(TableInfo& info);
	bool ListTables(std::vector<TableInfo>& tables);
	bool DeleteTable(const std::string& tableId);
	bool CopyTable(const std::string& tableId, TableInfo& info);
	bool Import(const std::string& tableId, const std::string& csvData);
	bool ListColumns(const std::string& tableId,
		std::vector<TableInfo::ColumnInfo>& columnInfo);

private:
	static const std::string apiRoot;
	static const std::string apiRootUpload;
	static const std::string tablesEndPoint;
	static const std::string importEndPoint;
	static const std::string columnsEndPoint;
	static const std::string copyEndPoint;
	static const std::string tableListKindText;
	static const std::string tableKindText;
	static const std::string columnKindText;
	static const std::string columnListKindText;
	static const std::string importKindText;
	static const std::string itemsKey;
	static const std::string kindKey;
	static const std::string tableIdKey;
	static const std::string nameKey;
	static const std::string columnIdKey;
	static const std::string columnsKey;
	static const std::string typeKey;
	static const std::string descriptionKey;
	static const std::string isExportableKey;
	static const std::string errorKey;
	static const std::string codeKey;
	static const std::string messageKey;
	static const std::string numberOfRowsImportedKey;

	static const std::string typeStringText;
	static const std::string typeNumberText;
	static const std::string typeDateTimeText;
	static const std::string typeLocationText;

	static const std::string fusionTableRefreshTokenFileName;

	struct AuthTokenData : public ModificationData
	{
		AuthTokenData(const std::string& authToken) : authToken(authToken) {}
		std::string authToken;
	};

	static bool ResponseHasError(cJSON* root);
	static bool KindMatches(cJSON* root, const std::string& kind);
	static bool AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndJSONContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndOctetContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData

	static bool ReadTable(cJSON* root, TableInfo& info);
	static bool ReadColumn(cJSON* root, TableInfo::ColumnInfo& info);

	static std::string BuildCreateTableData(const TableInfo& info);
	static cJSON* BuildColumnItem(const TableInfo::ColumnInfo& columnInfo);
	static std::string GetColumnTypeString(const TableInfo::ColumnInfo::ColumnType& type);
	static TableInfo::ColumnInfo::ColumnType GetColumnTypeFromString(const std::string& s);
};

#endif// GOOGLE_FUSION_TABLES_INTERFACE_H_
