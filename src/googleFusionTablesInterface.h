// File:  googleFusionTablesInterface.h
// Date:  1/7/2018
// Auth:  K. Loux
// Desc:  Interface to Google's Fusion Tables web API.

#ifndef GOOGLE_FUSION_TABLES_INTERFACE_H_
#define GOOGLE_FUSION_TABLES_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"

// Standard C++ headers
#include <memory>

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
			enum class ColumnType
			{
				String,
				Number,
				DateTime,
				Location
			};

			ColumnInfo() = default;
			ColumnInfo(const std::string& name, const ColumnType& type) : name(name), type(type) {}
			unsigned int columnId;
			std::string name;

			ColumnType type;
		};

		std::vector<ColumnInfo> columns;
	};

	typedef TableInfo::ColumnInfo ColumnInfo;
	typedef ColumnInfo::ColumnType ColumnType;

	bool CreateTable(TableInfo& info);
	bool ListTables(std::vector<TableInfo>& tables);
	bool DeleteTable(const std::string& tableId);
	bool CopyTable(const std::string& tableId, TableInfo& info);
	bool Import(const std::string& tableId, const std::string& csvData);
	bool ListColumns(const std::string& tableId,
		std::vector<TableInfo::ColumnInfo>& columnInfo);
	bool DeleteAllRows(const std::string& tableId);

	struct StyleInfo
	{
		std::string tableId;
		unsigned int styleId;
		std::string name;
		bool isDefaultForTable;

		struct Options
		{
			enum class Type
			{
				String,
				Number,
				Bool,
				Complex
			};

			Options() = default;
			Options(const std::string& key, const std::string& s) : key(key), type(Type::String), s(s) {}
			Options(const std::string& key, const double& n) : key(key), type(Type::Number), n(n) {}
			Options(const std::string& key, const bool& b) : key(key), type(Type::Bool), b(b) {}
			Options(const std::string& key, const std::vector<Options>& c) : key(key), type(Type::Complex), c(c) {}

			std::string key;
			Type type;

			std::string s;
			double n;
			bool b;
			std::vector<Options> c;
		};

		std::vector<Options> markerOptions;
		bool hasMarkerOptions = false;

		std::vector<Options> polylineOptions;
		bool hasPolylineOptions = false;

		std::vector<Options> polygonOptions;
		bool hasPolygonOptions = false;
	};

	bool CreateStyle(const std::string& tableId, StyleInfo& info);
	bool ListStyles(const std::string& tableId, std::vector<StyleInfo>& styles);
	bool DeleteStyle(const std::string& tableId, const unsigned int& styleId);

	struct TemplateInfo
	{
		std::string tableId;
		unsigned int templateId;
		std::string name;
		std::string body;
		bool isDefaultForTable;
	};

	bool CreateTemplate(const std::string& tableId, TemplateInfo& info);
	bool ListTemplates(const std::string& tableId, std::vector<TemplateInfo>& templates);
	bool DeleteTemplate(const std::string& tableId, const unsigned int& templateId);

	enum class TableAccess
	{
		Public,
		Private,
		Unlisted
	};

	bool SetTableAccess(const std::string& tableId, const TableAccess& access);

	bool SubmitQuery(const std::string& query, cJSON*& root);

private:
	static const std::string apiRoot;
	static const std::string apiRootUpload;
	static const std::string tablesEndPoint;
	static const std::string importEndPoint;
	static const std::string columnsEndPoint;
	static const std::string stylesEndPoint;
	static const std::string templatesEndPoint;
	static const std::string copyEndPoint;
	static const std::string queryEndPoint;
	static const std::string tableListKindText;
	static const std::string tableKindText;
	static const std::string columnKindText;
	static const std::string columnListKindText;
	static const std::string importKindText;
	static const std::string queryResponseKindText;
	static const std::string styleSettingListText;
	static const std::string styleSettingKindText;
	static const std::string templateListKindText;
	static const std::string templateKindText;
	static const std::string fromColumnKindText;
	static const std::string itemsKey;
	static const std::string kindKey;
	static const std::string tableIdKey;
	static const std::string styleIdKey;
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
	static const std::string isDefaultKey;
	static const std::string markerOptionsKey;
	static const std::string polylineOptionsKey;
	static const std::string polygonOptionsKey;
	static const std::string columnNameKey;
	static const std::string fillColorStylerKey;
	static const std::string templateIdKey;
	static const std::string bodyKey;

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

	static std::string BuildCreateStyleData(const StyleInfo& info);
	static void AddMarkerOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolylineOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolygonOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);

	static bool ReadStyle(cJSON* root, StyleInfo& info);
	static bool ReadOptions(cJSON* root, std::vector<StyleInfo::Options>& info);

	static std::string BuildCreateTemplateData(const TemplateInfo& info);
	static bool ReadTemplate(cJSON* root, TemplateInfo& info);
};

#endif// GOOGLE_FUSION_TABLES_INTERFACE_H_
