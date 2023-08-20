#pragma once

#include"SQLite3/sqlite3.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cctype>
#include <ctime>
#include <functional>
#include <unordered_map>
#include <memory>
#include <type_traits>
using namespace std;

#include "utils.hpp"
#include "variant.hpp"
#include "non_copy.hpp"
#include"json_cpp.hpp"


typedef Variant<double, int, uint32_t, sqlite3_int64, char*, const char*, blob, string, nullptr_t> SqliteValue;

class SmartDB : NonCopyable
{
public:
	SmartDB() : m_jsonHelper(m_buf, m_code){}
	explicit SmartDB(const string& fileName) : m_dbHandle(nullptr), m_statement(nullptr), m_isConned(false), m_code(0), m_jsonHelper(m_buf, m_code)
	{
		Open(fileName);
	}

	~SmartDB()
	{
		Close();
	}

	void Open(const string& fileName)
	{
		m_code = sqlite3_open(fileName.data(), &m_dbHandle);
		if (SQLITE_OK == m_code)
		{
			m_isConned = true;
		}
	}

	bool Close()
	{
		if (m_dbHandle == nullptr)
			return true;

		sqlite3_finalize(m_statement);
		m_code = CloseDBHandle();
		bool ret = (SQLITE_OK == m_code);
		m_statement = nullptr;
		m_dbHandle = nullptr;
		return ret;
	}

	bool IsConned() const
	{
		return m_isConned;
	}

	bool Excecute(const string& sqlStr)
	{
		m_code = sqlite3_exec(m_dbHandle, sqlStr.data(), nullptr, nullptr, nullptr);
		return SQLITE_OK == m_code;
	}


	template <typename... Args>
	bool Excecute(const string& sqlStr, Args && ... args)
	{
		if (!Prepare(sqlStr))
		{
			return false;
		}

		return ExcecuteArgs(std::forward<Args>(args)...);
	}

	bool Prepare(const string& sqlStr)
	{
		m_code = sqlite3_prepare_v2(m_dbHandle, sqlStr.data(), -1, &m_statement, nullptr);
		if (m_code != SQLITE_OK)
		{
			return false;
		}

		return true;
	}


	template <typename... Args>
	bool ExcecuteArgs(Args && ... args)
	{
		if (SQLITE_OK != detail::BindParams(m_statement, 1, std::forward<Args>(args)...))
		{
			return false;
		}

		m_code = sqlite3_step(m_statement);

		sqlite3_reset(m_statement);
		return m_code == SQLITE_DONE;
	}

	template<typename Tuple>
	bool ExcecuteTuple(const string& sqlStr, Tuple&& t)
	{
		if (!Prepare(sqlStr))
		{
			return false;
		}

		m_code = detail::ExcecuteTuple(m_statement, detail::MakeIndexes<std::tuple_size<Tuple>::value>::type(), std::forward<Tuple>(t));
		return m_code == SQLITE_DONE;
	}

	bool ExcecuteJson(const string& sqlStr, const char* json)
	{
		rapidjson::Document doc;
		doc.Parse<0>(json);
		if (doc.HasParseError())
		{
			cout << doc.GetParseError() << endl;
			return false;
		}

		if (!Prepare(sqlStr))
		{
			return false;
		}
		return JsonTransaction(doc);
	}

	template < typename R = sqlite_int64, typename... Args>
	R ExecuteScalar(const string& sqlStr, Args&&... args)
	{
		if (!Prepare(sqlStr))
			return GetErrorVal<R>();

		if (SQLITE_OK != detail::BindParams(m_statement, 1, std::forward<Args>(args)...))
		{
			return false;
		}

		m_code = sqlite3_step(m_statement);

		if (m_code != SQLITE_ROW)
			return GetErrorVal<R>();

		SqliteValue val = GetValue(m_statement, 0);
		R result = val.Get<R>();// get<R>(val);
		sqlite3_reset(m_statement);
		return result;
	}

	template <typename... Args>
	std::shared_ptr<rapidjson::Document> Query(const string& query, Args&&... args)
	{
		if (!PrepareStatement(query, std::forward<Args>(args)...))
			nullptr;

		auto doc = std::make_shared<rapidjson::Document>();

		m_buf.Clear();
		m_jsonHelper.BuildJsonObject(m_statement);

		doc->Parse<0>(m_buf.GetString());

		return doc;
	}

	bool Begin()
	{
		return Excecute(BEGIN);
	}

	bool RollBack()
	{
		return Excecute(ROLLBACK);
	}

	bool Commit()
	{
		return Excecute(COMMIT);
	}

	int GetLastErrorCode()
	{
		return m_code;
	}

private:
	int CloseDBHandle()
	{
		int code = sqlite3_close(m_dbHandle);
		while (code == SQLITE_BUSY)
		{
			// set rc to something that will exit the while loop 
			code = SQLITE_OK;
			sqlite3_stmt * stmt = sqlite3_next_stmt(m_dbHandle, NULL);

			if (stmt == nullptr)
				break;

			code = sqlite3_finalize(stmt);
			if (code == SQLITE_OK)
			{
				code = sqlite3_close(m_dbHandle);
			}
		}

		return code;
	}

	template <typename... Args>
	bool PrepareStatement(const string& sqlStr, Args&&... args)
	{
		if (!Prepare(sqlStr))
		{
			return false;
		}

		if (SQLITE_OK != detail::BindParams(m_statement, 1, std::forward<Args>(args)...))
		{
			return false;
		}

		return true;
	}

