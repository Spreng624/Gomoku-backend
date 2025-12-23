#include <gtest/gtest.h>
#include "AiPlayer_improved.h"
#include "Game.h"

class ImprovedAiPlayerTest : public ::testing::Test
{
protected:
    ImprovedAiPlayer aiBlack{Piece::BLACK, 3}; // 搜索深度3
    ImprovedAiPlayer aiWhite{Piece::WHITE, 3};
    Game game{15};

    void SetUp() override
    {
        game.reset();
    }

    // 辅助函数：在棋盘上放置棋子
    void placePieces(const std::vector<std::pair<int, int>> &positions, Piece color)
    {
        for (const auto &pos : positions)
        {
            game.makeMove(pos.first, pos.second, color);
        }
    }
};

// 测试AI创建
TEST_F(ImprovedAiPlayerTest, AiCreation)
{
    EXPECT_EQ(aiBlack.getColor(), Piece::BLACK);
    EXPECT_EQ(aiWhite.getColor(), Piece::WHITE);
}

// 测试空棋盘时的第一手
TEST_F(ImprovedAiPlayerTest, FirstMoveOnEmptyBoard)
{
    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // 第一手应该在中心附近
    EXPECT_GE(move.first, 5);
    EXPECT_LE(move.first, 9);
    EXPECT_GE(move.second, 5);
    EXPECT_LE(move.second, 9);

    // 位置应该是空的
    EXPECT_EQ(board[move.first][move.second], Piece::EMPTY);
}

// 测试AI识别胜利机会
TEST_F(ImprovedAiPlayerTest, FindWinningMove)
{
    // 创建黑棋四连的局面
    placePieces({{7, 7}, {7, 8}, {7, 9}, {7, 10}}, Piece::BLACK);

    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // AI应该下在(7, 11)或(7, 6)形成五连
    bool isWinningMove = (move == std::make_pair(7, 11)) ||
                         (move == std::make_pair(7, 6));
    EXPECT_TRUE(isWinningMove) << "Move: (" << move.first << ", " << move.second << ")";
}

// 测试AI防守对手的胜利威胁
TEST_F(ImprovedAiPlayerTest, DefendAgainstWin)
{
    // 白棋有四连，黑棋需要防守
    placePieces({{7, 7}, {7, 8}, {7, 9}, {7, 10}}, Piece::WHITE);

    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // 黑棋应该防守在(7, 11)或(7, 6)
    bool isDefensiveMove = (move == std::make_pair(7, 11)) ||
                           (move == std::make_pair(7, 6));
    EXPECT_TRUE(isDefensiveMove) << "Move: (" << move.first << ", " << move.second << ")";
}

// 测试AI创建活三
TEST_F(ImprovedAiPlayerTest, CreateLiveThree)
{
    // 黑棋有两个相连的棋子
    placePieces({{7, 7}, {7, 8}}, Piece::BLACK);

    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // 应该尝试扩展形成活三
    // 可能的扩展方向：(7, 6), (7, 9), (6, 7), (8, 7)等
    bool isReasonable = false;

    // 检查是否在现有棋子附近
    for (int dx = -2; dx <= 2; ++dx)
    {
        for (int dy = -2; dy <= 2; ++dy)
        {
            if (dx == 0 && dy == 0)
                continue;
            int x = 7 + dx;
            int y = 7 + dy;
            if (x >= 0 && x < 15 && y >= 0 && y < 15)
            {
                if (move == std::make_pair(x, y))
                {
                    isReasonable = true;
                    break;
                }
            }
        }
        if (isReasonable)
            break;
    }

    EXPECT_TRUE(isReasonable) << "Move: (" << move.first << ", " << move.second << ")";
}

// 测试棋盘边界处理
TEST_F(ImprovedAiPlayerTest, BoardEdgeHandling)
{
    // 在左上角放置棋子
    placePieces({{0, 0}, {0, 1}}, Piece::BLACK);

    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // 移动应该在棋盘范围内
    EXPECT_GE(move.first, 0);
    EXPECT_LT(move.first, 15);
    EXPECT_GE(move.second, 0);
    EXPECT_LT(move.second, 15);

    // 位置应该是空的
    EXPECT_EQ(board[move.first][move.second], Piece::EMPTY);
}

