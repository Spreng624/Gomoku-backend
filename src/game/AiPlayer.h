#ifndef AIPLAYER_H
#define AIPLAYER_H

#include "Game.h"
#include <cstdint>
#include <vector>
#include <utility>

class AiPlayer
{
private:
    Piece aiColor; // AI的棋子颜色
    int boardSize;

    // 评估函数：评估当前棋盘对AI的得分
    int evaluateBoard(const std::vector<std::vector<Piece>> &board, Piece aiColor) const;

    // 获取所有合法落子位置
    std::vector<std::pair<int, int>> getValidMoves(const std::vector<std::vector<Piece>> &board) const;

    // 极小化极大算法
    int minimax(std::vector<std::vector<Piece>> &board, int depth, bool isMaximizing, int alpha, int beta, Piece aiColor) const;

    // 检查位置是否在棋盘范围内
    bool isInBoard(int x, int y) const;

public:
    AiPlayer(Piece color);
    ~AiPlayer();

    void setBoardSize(int size);
    std::pair<int, int> getNextMove(const std::vector<std::vector<Piece>> &board);
    Piece getColor() const;
};

#endif // AIPLAYER_H