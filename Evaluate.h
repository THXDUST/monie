#ifndef EVALUATE_H
#define EVALUATE_H

#include "ChessBoard.h"

typedef struct Variation {
    Move* line;
    int score;
    int depth;
    int length;
} Variation;

typedef struct Evaluate {
    Variation* variations;
    int multiPvCount;
} Evaluate;

Evaluate Evaluate_findBestMove(ChessBoard* board, int depth, int multiPvCount);
int scorePosition(ChessBoard* board);
void tt_init(void);
void tt_free(void);

#endif