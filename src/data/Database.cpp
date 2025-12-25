#include "Database.h"
#include "Logger.h"
#include <sstream>
extern "C"
{
#include "sqlite3.h"
}

Database::Database() : db(nullptr), initialized(false)
{
}

Database::~Database()
{
    Close();
}

Database &Database::GetInstance()
{
    static Database instance;
    return instance;
}

bool Database::Initialize(const std::string &dbPath)
{
    if (initialized)
    {
        LOG_WARN("Database already initialized");
        return false;
    }
    initialized = true;

    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("Failed to open database: " + std::string(sqlite3_errmsg(db)));
        return false;
    }

    // 启用外键约束
    Execute("PRAGMA foreign_keys = ON");

    LOG_INFO("Database initialized successfully: " + dbPath);

    // 创建表
    if (!CreateTablesIfNotExist())
    {
        LOG_ERROR("Failed to create tables");
        return false;
    }

    return true;
}

bool Database::Close()
{
    if (!initialized || db == nullptr)
        return true;

    int rc = sqlite3_close(db);
    if (rc != SQLITE_OK)
    {
        LOG_ERROR("Failed to close database: " + std::string(sqlite3_errmsg(db)));
        return false;
    }

    db = nullptr;
    initialized = false;
    return true;
}

bool Database::Execute(const std::string &sql)
{
    if (!initialized)
    {
        LOG_ERROR("Database not initialized");
        return false;
    }

    LOG_TRACE("Executing SQL: " + sql);

    char *errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK)
    {
        LOG_ERROR("SQL Error: " + std::string(errMsg));
        sqlite3_free(errMsg);
        return false;
    }

    LOG_TRACE("SQL executed successfully");
    return true;
}

std::vector<std::vector<std::string>> Database::Query(const std::string &sql)
{
    std::vector<std::vector<std::string>> result;

    if (!initialized)
    {
        LOG_ERROR("Database not initialized");
        return result;
    }

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK)
    {
        LOG_ERROR("SQL Prepare Error: " + std::string(sqlite3_errmsg(db)));
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::vector<std::string> row;
        int colCount = sqlite3_column_count(stmt);

        for (int i = 0; i < colCount; i++)
        {
            const char *val = reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }

        result.push_back(row);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::string> Database::QueryRow(const std::string &sql)
{
    std::vector<std::string> row;

    if (!initialized)
    {
        LOG_ERROR("Database not initialized");
        return row;
    }

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK)
    {
        LOG_ERROR("SQL Prepare Error: " + std::string(sqlite3_errmsg(db)));
        return row;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int colCount = sqlite3_column_count(stmt);

        for (int i = 0; i < colCount; i++)
        {
            const char *val = reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }
    }

    sqlite3_finalize(stmt);
    return row;
}

std::string Database::QueryValue(const std::string &sql)
{
    auto row = QueryRow(sql);
    return row.empty() ? "" : row[0];
}

bool Database::TableExists(const std::string &tableName)
{
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + tableName + "';";
    auto result = Query(sql);
    return !result.empty();
}

bool Database::CreateTablesIfNotExist()
{
    // 创建 users 表
    std::string createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            rank TEXT DEFAULT '30K',
            ranking INTEGER DEFAULT 0,
            score REAL DEFAULT 0.0,
            win_count INTEGER DEFAULT 0,
            lose_count INTEGER DEFAULT 0,
            draw_count INTEGER DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";

    if (!Execute(createUsersTable))
        return false;

    // 创建 rooms 表
    std::string createRoomsTable = R"(
        CREATE TABLE IF NOT EXISTS rooms (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_id INTEGER NOT NULL,
            status INTEGER DEFAULT 0,
            black_player_id INTEGER,
            white_player_id INTEGER,
            board_size INTEGER DEFAULT 15,
            is_graded INTEGER DEFAULT 0,
            enable_takeback INTEGER DEFAULT 1,
            base_time_seconds INTEGER DEFAULT 600,
            byoyomi_seconds INTEGER DEFAULT 30,
            byoyomi_count INTEGER DEFAULT 5,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(owner_id) REFERENCES users(id)
        );
    )";

    if (!Execute(createRoomsTable))
        return false;

    // 创建 game_records 表
    std::string createGameRecordsTable = R"(
        CREATE TABLE IF NOT EXISTS game_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            roomId INTEGER NOT NULL,
            black_player_id INTEGER NOT NULL,
            white_player_id INTEGER NOT NULL,
            winner_id INTEGER,
            status INTEGER DEFAULT 0,
            moves_json TEXT,
            start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            end_time TIMESTAMP,
            FOREIGN KEY(roomId) REFERENCES rooms(id),
            FOREIGN KEY(black_player_id) REFERENCES users(id),
            FOREIGN KEY(white_player_id) REFERENCES users(id),
            FOREIGN KEY(winner_id) REFERENCES users(id)
        );
    )";

    if (!Execute(createGameRecordsTable))
        return false;

    LOG_INFO("All tables created successfully");
    return true;
}
