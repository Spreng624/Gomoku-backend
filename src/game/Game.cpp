#include "Game.h"
#include <stack>

Game::Game()
{
    this->boardSize = 15;
    reset();
}
Game::Game(int boardSize)
{
    this->boardSize = boardSize;
    reset();
}

void Game::reset()
{
    board.assign(this->boardSize, std::vector<Piece>(this->boardSize, Piece::EMPTY));
    while (!moveHistory.empty())
    {
        moveHistory.pop();
    }
    lastMove = {-1, -1};
    blackTime = 0.0;
    whiteTime = 0.0;
}

int Game::getBoardSize() const
{
    return this->boardSize;
}

std::vector<std::vector<Piece>> Game::getBoard() const
{
    return this->board;
}

std::pair<int, int> Game::getLastMove() const
{
    return lastMove;
}

bool Game::isBoardFull() const
{
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] == Piece::EMPTY)
            {
                return false;
            }
        }
    }
    return true;
}

int Game::getMoveCount() const
{
    return moveHistory.size();
}

/**
 * @brief 在指定位置落子
 * @param x 行坐标
 * @param y 列坐标
 * @param color 棋子颜色
 * @return 成功落子返回 true，否则返回 false (如位置越界或已被占用)
 */
bool Game::makeMove(int x, int y, Piece color)
{
    // 1. 检查坐标是否在棋盘范围内
    if (x < 0 || x >= this->boardSize || y < 0 || y >= this->boardSize)
    {
        return false;
    }

    // 2. 检查位置是否为空
    if (board[x][y] != Piece::EMPTY)
    {
        return false;
    }

    // 3. 落子
    board[x][y] = color;
    lastMove = {x, y};
    moveHistory.push({x, y});
    return true;
}

bool Game::undoMove()
{
    if (moveHistory.empty())
    {
        return false;
    }

    auto last = moveHistory.top();
    moveHistory.pop();

    // 撤销落子
    board[last.first][last.second] = Piece::EMPTY;

    // 更新lastMove
    if (moveHistory.empty())
    {
        lastMove = {-1, -1};
    }
    else
    {
        lastMove = moveHistory.top();
    }

    return true;
}

// 统计从 (startX, startY) 开始的 dx, dy 方向的连续子数量
int Game::countLine(int startX, int startY, int dx, int dy, Piece color) const
{
    int count = 0;
    int x = startX + dx;
    int y = startY + dy;

    while (x >= 0 && x < this->boardSize && y >= 0 && y < this->boardSize && board[x][y] == color)
    {
        count++;
        x += dx;
        y += dy;
    }
    return count;
}

Piece Game::checkWin(int x, int y) const
{
    // 1. 确认该位置是否有棋子，并获取颜色
    Piece color = board[x][y];
    if (color == Piece::EMPTY)
        return Piece::EMPTY;

    // 定义所有需要检查的八个方向，但只需要检查四个主方向 (水平、垂直、两个对角线)
    // 因为通过正反方向统计后求和，可以覆盖八个方向。
    // directions: {dx, dy}
    const std::vector<std::pair<int, int>> directions = {
        {1, 0}, // 垂直 (下)
        {0, 1}, // 水平 (右)
        {1, 1}, // 主对角线 (右下)
        {1, -1} // 副对角线 (左下)
    };

    // 2. 检查四个方向（每个方向包含正反两个方向的连续子数量）
    for (const auto &dir : directions)
    {
        int dx = dir.first;
        int dy = dir.second;

        // 统计正向和反向 (dx, dy) 连续数量
        int count1 = countLine(x, y, dx, dy, color);
        int count2 = countLine(x, y, -dx, -dy, color);

        if (count1 + count2 + 1 >= 5)
            return color;
    }

    return Piece::EMPTY;
}

Piece Game::checkWin() const
{
    // 检查整个棋盘是否有胜利者
    // 遍历所有棋子位置
    for (int i = 0; i < boardSize; ++i)
    {
        for (int j = 0; j < boardSize; ++j)
        {
            if (board[i][j] != Piece::EMPTY)
            {
                Piece winner = checkWin(i, j);
                if (winner != Piece::EMPTY)
                {
                    return winner;
                }
            }
        }
    }
    return Piece::EMPTY;
}