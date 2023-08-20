#pragma once
#include <string>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"
using namespace rapidjson;

class JsonCpp
{
	typedef Writer<StringBuffer> JsonWriter;
public:

	JsonCpp() : m_writer(m_buf)
	{
	}

	~JsonCpp()
	{
	}

	void StartArray()
	{
		m_writer.StartArray();
	}
	
	void EndArray()
	{
		m_writer.EndArray();
	}

	void StartObject()
	{
		m_writer.StartObject();
	}

	void EndObject()
	{
		m_writer.EndObject();
	}

	template<typename T>
	void WriteJson(std::string& key, T&& value)
	{
		m_writer.String(key.c_str());
		WriteValue(std::forward<T>(value));
	}

	template<typename T>
	void WriteJson(const char* key, T&& value)
	{
		m_writer.String(key);
		WriteValue(std::forward<T>(value));	
	}
	
	const char* GetString() const
	{
		return m_buf.GetString();
	}

private:
	template<typename V>
	typename std::enable_if<std::is_same<V, int>::value>::type WriteValue(V value)
	{
		m_writer.Int(value);
	}

	template<typename V>
	typename std::enable_if<std::is_same<V, unsigned int>::value>::type WriteValue(V value)
	{
		m_writer.Uint(value);
	}

	template<typename V>
	typename std::enable_if<std::is_same<V, int64_t>::value>::type WriteValue(V value)
	{
		m_writer.Int64(value);
	}

	template<typename V>
	typename std::enable_if<std::is_floating_point<V>::value>::type WriteValue(V value)
	{
		m_writer.Double(value);
	}

	template<typename V>
	typename std::enable_if<std::is_same<V, bool>::value>::type WriteValue(V value)
	{
		m_writer.Bool(value);
	}

	template<typename V>
	typename std::enable_if<std::is_pointer<V>::value>::type WriteValue(V value)
	{
		m_writer.String(value);
	}

	template<typename V>
	typename std::enable_if<std::is_array<V>::value>::type WriteValue(V value)
	{
		m_writer.String(value);
	}

	template<typename V>
	typename std::enable_if<std::is_same<V, std::nullptr_t>::value>::type WriteValue(V value)
	{
		m_writer.Null();
	}

private:
	StringBuffer m_buf;
	JsonWriter m_writer;

	Document m_doc;
};


void test_json() {
	// test jsoncpp
	JsonCpp jcp;
	jcp.StartArray();
	for(int i = 0; i < 1; i++) {
		jcp.StartObject();
		jcp.WriteJson("ID", i);
		jcp.WriteJson("Name", "Xwell");
		jcp.WriteJson("Address", "www.csdn");
		jcp.EndObject();
	}
	jcp.EndArray();
	std::cout <<jcp.GetString() << std::endl;
}
