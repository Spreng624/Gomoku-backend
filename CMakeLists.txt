cmake_minimum_required(VERSION 3.14)
project(GomokuBackend LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用测试
# enable_testing()

# # 使用 FetchContent 下载 GoogleTest
# include(FetchContent)
# FetchContent_Declare(
#   googletest
#   URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
# )
# # 对于 Windows，防止覆盖 Windows 宏
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)

include_directories(
    src
    src/game
    src/network
    src/utils
    src/core
    src/data
)

file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp" "src/*.hpp")
message(STATUS "Sources: ${SOURCES}")
add_executable(${PROJECT_NAME} ${SOURCES} )

target_include_directories(${PROJECT_NAME} PRIVATE src)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE bcrypt)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ws2_32 bcrypt ${SQLite3_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ${SQLite3_LIBRARIES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${SQLite3_INCLUDE_DIRS})

# # --- 测试目标 ---
# # 收集测试源文件
# file(GLOB_RECURSE TEST_SOURCES "test/*.cpp")
# message(STATUS "Test sources: ${TEST_SOURCES}")

# # 创建测试可执行文件
# add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

# # 链接测试目标
# target_link_libraries(${PROJECT_NAME}_tests PRIVATE
#     ${PROJECT_NAME}
#     gtest
#     gtest_main
#     Threads::Threads
# )

# if(WIN32)
#     target_link_libraries(${PROJECT_NAME}_tests PRIVATE ws2_32 bcrypt ${SQLite3_LIBRARIES})
# else()
#     target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${SQLite3_LIBRARIES})
# endif()

# target_include_directories(${PROJECT_NAME}_tests PRIVATE
#     src
#     ${SQLite3_INCLUDE_DIRS}
# )

# # 添加测试
# add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

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