	bool JsonTransaction(const rapidjson::Document& doc)
	{
		for (int i = 0; i < doc.Size(); i++)
		{	
			if (!m_jsonHelper.ExcecuteJson(m_statement, doc[i]))
			{
				break;
			}
		}
		
		if (m_code != SQLITE_DONE) {
			return false;
		}
		return true;
	}

private:

	SqliteValue GetValue(sqlite3_stmt *stmt, const int& index)
	{
		int type = sqlite3_column_type(stmt, index);
		auto it = m_valmap.find(type);
		if (it == m_valmap.end())
			throw std::invalid_argument("can not find this type");

		return it->second(stmt, index);
	}

	template<typename T>
	typename std::enable_if <std::is_arithmetic<T>::value, T>::type
		GetErrorVal()
	{
			return T(-9999);
		}

	template<typename T>
	typename std::enable_if <!std::is_arithmetic<T>::value, T>::type
		GetErrorVal()
	{
			return "";
		}

private:
	sqlite3* m_dbHandle;
	sqlite3_stmt* m_statement;
	bool m_isConned;
	int m_code;

	//JsonBuilder m_jsonBuilder; 
	detail::JsonHelper m_jsonHelper;
	rapidjson::StringBuffer m_buf;

	static std::unordered_map<int, std::function <SqliteValue(sqlite3_stmt*, int)>> m_valmap;
	
};
std::unordered_map<int, std::function <SqliteValue(sqlite3_stmt*, int)>> SmartDB::m_valmap =
{
	{ std::make_pair(SQLITE_INTEGER, [](sqlite3_stmt *stmt, int index){return sqlite3_column_int64(stmt, index); }) },
	{ std::make_pair(SQLITE_FLOAT, [](sqlite3_stmt *stmt, int index){return sqlite3_column_double(stmt, index); }) },
	{ std::make_pair(SQLITE_BLOB, [](sqlite3_stmt *stmt, int index){return string((const char*) sqlite3_column_blob(stmt, index));/* SmartDB::GetBlobVal(stmt, index);*/ }) },
	{ std::make_pair(SQLITE_TEXT, [](sqlite3_stmt *stmt, int index){return string((const char*) sqlite3_column_text(stmt, index)); }) },
	{ std::make_pair(SQLITE_NULL, [](sqlite3_stmt *stmt, int index){return nullptr; }) }
};

void test_smartDB() {
	SmartDB db;
    db.Open("test.db");

	// test excecute
    const string sqlcreat = "CREATE TABLE if not exists PersonTable(ID INTEGER NOT NULL, Name Text, Address Text);";
    if (!db.Excecute(sqlcreat))
        return;

    const string sqlinsert = "INSERT INTO PersonTable(ID, Name, Address) VALUES(?, ?, ?);";
    int id = 1;
    string name = "smartDB";
    string city = "www.database";

    if (!db.Excecute(sqlinsert, id, name, city)){
		return;
	}

	// test tuple
	db.ExcecuteTuple(sqlinsert, std::forward_as_tuple(id+1, "smartCpp", "www.cpp"));

	// test json
    auto state = db.ExcecuteJson(sqlinsert, "[{\"ID\":3,\"Name\":\"Xwell\",\"Address\":\"www.csdn\"}]");

	// test jsoncpp
	JsonCpp jcp;
	jcp.StartArray();
	for(int i = 0; i < 5; i++) {
		jcp.StartObject();
		jcp.WriteJson("ID", i+4);
		jcp.WriteJson("Name", "smartXwell");
		jcp.WriteJson("Address", "www.csdn");
		jcp.EndObject();
	}
	jcp.EndArray();
	state = db.ExcecuteJson(sqlinsert, jcp.GetString());

	// test json doc
	auto p = db.Query("select * from PersonTable");
	cout << p -> Size() << endl;
	cout << p -> GetString() << endl;
}

void test_time() {
	SmartDB db;
    db.Open("test_time.db");
    const string sqlcreat = "CREATE TABLE if not exists TestTimeTable(ID INTEGER NOT NULL, KPIID INTEGER, CODE INTEGER, V1 INTEGER, V2 INTEGER, V3 REAL, V4 TEXT);";
    if (!db.Excecute(sqlcreat))
        return;

    clock_t start = clock();
    const string sqlinsert = "INSERT INTO TestTimeTable(ID, KPIID, CODE, V1, V2, V3, V4) VALUES(?, ?, ?, ?, ?, ?, ?);";
    bool ret = db.Prepare(sqlinsert);
    db.Begin();	// begin task
    for (size_t i = 0; i < 1000000; i++)
    {
        ret = db.ExcecuteArgs(i, i, i, i, i, i + 1.25, "it is a test");
        if (!ret)
            break;
    }

    if (ret)
        db.Commit(); //commit task
    else
        db.RollBack(); //rollback task
	clock_t end = clock();
    cout << "insert 1000000 time cost: " << (double)(end - start) / CLOCKS_PER_SEC << " s" << endl;

    auto p = db.Query("select * from TestTimeTable");
	clock_t end2 = clock();
    cout << "query all time cost: " << (double)(end2 - end) / CLOCKS_PER_SEC << " s" << endl;
    cout << "size: " << p->Size() << endl;
}

