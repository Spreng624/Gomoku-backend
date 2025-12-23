#ifndef GAME_H
#define GAME_H

#include <iostream>
#include <vector>
#include <utility>
#include <stack>

enum class Piece
{
  EMPTY = 0,
  BLACK = 1,
  WHITE = 2
};

class Game
{
private:
  int boardSize;
  std::vector<std::vector<Piece>> board;
  std::stack<std::pair<int, int>> moveHistory; // 落子历史记录
  std::pair<int, int> lastMove;
  double blackTime;
  double whiteTime;
  int countLine(int startX, int startY, int dx, int dy, Piece color) const;

public:
  Game();
  Game(int boardSize);
  void reset();
  int getBoardSize() const;
  bool makeMove(int x, int y, Piece color);
  bool undoMove(); // 悔棋
  Piece checkWin(int x, int y) const;
  Piece checkWin() const; // 检查整个棋盘是否有胜利者
  std::vector<std::vector<Piece>> getBoard() const;
  std::pair<int, int> getLastMove() const;
  bool isBoardFull() const; // 检查棋盘是否已满
  int getMoveCount() const; // 获取落子数量
};

#endif