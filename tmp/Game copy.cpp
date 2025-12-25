#include "Game.h"
#include <algorithm>
#include <sstream>
#include <iostream>

Game::Game() : boardSize(15), currentPlayer(Piece::BLACK), isLocal(true),
               blackTimeLeft("15:00"), whiteTimeLeft("15:00"),
               winner(Piece::EMPTY), gameOver(false)
{
    reset();
}

void Game::setConfig(const std::string &config)
{
    // 简单实现：解析配置字符串
    // 格式示例："size=15;time=300;isLocal=true"
    // 这里仅做占位实现
    std::cout << "Game config set: " << config << std::endl;
}

void Game::reset()
{
    board.assign(boardSize, std::vector<Piece>(boardSize, Piece::EMPTY));
    currentPlayer = Piece::BLACK;
    winner = Piece::EMPTY;
    gameOver = false;
    blackTimeLeft = "15:00";
    whiteTimeLeft = "15:00";

    // 清空历史记录
    while (!moveHistory.empty())
    {
        moveHistory.pop();
    }

    // 触发所有回调
    emitAllCallbacks();
}

bool Game::move(int x, int y)
{
    if (!isValidMove(x, y))
    {
        return false;
    }

    // 落子
    board[x][y] = currentPlayer;

    // 记录历史
    std::string moveStr = std::to_string(x) + "," + std::to_string(y) + " " +
                          (currentPlayer == Piece::BLACK ? "B" : "W");
    moveHistory.push(moveStr);

    // 检查胜利
    if (checkWin(x, y, currentPlayer))
    {
        gameOver = true;
        winner = currentPlayer;
        emitBoardChanged();
        emitGameEnded(winner, "五子连珠获胜");
        return true;
    }

    // 切换玩家
    currentPlayer = (currentPlayer == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;

    // 触发回调
    emitBoardChanged();
    emitCurrentPlayerChanged();
    emitMoveHistoryUpdated();

    return true;
}

bool Game::undo()
{
    if (moveHistory.empty())
        return false;

    // 获取最后一步
    std::string lastMove = moveHistory.top();
    moveHistory.pop();

    // 解析坐标
    size_t commaPos = lastMove.find(',');
    if (commaPos != std::string::npos)
    {
        int x = std::stoi(lastMove.substr(0, commaPos));
        size_t spacePos = lastMove.find(' ');
        if (spacePos != std::string::npos)
        {
            int y = std::stoi(lastMove.substr(commaPos + 1, spacePos - commaPos - 1));
            board[x][y] = Piece::EMPTY;
        }
    }

    // 切换回上一个玩家
    currentPlayer = (currentPlayer == Piece::BLACK) ? Piece::WHITE : Piece::BLACK;

    // 如果游戏已结束，重置结束状态
    if (gameOver)
    {
        gameOver = false;
        winner = Piece::EMPTY;
    }

    // 触发回调
    emitBoardChanged();
    emitCurrentPlayerChanged();
    emitMoveHistoryUpdated();

    return true;
}

void Game::start()
{
    gameOver = false;
    winner = Piece::EMPTY;
    emitGameStarted();
}

void Game::pause()
{
    // 暂停游戏逻辑
    std::cout << "Game paused" << std::endl;
}

void Game::end()
{
    gameOver = true;
    emitGameEnded(Piece::EMPTY, "游戏结束");
}

void Game::sync(const std::string &stateStr)
{
    // 同步状态（联网模式）
    // 解析状态字符串并更新游戏状态
    std::cout << "Syncing game state: " << stateStr << std::endl;

    // 这里应该解析 stateStr 并更新 board, currentPlayer, moveHistory 等
    // 简单实现：触发同步请求回调
    emitSyncRequest(stateStr);
}

// ==================== 状态查询接口 ====================

const std::vector<std::vector<Piece>> &Game::getBoard() const
{
    return board;
}

Piece Game::getCurrentPlayer() const
{
    return currentPlayer;
}

bool Game::isGameOver() const
{
    return gameOver;
}

Piece Game::getWinner() const
{
    return winner;
}

std::string Game::getMoveHistory() const
{
    std::string historyStr;
    std::stack<std::string> temp = moveHistory;
    while (!temp.empty())
    {
        historyStr = temp.top() + " " + historyStr;
        temp.pop();
    }
    return historyStr;
}

std::pair<std::string, std::string> Game::getTimeLeft() const
{
    return {blackTimeLeft, whiteTimeLeft};
}

bool Game::isValidMove(int x, int y) const
{
    if (x < 0 || x >= boardSize || y < 0 || y >= boardSize)
    {
        return false;
    }
    return board[x][y] == Piece::EMPTY;
}

// ==================== 回调设置接口 ====================

void Game::setOnBoardChanged(std::function<void(const std::vector<std::vector<Piece>> &)> callback)
{
    onBoardChanged = callback;
}

void Game::setOnCurrentPlayerChanged(std::function<void(Piece)> callback)
{
    onCurrentPlayerChanged = callback;
}

void Game::setOnGameStarted(std::function<void()> callback)
{
    onGameStarted = callback;
}

void Game::setOnGameEnded(std::function<void(Piece winner, const std::string &reason)> callback)
{
    onGameEnded = callback;
}

void Game::setOnMoveHistoryUpdated(std::function<void(const std::string &)> callback)
{
    onMoveHistoryUpdated = callback;
}

void Game::setOnTimeUpdated(std::function<void(const std::string &, const std::string &)> callback)
{
    onTimeUpdated = callback;
}

void Game::setOnSyncRequest(std::function<void(const std::string &)> callback)
{
    onSyncRequest = callback;
}

// ==================== 私有辅助函数 ====================

bool Game::checkWin(int x, int y, Piece player) const
{
    // 检查8个方向
    const int directions[4][2] = {
        {1, 0}, // 水平
        {0, 1}, // 垂直
        {1, 1}, // 正斜线
        {1, -1} // 反斜线
    };

    for (int i = 0; i < 4; i++)
    {
        int count = 1; // 当前位置

        // 正向计数
        for (int step = 1; step < 5; step++)
        {
            int nx = x + directions[i][0] * step;
            int ny = y + directions[i][1] * step;

            if (nx < 0 || nx >= boardSize || ny < 0 || ny >= boardSize)
            {
                break;
            }

            if (board[nx][ny] == player)
            {
                count++;
            }
            else
            {
                break;
            }
        }

        // 反向计数
        for (int step = 1; step < 5; step++)
        {
            int nx = x - directions[i][0] * step;
            int ny = y - directions[i][1] * step;

            if (nx < 0 || nx >= boardSize || ny < 0 || ny >= boardSize)
            {
                break;
            }

            if (board[nx][ny] == player)
            {
                count++;
            }
            else
            {
                break;
            }
        }

        if (count >= 5)
        {
            return true;
        }
    }

    return false;
}

void Game::emitAllCallbacks()
{
    emitBoardChanged();
    emitCurrentPlayerChanged();
    emitMoveHistoryUpdated();
    emitTimeUpdated();
}

void Game::emitBoardChanged()
{
    if (onBoardChanged)
        onBoardChanged(board);
}

void Game::emitCurrentPlayerChanged()
{
    if (onCurrentPlayerChanged)
        onCurrentPlayerChanged(currentPlayer);
}

void Game::emitGameStarted()
{
    if (onGameStarted)
        onGameStarted();
}

void Game::emitGameEnded(Piece winner, const std::string &reason)
{
    if (onGameEnded)
        onGameEnded(winner, reason);
}

void Game::emitMoveHistoryUpdated()
{
    if (onMoveHistoryUpdated)
        onMoveHistoryUpdated(getMoveHistory());
}

void Game::emitTimeUpdated()
{
    if (onTimeUpdated)
        onTimeUpdated(blackTimeLeft, whiteTimeLeft);
}

void Game::emitSyncRequest(const std::string &stateStr)
{
    if (onSyncRequest)
        onSyncRequest(stateStr);
}

// ==================== 状态序列化接口 ====================

std::string Game::serializeState() const
{
    std::ostringstream oss;

    // 配置部分
    oss << "v:80|f:1|t:classic||";

    // 移动历史部分
    std::stack<std::string> temp = moveHistory;
    std::vector<std::string> moves;

    // 将栈中的移动转移到向量中（顺序反转）
    while (!temp.empty())
    {
        moves.push_back(temp.top());
        temp.pop();
    }

    // 反转向量，使最早的在前面
    std::reverse(moves.begin(), moves.end());

    // 解析每个移动并转换为新格式
    for (size_t i = 0; i < moves.size(); i++)
    {
        const std::string &moveStr = moves[i];
        size_t commaPos = moveStr.find(',');
        if (commaPos != std::string::npos)
        {
            int x = std::stoi(moveStr.substr(0, commaPos));
            size_t spacePos = moveStr.find(' ');
            if (spacePos != std::string::npos)
            {
                int y = std::stoi(moveStr.substr(commaPos + 1, spacePos - commaPos - 1));
                char playerChar = moveStr[spacePos + 1];
                int player = (playerChar == 'B') ? 0 : 1; // 黑棋为0，白棋为1

                oss << player << "," << x << "," << y;
                if (i != moves.size() - 1)
                {
                    oss << ";";
                }
            }
        }
    }

    // 添加当前玩家
    if (!moves.empty())
    {
        oss << ";";
    }
    oss << (currentPlayer == Piece::BLACK ? 0 : 1);

    return oss.str();
}

bool Game::deserializeState(const std::string &stateStr)
{
    // 清空当前状态
    reset();

    // 查找配置部分和移动记录部分的分隔符
    size_t configEnd = stateStr.find("||");
    if (configEnd == std::string::npos)
    {
        std::cerr << "Invalid state string format: missing '||' separator" << std::endl;
        return false;
    }

    // 跳过配置部分，获取移动记录部分
    std::string movesPart = stateStr.substr(configEnd + 2);
    if (movesPart.empty())
    {
        // 空棋盘状态
        return true;
    }

    // 按分号分割移动记录
    std::vector<std::string> moveTokens;
    size_t start = 0;
    size_t end = movesPart.find(';');

    while (end != std::string::npos)
    {
        moveTokens.push_back(movesPart.substr(start, end - start));
        start = end + 1;
        end = movesPart.find(';', start);
    }
    // 添加最后一个token
    moveTokens.push_back(movesPart.substr(start));

    if (moveTokens.empty())
    {
        return true;
    }

    // 最后一个token是当前玩家
    std::string currentPlayerStr = moveTokens.back();
    moveTokens.pop_back();

    // 解析当前玩家
    int currentPlayerInt;
    try
    {
        currentPlayerInt = std::stoi(currentPlayerStr);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse current player: " << e.what() << std::endl;
        return false;
    }

    // 设置当前玩家 (0=黑棋, 1=白棋)
    currentPlayer = (currentPlayerInt == 0) ? Piece::BLACK : Piece::WHITE;

    // 清空历史记录
    while (!moveHistory.empty())
    {
        moveHistory.pop();
    }

    // 重置棋盘
    board.assign(boardSize, std::vector<Piece>(boardSize, Piece::EMPTY));

    // 解析并应用每个移动
    for (const std::string &moveToken : moveTokens)
    {
        // 格式: player,x,y
        size_t comma1 = moveToken.find(',');
        if (comma1 == std::string::npos)
        {
            std::cerr << "Invalid move token format: " << moveToken << std::endl;
            continue;
        }

        size_t comma2 = moveToken.find(',', comma1 + 1);
        if (comma2 == std::string::npos)
        {
            std::cerr << "Invalid move token format: " << moveToken << std::endl;
            continue;
        }

        try
        {
            int player = std::stoi(moveToken.substr(0, comma1));
            int x = std::stoi(moveToken.substr(comma1 + 1, comma2 - comma1 - 1));
            int y = std::stoi(moveToken.substr(comma2 + 1));

            // 验证坐标范围
            if (x < 0 || x >= boardSize || y < 0 || y >= boardSize)
            {
                std::cerr << "Move coordinates out of bounds: " << x << "," << y << std::endl;
                continue;
            }

            // 设置棋子
            Piece piece = (player == 0) ? Piece::BLACK : Piece::WHITE;
            board[x][y] = piece;

            // 记录历史（使用与move方法相同的格式）
            std::string historyStr = std::to_string(x) + "," + std::to_string(y) + " " +
                                     (piece == Piece::BLACK ? "B" : "W");
            moveHistory.push(historyStr);

            // 检查胜利
            if (checkWin(x, y, piece))
            {
                gameOver = true;
                winner = piece;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to parse move token '" << moveToken << "': " << e.what() << std::endl;
            continue;
        }
    }

    // 触发回调更新UI
    emitAllCallbacks();

    return true;
}
