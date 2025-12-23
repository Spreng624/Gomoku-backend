#include "AiPlayer.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <random>

AiPlayer::AiPlayer(Piece color) : aiColor(color), boardSize(15) {}

AiPlayer::~AiPlayer() {}

void AiPlayer::setBoardSize(int size)
{
    boardSize = size;
}

Piece AiPlayer::getColor() const
{
    return aiColor;
}

bool AiPlayer::isInBoard(int x, int y) const
{
    return x >= 0 && x < boardSize && y >= 0 && y < boardSize;
}

std::vector<std::pair<int, int>> AiPlayer::getValidMoves(const std::vector<std::vector<Piece>> &board) const
{
    std::vector<std::pair<int, int>> moves;
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == Piece::EMPTY)
            {
                moves.push_back({i, j});
            }
        }
    }

    return moves;
}

int AiPlayer::evaluateBoard(const std::vector<std::vector<Piece>> &board, Piece aiColor) const
{
    // 简单的评估函数：计算AI的潜在连线
    // 这是一个简化的评估函数，实际五子棋AI需要更复杂的评估

    int score = 0;
    Piece humanColor = (aiColor == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;

    // 检查四个方向
    const std::vector<std::pair<int, int>> directions = {
        {1, 0}, // 垂直
        {0, 1}, // 水平
        {1, 1}, // 主对角线
        {1, -1} // 副对角线
    };

    // 简化的评估：计算连续棋子数量
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == aiColor)
            {
                score += 10;
            }
            else if (board[i][j] == humanColor)
            {
                score -= 10;
            }
        }
    }

    return score;
}

int AiPlayer::minimax(std::vector<std::vector<Piece>> &board, int depth, bool isMaximizing,
                      int alpha, int beta, Piece aiColor) const
{
    // 深度限制或游戏结束
    if (depth == 0)
    {
        return evaluateBoard(board, aiColor);
    }

    auto moves = getValidMoves(board);
    if (moves.empty())
    {
        return evaluateBoard(board, aiColor);
    }

    if (isMaximizing)
    {
        int maxEval = INT_MIN;
        for (const auto &move : moves)
        {
            // 模拟落子
            board[move.first][move.second] = aiColor;
            int eval = minimax(board, depth - 1, false, alpha, beta, aiColor);
            // 撤销落子
            board[move.first][move.second] = Piece::EMPTY;

            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha)
                break; // Alpha-Beta剪枝
        }
        return maxEval;
    }
    else
    {
        int minEval = INT_MAX;
        Piece humanColor = (aiColor == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;
        for (const auto &move : moves)
        {
            // 模拟落子
            board[move.first][move.second] = humanColor;
            int eval = minimax(board, depth - 1, true, alpha, beta, aiColor);
            // 撤销落子
            board[move.first][move.second] = Piece::EMPTY;

            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha)
                break; // Alpha-Beta剪枝
        }
        return minEval;
    }
}

std::pair<int, int> AiPlayer::getNextMove(const std::vector<std::vector<Piece>> &board)
{
    // 如果棋盘为空，返回中心位置
    bool isEmpty = true;
    for (int i = 0; i < boardSize && isEmpty; ++i)
    {
        for (int j = 0; j < boardSize && isEmpty; ++j)
        {
            if (board[i][j] != Piece::EMPTY)
            {
                isEmpty = false;
            }
        }
    }

    if (isEmpty)
    {
        // 返回棋盘中心
        return {boardSize / 2, boardSize / 2};
    }

    auto moves = getValidMoves(board);
    if (moves.empty())
    {
        return {-1, -1}; // 没有合法移动
    }

    // 使用极小化极大算法选择最佳移动（深度设为2以保持简单）
    int bestScore = INT_MIN;
    std::pair<int, int> bestMove = moves[0];

    // 创建可修改的棋盘副本
    std::vector<std::vector<Piece>> boardCopy = board;

    for (const auto &move : moves)
    {
        // 模拟落子
        boardCopy[move.first][move.second] = aiColor;
        int score = minimax(boardCopy, 2, false, INT_MIN, INT_MAX, aiColor);
        // 撤销落子
        boardCopy[move.first][move.second] = Piece::EMPTY;

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;
        }
    }

    return bestMove;
}