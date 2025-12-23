#include <gtest/gtest.h>
#include "User.h"

class UserTest : public ::testing::Test
{
protected:
    User user{"testUser", "password123"};
};

// 测试用户创建
TEST_F(UserTest, UserCreation)
{
    EXPECT_EQ(user.GetUsername(), "testUser");
    EXPECT_EQ(user.GetPassword(), "password123");
    EXPECT_EQ(user.GetScore(), 0);
    EXPECT_EQ(user.win_count, 0);
    EXPECT_EQ(user.lose_count, 0);
    EXPECT_EQ(user.draw_count, 0);
}

// 测试段位计算
TEST_F(UserTest, RankByScore)
{
    user.score = 0;
    user.UpdateRankByScore();
    EXPECT_EQ(user.GetLevel(), "30K");

    user.score = 100;
    user.UpdateRankByScore();
    EXPECT_EQ(user.GetLevel(), "25K");

    user.score = 1500;
    user.UpdateRankByScore();
    EXPECT_EQ(user.GetLevel(), "1D");

    user.score = 4000;
    user.UpdateRankByScore();
    EXPECT_EQ(user.GetLevel(), "9D");
}

// 测试 K 值计算
TEST_F(UserTest, KValueCalculation)
{
    user.win_count = 0;
    user.lose_count = 0;
    user.draw_count = 0;
    int k1 = user.K();

    user.win_count = 100;
    user.lose_count = 100;
    user.draw_count = 100;
    int k2 = user.K();

    EXPECT_GT(k1, k2); // 新手 K 值应该大于高手
}

// 测试编辑用户名
TEST_F(UserTest, EditUsername)
{
    user.EditUsername("newName");
    EXPECT_EQ(user.GetUsername(), "newName");
}

// 测试编辑密码
TEST_F(UserTest, EditPassword)
{
    user.EditPassword("newPassword");
    EXPECT_EQ(user.GetPassword(), "newPassword");
}
