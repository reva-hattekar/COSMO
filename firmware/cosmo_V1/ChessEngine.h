#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <Arduino.h>

// Piece representations
// Positive = White, Negative = Black, 0 = Empty
#define PIECE_EMPTY  0
#define PIECE_PAWN   1
#define PIECE_KNIGHT 2
#define PIECE_BISHOP 3
#define PIECE_ROOK   4
#define PIECE_QUEEN  5
#define PIECE_KING   6

#define COLOR_WHITE  1
#define COLOR_BLACK -1

#define CASTLE_WK 1
#define CASTLE_WQ 2
#define CASTLE_BK 4
#define CASTLE_BQ 8

#define MOVE_NORMAL   0
#define MOVE_CASTLE_K 1
#define MOVE_CASTLE_Q 2
#define MOVE_ENPASSANT 3
#define MOVE_PROMO_Q  4

enum GameStatus {
  STATUS_ACTIVE = 0,
  STATUS_CHECK = 1,
  STATUS_CHECKMATE = 2,
  STATUS_STALEMATE = 3,
  STATUS_DRAW = 4
};

enum AIDifficulty {
  DIFF_EASY = 0,
  DIFF_MEDIUM = 1,
  DIFF_HARD = 2
};

struct ChessMove {
  int8_t from;
  int8_t to;
  int8_t flags;
  int8_t captured;
};

struct EngineStateSnapshot {
  int8_t board[64];
  int8_t turn;
  uint8_t castlingRights;
  int8_t enPassantSq;
  uint8_t halfmoveClock;
  ChessMove lastMove;
};

class ChessEngine {
public:
  ChessEngine();

  void reset();
  int8_t getPiece(int sq) const;
  int8_t getTurn() const;
  int8_t getEnPassantSq() const;
  uint8_t getCastlingRights() const;

  // Move generation & validation
  int generateLegalMoves(int8_t fromSq, ChessMove* outMoves, int maxMoves);
  int generateAllLegalMoves(int8_t color, ChessMove* outMoves, int maxMoves);
  bool isLegalMove(const ChessMove& move);

  // Attack & check queries
  bool isSquareAttacked(int8_t sq, int8_t byColor) const;
  bool isInCheck(int8_t color) const;
  int8_t findKing(int8_t color) const;
  GameStatus evaluateGameStatus();

  // Move execution & Undo
  bool executeMove(const ChessMove& move);
  bool undoMove();
  bool canUndo() const;
  int getHistoryDepth() const;
  const ChessMove* getLastMove() const;

  // AI Minimax Engine
  bool computeAIMove(AIDifficulty difficulty, ChessMove& outMove);

private:
  int8_t board[64];
  int8_t turn;
  uint8_t castlingRights;
  int8_t enPassantSq;
  uint8_t halfmoveClock;

  // History stack for undo (up to 48 half-moves)
  static const int MAX_HISTORY = 48;
  EngineStateSnapshot history[MAX_HISTORY];
  int historyCount;

  // Internal move generation
  int generatePseudoMoves(int8_t fromSq, ChessMove* outMoves, int maxMoves) const;
  int generateAllPseudoMoves(int8_t color, ChessMove* outMoves, int maxMoves) const;
  void applyMoveRaw(const ChessMove& move, EngineStateSnapshot& savedState);
  void restoreMoveRaw(const EngineStateSnapshot& savedState);

  // AI Evaluation
  int evaluateBoard() const;
  int minimax(int depth, int alpha, int beta, bool maximizingPlayer);
};

#endif // CHESS_ENGINE_H

