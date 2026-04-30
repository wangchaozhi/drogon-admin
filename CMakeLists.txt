cmake_minimum_required(VERSION 3.20)
project(c_web CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W3 /EHsc /MP)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# 收集各层源码，保持模块化扩展的零配置
file(GLOB_RECURSE CORE_SRC     CONFIGURE_DEPENDS src/core/*.cc    src/core/*.cpp)
file(GLOB_RECURSE COMMON_SRC   CONFIGURE_DEPENDS src/common/*.cc  src/common/*.cpp)
file(GLOB_RECURSE FILTER_SRC   CONFIGURE_DEPENDS src/filters/*.cc src/filters/*.cpp)
file(GLOB_RECURSE PLUGIN_SRC   CONFIGURE_DEPENDS src/plugins/*.cc src/plugins/*.cpp)
file(GLOB_RECURSE MODULES_SRC  CONFIGURE_DEPENDS src/modules/*.cc src/modules/*.cpp)

add_executable(c_web
    src/main.cc
    ${CORE_SRC}
    ${COMMON_SRC}
    ${FILTER_SRC}
    ${PLUGIN_SRC}
    ${MODULES_SRC}
)

target_include_directories(c_web PRIVATE src)
target_link_libraries(c_web PRIVATE Drogon::Drogon OpenSSL::SSL OpenSSL::Crypto)

# 拷贝运行配置到输出目录，方便 run-in-place
add_custom_command(TARGET c_web POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/config
            $<TARGET_FILE_DIR:c_web>/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/sql
            $<TARGET_FILE_DIR:c_web>/sql
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:c_web>/data
)
