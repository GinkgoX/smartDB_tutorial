#pragma once

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"


const string BEGIN = "BEGIN";
const string COMMIT = "COMMIT";
const string ROLLBACK = "ROLLBACK";

struct blob
{
	const char *pBuf;
	int size;
};
namespace detail
{
	template <typename T>
	typename std::enable_if<std::is_floating_point<T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, T t)
	{
			return sqlite3_bind_double(statement, current, std::forward<T>(t));
		}

	template <typename T>
	typename std::enable_if<std::is_integral<T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, T t)
	{
			return BindIntValue(statement, current, t);
		}

	template <typename T>
	typename std::enable_if<std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value, int>::type
		BindIntValue(sqlite3_stmt *statement, int current, T t)
	{
			return sqlite3_bind_int64(statement, current, std::forward<T>(t));
		}

	template <typename T>
	typename std::enable_if<!std::is_same<T, int64_t>::value&&!std::is_same<T, uint64_t>::value, int>::type
		BindIntValue(sqlite3_stmt *statement, int current, T t)
	{
			return sqlite3_bind_int(statement, current, std::forward<T>(t));
		}


	template <typename T>
	typename std::enable_if<std::is_same<std::string, T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, const T& t)
	{
			return sqlite3_bind_text(statement, current, t.data(), t.length(), SQLITE_TRANSIENT);
		}

	template <typename T>
	typename std::enable_if<std::is_same<char*, T>::value || std::is_same<const char*, T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, T t)
	{
			return sqlite3_bind_text(statement, current, t, strlen(t) + 1, SQLITE_TRANSIENT);
		}

	template <typename T>
	typename std::enable_if<std::is_same<blob, T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, const T& t)
	{
			return sqlite3_bind_blob(statement, current, t.pBuf, t.size, SQLITE_TRANSIENT);
		}

	template <typename T>
	typename std::enable_if<std::is_same<nullptr_t, T>::value, int>::type
		BindValue(sqlite3_stmt *statement, int current, const T& t)
	{
			return sqlite3_bind_null(statement, current);
		}

	template <typename T, typename... Args>
	inline int BindParams(sqlite3_stmt *statement, int current, T&&first, Args&&... args)
	{
		int code = BindValue(statement, current, first);
		if (code != SQLITE_OK)
			return code;

		code = BindParams(statement, current + 1, std::forward<Args>(args)...);

		return code;
	}

	inline int BindParams(sqlite3_stmt *statement, int current)
	{
		return SQLITE_OK;
	}

	using JsonBuilder = rapidjson::Writer<rapidjson::StringBuffer>;

	class JsonHelper
	{
	public:
		JsonHelper(rapidjson::StringBuffer& buf, int code) : m_code(code), m_jsonBuilder(buf)
		{

		}

		bool ExcecuteJson(sqlite3_stmt *stmt, const rapidjson::Value& val)
		{
			int i = 0;
			for(auto iter = val.MemberBegin(); iter != val.MemberEnd(); ++iter) {
				auto key = iter->name.GetString();
				if (SQLITE_OK != BindJsonValue(stmt, val[key], i + 1)){
					return false;
				}
				i += 1;
			}

			m_code = sqlite3_step(stmt);
			
			sqlite3_reset(stmt);
			return SQLITE_DONE == m_code;
		}

		void BuildJsonObject(sqlite3_stmt *stmt)
		{
			int colCount = sqlite3_column_count(stmt);

			m_jsonBuilder.StartArray();
			while (true)
			{
				m_code = sqlite3_step(stmt);
				if (m_code == SQLITE_DONE)
				{
					break;
				}

				BuildJsonArray(stmt, colCount);
			}

			m_jsonBuilder.EndArray();
			sqlite3_reset(stmt);
		}

	private:
		int BindJsonValue(sqlite3_stmt *stmt, const rapidjson::Value& t, int index)
		{
			auto type = t.GetType();
			if (type == rapidjson::kNullType)
			{
				m_code = sqlite3_bind_null(stmt, index);
			}
			else if (type == rapidjson::kStringType)
			{
				m_code = sqlite3_bind_text(stmt, index, t.GetString(), -1, SQLITE_STATIC);
			}
			else if (type == rapidjson::kNumberType)
			{
				BindNumber(stmt, t, index);				
			}
			else
			{
				throw std::invalid_argument("can not find this type.");
			}

			return m_code;
		}

		void BindNumber(sqlite3_stmt *stmt, const rapidjson::Value& t, int index)
		{
			if (t.IsInt() || t.IsUint())
				m_code = sqlite3_bind_int(stmt, index, t.GetInt());
			else if (t.IsInt64() || t.IsUint64())
				m_code = sqlite3_bind_int64(stmt, index, t.GetInt64());
			else
				m_code = sqlite3_bind_double(stmt, index, t.GetDouble());
		}

		void ToUpper(char* s)
		{
			size_t len = strlen(s);
			for (size_t i = 0; i < len; i++)
			{
				s[i] = std::toupper(s[i]);
			}
		}

		void BuildJsonArray(sqlite3_stmt *stmt, int colCount)
		{
			m_jsonBuilder.StartObject();

			for (int i = 0; i < colCount; ++i)
			{
				char* name = (char*) sqlite3_column_name(stmt, i);
				ToUpper(name);

				m_jsonBuilder.String(name);
				BuildJsonValue(stmt, i);
			}

			m_jsonBuilder.EndObject();
		}

		void BuildJsonValue(sqlite3_stmt *stmt, int index)
		{
			int type = sqlite3_column_type(stmt, index);
			auto it = m_builderMap.find(type);
			if (it == m_builderMap.end())
				throw std::invalid_argument("can not find this type");

			it->second(stmt, index, m_jsonBuilder);
		}

		int& m_code;
		JsonBuilder m_jsonBuilder;
		static std::unordered_map<int, std::function<void(sqlite3_stmt *stmt, int index, JsonBuilder&)>> m_builderMap;
	};

	std::unordered_map<int, std::function<void(sqlite3_stmt *stmt, int index, JsonBuilder&)>> JsonHelper::m_builderMap
	{
		{ std::make_pair(SQLITE_INTEGER, [](sqlite3_stmt *stmt, int index, JsonBuilder& builder){ builder.Int64(sqlite3_column_int64(stmt, index)); }) },
		{ std::make_pair(SQLITE_FLOAT, [](sqlite3_stmt *stmt, int index, JsonBuilder& builder){ builder.Double(sqlite3_column_double(stmt, index)); }) },
		{ std::make_pair(SQLITE_BLOB, [](sqlite3_stmt *stmt, int index, JsonBuilder& builder){ builder.String((const char*) sqlite3_column_blob(stmt, index)); }) },
		{ std::make_pair(SQLITE_TEXT, [](sqlite3_stmt *stmt, int index, JsonBuilder& builder){ builder.String((const char*) sqlite3_column_text(stmt, index)); }) },
		{ std::make_pair(SQLITE_NULL, [](sqlite3_stmt *stmt, int index, JsonBuilder& builder){builder.Null(); }) }
	};

	template<int...>
	struct IndexTuple{};

	template<int N, int... Indexes>
	struct MakeIndexes : MakeIndexes<N - 1, N - 1, Indexes...>{};

	template<int... indexes>
	struct MakeIndexes<0, indexes...>
	{
		typedef IndexTuple<indexes...> type;
	};

	template<int... Indexes, class Tuple>
	int ExcecuteTuple(sqlite3_stmt *stmt, IndexTuple< Indexes... >&& in, Tuple&& t)
	{
		if (SQLITE_OK != detail::BindParams(stmt, 1, get<Indexes>(std::forward<Tuple>(t))...))
		{
			return false;
		}

		int code = sqlite3_step(stmt);
		sqlite3_reset(stmt);
		return code;
	}
}

