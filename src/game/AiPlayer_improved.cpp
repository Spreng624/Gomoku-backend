#include "AiPlayer_improved.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <random>
#include <chrono>
#include <cstring>

using namespace std;

// 方向定义
const vector<Direction> ImprovedAiPlayer::directions = {
    {1, 0}, // 垂直
    {0, 1}, // 水平
    {1, 1}, // 主对角线
    {1, -1} // 副对角线
};

ImprovedAiPlayer::ImprovedAiPlayer(Piece color, int depth)
    : aiColor(color), boardSize(15), searchDepth(depth)
{
}

ImprovedAiPlayer::~ImprovedAiPlayer()
{
}

void ImprovedAiPlayer::setBoardSize(int size)
{
    boardSize = size;
}

void ImprovedAiPlayer::setSearchDepth(int depth)
{
    searchDepth = depth;
}

Piece ImprovedAiPlayer::getColor() const
{
    return aiColor;
}

bool ImprovedAiPlayer::isInBoard(int x, int y, int boardSize)
{
    return x >= 0 && x < boardSize && y >= 0 && y < boardSize;
}

uint64_t ImprovedAiPlayer::generateBoardHash(const vector<vector<Piece>> &board) const
{
    // 简单的哈希函数，用于缓存评估结果
    uint64_t hash = 0;
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            hash = hash * 31 + static_cast<int>(board[i][j]);
        }
    }
    return hash;
}

bool ImprovedAiPlayer::isWinningMove(const vector<vector<Piece>> &board, int x, int y, Piece color) const
{
    // 检查落子后是否形成五连
    for (const auto &dir : directions)
    {
        int count = 1; // 当前位置

        // 正向检查
        for (int k = 1; k < 5; ++k)
        {
            int nx = x + dir.dx * k;
            int ny = y + dir.dy * k;
            if (!isInBoard(nx, ny, boardSize) || board[nx][ny] != color)
            {
                break;
            }
            count++;
        }

        // 反向检查
        for (int k = 1; k < 5; ++k)
        {
            int nx = x - dir.dx * k;
            int ny = y - dir.dy * k;
            if (!isInBoard(nx, ny, boardSize) || board[nx][ny] != color)
            {
                break;
            }
            count++;
        }

        if (count >= 5)
        {
            return true;
        }
    }
    return false;
}

PatternType ImprovedAiPlayer::checkPattern(const vector<vector<Piece>> &board, int x, int y,
                                           int dx, int dy, Piece color) const
{
    // 检查特定方向的模式
    int count = 1;
    int emptyEnds = 0;

    // 正向检查
    bool blockedForward = false;
    for (int k = 1; k < 5; ++k)
    {
        int nx = x + dx * k;
        int ny = y + dy * k;
        if (!isInBoard(nx, ny, boardSize))
        {
            blockedForward = true;
            break;
        }
        if (board[nx][ny] == color)
        {
            count++;
        }
        else if (board[nx][ny] == Piece::EMPTY)
        {
            emptyEnds++;
            break;
        }
        else
        {
            blockedForward = true;
            break;
        }
    }

    // 反向检查
    bool blockedBackward = false;
    for (int k = 1; k < 5; ++k)
    {
        int nx = x - dx * k;
        int ny = y - dy * k;
        if (!isInBoard(nx, ny, boardSize))
        {
            blockedBackward = true;
            break;
        }
        if (board[nx][ny] == color)
        {
            count++;
        }
        else if (board[nx][ny] == Piece::EMPTY)
        {
            emptyEnds++;
            break;
        }
        else
        {
            blockedBackward = true;
            break;
        }
    }

    // 根据连续数量和空位判断模式
    if (count >= 5)
        return PatternType::FIVE;
    if (count == 4)
    {
        if (emptyEnds == 2)
            return PatternType::LIVE_FOUR;
        if (emptyEnds == 1)
            return PatternType::RUSH_FOUR;
    }
    if (count == 3)
    {
        if (emptyEnds == 2)
            return PatternType::LIVE_THREE;
        if (emptyEnds == 1)
            return PatternType::SLEEP_THREE;
    }
    if (count == 2)
    {
        if (emptyEnds == 2)
            return PatternType::LIVE_TWO;
        if (emptyEnds == 1)
            return PatternType::SLEEP_TWO;
    }

    return PatternType::SINGLE;
}

