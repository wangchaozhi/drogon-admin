cmake_minimum_required(VERSION 3.20)
project(drogon-admin CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3 /EHsc /MP)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(unofficial-argon2 CONFIG QUIET)
if(TARGET unofficial::argon2::libargon2)
    set(ARGON2_TARGET unofficial::argon2::libargon2)
else()
    find_path(ARGON2_INCLUDE_DIR NAMES argon2.h REQUIRED)
    find_library(ARGON2_LIBRARY NAMES argon2 REQUIRED)
    add_library(drogon-admin-argon2 UNKNOWN IMPORTED)
    set_target_properties(drogon-admin-argon2 PROPERTIES
        IMPORTED_LOCATION "${ARGON2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ARGON2_INCLUDE_DIR}"
    )
    set(ARGON2_TARGET drogon-admin-argon2)
endif()

# 收集各层源码，保持模块化扩展的零配置
file(GLOB_RECURSE CORE_SRC     CONFIGURE_DEPENDS src/core/*.cc    src/core/*.cpp)
file(GLOB_RECURSE COMMON_SRC   CONFIGURE_DEPENDS src/common/*.cc  src/common/*.cpp)
file(GLOB_RECURSE FILTER_SRC   CONFIGURE_DEPENDS src/filters/*.cc src/filters/*.cpp)
file(GLOB_RECURSE PLUGIN_SRC   CONFIGURE_DEPENDS src/plugins/*.cc src/plugins/*.cpp)
file(GLOB_RECURSE MODULES_SRC  CONFIGURE_DEPENDS src/modules/*.cc src/modules/*.cpp)

add_executable(drogon-admin
    src/main.cc
    ${CORE_SRC}
    ${COMMON_SRC}
    ${FILTER_SRC}
    ${PLUGIN_SRC}
    ${MODULES_SRC}
)

target_include_directories(drogon-admin PRIVATE src)
target_link_libraries(drogon-admin PRIVATE
    Drogon::Drogon
    OpenSSL::SSL
    OpenSSL::Crypto
    ${ARGON2_TARGET}
)

# 拷贝运行配置到输出目录，方便 run-in-place
add_custom_command(TARGET drogon-admin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:drogon-admin>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/sql
            $<TARGET_FILE_DIR:drogon-admin>/sql
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:drogon-admin>/data
)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    add_executable(crypto-util-test tests/CryptoUtilTest.cc)
    target_include_directories(crypto-util-test PRIVATE src)
    target_link_libraries(crypto-util-test PRIVATE
        Drogon::Drogon
        OpenSSL::Crypto
        ${ARGON2_TARGET}
    )
    add_test(NAME crypto-util-test COMMAND crypto-util-test)

    add_test(
        NAME api-integration-test
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tests/ApiIntegrationTest.py"
                --server "$<TARGET_FILE:drogon-admin>"
                --schema "${CMAKE_SOURCE_DIR}/sql/schema.sql"
    )
    set_tests_properties(api-integration-test PROPERTIES
        TIMEOUT 60
        LABELS "integration"
    )
endif()
