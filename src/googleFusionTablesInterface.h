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
	GoogleFusionTablesInterface(const String& userAgent,
		const String& oAuthClientId, const String& oAuthClientSecret);

	static const unsigned int writeRequestRateLimit;// [requests per minute]

	struct TableInfo
	{
		String description;
		String name;
		String tableId;
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
			ColumnInfo(const String& name, const ColumnType& type) : name(name), type(type) {}
			unsigned int columnId;
			String name;

			ColumnType type;
		};

		std::vector<ColumnInfo> columns;
	};

	typedef TableInfo::ColumnInfo ColumnInfo;
	typedef ColumnInfo::ColumnType ColumnType;

	bool CreateTable(TableInfo& info);
	bool ListTables(std::vector<TableInfo>& tables);
	bool DeleteTable(const String& tableId);
	bool CopyTable(const String& tableId, TableInfo& info);
	bool Import(const String& tableId, const String& csvData);
	bool ListColumns(const String& tableId,
		std::vector<TableInfo::ColumnInfo>& columnInfo);
	bool DeleteAllRows(const String& tableId);
	bool DeleteRow(const String& tableId, const unsigned int& rowId);
	bool DeleteRows(const String& tableId, const std::vector<unsigned int>& rowIds);

	struct StyleInfo
	{
		String tableId;
		unsigned int styleId;
		String name;
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
			Options(const String& key, const String& s) : key(key), type(Type::String), s(s) {}
			Options(const String& key, const double& n) : key(key), type(Type::Number), n(n) {}
			Options(const String& key, const bool& b) : key(key), type(Type::Bool), b(b) {}
			Options(const String& key, const std::vector<Options>& c) : key(key), type(Type::Complex), c(c) {}

			String key;
			Type type;

			String s;
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

	bool CreateStyle(const String& tableId, StyleInfo& info);
	bool ListStyles(const String& tableId, std::vector<StyleInfo>& styles);
	bool DeleteStyle(const String& tableId, const unsigned int& styleId);

	struct TemplateInfo
	{
		String tableId;
		unsigned int templateId;
		String name;
		String body;
		bool isDefaultForTable;
	};

	bool CreateTemplate(const String& tableId, TemplateInfo& info);
	bool ListTemplates(const String& tableId, std::vector<TemplateInfo>& templates);
	bool DeleteTemplate(const String& tableId, const unsigned int& templateId);

	enum class TableAccess
	{
		Public,
		Private,
		Unlisted
	};

	bool SetTableAccess(const String& tableId, const TableAccess& access);

	bool SubmitQuery(const String& query, cJSON*& root, String* csvData = nullptr);
	bool SubmitQueryMediaDownload(const String& query, String& csvData);

private:
	static const String apiRoot;
	static const String apiRootUpload;
	static const String tablesEndPoint;
	static const String importEndPoint;
	static const String columnsEndPoint;
	static const String stylesEndPoint;
	static const String templatesEndPoint;
	static const String copyEndPoint;
	static const String queryEndPoint;
	static const String tableListKindText;
	static const String tableKindText;
	static const String columnKindText;
	static const String columnListKindText;
	static const String importKindText;
	static const String queryResponseKindText;
	static const String styleSettingListText;
	static const String styleSettingKindText;
	static const String templateListKindText;
	static const String templateKindText;
	static const String fromColumnKindText;
	static const String itemsKey;
	static const String kindKey;
	static const String tableIdKey;
	static const String styleIdKey;
	static const String nameKey;
	static const String columnIdKey;
	static const String columnsKey;
	static const String typeKey;
	static const String descriptionKey;
	static const String isExportableKey;
	static const String errorKey;
	static const String codeKey;
	static const String messageKey;
	static const String numberOfRowsImportedKey;
	static const String isDefaultKey;
	static const String markerOptionsKey;
	static const String polylineOptionsKey;
	static const String polygonOptionsKey;
	static const String columnNameKey;
	static const String fillColorStylerKey;
	static const String templateIdKey;
	static const String bodyKey;

	static const String typeStringText;
	static const String typeNumberText;
	static const String typeDateTimeText;
	static const String typeLocationText;

	static const String fusionTableRefreshTokenFileName;

	struct AuthTokenData : public ModificationData
	{
		AuthTokenData(const String& authToken) : authToken(authToken) {}
		String authToken;
	};

	static bool ResponseHasError(cJSON* root);
	static bool ResponseTooLarge(cJSON* root);
	static bool KindMatches(cJSON* root, const String& kind);
	static bool AddAuthAndDeleteToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndJSONContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData
	static bool AddAuthAndOctetContentTypeToCurlHeader(CURL* curl, const ModificationData* data);// Expects AuthTokenData

	static bool ReadTable(cJSON* root, TableInfo& info);
	static bool ReadColumn(cJSON* root, TableInfo::ColumnInfo& info);

	static String BuildCreateTableData(const TableInfo& info);
	static cJSON* BuildColumnItem(const TableInfo::ColumnInfo& columnInfo);
	static String GetColumnTypeString(const TableInfo::ColumnInfo::ColumnType& type);
	static TableInfo::ColumnInfo::ColumnType GetColumnTypeFromString(const String& s);

	static String BuildCreateStyleData(const StyleInfo& info);
	static void AddMarkerOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolylineOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddPolygonOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);
	static void AddOptions(cJSON* root, const std::vector<StyleInfo::Options>& info);

	static bool ReadStyle(cJSON* root, StyleInfo& info);
	static bool ReadOptions(cJSON* root, std::vector<StyleInfo::Options>& info);

	static String BuildCreateTemplateData(const TemplateInfo& info);
	static bool ReadTemplate(cJSON* root, TemplateInfo& info);
};

#endif// GOOGLE_FUSION_TABLES_INTERFACE_H_
