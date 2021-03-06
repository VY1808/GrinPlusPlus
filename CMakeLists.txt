cmake_minimum_required(VERSION 3.1)

# HunterGate Package Manager
set(HUNTER_ROOT ${CMAKE_BINARY_DIR})
set(HUNTER_STATUS_DEBUG TRUE)
include("HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.23.83.tar.gz"
    SHA1 "12dec078717539eb7b03e6d2a17797cba9be9ba9"
)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_REVISION 0)

project(GRIN_PLUS_PLUS VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION})

message("Grin++ Version: ${PROJECT_VERSION}")

#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")

    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100") # unreferenced formal parameter
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4324") # '__blake2b_state': structure was padded due to alignment specifier

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")

    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /INCREMENTAL:NO")
    SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /INCREMENTAL:NO")
    SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /OPT:REF /INCREMENTAL:NO")
endif()

add_definitions(-DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_definitions(-DNOMINMAX)
add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
add_definitions(-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)

include_directories(
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/include/thirdparty/mio/include
	${PROJECT_SOURCE_DIR}/include/thirdparty/stduuid/include
	${PROJECT_SOURCE_DIR}/include/thirdparty/lru-cache/include
)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Dependencies
# Async++
hunter_add_package(Async++)
find_package(Async++ CONFIG REQUIRED)
set_target_properties(Async++::Async++ PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)

# OpenSSL
hunter_add_package(OpenSSL)
find_package(OpenSSL REQUIRED)

#JsonCPP
hunter_add_package(jsoncpp)
find_package(jsoncpp CONFIG REQUIRED)
set_target_properties(jsoncpp_lib_static PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)

#websocketpp
hunter_add_package(websocketpp)
find_package(websocketpp CONFIG REQUIRED)

# Grin++ Projects
add_subdirectory(Infrastructure)
add_subdirectory(Crypto)
add_subdirectory(Core)
add_subdirectory(Config)
add_subdirectory(PMMR)
add_subdirectory(PoW)
add_subdirectory(Database)
add_subdirectory(BlockChain)
add_subdirectory(TxPool)
add_subdirectory(Net)
add_subdirectory(P2P)
add_subdirectory(Wallet)
add_subdirectory(Server)

add_executable(RunAllTests RunAllTests.cpp)