// 测试几乎满的棋盘
TEST_F(ImprovedAiPlayerTest, NearlyFullBoard)
{
    // 填充大部分棋盘，只留一个空位
    for (int i = 0; i < 15; ++i)
    {
        for (int j = 0; j < 15; ++j)
        {
            if (i == 7 && j == 7)
                continue; // 留一个空位
            Piece color = ((i + j) % 2 == 0) ? Piece::BLACK : Piece::WHITE;
            game.makeMove(i, j, color);
        }
    }

    auto board = game.getBoard();
    auto move = aiBlack.getNextMove(board);

    // AI应该选择唯一的空位
    EXPECT_EQ(move, std::make_pair(7, 7));
}

// 测试对称局面处理
TEST_F(ImprovedAiPlayerTest, SymmetricPosition)
{
    // 创建对称局面
    placePieces({{7, 7}}, Piece::BLACK);

    auto board = game.getBoard();
    auto move1 = aiBlack.getNextMove(board);

    // 重置棋盘，在对称位置放置棋子
    game.reset();
    placePieces({{7, 7}}, Piece::BLACK);

    // AI的选择应该是一致的（不一定完全相同，但应该是合理的对称移动）
    // 我们只检查移动是否有效
    EXPECT_NE(move1, std::make_pair(-1, -1));
    EXPECT_GE(move1.first, 0);
    EXPECT_LT(move1.first, 15);
    EXPECT_GE(move1.second, 0);
    EXPECT_LT(move1.second, 15);
}

// 测试搜索深度影响
TEST_F(ImprovedAiPlayerTest, SearchDepth)
{
    // 创建需要深度搜索才能找到最佳移动的局面
    std::vector<std::pair<int, int>> blackPositions = {{7, 7}, {7, 9}};
    std::vector<std::pair<int, int>> whitePositions = {{8, 8}, {8, 10}};

    placePieces(blackPositions, Piece::BLACK);
    placePieces(whitePositions, Piece::WHITE);

    // 这里我们主要测试不同深度的AI都能返回有效移动
    ImprovedAiPlayer shallowAi(Piece::BLACK, 2);
    ImprovedAiPlayer deepAi(Piece::BLACK, 4);

    auto board = game.getBoard();
    auto shallowMove = shallowAi.getNextMove(board);
    auto deepMove = deepAi.getNextMove(board);

    // 两个移动都应该是有效的
    EXPECT_NE(shallowMove, std::make_pair(-1, -1));
    EXPECT_NE(deepMove, std::make_pair(-1, -1));

    // 位置应该是空的
    EXPECT_EQ(board[shallowMove.first][shallowMove.second], Piece::EMPTY);
    EXPECT_EQ(board[deepMove.first][deepMove.second], Piece::EMPTY);
}

// 测试性能（不检查具体时间，只检查是否能快速返回）
TEST_F(ImprovedAiPlayerTest, Performance)
{
    // 中等复杂的局面
    std::vector<std::pair<int, int>> blackPositions = {{7, 7}, {8, 8}, {9, 9}, {7, 8}, {8, 9}};
    std::vector<std::pair<int, int>> whitePositions = {{6, 7}, {7, 6}, {8, 7}, {6, 8}, {7, 9}};

    placePieces(blackPositions, Piece::BLACK);
    placePieces(whitePositions, Piece::WHITE);

    auto board = game.getBoard();

    // 多次调用以确保稳定性
    for (int i = 0; i < 5; ++i)
    {
        auto move = aiBlack.getNextMove(board);
        EXPECT_NE(move, std::make_pair(-1, -1));
        EXPECT_EQ(board[move.first][move.second], Piece::EMPTY);
    }
}

// 辅助函数实现
void ImprovedAiPlayerTest::placePieces(const std::vector<std::pair<int, int>> &positions, Piece color)
{
    for (const auto &pos : positions)
    {
        EXPECT_TRUE(game.makeMove(pos.first, pos.second, color));
    }
}