#ifndef GAME_H
#define GAME_H

#include <vector>
#include <stack>
#include <string>
#include <functional>

enum class Piece
{
    EMPTY,
    BLACK,
    WHITE
};

class Game
{
public:
    enum Status
    {
        Idle,
        Active,
        Paused,
        Settled
    } status;
    Game();

    // ==================== 配置接口 ====================

    void setConfig(const std::string &config);

    // ==================== 游戏操作接口（输入） ====================

    bool move(int x, int y);
    bool undo();
    void start();
    void pause();
    void end();
    void reset();
    void sync(const std::string &stateStr);

    // ==================== 状态查询接口 ====================

    const std::vector<std::vector<Piece>> &getBoard() const;
    Piece getCurrentPlayer() const;
    bool isGameOver() const;
    Piece getWinner() const;
    std::string getMoveHistory() const;
    std::pair<std::string, std::string> getTimeLeft() const;
    bool isValidMove(int x, int y) const;

    // ==================== 状态序列化接口 ====================

    std::string serializeState() const;
    bool deserializeState(const std::string &stateStr);

    // ==================== 回调设置接口（输出） ====================

    void setOnBoardChanged(std::function<void(const std::vector<std::vector<Piece>> &)> callback);
    void setOnCurrentPlayerChanged(std::function<void(Piece)> callback);
    void setOnGameStarted(std::function<void()> callback);
    void setOnGameEnded(std::function<void(Piece winner, const std::string &reason)> callback);
    void setOnMoveHistoryUpdated(std::function<void(const std::string &)> callback);
    void setOnTimeUpdated(std::function<void(const std::string &, const std::string &)> callback);
    void setOnSyncRequest(std::function<void(const std::string &)> callback);

private:
    bool checkWin(int x, int y, Piece player) const;
    void emitAllCallbacks();
    void emitBoardChanged();
    void emitCurrentPlayerChanged();
    void emitGameStarted();
    void emitGameEnded(Piece winner, const std::string &reason);
    void emitMoveHistoryUpdated();
    void emitTimeUpdated();
    void emitSyncRequest(const std::string &stateStr);

    // ==================== 回调函数 ====================

    std::function<void(const std::vector<std::vector<Piece>> &)> onBoardChanged;
    std::function<void(Piece)> onCurrentPlayerChanged;
    std::function<void()> onGameStarted;
    std::function<void(Piece, const std::string &)> onGameEnded;
    std::function<void(const std::string &)> onMoveHistoryUpdated;
    std::function<void(const std::string &, const std::string &)> onTimeUpdated;
    std::function<void(const std::string &)> onSyncRequest;

    // ==================== 游戏状态 ====================

    bool isLocal = true;
    int boardSize;
    Piece currentPlayer;
    Piece winner = Piece::EMPTY;
    bool gameOver = false;
    std::vector<std::vector<Piece>> board;

    // 时间管理
    std::string blackTimeLeft; // 黑方剩余时间（格式："mm:ss"）
    std::string whiteTimeLeft; // 白方剩余时间（格式："mm:ss"）

    // 历史记录
    std::stack<std::string> moveHistory;
};

#endif // GAME_H
