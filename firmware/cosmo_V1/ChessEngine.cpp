#include "ChessEngine.h"

// Piece square tables (White perspective; mirrored for Black: sq ^ 56)
static const int8_t pawnPst[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
  50, 50, 50, 50, 50, 50, 50, 50,
  10, 10, 20, 30, 30, 20, 10, 10,
   5,  5, 10, 25, 25, 10,  5,  5,
   0,  0,  0, 20, 20,  0,  0,  0,
   5, -5,-10,  0,  0,-10, -5,  5,
   5, 10, 10,-20,-20, 10, 10,  5,
   0,  0,  0,  0,  0,  0,  0,  0
};

static const int8_t knightPst[64] = {
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  0,  0,  0,-20,-40,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50
};

static const int8_t bishopPst[64] = {
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10, 10, 10, 10, 10, 10, 10,-10,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -20,-10,-10,-10,-10,-10,-10,-20
};

static const int8_t rookPst[64] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   5, 10, 10, 10, 10, 10, 10,  5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   0,  0,  0,  5,  5,  0,  0,  0
};

static const int8_t queenPst[64] = {
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -10,  0,  5,  5,  5,  5,  0,-10,
  -5,  0,  5,  5,  5,  5,  0, -5,
   0,  0,  5,  5,  5,  5,  0, -5,
 -10,  5,  5,  5,  5,  5,  0,-10,
 -10,  0,  5,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20
};

static const int8_t kingMidPst[64] = {
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -10,-20,-20,-20,-20,-20,-20,-10,
  20, 20,  0,  0,  0,  0, 20, 20,
  20, 30, 10,  0,  0, 10, 30, 20
};

ChessEngine::ChessEngine() {
  reset();
}

void ChessEngine::reset() {
  // Standard initial board layout
  static const int8_t initBoard[64] = {
    PIECE_ROOK,  PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN, PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK,
    PIECE_PAWN,  PIECE_PAWN,   PIECE_PAWN,   PIECE_PAWN,  PIECE_PAWN, PIECE_PAWN,   PIECE_PAWN,   PIECE_PAWN,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
   -PIECE_PAWN, -PIECE_PAWN,  -PIECE_PAWN,  -PIECE_PAWN, -PIECE_PAWN, -PIECE_PAWN, -PIECE_PAWN, -PIECE_PAWN,
   -PIECE_ROOK, -PIECE_KNIGHT,-PIECE_BISHOP,-PIECE_QUEEN,-PIECE_KING, -PIECE_BISHOP,-PIECE_KNIGHT,-PIECE_ROOK
  };

  for (int i = 0; i < 64; i++) {
    board[i] = initBoard[i];
  }

  turn = COLOR_WHITE;
  castlingRights = CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ;
  enPassantSq = -1;
  halfmoveClock = 0;
  historyCount = 0;
}

int8_t ChessEngine::getPiece(int sq) const {
  if (sq < 0 || sq >= 64) return 0;
  return board[sq];
}

int8_t ChessEngine::getTurn() const {
  return turn;
}

int8_t ChessEngine::getEnPassantSq() const {
  return enPassantSq;
}

uint8_t ChessEngine::getCastlingRights() const {
  return castlingRights;
}

int8_t ChessEngine::findKing(int8_t color) const {
  int8_t target = (color == COLOR_WHITE) ? PIECE_KING : -PIECE_KING;
  for (int8_t i = 0; i < 64; i++) {
    if (board[i] == target) return i;
  }
  return -1;
}

