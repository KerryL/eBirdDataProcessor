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
	static const std::string tablesEndPoint;
	static const std::string importEndPoint;
	static const std::string columnsEndPoint;
	static const std::string tableListKindText;
	static const std::string tableKindText;
	static const std::string columnKindText;
	static const std::string columnListKindText;
	static const std::string itemsKey;
	static const std::string kindKey;
	static const std::string tableIdKey;
	static const std::string nameKey;
	static const std::string columnIdKey;
	static const std::string columnsKey;
	static const std::string typeKey;
	static const std::string descriptionKey;
	static const std::string isExportableKey;

	static const std::string typeStringText;
	static const std::string typeNumberText;
	static const std::string typeDateTimeText;
	static const std::string locationText;
};

#endif// GOOGLE_FUSION_TABLES_INTERFACE_H_
