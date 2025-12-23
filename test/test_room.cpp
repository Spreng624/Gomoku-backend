#include <gtest/gtest.h>
#include "Room.h"

class RoomTest : public ::testing::Test
{
protected:
    Room room{1};
};

// 测试房间创建
TEST_F(RoomTest, RoomCreation)
{
    EXPECT_EQ(room.GetRoomId(), 1);
    EXPECT_EQ(room.status, RoomStatus::Free);
    EXPECT_EQ(room.playerIds.size(), 0);
}

// 测试添加玩家
TEST_F(RoomTest, AddPlayer)
{
    EXPECT_TRUE(room.AddPlayer(100));
    EXPECT_EQ(room.ownerId, 100);
    EXPECT_EQ(room.playerIds.size(), 1);

    EXPECT_TRUE(room.AddPlayer(200));
    EXPECT_EQ(room.playerIds.size(), 2);
}

// 测试玩家已存在
TEST_F(RoomTest, DuplicatePlayer)
{
    room.AddPlayer(100);
    EXPECT_FALSE(room.AddPlayer(100));
}

// 测试房间满
TEST_F(RoomTest, RoomFull)
{
    room.AddPlayer(100);
    room.AddPlayer(200);
    EXPECT_FALSE(room.AddPlayer(300));
}

// 测试移除玩家
TEST_F(RoomTest, RemovePlayer)
{
    room.AddPlayer(100);
    room.AddPlayer(200);

    EXPECT_EQ(room.RemovePlayer(100), 0);
    EXPECT_EQ(room.playerIds.size(), 1);
    EXPECT_EQ(room.ownerId, 200);
}

// 测试选择棋色
TEST_F(RoomTest, TakeColor)
{
    room.AddPlayer(100);
    room.AddPlayer(200);

    EXPECT_TRUE(room.TakeBlack(100));
    EXPECT_FALSE(room.TakeBlack(200)); // 黑棋已被占用
    EXPECT_TRUE(room.TakeWhite(200));
}

// 测试开始游戏
TEST_F(RoomTest, StartGame)
{
    room.AddPlayer(100);
    room.AddPlayer(200);
    room.TakeBlack(100);
    room.TakeWhite(200);

    EXPECT_TRUE(room.StartGame(100));
    EXPECT_EQ(room.status, RoomStatus::Playing);
}

// 测试游戏中无法开始
TEST_F(RoomTest, CannotStartWhenPlaying)
{
    room.AddPlayer(100);
    room.AddPlayer(200);
    room.TakeBlack(100);
    room.TakeWhite(200);
    room.StartGame(100);

    EXPECT_FALSE(room.StartGame(100));
}
