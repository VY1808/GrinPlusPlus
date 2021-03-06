set(TARGET_NAME PMMR)
set(TEST_TARGET_NAME PMMR_Tests)

if(MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W2 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2 /WX")
    endif()
endif()

hunter_add_package(minizip)
hunter_add_package(ZLIB)
hunter_add_package(Async++)

find_package(minizip CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)

find_package(Async++ CONFIG REQUIRED)
set_target_properties(Async++::Async++ PROPERTIES MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE)

# PMMR
file(GLOB PMMR_SRC
    "HeaderMMRImpl.cpp"
    "KernelMMR.cpp"
    "OutputPMMR.cpp"
    "RangeProofPMMR.cpp"
    "TxHashSetImpl.cpp"
	"TxHashSetManager.cpp"
    "TxHashSetValidator.cpp"
	"Common/*.cpp"
	"Common/CRoaring/*.c"
	"Zip/*.cpp"
)

add_library(${TARGET_NAME} SHARED ${PMMR_SRC})
target_compile_definitions(${TARGET_NAME} PRIVATE MW_PMMR)
add_dependencies(${TARGET_NAME} Infrastructure Crypto Core)
target_link_libraries(${TARGET_NAME} Infrastructure Crypto Core minizip::minizip ZLIB::zlib Async++::Async++)

# Tests
file(GLOB PMMR_TESTS_SRC
	"Tests/*.cpp"
)

add_executable(${TEST_TARGET_NAME} ${PMMR_SRC} ${PMMR_TESTS_SRC})
target_compile_definitions(${TEST_TARGET_NAME} PRIVATE MW_PMMR)
add_dependencies(${TEST_TARGET_NAME} Infrastructure Crypto Core)
target_link_libraries(${TEST_TARGET_NAME} Infrastructure Crypto Core minizip::minizip ZLIB::zlib Async++::Async++)