#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdint>

// 静态用户
class User
{
private:
public:
    uint64_t id;          // 用户ID
    std::string username; // 用户名
    std::string password; // 密码
    std::string rank;     // 段位
    int ranking;          // 排名
    double score;         // 等级分

    int win_count;  // 胜利次数
    int lose_count; // 失败次数
    int draw_count; // 和局次数

    int K(); // 计算K值

    User(std::string username, std::string password);
    ~User();
    std::string GetUsername() const;
    std::string GetPassword() const;
    std::string GetLevel() const;
    uint64_t GetID() const;
    int GetScore() const;
    int GetRanking() const;
    int EditUsername(std::string newUsername);
    int EditPassword(std::string newPassword);
    void SetOnline(std::string session);
    void SetOffline();
    void UpdateRankByScore();
    void SaveToDatabase() const;            // 保存到数据库
    void LoadFromDatabase(uint64_t userId); // 从数据库加载
    friend void UpdateScore(User &winner, User &loser, bool isDraw);
};

#endif // USER_H