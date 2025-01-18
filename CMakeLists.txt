cmake_minimum_required(VERSION 3.20)
project(bybit_api)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj -DVERBOSE_LOG)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

add_definitions(-fPIC)

find_package(Boost 1.83 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum REQUIRED)

include_directories(include vk_cpp_common/include SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
add_subdirectory(vk_cpp_common)

set(HEADERS
        include/vk/bybit/v2/bybit_models_v2.h
        include/vk/bybit/v5/bybit_models_v5.h
        include/vk/bybit/bybit.h
        include/vk/bybit/v2/bybit_futures_rest_client_v2.h
        include/vk/bybit/v5/bybit_futures_rest_client_v5.h
        include/vk/bybit/bybit_http_session.h
        include/vk/bybit/v2/bybit_futures_ws_client_v2.h
        include/vk/bybit/v2/bybit_futures_ws_session_v2.h
        include/vk/bybit/v2/bybit_ws_stream_manager_v2.h
        include/vk/bybit/v5/bybit_futures_ws_client_v5.h
        include/vk/bybit/v5/bybit_futures_ws_session_v5.h
        include/vk/bybit/v5/bybit_ws_stream_manager_v5.h
        include/vk/bybit/v2/bybit_event_models_v2.h
        include/vk/bybit/v5/bybit_event_models_v5.h)

set(SOURCES
        src/bybit.cpp
        src/v2/bybit_models_v2.cpp
        src/v2/bybit_futures_rest_client_v2.cpp
        src/v5/bybit_models_v5.cpp
        src/v5/bybit_futures_rest_client_v5.cpp
        src/bybit_http_session.cpp
        src/v2/bybit_futures_ws_client_v2.cpp
        src/v2/bybit_futures_ws_session_v2.cpp
        src/v2/bybit_ws_stream_manager_v2.cpp
        src/v5/bybit_futures_ws_client_v5.cpp
        src/v5/bybit_futures_ws_session_v5.cpp
        src/v5/bybit_ws_stream_manager_v5.cpp
        src/v2/bybit_event_models_v2.cpp
        src/v5/bybit_event_models_v5.cpp)

if (MODULE_MANAGER)
    add_library(bybit_api MODULE ${SOURCES} ${HEADERS})
else ()
    add_library(bybit_api STATIC ${SOURCES} ${HEADERS})
endif ()

set_target_properties(bybit_api PROPERTIES POSITION_INDEPENDENT_CODE 1)
target_link_libraries(bybit_api PRIVATE OpenSSL::Crypto OpenSSL::SSL vk_common nlohmann_json::nlohmann_json)
target_compile_definitions(bybit_api PUBLIC VERBOSE_LOG)

add_executable(test_v2 test/main_v2.cpp)
target_link_libraries(test_v2 PRIVATE spdlog::spdlog_header_only bybit_api)

add_executable(test_v5 test/main_v5.cpp)
target_link_libraries(test_v5 PRIVATE spdlog::spdlog_header_only bybit_api)