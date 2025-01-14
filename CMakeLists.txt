set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_minimum_required(VERSION 3.14)

project(lsmDatabase)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(/home/danil/ITMO_Projects/database_lsm/service_template/third_party/userver userver)

add_executable(lsmDatabase src/main.cpp src/handlers/handler.cpp src/handlers/vote.cpp src/handlers/heartbeat.cpp src/interfaces/lsmstorage.cpp src/interfaces/memtable.cpp src/interfaces/sstable.cpp src/handlers/add_node.cpp src/handlers/remove_node.cpp)

target_link_libraries(lsmDatabase PRIVATE PUBLIC userver::core)