bool ChessEngine::isSquareAttacked(int8_t sq, int8_t byColor) const {
  int8_t r = sq / 8;
  int8_t f = sq % 8;

  // 1. Pawn attacks
  int8_t pawnRow = r - byColor;
  if (pawnRow >= 0 && pawnRow <= 7) {
    if (f > 0 && board[pawnRow * 8 + (f - 1)] == (byColor * PIECE_PAWN)) return true;
    if (f < 7 && board[pawnRow * 8 + (f + 1)] == (byColor * PIECE_PAWN)) return true;
  }

  // 2. Knight attacks
  static const int8_t kOffsets[8][2] = {
    {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
  };
  for (int i = 0; i < 8; i++) {
    int8_t nr = r + kOffsets[i][0];
    int8_t nf = f + kOffsets[i][1];
    if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
      if (board[nr * 8 + nf] == (byColor * PIECE_KNIGHT)) return true;
    }
  }

  // 3. Bishop / Queen diagonal rays
  static const int8_t diagDirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
  for (int i = 0; i < 4; i++) {
    int8_t cr = r + diagDirs[i][0];
    int8_t cf = f + diagDirs[i][1];
    while (cr >= 0 && cr <= 7 && cf >= 0 && cf <= 7) {
      int8_t p = board[cr * 8 + cf];
      if (p != 0) {
        if (p == (byColor * PIECE_BISHOP) || p == (byColor * PIECE_QUEEN)) return true;
        break;
      }
      cr += diagDirs[i][0];
      cf += diagDirs[i][1];
    }
  }

  // 4. Rook / Queen orthogonal rays
  static const int8_t orthoDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
  for (int i = 0; i < 4; i++) {
    int8_t cr = r + orthoDirs[i][0];
    int8_t cf = f + orthoDirs[i][1];
    while (cr >= 0 && cr <= 7 && cf >= 0 && cf <= 7) {
      int8_t p = board[cr * 8 + cf];
      if (p != 0) {
        if (p == (byColor * PIECE_ROOK) || p == (byColor * PIECE_QUEEN)) return true;
        break;
      }
      cr += orthoDirs[i][0];
      cf += orthoDirs[i][1];
    }
  }

  // 5. King attacks (adjacent squares)
  static const int8_t kingOffsets[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
  };
  for (int i = 0; i < 8; i++) {
    int8_t nr = r + kingOffsets[i][0];
    int8_t nf = f + kingOffsets[i][1];
    if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
      if (board[nr * 8 + nf] == (byColor * PIECE_KING)) return true;
    }
  }

  return false;
}

bool ChessEngine::isInCheck(int8_t color) const {
  int8_t kingSq = findKing(color);
  if (kingSq < 0) return false;
  return isSquareAttacked(kingSq, -color);
}

