#include<iostream>
#include<string>
#include"SQLite3/sqlite3.h"

using namespace std;

bool test_sqlite() {
  sqlite3* db_handle = nullptr;
    int result = sqlite3_open("test.db", &db_handle);
    if(result != SQLITE_OK) {
        sqlite3_close(db_handle);
        return false;
    }

    // create table
    const char* sql_create = "CREATE TABLE if not exists personTable(ID INTEGER NOT NULL, Name Text, Address Text);";
    result = sqlite3_exec(db_handle, sql_create, nullptr, nullptr, nullptr);

    // insert data
    // create stmt
    sqlite3_stmt* stmt = nullptr;
    const char* sql_insert = "INSERT INTO personTable(ID, Name, Address) VALUES(?, ?, ?);";
    sqlite3_prepare_v2(db_handle, sql_insert, strlen(sql_insert), &stmt, nullptr);
    
    // update data
    int id = 1;
    const char* name = "Sqlite3";
    const char* address = "Database";
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, name, strlen(name), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, address, strlen(address), SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    result = sqlite3_exec(db_handle, sql_insert, nullptr, nullptr, nullptr);

    // free stmt
    sqlite3_finalize(stmt);

    // close sqlite
    sqlite3_close(db_handle);
    return result = SQLITE_OK;
}