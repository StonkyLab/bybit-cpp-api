cmake_minimum_required(VERSION 3.20)
project(bybit_api)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj -DVERBOSE_LOG)
else ()
    add_definitions(-fPIC)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

find_package(Boost 1.83 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum REQUIRED)

include_directories(include vk_cpp_common/include SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

if (NOT TARGET vk_common)
    add_subdirectory(vk_cpp_common)
endif ()

set(HEADERS
        include/vk/bybit/bybit_models.h
        include/vk/bybit/bybit.h
        include/vk/bybit/bybit_rest_client.h
        include/vk/bybit/bybit_http_session.h
        include/vk/bybit/bybit_ws_client.h
        include/vk/bybit/bybit_ws_session.h
        include/vk/bybit/bybit_ws_stream_manager.h
        include/vk/bybit/bybit_event_models.h
        include/vk/bybit/bybit_exchange_connector.h)

set(SOURCES
        src/bybit.cpp
        src/bybit_models.cpp
        src/bybit_rest_client.cpp
        src/bybit_http_session.cpp
        src/bybit_ws_client.cpp
        src/bybit_ws_session.cpp
        src/bybit_ws_stream_manager.cpp
        src/bybit_event_models.cpp
        src/bybit_exchange_connector.cpp)

if (MODULE_MANAGER)
    add_library(bybit_api SHARED ${SOURCES} ${HEADERS})
else ()
    add_library(bybit_api STATIC ${SOURCES} ${HEADERS})

    add_executable(bybit_test test/main.cpp)
    target_link_libraries(bybit_test PRIVATE spdlog::spdlog_header_only bybit_api)
endif ()

target_link_libraries(bybit_api PRIVATE OpenSSL::Crypto OpenSSL::SSL vk_common nlohmann_json::nlohmann_json)