int ChessEngine::generatePseudoMoves(int8_t fromSq, ChessMove* outMoves, int maxMoves) const {
  int8_t piece = board[fromSq];
  if (piece == 0) return 0;

  int8_t pColor = (piece > 0) ? COLOR_WHITE : COLOR_BLACK;
  int8_t pType = abs(piece);
  int8_t r = fromSq / 8;
  int8_t f = fromSq % 8;
  int count = 0;

  #define ADD_MOVE(dest, flg, cap) do { \
    if (count < maxMoves) { \
      outMoves[count].from = fromSq; \
      outMoves[count].to = (dest); \
      outMoves[count].flags = (flg); \
      outMoves[count].captured = (cap); \
      count++; \
    } \
  } while(0)

  // 1. PAWN
  if (pType == PIECE_PAWN) {
    int8_t fwd = (pColor == COLOR_WHITE) ? 1 : -1;
    int8_t startRank = (pColor == COLOR_WHITE) ? 1 : 6;
    int8_t promoRank = (pColor == COLOR_WHITE) ? 7 : 0;

    // Single push
    int8_t nextSq = (r + fwd) * 8 + f;
    if (nextSq >= 0 && nextSq <= 63 && board[nextSq] == 0) {
      if ((r + fwd) == promoRank) {
        ADD_MOVE(nextSq, MOVE_PROMO_Q, 0);
      } else {
        ADD_MOVE(nextSq, MOVE_NORMAL, 0);
        // Double push from start rank
        if (r == startRank) {
          int8_t dblSq = (r + 2 * fwd) * 8 + f;
          if (board[dblSq] == 0) {
            ADD_MOVE(dblSq, MOVE_NORMAL, 0);
          }
        }
      }
    }

    // Diagonal captures
    int8_t capFiles[2] = { (int8_t)(f - 1), (int8_t)(f + 1) };
    for (int i = 0; i < 2; i++) {
      int8_t cf = capFiles[i];
      if (cf >= 0 && cf <= 7) {
        int8_t targetSq = (r + fwd) * 8 + cf;
        int8_t targetPiece = board[targetSq];
        if (targetPiece != 0 && ((targetPiece > 0) != (piece > 0))) {
          if ((r + fwd) == promoRank) {
            ADD_MOVE(targetSq, MOVE_PROMO_Q, targetPiece);
          } else {
            ADD_MOVE(targetSq, MOVE_NORMAL, targetPiece);
          }
        } else if (targetSq == enPassantSq && enPassantSq >= 0) {
          // En passant capture
          int8_t capSq = r * 8 + cf;
          ADD_MOVE(targetSq, MOVE_ENPASSANT, board[capSq]);
        }
      }
    }
  }

  // 2. KNIGHT
  else if (pType == PIECE_KNIGHT) {
    static const int8_t offsets[8][2] = {
      {1, 2}, {2, 1}, {-1, 2}, {-2, 1}, {1, -2}, {2, -1}, {-1, -2}, {-2, -1}
    };
    for (int i = 0; i < 8; i++) {
      int8_t nr = r + offsets[i][0];
      int8_t nf = f + offsets[i][1];
      if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
        int8_t targetSq = nr * 8 + nf;
        int8_t tp = board[targetSq];
        if (tp == 0 || ((tp > 0) != (piece > 0))) {
          ADD_MOVE(targetSq, MOVE_NORMAL, tp);
        }
      }
    }
  }

  // 3. BISHOP / QUEEN (Diagonals)
  if (pType == PIECE_BISHOP || pType == PIECE_QUEEN) {
    static const int8_t dirs[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (int i = 0; i < 4; i++) {
      int8_t cr = r + dirs[i][0];
      int8_t cf = f + dirs[i][1];
      while (cr >= 0 && cr <= 7 && cf >= 0 && cf <= 7) {
        int8_t targetSq = cr * 8 + cf;
        int8_t tp = board[targetSq];
        if (tp == 0) {
          ADD_MOVE(targetSq, MOVE_NORMAL, 0);
        } else {
          if ((tp > 0) != (piece > 0)) {
            ADD_MOVE(targetSq, MOVE_NORMAL, tp);
          }
          break;
        }
        cr += dirs[i][0];
        cf += dirs[i][1];
      }
    }
  }

  // 4. ROOK / QUEEN (Orthogonals)
  if (pType == PIECE_ROOK || pType == PIECE_QUEEN) {
    static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; i++) {
      int8_t cr = r + dirs[i][0];
      int8_t cf = f + dirs[i][1];
      while (cr >= 0 && cr <= 7 && cf >= 0 && cf <= 7) {
        int8_t targetSq = cr * 8 + cf;
        int8_t tp = board[targetSq];
        if (tp == 0) {
          ADD_MOVE(targetSq, MOVE_NORMAL, 0);
        } else {
          if ((tp > 0) != (piece > 0)) {
            ADD_MOVE(targetSq, MOVE_NORMAL, tp);
          }
          break;
        }
        cr += dirs[i][0];
        cf += dirs[i][1];
      }
    }
  }

  // 5. KING
  if (pType == PIECE_KING) {
    static const int8_t offsets[8][2] = {
      {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    for (int i = 0; i < 8; i++) {
      int8_t nr = r + offsets[i][0];
      int8_t nf = f + offsets[i][1];
      if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) {
        int8_t targetSq = nr * 8 + nf;
        int8_t tp = board[targetSq];
        if (tp == 0 || ((tp > 0) != (piece > 0))) {
          ADD_MOVE(targetSq, MOVE_NORMAL, tp);
        }
      }
    }

    // Castling checks (King must not be in check currently)
    if (!isInCheck(pColor)) {
      if (pColor == COLOR_WHITE && fromSq == 4) {
        // White Kingside: e1 -> g1 (sq 4 -> 6)
        if ((castlingRights & CASTLE_WK) && board[5] == 0 && board[6] == 0) {
          if (!isSquareAttacked(5, COLOR_BLACK) && !isSquareAttacked(6, COLOR_BLACK)) {
            ADD_MOVE(6, MOVE_CASTLE_K, 0);
          }
        }
        // White Queenside: e1 -> c1 (sq 4 -> 2)
        if ((castlingRights & CASTLE_WQ) && board[1] == 0 && board[2] == 0 && board[3] == 0) {
          if (!isSquareAttacked(3, COLOR_BLACK) && !isSquareAttacked(2, COLOR_BLACK)) {
            ADD_MOVE(2, MOVE_CASTLE_Q, 0);
          }
        }
      } else if (pColor == COLOR_BLACK && fromSq == 60) {
        // Black Kingside: e8 -> g8 (sq 60 -> 62)
        if ((castlingRights & CASTLE_BK) && board[61] == 0 && board[62] == 0) {
          if (!isSquareAttacked(61, COLOR_WHITE) && !isSquareAttacked(62, COLOR_WHITE)) {
            ADD_MOVE(62, MOVE_CASTLE_K, 0);
          }
        }
        // Black Queenside: e8 -> c8 (sq 60 -> 58)
        if ((castlingRights & CASTLE_BQ) && board[57] == 0 && board[58] == 0 && board[59] == 0) {
          if (!isSquareAttacked(59, COLOR_WHITE) && !isSquareAttacked(58, COLOR_WHITE)) {
            ADD_MOVE(58, MOVE_CASTLE_Q, 0);
          }
        }
      }
    }
  }

  #undef ADD_MOVE
  return count;
}

