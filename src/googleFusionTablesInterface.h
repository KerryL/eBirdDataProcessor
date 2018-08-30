// File:  googleFusionTablesInterface.h
// Date:  1/7/2018
// Auth:  K. Loux
// Desc:  Interface to Google's Fusion Tables web API.

#ifndef GOOGLE_FUSION_TABLES_INTERFACE_H_
#define GOOGLE_FUSION_TABLES_INTERFACE_H_

// Local headers
#include "email/jsonInterface.h"
#include "utilities/uString.h"

// Standard C++ headers
#include <memory>

class GoogleFusionTablesInterface : public JSONInterface
{
public:
	GoogleFusionTablesInterface(const UString::String& userAgent,
		const UString::String& oAuthClientId, const UString::String& oAuthClientSecret);

	static const unsigned int writeRequestRateLimit;// [requests per minute]

	struct TableInfo
	{
		UString::String description;
		UString::String name;
		UString::String tableId;
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
			ColumnInfo(const UString::String& name, const ColumnType& type) : name(name), type(type) {}
			unsigned int columnId;
			UString::String name;

			ColumnType type;
		};

		std::vector<ColumnInfo> columns;
	};

	typedef TableInfo::ColumnInfo ColumnInfo;
	typedef ColumnInfo::ColumnType ColumnType;

	bool CreateTable(TableInfo& info);
	bool ListTables(std::vector<TableInfo>& tables);
	bool DeleteTable(const UString::String& tableId);
	bool CopyTable(const UString::String& tableId, TableInfo& info);
	bool Import(const UString::String& tableId, const UString::String& csvData);
	bool ListColumns(const UString::String& tableId,
		std::vector<TableInfo::ColumnInfo>& columnInfo);
	bool DeleteAllRows(const UString::String& tableId);
	bool DeleteRow(const UString::String& tableId, const unsigned int& rowId);
	bool DeleteRows(const UString::String& tableId, const std::vector<unsigned int>& rowIds);

	struct StyleInfo
	{
		UString::String tableId;
		unsigned int styleId;
		UString::String name;
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
			Options(const UString::String& key, const UString::String& s) : key(key), type(Type::String), s(s) {}
			Options(const UString::String& key, const double& n) : key(key), type(Type::Number), n(n) {}
			Options(const UString::String& key, const bool& b) : key(key), type(Type::Bool), b(b) {}
			Options(const UString::String& key, const std::vector<Options>& c) : key(key), type(Type::Complex), c(c) {}

			UString::String key;
			Type type;

			UString::String s;
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

	bool CreateStyle(const UString::String& tableId, StyleInfo& info);
	bool ListStyles(const UString::String& tableId, std::vector<StyleInfo>& styles);
	bool DeleteStyle(const UString::String& tableId, const unsigned int& styleId);

	struct TemplateInfo
	{
		UString::String tableId;
		unsigned int templateId;
		UString::String name;
		UString::String body;
		bool isDefaultForTable;
	};

	bool CreateTemplate(const UString::String& tableId, TemplateInfo& info);
	bool ListTemplates(const UString::String& tableId, std::vector<TemplateInfo>& templates);
	bool DeleteTemplate(const UString::String& tableId, const unsigned int& templateId);

	enum class TableAccess
	{
		Public,
		Private,
		Unlisted
	};

	bool SetTableAccess(const UString::String& tableId, const TableAccess& access);

	bool SubmitQuery(const UString::String& query, cJSON*& root, UString::String* csvData = nullptr);
	bool SubmitQueryMediaDownload(const UString::String& query, UString::String& csvData);

private:
	static const UString::String apiRoot;
	static const UString::String apiRootUpload;
	static const UString::String tablesEndPoint;
	static const UString::String importEndPoint;
	static const UString::String columnsEndPoint;
	static const UString::String stylesEndPoint;
	static const UString::String templatesEndPoint;
	static const UString::String copyEndPoint;
	static const UString::String queryEndPoint;
	static const UString::String tableListKindText;
	static const UString::String tableKindText;
	static const UString::String columnKindText;
	static const UString::String columnListKindText;
	static const UString::String importKindText;
	static const UString::String queryResponseKindText;
	static const UString::String styleSettingListText;
	static const UString::String styleSettingKindText;
	static const UString::String templateListKindText;
	static const UString::String templateKindText;
	static const UString::String fromColumnKindText;
	static const UString::String itemsKey;
	static const UString::String kindKey;
	static const UString::String tableIdKey;
	static const UString::String styleIdKey;
	static const UString::String nameKey;
	static const UString::String columnIdKey;
	static const UString::String columnsKey;
	static const UString::String typeKey;
	static const UString::String descriptionKey;
	static const UString::String isExportableKey;
	static const UString::String errorKey;
	static const UString::String codeKey;
	static const UString::String messageKey;
	static const UString::String numberOfRowsImportedKey;
	static const UString::String isDefaultKey;
	static const UString::String markerOptionsKey;
	static const UString::String polylineOptionsKey;
	static const UString::String polygonOptionsKey;
	static const UString::String columnNameKey;
	static const UString::String fillColorStylerKey;
	static const UString::String templateIdKey;
	static const UString::String bodyKey;

	static const UString::String typeStringText;
	static const UString::String typeNumberText;
	static const UString::String typeDateTimeText;
	static const UString::String typeLocationText;

	static const UString::String fusionTableRefreshTokenFileName;

	struct AuthTokenData : public ModificationData
	{
		AuthTokenData(const UString::String& authToken) : authToken(authToken) {}
		UString::String authToken;
	};

	static bool ResponseHasError(cJSON* root);
	static bool ResponseTooLarge(cJSON* root);
	static bool KindMatches(cJSON* root, const UString::String& kind);
	static bool AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndJSONContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndOctetContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData

	static bool ReadTable(cJSON* root, TableInfo& info);
	static bool ReadColumn(cJSON* root, TableInfo::ColumnInfo& info);

	static UString::String BuildCreateTableData(const TableInfo& info);
	static cJSON* BuildColumnItem(const TableInfo::ColumnInfo& columnInfo);
	static UString::String GetColumnTypeString(const TableInfo::ColumnInfo::ColumnType& type);
	static TableInfo::ColumnInfo::ColumnType GetColumnTypeFromString(const UString::String& s);

	static UString::String BuildCreateStyleData(const StyleInfo& info);
	static void AddMarkerOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolylineOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolygonOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);

	static bool ReadStyle(cJSON* root, StyleInfo& info);
	static bool ReadOptions(cJSON* root, std::vector<StyleInfo::Options>& info);

	static UString::String BuildCreateTemplateData(const TemplateInfo& info);
	static bool ReadTemplate(cJSON* root, TemplateInfo& info);
};

#endif// GOOGLE_FUSION_TABLES_INTERFACE_H_
