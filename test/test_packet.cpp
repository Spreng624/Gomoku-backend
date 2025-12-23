#include <gtest/gtest.h>
#include "Packet.h"

class PacketTest : public ::testing::Test
{
};

// 测试 Packet 创建
TEST_F(PacketTest, PacketCreation)
{
    Packet packet(123, MsgType::Login, 456);
    EXPECT_EQ(packet.sessionId, 123);
    EXPECT_EQ(packet.msgType, MsgType::Login);
    EXPECT_EQ(packet.requestId, 456);
    EXPECT_TRUE(packet.IsRequest());
    EXPECT_FALSE(packet.IsPush());
}

// 测试推送包
TEST_F(PacketTest, PushPacket)
{
    Packet packet(123, MsgType::PlayerJoined, 0);
    EXPECT_FALSE(packet.IsRequest());
    EXPECT_TRUE(packet.IsPush());
}

// 测试参数添加和获取
TEST_F(PacketTest, AddAndGetParams)
{
    Packet packet(123, MsgType::Login);

    packet.AddParam("username", std::string("testUser"));
    packet.AddParam("userId", uint64_t(999));
    packet.AddParam("isGuest", false);

    EXPECT_EQ(packet.GetParam<std::string>("username"), "testUser");
    EXPECT_EQ(packet.GetParam<uint64_t>("userId"), 999UL);
    EXPECT_EQ(packet.GetParam<bool>("isGuest"), false);
}

// 测试序列化和反序列化
TEST_F(PacketTest, SerializationAndDeserialization)
{
    Packet originalPacket(123, MsgType::Login, 456);
    originalPacket.AddParam("username", std::string("testUser"));
    originalPacket.AddParam("score", uint64_t(1500));
    originalPacket.AddParam("isActive", true);

    // 序列化
    auto bytes = originalPacket.ToBytes();
    EXPECT_GT(bytes.size(), 0);

    // 反序列化
    Packet deserializedPacket;
    bool success = deserializedPacket.FromData(123, bytes);
    EXPECT_TRUE(success);
    EXPECT_EQ(deserializedPacket.sessionId, 123);
    EXPECT_EQ(deserializedPacket.msgType, MsgType::Login);
    EXPECT_EQ(deserializedPacket.requestId, 456);
    EXPECT_EQ(deserializedPacket.GetParam<std::string>("username"), "testUser");
    EXPECT_EQ(deserializedPacket.GetParam<uint64_t>("score"), 1500UL);
    EXPECT_EQ(deserializedPacket.GetParam<bool>("isActive"), true);
}
