#include <gtest/gtest.h>
#include "Database.h"
#include <filesystem>

class DatabaseTest : public ::testing::Test
{
protected:
    const std::string TEST_DB = "test_gomoku.db";

    void SetUp() override
    {
        // 删除旧的测试数据库
        if (std::filesystem::exists(TEST_DB))
        {
            std::filesystem::remove(TEST_DB);
        }
    }

    void TearDown() override
    {
        // 清理测试数据库
        Database::GetInstance().Close();
        if (std::filesystem::exists(TEST_DB))
        {
            std::filesystem::remove(TEST_DB);
        }
    }
};

// 测试数据库初始化
TEST_F(DatabaseTest, DatabaseInitialization)
{
    Database &db = Database::GetInstance();
    EXPECT_TRUE(db.Initialize(TEST_DB));
    EXPECT_TRUE(db.IsInitialized());
    EXPECT_TRUE(db.TableExists("users"));
    EXPECT_TRUE(db.TableExists("rooms"));
    EXPECT_TRUE(db.TableExists("game_records"));
}

// 测试 SQL 执行
TEST_F(DatabaseTest, ExecuteSQL)
{
    Database &db = Database::GetInstance();
    db.Initialize(TEST_DB);

    std::string sql = "INSERT INTO users (username, password) VALUES ('testUser', 'pwd123');";
    EXPECT_TRUE(db.Execute(sql));
}

// 测试 SQL 查询
TEST_F(DatabaseTest, QuerySQL)
{
    Database &db = Database::GetInstance();
    db.Initialize(TEST_DB);

    db.Execute("INSERT INTO users (username, password) VALUES ('testUser', 'pwd123');");

    auto results = db.Query("SELECT username FROM users;");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0][0], "testUser");
}

// 测试单行查询
TEST_F(DatabaseTest, QueryRow)
{
    Database &db = Database::GetInstance();
    db.Initialize(TEST_DB);

    db.Execute("INSERT INTO users (username, password) VALUES ('testUser', 'pwd123');");

    auto row = db.QueryRow("SELECT username, password FROM users;");
    EXPECT_EQ(row.size(), 2);
    EXPECT_EQ(row[0], "testUser");
    EXPECT_EQ(row[1], "pwd123");
}

// 测试查询单个值
TEST_F(DatabaseTest, QueryValue)
{
    Database &db = Database::GetInstance();
    db.Initialize(TEST_DB);

    db.Execute("INSERT INTO users (username, password) VALUES ('testUser', 'pwd123');");

    std::string username = db.QueryValue("SELECT username FROM users LIMIT 1;");
    EXPECT_EQ(username, "testUser");
}