int ImprovedAiPlayer::evaluatePosition(const vector<vector<Piece>> &board, int x, int y, Piece color) const
{
    if (board[x][y] != Piece::EMPTY)
    {
        return 0;
    }

    int score = 0;
    Piece opponent = (color == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;

    // 检查每个方向
    for (const auto &dir : directions)
    {
        // 评估进攻（自己的颜色）
        PatternType myPattern = checkPattern(board, x, y, dir.dx, dir.dy, color);
        score += static_cast<int>(myPattern);

        // 评估防守（对手的颜色）
        PatternType oppPattern = checkPattern(board, x, y, dir.dx, dir.dy, opponent);
        score += static_cast<int>(oppPattern) / 2; // 防守价值略低于进攻
    }

    // 中心位置加成
    int centerX = boardSize / 2;
    int centerY = boardSize / 2;
    int distance = abs(x - centerX) + abs(y - centerY);
    score += (boardSize - distance) * 5;

    return score;
}

int ImprovedAiPlayer::evaluateBoard(const vector<vector<Piece>> &board) const
{
    // 检查缓存
    uint64_t hash = generateBoardHash(board);
    auto it = evaluationCache.find(hash);
    if (it != evaluationCache.end())
    {
        return it->second;
    }

    int score = 0;

    // 检查是否已经获胜
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == aiColor && isWinningMove(board, i, j, aiColor))
            {
                evaluationCache[hash] = 1000000;
                return 1000000;
            }
            if (board[i][j] != Piece::EMPTY && board[i][j] != aiColor)
            {
                Piece opponent = (aiColor == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;
                if (isWinningMove(board, i, j, opponent))
                {
                    evaluationCache[hash] = -1000000;
                    return -1000000;
                }
            }
        }
    }

    // 评估每个位置
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == aiColor)
            {
                // 自己的棋子
                for (const auto &dir : directions)
                {
                    PatternType pattern = checkPattern(board, i, j, dir.dx, dir.dy, aiColor);
                    score += static_cast<int>(pattern);
                }
            }
            else if (board[i][j] != Piece::EMPTY)
            {
                // 对手的棋子
                Piece opponent = (aiColor == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;
                for (const auto &dir : directions)
                {
                    PatternType pattern = checkPattern(board, i, j, dir.dx, dir.dy, opponent);
                    score -= static_cast<int>(pattern) / 2;
                }
            }
        }
    }

    evaluationCache[hash] = score;
    return score;
}

vector<pair<int, int>> ImprovedAiPlayer::getHeuristicMoves(const vector<vector<Piece>> &board) const
{
    vector<pair<int, int>> moves;
    vector<pair<int, int>> candidateMoves;

    // 收集所有空位并评分
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == Piece::EMPTY)
            {
                // 只考虑有棋子相邻的位置（提高效率）
                bool hasNeighbor = false;
                for (int dx = -2; dx <= 2 && !hasNeighbor; ++dx)
                {
                    for (int dy = -2; dy <= 2 && !hasNeighbor; ++dy)
                    {
                        if (dx == 0 && dy == 0)
                            continue;
                        int nx = i + dx;
                        int ny = j + dy;
                        if (isInBoard(nx, ny, boardSize) && board[nx][ny] != Piece::EMPTY)
                        {
                            hasNeighbor = true;
                        }
                    }
                }

                if (hasNeighbor || (i == boardSize / 2 && j == boardSize / 2))
                {
                    int score = evaluatePosition(board, i, j, aiColor);
                    candidateMoves.push_back({i, j});
                    // 简单评分存储，实际可以使用优先队列
                }
            }
        }
    }

    // 如果棋盘完全为空，返回中心
    if (candidateMoves.empty())
    {
        candidateMoves.push_back({boardSize / 2, boardSize / 2});
    }

    // 按评分排序（简化处理，取前20个）
    int limit = min(20, static_cast<int>(candidateMoves.size()));
    for (int i = 0; i < limit; ++i)
    {
        moves.push_back(candidateMoves[i]);
    }

    return moves;
}

