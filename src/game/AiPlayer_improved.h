#ifndef AIPLAYER_IMPROVED_H
#define AIPLAYER_IMPROVED_H

#include "Game.h"
#include <cstdint>
#include <vector>
#include <utility>
#include <unordered_map>
#include <memory>

// 五子棋模式定义
enum class PatternType
{
    FIVE = 100000,     // 五连
    LIVE_FOUR = 10000, // 活四
    RUSH_FOUR = 1000,  // 冲四
    LIVE_THREE = 1000, // 活三
    SLEEP_THREE = 100, // 眠三
    LIVE_TWO = 100,    // 活二
    SLEEP_TWO = 10,    // 眠二
    SINGLE = 1         // 单子
};

// 方向定义
struct Direction
{
    int dx;
    int dy;
};

class ImprovedAiPlayer
{
private:
    Piece aiColor;
    int boardSize;
    int searchDepth;

    // 评估缓存
    mutable std::unordered_map<uint64_t, int> evaluationCache;

    // 方向数组
    static const std::vector<Direction> directions;

    // 评估函数：计算棋盘对AI的得分
    int evaluateBoard(const std::vector<std::vector<Piece>> &board) const;

    // 评估一个位置对指定颜色的价值
    int evaluatePosition(const std::vector<std::vector<Piece>> &board, int x, int y, Piece color) const;

    // 检查特定模式
    PatternType checkPattern(const std::vector<std::vector<Piece>> &board, int x, int y,
                             int dx, int dy, Piece color) const;

    // 获取启发式移动（只搜索有意义的空位）
    std::vector<std::pair<int, int>> getHeuristicMoves(const std::vector<std::vector<Piece>> &board) const;

    // 迭代加深的Alpha-Beta搜索
    int alphaBeta(std::vector<std::vector<Piece>> &board, int depth, int alpha, int beta,
                  bool maximizingPlayer, int &bestX, int &bestY) const;

    // 快速胜负判断
    bool isWinningMove(const std::vector<std::vector<Piece>> &board, int x, int y, Piece color) const;

    // 生成棋盘哈希
    uint64_t generateBoardHash(const std::vector<std::vector<Piece>> &board) const;

public:
    ImprovedAiPlayer(Piece color, int depth = 4);
    ~ImprovedAiPlayer();

    void setBoardSize(int size);
    void setSearchDepth(int depth);

    std::pair<int, int> getNextMove(const std::vector<std::vector<Piece>> &board);
    Piece getColor() const;

    // 工具函数
    static bool isInBoard(int x, int y, int boardSize);
};

#endif // AIPLAYER_IMPROVED_H