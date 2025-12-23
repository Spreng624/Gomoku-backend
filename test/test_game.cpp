#include <gtest/gtest.h>
#include "Game.h"

class GameTest : public ::testing::Test
{
protected:
    Game game{15};

    void SetUp() override
    {
        game.reset();
    }
};

// 测试棋盘初始化
TEST_F(GameTest, BoardInitialization)
{
    EXPECT_EQ(game.getBoardSize(), 15);
    auto board = game.getBoard();
    for (int i = 0; i < 15; ++i)
    {
        for (int j = 0; j < 15; ++j)
        {
            EXPECT_EQ(board[i][j], Piece::EMPTY);
        }
    }
}

// 测试有效落子
TEST_F(GameTest, ValidMove)
{
    EXPECT_TRUE(game.makeMove(7, 7, Piece::BLACK));
    auto board = game.getBoard();
    EXPECT_EQ(board[7][7], Piece::BLACK);
}

// 测试重复落子（应该失败）
TEST_F(GameTest, DuplicateMove)
{
    EXPECT_TRUE(game.makeMove(7, 7, Piece::BLACK));
    EXPECT_FALSE(game.makeMove(7, 7, Piece::WHITE));
}

// 测试越界落子（应该失败）
TEST_F(GameTest, OutOfBoundsMove)
{
    EXPECT_FALSE(game.makeMove(-1, 0, Piece::BLACK));
    EXPECT_FALSE(game.makeMove(15, 0, Piece::BLACK));
    EXPECT_FALSE(game.makeMove(0, -1, Piece::BLACK));
    EXPECT_FALSE(game.makeMove(0, 15, Piece::BLACK));
}

// 测试水平赢
TEST_F(GameTest, HorizontalWin)
{
    for (int i = 0; i < 5; ++i)
    {
        game.makeMove(7, 7 + i, Piece::BLACK);
    }
    EXPECT_EQ(game.checkWin(7, 10), Piece::BLACK);
}

// 测试垂直赢
TEST_F(GameTest, VerticalWin)
{
    for (int i = 0; i < 5; ++i)
    {
        game.makeMove(7 + i, 7, Piece::WHITE);
    }
    EXPECT_EQ(game.checkWin(11, 7), Piece::WHITE);
}

// 测试对角线赢
TEST_F(GameTest, DiagonalWin)
{
    for (int i = 0; i < 5; ++i)
    {
        game.makeMove(7 + i, 7 + i, Piece::BLACK);
    }
    EXPECT_EQ(game.checkWin(11, 11), Piece::BLACK);
}

// 测试副对角线赢
TEST_F(GameTest, AntiDiagonalWin)
{
    for (int i = 0; i < 5; ++i)
    {
        game.makeMove(7 + i, 11 - i, Piece::WHITE);
    }
    EXPECT_EQ(game.checkWin(11, 7), Piece::WHITE);
}

// 测试未赢（4连）
TEST_F(GameTest, NotWinFour)
{
    for (int i = 0; i < 4; ++i)
    {
        game.makeMove(7, 7 + i, Piece::BLACK);
    }
    EXPECT_EQ(game.checkWin(7, 9), Piece::EMPTY);
}

// 测试悔棋功能
TEST_F(GameTest, UndoMove)
{
    EXPECT_TRUE(game.makeMove(7, 7, Piece::BLACK));
    EXPECT_TRUE(game.makeMove(7, 8, Piece::WHITE));

    EXPECT_EQ(game.getMoveCount(), 2);

    // 悔棋一步
    EXPECT_TRUE(game.undoMove());
    EXPECT_EQ(game.getMoveCount(), 1);

    auto board = game.getBoard();
    EXPECT_EQ(board[7][7], Piece::BLACK);
    EXPECT_EQ(board[7][8], Piece::EMPTY);

    // 再悔棋一步
    EXPECT_TRUE(game.undoMove());
    EXPECT_EQ(game.getMoveCount(), 0);
    EXPECT_EQ(board[7][7], Piece::EMPTY);

    // 没有棋可悔
    EXPECT_FALSE(game.undoMove());
}

// 测试棋盘是否已满
TEST_F(GameTest, IsBoardFull)
{
    // 填充整个棋盘
    for (int i = 0; i < 15; ++i)
    {
        for (int j = 0; j < 15; ++j)
        {
            Piece color = ((i + j) % 2 == 0) ? Piece::BLACK : Piece::WHITE;
            EXPECT_TRUE(game.makeMove(i, j, color));
        }
    }

    EXPECT_TRUE(game.isBoardFull());
    EXPECT_EQ(game.getMoveCount(), 225);

    // 棋盘已满后不能再落子
    EXPECT_FALSE(game.makeMove(0, 0, Piece::BLACK));
}

// 测试检查整个棋盘胜利者
TEST_F(GameTest, CheckWinWholeBoard)
{
    // 没有胜利者
    EXPECT_EQ(game.checkWin(), Piece::EMPTY);

    // 创建水平胜利
    for (int i = 0; i < 5; ++i)
    {
        game.makeMove(7, 7 + i, Piece::BLACK);
    }

    EXPECT_EQ(game.checkWin(), Piece::BLACK);
}

// 测试落子计数
TEST_F(GameTest, MoveCount)
{
    EXPECT_EQ(game.getMoveCount(), 0);

    game.makeMove(0, 0, Piece::BLACK);
    EXPECT_EQ(game.getMoveCount(), 1);

    game.makeMove(0, 1, Piece::WHITE);
    EXPECT_EQ(game.getMoveCount(), 2);

    game.undoMove();
    EXPECT_EQ(game.getMoveCount(), 1);

    game.undoMove();
    EXPECT_EQ(game.getMoveCount(), 0);
}