int ChessEngine::generateAllPseudoMoves(int8_t color, ChessMove* outMoves, int maxMoves) const {
  int total = 0;
  for (int8_t sq = 0; sq < 64; sq++) {
    int8_t p = board[sq];
    if (p != 0 && ((p > 0) == (color == COLOR_WHITE))) {
      int count = generatePseudoMoves(sq, outMoves + total, maxMoves - total);
      total += count;
      if (total >= maxMoves) break;
    }
  }
  return total;
}

void ChessEngine::applyMoveRaw(const ChessMove& move, EngineStateSnapshot& saved) {
  // Save current state for rollback
  memcpy(saved.board, board, 64);
  saved.turn = turn;
  saved.castlingRights = castlingRights;
  saved.enPassantSq = enPassantSq;
  saved.halfmoveClock = halfmoveClock;
  saved.lastMove = move;

  int8_t movingPiece = board[move.from];
  int8_t pColor = (movingPiece > 0) ? COLOR_WHITE : COLOR_BLACK;
  int8_t pType = abs(movingPiece);

  // Update halfmove clock (50-move rule)
  if (pType == PIECE_PAWN || move.captured != 0) {
    halfmoveClock = 0;
  } else {
    halfmoveClock++;
  }

  // Move the piece
  board[move.to] = movingPiece;
  board[move.from] = 0;

  // Handle special moves
  enPassantSq = -1;

  if (pType == PIECE_PAWN) {
    // Pawn double push -> set en passant target
    if (abs(move.to - move.from) == 16) {
      enPassantSq = (move.from + move.to) / 2;
    }
    // En passant capture -> remove captured pawn
    else if (move.flags == MOVE_ENPASSANT) {
      int8_t capSq = (move.from / 8) * 8 + (move.to % 8);
      board[capSq] = 0;
    }
    // Pawn promotion
    else if (move.flags == MOVE_PROMO_Q) {
      board[move.to] = pColor * PIECE_QUEEN;
    }
  }
  // Castling -> move the rook as well
  else if (pType == PIECE_KING) {
    if (move.flags == MOVE_CASTLE_K) {
      if (pColor == COLOR_WHITE) { board[7] = 0; board[5] = PIECE_ROOK; }
      else { board[63] = 0; board[61] = -PIECE_ROOK; }
    } else if (move.flags == MOVE_CASTLE_Q) {
      if (pColor == COLOR_WHITE) { board[0] = 0; board[3] = PIECE_ROOK; }
      else { board[56] = 0; board[59] = -PIECE_ROOK; }
    }
  }

  // Update castling rights if King or Rook moved or was captured
  if (movingPiece == PIECE_KING) {
    castlingRights &= ~(CASTLE_WK | CASTLE_WQ);
  } else if (movingPiece == -PIECE_KING) {
    castlingRights &= ~(CASTLE_BK | CASTLE_BQ);
  } else if (movingPiece == PIECE_ROOK) {
    if (move.from == 0) castlingRights &= ~CASTLE_WQ;
    else if (move.from == 7) castlingRights &= ~CASTLE_WK;
  } else if (movingPiece == -PIECE_ROOK) {
    if (move.from == 56) castlingRights &= ~CASTLE_BQ;
    else if (move.from == 63) castlingRights &= ~CASTLE_BK;
  }

  if (move.to == 0) castlingRights &= ~CASTLE_WQ;
  else if (move.to == 7) castlingRights &= ~CASTLE_WK;
  else if (move.to == 56) castlingRights &= ~CASTLE_BQ;
  else if (move.to == 63) castlingRights &= ~CASTLE_BK;

  // Toggle turn
  turn = -turn;
}

