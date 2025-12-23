#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// 前向声明
struct sqlite3;
struct sqlite3_stmt;

class Database
{
private:
    sqlite3 *db;
    bool initialized;

    // 私有构造函数（单例模式）
    Database();

public:
    ~Database();

    // 单例模式
    static Database &GetInstance();

    // 禁用拷贝
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // 数据库初始化和关闭
    bool Initialize(const std::string &dbPath);
    bool Close();
    bool IsInitialized() const { return initialized; }

    // 执行 SQL（用于 INSERT、UPDATE、DELETE）
    bool Execute(const std::string &sql);

    // 查询 SQL（返回结果集）
    std::vector<std::vector<std::string>> Query(const std::string &sql);

    // 查询单行（返回单行结果）
    std::vector<std::string> QueryRow(const std::string &sql);

    // 查询单个值
    std::string QueryValue(const std::string &sql);

    // 检查表是否存在
    bool TableExists(const std::string &tableName);

    // 创建表的 SQL 语句
    bool CreateTablesIfNotExist();

private:
    // 辅助函数
    bool ExecuteImpl(const std::string &sql);
};

#endif // DATABASE_H
