cmake_minimum_required(VERSION 3.14)
project(GomokuBackend LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用测试
enable_testing()

# 使用本地googletest
set(GTEST_ROOT "${CMAKE_SOURCE_DIR}/googletest-1.14.0/googletest")
include_directories(${GTEST_ROOT}/include ${GTEST_ROOT})

# 编译gtest库
add_library(gtest STATIC ${GTEST_ROOT}/src/gtest-all.cc)
set_target_properties(gtest PROPERTIES
    COMPILE_FLAGS "-DGTEST_HAS_PTHREAD=0"
)

add_library(gtest_main STATIC ${GTEST_ROOT}/src/gtest_main.cc)
target_link_libraries(gtest_main gtest)

include_directories(
    src
    src/game
    src/network
    src/utils
    src/core
    src/data
)

file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp" "src/*.hpp")
list(FILTER SOURCES EXCLUDE REGEX "src/main.cpp")
message(STATUS "Sources: ${SOURCES}")
add_library(${PROJECT_NAME}_lib ${SOURCES})

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_lib)

target_include_directories(${PROJECT_NAME}_lib PRIVATE src)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME}_lib PRIVATE bcrypt)
if(WIN32)
    target_link_libraries(${PROJECT_NAME}_lib PRIVATE Threads::Threads ws2_32 bcrypt)
else()
    target_link_libraries(${PROJECT_NAME}_lib PRIVATE Threads::Threads)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)

# --- 测试目标 ---
# 收集测试源文件
file(GLOB_RECURSE TEST_SOURCES "test/*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_client.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_packet.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX "test_logger.cpp")
message(STATUS "Test sources: ${TEST_SOURCES}")

# 创建测试可执行文件
add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# 链接测试目标
target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    ${PROJECT_NAME}_lib
    gtest_main
    Threads::Threads
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ws2_32 bcrypt)
else()
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE)
endif()

target_include_directories(${PROJECT_NAME}_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# 添加测试
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

# # --- 客户端测试 ---
# add_executable(test_client test_client.cpp)
# target_link_libraries(test_client PRIVATE ${PROJECT_NAME} Threads::Threads)
# if(WIN32)
#     target_link_libraries(test_client PRIVATE ws2_32 bcrypt)
# endif()
# target_include_directories(test_client PRIVATE src)

# # --- 日志测试 ---
# add_executable(test_logger test_logger.cpp)
# target_link_libraries(test_logger PRIVATE ${PROJECT_NAME})
# target_include_directories(test_logger PRIVATE src)
