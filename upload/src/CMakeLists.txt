# cmake needs this line
SET(CMAKE_BUILD_TYPE "Release")

add_library(sqlite3 ../include/SQLite3/sqlite3)
add_library(rapidjson ../include/rapidjson)
add_executable(main main.cpp)
target_link_libraries(main sqlite3)