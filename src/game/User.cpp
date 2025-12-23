#include "User.h"
#include "Database.h"
#include <cmath>
#include <sstream>

struct RankThreshold
{
    int scoreThreshold;
    std::string rankName;
};

const std::vector<RankThreshold> score2rankMap = {
    {0, "30K"},
    {100, "25K"},
    {300, "20K"},
    {500, "15K"},
    {800, "10K"},
    {1200, "5K"},
    {1500, "1D"},
    {1800, "2D"},
    {2100, "3D"},
    {2400, "4D"},
    {2700, "5D"},
    {3000, "6D"},
    {3500, "7D"},
    {4000, "9D"}};

User::User(std::string name, std::string pwd)
    : username(name), password(pwd),
      rank(score2rankMap.front().rankName),
      ranking(0), score(0.0),
      win_count(0), lose_count(0), draw_count(0), id(0) {}

User::~User() {}

int User::K()
{
    double K, K_min, K_max, D;
    int matchCount = this->win_count + this->lose_count + this->draw_count;
    K_min = 20.0;
    K_max = 100.0;
    D = 30.0;
    K = K_min + (K_max - K_min) * exp(-matchCount / D);
    return (int)K;
}

std::string User::GetUsername() const
{
    return this->username;
}

std::string User::GetPassword() const
{
    return this->password;
}

std::string User::GetLevel() const
{
    return this->rank;
}

uint64_t User::GetID() const
{
    return this->id;
}

int User::GetScore() const
{
    return (int)this->score;
}

int User::GetRanking() const
{
    return this->ranking;
}

int User::EditUsername(std::string newUsername)
{
    this->username = newUsername;
    return 0;
}

int User::EditPassword(std::string newPassword)
{
    this->password = newPassword;
    return 0;
}

void User::SetOnline(std::string session)
{
    // TODO: 记录用户在线状态和 session
}

void User::SetOffline()
{
    // TODO: 更新用户离线状态
}

void User::SaveToDatabase() const
{
    Database &db = Database::GetInstance();
    if (!db.IsInitialized())
        return;

    std::ostringstream sql;
    sql << "UPDATE users SET rank='" << rank << "', ranking=" << ranking
        << ", score=" << score << ", win_count=" << win_count
        << ", lose_count=" << lose_count << ", draw_count=" << draw_count
        << ", updated_at=CURRENT_TIMESTAMP WHERE id=" << id << ";";

    db.Execute(sql.str());
}

void User::LoadFromDatabase(uint64_t userId)
{
    Database &db = Database::GetInstance();
    if (!db.IsInitialized())
        return;

    std::ostringstream sql;
    sql << "SELECT username, password, rank, ranking, score, win_count, lose_count, draw_count "
        << "FROM users WHERE id=" << userId << ";";

    auto row = db.QueryRow(sql.str());
    if (row.empty())
        return;

    this->id = userId;
    this->username = row[0];
    this->password = row[1];
    this->rank = row[2];
    this->ranking = std::stoi(row[3]);
    this->score = std::stod(row[4]);
    this->win_count = std::stoi(row[5]);
    this->lose_count = std::stoi(row[6]);
    this->draw_count = std::stoi(row[7]);
}

void User::UpdateRankByScore()
{
    if (score < score2rankMap.front().scoreThreshold)
    {
        this->rank = score2rankMap.front().rankName;
        return;
    }

    auto it = std::lower_bound(
        score2rankMap.begin(),
        score2rankMap.end(),
        RankThreshold{(int)score, ""},
        [](const RankThreshold &a, const RankThreshold &b)
        {
            return a.scoreThreshold < b.scoreThreshold;
        });

    if (it == score2rankMap.end())
    {
        this->rank = score2rankMap.back().rankName;
        return;
    }

    if (it->scoreThreshold == score)
    {
        this->rank = it->rankName;
    }
    else if (it != score2rankMap.begin())
    {
        this->rank = (--it)->rankName;
    }
    else
    {
        this->rank = score2rankMap.front().rankName;
    }

    SaveToDatabase();
}

void UpdateScore(User &winner, User &loser, bool isDraw)
{
    double R_A, R_B, E_A, E_B, S_A, S_B;
    R_A = winner.score;
    R_B = loser.score;
    E_A = 1.0 / (1.0 + pow(10.0, (R_B - R_A) / 400.0));
    E_B = 1.0 / (1.0 + pow(10.0, (R_A - R_B) / 400.0));

    if (isDraw)
    {
        S_A = 0.5;
        S_B = 0.5;
    }
    else
    {
        S_A = 1.0;
        S_B = 0.0;
    }

    R_A += winner.K() * (S_A - E_A);
    R_B += loser.K() * (S_B - E_B);
    winner.score = R_A;
    loser.score = R_B;

    if (isDraw)
    {
        winner.draw_count++;
        loser.draw_count++;
    }
    else
    {
        winner.win_count++;
        loser.lose_count++;
    }

    winner.UpdateRankByScore();
    loser.UpdateRankByScore();

    // 保存到数据库
    winner.SaveToDatabase();
    loser.SaveToDatabase();
}