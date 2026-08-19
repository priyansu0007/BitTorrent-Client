cmake_minimum_required(VERSION 3.20)
project(BitTorrentClient
    VERSION 0.1.0
    LANGUAGES CXX
)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Compiler flag & warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpendatic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
    )
endif()

# Core library target
set(CORE_SOURCES
    src/bencode.cpp
    src/torrent_file.cpp
    src/crypto.cpp
    src/url.cpp
    src/tracker.cpp
    src/message.cpp
    src/storage.cpp
    src/piece_manager.cpp
    src/peer_connection.cpp
    src/torrent_client.cpp
)

# filter non-existing source files
set(EXISTING_CORE_SOURCES "")
foreach(SOURCE_FILE ${CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
        list(APPEND EXISTING_CORE_SOURCES ${SOURCE_FILE})
    endif()
endforeach()

add_library(bittorrent_core STATIC ${EXISTING_CORE_SOURCES})

#expose include/ directory
target_include_directories(bittorrent_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(bittorrent_core PUBLIC 
    OpenSSL::Crypto
    OpenSSL::SSL
    Threads::Threads
)

#executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(torrent_client src/main.cpp)
    target_link_libraries(torrent_client PRIVATE bittorrent_core)
endif()

#test sub-directory
option(BUILD_TESTING "Build test suite" ON)
if(BUILD_TESTING)
    enable_testing()
    
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    macro(add_bittorrent_test TEST_NAME TEST_SOURCE)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${TEST_SOURCE}")
            add_executable(${TEST_NAME} ${TEST_SOURCE})
            target_link_libraries(${TEST_NAME} PRIVATE
                bittorrent_core
                GTest::gtest_main)
            add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        endif()
    endmacro()

    add_bittorrent_test(test_bencode tests/test_bencode.cpp)
    add_bittorrent_test(test_handshake tests/test_handshake.cpp)
    add_bittorrent_test(test_message tests/test_message.cpp)
endif()

