#include <gtest/gtest.h>

// Google Test 会自动生成 main 函数
// 该文件可以为空，或者用来配置全局测试设置

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
