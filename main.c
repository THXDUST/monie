#define _CRT_SECURE_NO_WARNINGS

#include "ChessBoard.h"
#include "Evaluate.h"
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

void playPvP(ChessBoard* board) {
    char uciMove[10];
    while (!ChessBoard_isGameOver(board)) {
        printf("Enter your move (UCI format, e.g., e2e4): ");
        scanf("%s", uciMove);
        if (!ChessBoard_moveFromUCI(board, uciMove)) {
            printf("Invalid move. Try again.\n");
            continue;
        }
        ChessBoard_printBoard(board);
    }
}

void playPvE(ChessBoard* board, int color, int depth) {
    char uciMove[10];
	while (!ChessBoard_isGameOver(board)) {
        if (board->side == color) {
            printf("Enter your move (UCI format, e.g., e2e4): ");
            scanf("%s", uciMove);
            if (!ChessBoard_moveFromUCI(board, uciMove)) {
                printf("Invalid move. Try again.\n");
                continue;
            }
            ChessBoard_printBoard(board);
        }
        else {
            printf("---------------------\n");
            Evaluate eval = Evaluate_findBestMove(board, depth, 1);
            Move bestMove = eval.variations[0].line[0];
            int from = bestMove.from;
            int to = bestMove.to;
            int scoreBot = eval.variations[0].score;
            printf("Best move: %c%d%c%d (%.2f)\n", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, scoreBot / 100.0f);
            snprintf(uciMove, sizeof(uciMove), "%c%d%c%d%c", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, bestMove.promoteTo);
            ChessBoard_moveFromUCI(board, uciMove);
            ChessBoard_printBoard(board);
        }
    }
}

void playEvE(ChessBoard* board, int depth_white, int depth_black) {
    char uciMove[10];
    while (!ChessBoard_isGameOver(board)) {
        printf("---------------------\n");
        int depth = (board->side == WHITE) ? depth_white : depth_black;
        Evaluate eval = Evaluate_findBestMove(board, depth, 1);
        Move bestMove = eval.variations[0].line[0];
        int from = bestMove.from;
        int to = bestMove.to;
        int scoreBot = eval.variations[0].score;
        printf("Best move: %c%d%c%d (%.2f)\n", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, scoreBot / 100.0f);
        snprintf(uciMove, sizeof(uciMove), "%c%d%c%d%c", (from % 8) + 'a', (from / 8) + 1, (to % 8) + 'a', (to / 8) + 1, bestMove.promoteTo);
        ChessBoard_moveFromUCI(board, uciMove);
        ChessBoard_printBoard(board);
    }
}


int main() {
    ChessBoard board;

    int choice, choice2, depth;
    int depth_white, depth_black;

    char* fenInicial = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    ChessBoard_setupFromFen(&board, fenInicial);

    int score = -scorePosition(&board);
    int from, to, scoreBot;
    Evaluate eval;
    Move bestMove;
    char uciMove[10];

    printf("Escolha:\n\t1. Jogar contra player\n\t2. Jogar contra engine\n\t3. Assistir engine contra engine\n~> ");
    scanf("%d", &choice);

    if (choice == 2) {
		printf("Qual profundidade de busca? (ex: 4)\n~> ");
		scanf("%d", &depth);
    } else if (choice == 3) {
        printf("Profundidade para as brancas? (ex: 4)\n~> ");
        scanf("%d", &depth_white);
        printf("Profundidade para as pretas? (ex: 4)\n~> ");
        scanf("%d", &depth_black);
	}

    switch (choice) {
    case 1:
        playPvP(&board);
        break;
    case 2:
        printf("\n\n\t1. Jogar como brancas\n\t2. Jogar como pretas\n~> ");
        scanf("%d", &choice2);
        if (choice2 == 1) playPvE(&board, WHITE, depth);
        else playPvE(&board, BLACK, depth);
        break;
    case 3:
        playEvE(&board, depth_white, depth_black);
        break;
    }

    printf("\n\nGame Over: %d \n", ChessBoard_isGameOver(&board));

    return 0;
}