int ImprovedAiPlayer::alphaBeta(vector<vector<Piece>> &board, int depth, int alpha, int beta,
                                bool maximizingPlayer, int &bestX, int &bestY) const
{
    // 终止条件
    if (depth == 0)
    {
        return evaluateBoard(board);
    }

    // 获取启发式移动
    auto moves = getHeuristicMoves(board);
    if (moves.empty())
    {
        return evaluateBoard(board);
    }

    if (maximizingPlayer)
    {
        int maxEval = INT_MIN;
        int localBestX = -1, localBestY = -1;

        for (const auto &move : moves)
        {
            int x = move.first, y = move.second;

            // 检查是否获胜
            if (isWinningMove(board, x, y, aiColor))
            {
                bestX = x;
                bestY = y;
                return 1000000 - (searchDepth - depth); // 深度越浅，分数越高
            }

            // 模拟落子
            board[x][y] = aiColor;
            int eval = alphaBeta(board, depth - 1, alpha, beta, false, bestX, bestY);
            board[x][y] = Piece::EMPTY;

            if (eval > maxEval)
            {
                maxEval = eval;
                localBestX = x;
                localBestY = y;
            }

            alpha = max(alpha, eval);
            if (beta <= alpha)
            {
                break; // Alpha-Beta剪枝
            }
        }

        if (depth == searchDepth && localBestX != -1)
        {
            bestX = localBestX;
            bestY = localBestY;
        }

        return maxEval;
    }
    else
    {
        int minEval = INT_MAX;
        Piece opponent = (aiColor == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;

        for (const auto &move : moves)
        {
            int x = move.first, y = move.second;

            // 检查对手是否获胜
            if (isWinningMove(board, x, y, opponent))
            {
                return -1000000 + (searchDepth - depth); // 深度越浅，负分越少
            }

            // 模拟落子
            board[x][y] = opponent;
            int eval = alphaBeta(board, depth - 1, alpha, beta, true, bestX, bestY);
            board[x][y] = Piece::EMPTY;

            minEval = min(minEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha)
            {
                break; // Alpha-Beta剪枝
            }
        }

        return minEval;
    }
}

pair<int, int> ImprovedAiPlayer::getNextMove(const vector<vector<Piece>> &board)
{
    // 清空缓存
    evaluationCache.clear();

    // 如果棋盘为空，返回中心
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
        return {boardSize / 2, boardSize / 2};
    }

    // 创建可修改的棋盘副本
    vector<vector<Piece>> boardCopy = board;
    int bestX = -1, bestY = -1;

    // 使用迭代加深搜索
    for (int depth = 1; depth <= searchDepth; depth++)
    {
        int currentBestX = -1, currentBestY = -1;
        alphaBeta(boardCopy, depth, INT_MIN, INT_MAX, true, currentBestX, currentBestY);

        if (currentBestX != -1 && currentBestY != -1)
        {
            bestX = currentBestX;
            bestY = currentBestY;

            // 检查是否已经获胜
            if (isWinningMove(board, bestX, bestY, aiColor))
            {
                break;
            }
        }
    }

    // 如果没有找到最佳移动，使用启发式选择
    if (bestX == -1 || bestY == -1)
    {
        auto moves = getHeuristicMoves(board);
        if (!moves.empty())
        {
            // 选择评分最高的移动
            int bestScore = INT_MIN;
            for (const auto &move : moves)
            {
                int score = evaluatePosition(board, move.first, move.second, aiColor);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestX = move.first;
                    bestY = move.second;
                }
            }
        }
        else
        {
            // 回退到第一个空位
            for (int i = 0; i < boardSize && bestX == -1; ++i)
            {
                for (int j = 0; j < boardSize && bestX == -1; ++j)
                {
                    if (board[i][j] == Piece::EMPTY)
                    {
                        bestX = i;
                        bestY = j;
                    }
                }
            }
        }
    }

    return {bestX, bestY};
}