void ChessEngine::restoreMoveRaw(const EngineStateSnapshot& saved) {
  memcpy(board, saved.board, 64);
  turn = saved.turn;
  castlingRights = saved.castlingRights;
  enPassantSq = saved.enPassantSq;
  halfmoveClock = saved.halfmoveClock;
}

int ChessEngine::generateLegalMoves(int8_t fromSq, ChessMove* outMoves, int maxMoves) {
  ChessMove pseudo[32];
  int pseudoCount = generatePseudoMoves(fromSq, pseudo, 32);
  int legalCount = 0;
  int8_t pColor = (board[fromSq] > 0) ? COLOR_WHITE : COLOR_BLACK;

  EngineStateSnapshot snap;
  for (int i = 0; i < pseudoCount; i++) {
    applyMoveRaw(pseudo[i], snap);
    if (!isInCheck(pColor)) {
      if (legalCount < maxMoves) {
        outMoves[legalCount++] = pseudo[i];
      }
    }
    restoreMoveRaw(snap);
  }

  return legalCount;
}

int ChessEngine::generateAllLegalMoves(int8_t color, ChessMove* outMoves, int maxMoves) {
  int total = 0;
  for (int8_t sq = 0; sq < 64; sq++) {
    int8_t p = board[sq];
    if (p != 0 && ((p > 0) == (color == COLOR_WHITE))) {
      int count = generateLegalMoves(sq, outMoves + total, maxMoves - total);
      total += count;
      if (total >= maxMoves) break;
    }
  }
  return total;
}

bool ChessEngine::isLegalMove(const ChessMove& move) {
  ChessMove legalMoves[32];
  int count = generateLegalMoves(move.from, legalMoves, 32);
  for (int i = 0; i < count; i++) {
    if (legalMoves[i].to == move.to) return true;
  }
  return false;
}

GameStatus ChessEngine::evaluateGameStatus() {
  ChessMove moves[64];
  int count = generateAllLegalMoves(turn, moves, 64);

  if (count == 0) {
    if (isInCheck(turn)) {
      return STATUS_CHECKMATE;
    }
    return STATUS_STALEMATE;
  }

  if (halfmoveClock >= 100) {
    return STATUS_DRAW; // 50-move rule
  }

  if (isInCheck(turn)) {
    return STATUS_CHECK;
  }

  return STATUS_ACTIVE;
}

bool ChessEngine::executeMove(const ChessMove& move) {
  if (historyCount >= MAX_HISTORY) {
    // Shift history back by 1 if full
    for (int i = 0; i < MAX_HISTORY - 1; i++) {
      history[i] = history[i + 1];
    }
    historyCount = MAX_HISTORY - 1;
  }

  applyMoveRaw(move, history[historyCount]);
  historyCount++;
  return true;
}

bool ChessEngine::undoMove() {
  if (historyCount <= 0) return false;
  historyCount--;
  restoreMoveRaw(history[historyCount]);
  return true;
}

bool ChessEngine::canUndo() const {
  return historyCount > 0;
}

int ChessEngine::getHistoryDepth() const {
  return historyCount;
}

const ChessMove* ChessEngine::getLastMove() const {
  if (historyCount <= 0) return nullptr;
  return &history[historyCount - 1].lastMove;
}

// ======================================
// AI MINIMAX ENGINE
// ======================================

int ChessEngine::evaluateBoard() const {
  static const int pieceValues[7] = {0, 100, 320, 330, 500, 900, 20000};
  int score = 0;

  for (int sq = 0; sq < 64; sq++) {
    int8_t p = board[sq];
    if (p == 0) continue;

    int8_t type = abs(p);
    int val = pieceValues[type];
    int pst = 0;

    int evalSq = (p > 0) ? sq : (sq ^ 56);

    switch (type) {
      case PIECE_PAWN:   pst = pawnPst[evalSq]; break;
      case PIECE_KNIGHT: pst = knightPst[evalSq]; break;
      case PIECE_BISHOP: pst = bishopPst[evalSq]; break;
      case PIECE_ROOK:   pst = rookPst[evalSq]; break;
      case PIECE_QUEEN:  pst = queenPst[evalSq]; break;
      case PIECE_KING:   pst = kingMidPst[evalSq]; break;
    }

    if (p > 0) {
      score += (val + pst);
    } else {
      score -= (val + pst);
    }
  }

  return score;
}

int ChessEngine::minimax(int depth, int alpha, int beta, bool maximizing) {
  if (depth == 0) {
    return evaluateBoard();
  }

  int8_t activeColor = maximizing ? COLOR_WHITE : COLOR_BLACK;
  ChessMove moves[64];
  int count = generateAllLegalMoves(activeColor, moves, 64);

  if (count == 0) {
    if (isInCheck(activeColor)) {
      return maximizing ? (-20000 + (3 - depth) * 100) : (20000 - (3 - depth) * 100);
    }
    return 0; // Stalemate
  }

  // Move ordering: put captures first for better alpha-beta pruning
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (abs(moves[j].captured) > abs(moves[i].captured)) {
        ChessMove tmp = moves[i];
        moves[i] = moves[j];
        moves[j] = tmp;
      }
    }
  }

  EngineStateSnapshot snap;
  if (maximizing) {
    int maxEval = -30000;
    for (int i = 0; i < count; i++) {
      applyMoveRaw(moves[i], snap);
      int eval = minimax(depth - 1, alpha, beta, false);
      restoreMoveRaw(snap);

      if (eval > maxEval) maxEval = eval;
      if (eval > alpha) alpha = eval;
      if (beta <= alpha) break; // Beta cut-off
    }
    return maxEval;
  } else {
    int minEval = 30000;
    for (int i = 0; i < count; i++) {
      applyMoveRaw(moves[i], snap);
      int eval = minimax(depth - 1, alpha, beta, true);
      restoreMoveRaw(snap);

      if (eval < minEval) minEval = eval;
      if (eval < beta) beta = eval;
      if (beta <= alpha) break; // Alpha cut-off
    }
    return minEval;
  }
}

bool ChessEngine::computeAIMove(AIDifficulty difficulty, ChessMove& outMove) {
  int depth = 2; // Default Medium
  if (difficulty == DIFF_EASY) depth = 1;
  else if (difficulty == DIFF_HARD) depth = 3;

  ChessMove moves[64];
  int count = generateAllLegalMoves(turn, moves, 64);
  if (count == 0) return false;

  // Move ordering
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (abs(moves[j].captured) > abs(moves[i].captured)) {
        ChessMove tmp = moves[i];
        moves[i] = moves[j];
        moves[j] = tmp;
      }
    }
  }

  bool maximizing = (turn == COLOR_WHITE);
  int bestScore = maximizing ? -30000 : 30000;
  int bestIndex = 0;

  // If Easy, introduce 20% randomness among decent moves
  if (difficulty == DIFF_EASY && count > 1 && (random(100) < 25)) {
    bestIndex = random(min(count, 3));
    outMove = moves[bestIndex];
    return true;
  }

  EngineStateSnapshot snap;
  int alpha = -30000;
  int beta = 30000;

  for (int i = 0; i < count; i++) {
    applyMoveRaw(moves[i], snap);
    int score = minimax(depth - 1, alpha, beta, !maximizing);
    restoreMoveRaw(snap);

    if (maximizing) {
      if (score > bestScore) {
        bestScore = score;
        bestIndex = i;
      }
      if (score > alpha) alpha = score;
    } else {
      if (score < bestScore) {
        bestScore = score;
        bestIndex = i;
      }
      if (score < beta) beta = score;
    }
  }

  outMove = moves[bestIndex];
  return true;
}

