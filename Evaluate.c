#include "Evaluate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <intrin.h>
#include <time.h>

#include "ChessBoard.h"

#define TT_SIZE 16777216
#define TT_MASK (TT_SIZE - 1)
#define TT_EXACT 0
#define TT_LOWER 1
#define TT_UPPER 2

static clock_t search_start_time;
static double max_search_time = 5.0;
static int nodes_checked = 0;
static int qnodes_checked = 0;
static int tt_hits = 0;
static int cutoffs = 0;
static int futil_cuts = 0;

typedef struct {
    unsigned long long hash;
    int score;
    int depth;
    char flag;
    Move bestMove;
} TTEntry;

static TTEntry* transposition_table = NULL;

// Piece values
const int pieceValues[] = {
//   0    1    2    3    4     5
    100, 350, 350, 525, 1025, 32767,
    -100, -350, -350, -525, -1025, -32767
//    6     7     8     9     10     11
};

// PST (apenas para WHITE)
const int pstWhite[6][64] = {
    { 0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 20, 30, 40, 40, 30, 20, 10,
      5, 10, 15, 20, 20, 15, 10,  5,
      0,  5, 10, 15, 15, 10,  5,  0,
      5,  5, 10, 15, 15, 10,  5,  5,
      5, 10, 10, -20, -20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0 }, // P

    { -50, -30, -20, -20, -20, -20, -30, -50,
      -40,  -10,   0,   5,   5,   0, -10, -40,
      -30,   5,  15,  20,  20,  15,   5, -30,
      -25,  10,  25,  30,  30,  25,  10, -25,
      -20,  10,  25,  30,  30,  25,  10, -20,
      -30,   5,  15,  20,  20,  15,   5, -30,
      -40, -10,   0,   5,   5,   0, -10, -40,
      -50, -30, -20, -20, -20, -20, -30, -50 }, // N

    { -20, -10,  -5,  -5,  -5,  -5, -10, -20,
      -10,   8,  10,  15,  15,  10,   8, -10,
       -5,  10,  18,  22,  22,  18,  10,  -5,
       -5,  15,  22,  28,  28,  22,  15,  -5,
       -5,  15,  22,  28,  28,  22,  15,  -5,
       -5,  10,  18,  22,  22,  18,  10,  -5,
      -10,   8,  10,  15,  15,  10,   8, -10,
      -20, -10,  -5,  -5,  -5,  -5, -10, -20 }, // B

    {   0,   0,   0,   5,   5,   0,   0,   0,
       -5,   0,   5,  10,  10,   5,   0,  -5,
       -5,   5,  10,  15,  15,  10,   5,  -5,
       -5,  10,  15,  20,  20,  15,  10,  -5,
       -5,  10,  15,  20,  20,  15,  10,  -5,
       -5,   5,  10,  15,  15,  10,   5,  -5,
       -5,   0,   5,  10,  10,   5,   0,  -5,
        0,   0,   0,   5,   5,   0,   0,   0 }, // R

    { -20, -10, -10,  -5,  -5, -10, -10, -20,
      -10,   0,   5,  10,  10,   5,   0, -10,
      -10,   5,  10,  15,  15,  10,   5, -10,
       -5,  10,  15,  25,  25,  15,  10,  -5,
        0,  10,  15,  25,  25,  15,  10,   0,
      -10,   5,  10,  15,  15,  10,   5, -10,
      -10,   0,   5,  10,  10,   5,   0, -10,
      -20, -10, -10,  -5,  -5, -10, -10, -20 }, // Q

    { -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -20, -30, -30, -40, -40, -30, -30, -20,
      -10, -20, -20, -20, -20, -20, -20, -10,
       20,  20,   0,   0,   0,   0,  20,  20,
       20,  30,  10,   0,   0,  10,  30,  20 }  // K
};

int history_table[2][64][64];
int countermove_table[2][64][64];

uint64_t popcount64(uint64_t x) {
    x -= (x >> 1) & 0x5555555555555555;
    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;
    x += x >> 8;
    x += x >> 16;
    x += x >> 32;
    return x & 0x7F;
}

void tt_init() {
    if (!transposition_table) transposition_table = calloc(TT_SIZE, sizeof(TTEntry));
    memset(history_table, 0, sizeof(history_table));
    memset(countermove_table, 0, sizeof(countermove_table));
    nodes_checked = 0;
    qnodes_checked = 0;
    tt_hits = 0;
    cutoffs = 0;
    futil_cuts = 0;
}

/* Só garante memória da TT sem zerar entradas (reuso entre jogadas acelera a busca) */
static void tt_ensure(void) {
    if (!transposition_table) transposition_table = calloc(TT_SIZE, sizeof(TTEntry));
}

void tt_free() {
    if (transposition_table) free(transposition_table);
    transposition_table = NULL;
}

void tt_clear() {
    if (transposition_table) memset(transposition_table, 0, TT_SIZE * sizeof(TTEntry));
    memset(history_table, 0, sizeof(history_table));
    memset(countermove_table, 0, sizeof(countermove_table));
}

void tt_store(unsigned long long hash, int score, int depth, char flag, Move bestMove) {
    if (!transposition_table) return;
    int index = hash & TT_MASK;
    TTEntry* e = &transposition_table[index];
    /* Não substituir entrada mais profunda do mesmo nó (evita "downgrades") */
    if (e->hash == hash && e->depth > depth) return;

    e->hash = hash;
    e->score = score;
    e->depth = depth;
    e->flag = flag;
    e->bestMove = bestMove;
}

int tt_probe(unsigned long long hash, int depth, int alpha, int beta, int* score, Move* bestMove) {
    if (!transposition_table) return 0;
    int index = hash & TT_MASK;
    TTEntry* entry = &transposition_table[index];

    if (entry->hash != hash) return 0;
    if (entry->depth < depth) return 0;

    *bestMove = entry->bestMove;

    if (entry->flag == TT_EXACT) {
        *score = entry->score;
        tt_hits++;
        return 1;
    }
    if (entry->flag == TT_LOWER && entry->score >= beta) {
        *score = entry->score;
        tt_hits++;
        return 1;
    }
    if (entry->flag == TT_UPPER && entry->score <= alpha) {
        *score = entry->score;
        tt_hits++;
        return 1;
    }
    return 0;
}

// EVAL SEMPRE DO PONTO DE VISTA DA POSIÇÃO (não inverte)
int scorePosition(ChessBoard* board) {
    int score = 0;

    // WHITE pieces (0-5)
    for (int piece = P; piece <= Q; piece++) {
        uint64_t bb = board->pieceBB[piece];
        while (bb) {
            int sq = get_lsb(bb);
            score += pieceValues[piece];
            score += pstWhite[piece][sq];
            pop_bit(bb, sq);
        }
    }

    // BLACK pieces (6-11) - INVERTIDAS
    for (int piece = p; piece <= q; piece++) {
        uint64_t bb = board->pieceBB[piece];
        while (bb) {
            int sq = get_lsb(bb);
            int sq_inverted = sq ^ 56;  // Inverter verticalmente
            score += pieceValues[piece];  // Já negativo (-100, -350, etc)
            score += pstWhite[piece - 6][sq_inverted];
            pop_bit(bb, sq);
        }
    }

    // Rei
    score += pieceValues[K] * popcount64(board->pieceBB[K]);
    score += pieceValues[k] * popcount64(board->pieceBB[k]);

    // BONUS: Rainha atacada
    uint64_t white_queens = board->pieceBB[Q];
    while (white_queens) {
        int sq = get_lsb(white_queens);
        if (is_square_attacked(board, sq, BLACK)) {
            score -= 50;
        }
        pop_bit(white_queens, sq);
    }

    uint64_t black_queens = board->pieceBB[q];
    while (black_queens) {
        int sq = get_lsb(black_queens);
        if (is_square_attacked(board, sq, WHITE)) {
            score += 50;
        }
        pop_bit(black_queens, sq);
    }

    // RETORNA SEMPRE DO PONTO DE VISTA DE WHITE
    // Não inverte baseado em board->side!
    return score;
}

/* Negamax: >0 favorece quem tem a vez de jogar (usa scorePosition em POV branco). */
static inline int evalRelative(ChessBoard* board) {
    int w = scorePosition(board);
    return (board->side == WHITE) ? w : -w;
}

int killer_moves[100][2];

#define QUIES_MAX_PLY 8

int quiescence(ChessBoard* board, int alpha, int beta, int qply) {
    qnodes_checked++;

    if (qply > QUIES_MAX_PLY)
        return evalRelative(board);

    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);
    int in_check = is_square_attacked(board, kingSq, !board->side);

    int stand_pat = evalRelative(board);
    if (!in_check) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    }

    /*
     * Em xeque a quiescência só-capturas é inválida (evasões não-captura existem).
     * Geramos todas as jogadas legais neste caso (árguere mais estreito).
     */
    if (in_check) {
        uint64_t from_mask = board->occupancy[board->side];
        while (from_mask) {
            int f = get_lsb(from_mask);
            pop_bit(from_mask, f);
            for (int t = 0; t < 64; t++) {
                ChessMoveState state;
                if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                    int score = -quiescence(board, -beta, -alpha, qply + 1);
                    ChessBoard_unmakeMove(board, &state);
                    if (score >= beta) return beta;
                    if (score > alpha) alpha = score;
                }
            }
        }
        return alpha;
    }

    /* Só capturas para casas ocupadas pelo adversário, bem menos tentativas que 64×64. */
    uint64_t enemy = board->occupancy[!board->side];
    while (enemy) {
        int t = get_lsb(enemy);
        pop_bit(enemy, t);

        uint64_t friendly = board->occupancy[board->side];
        while (friendly) {
            int f = get_lsb(friendly);
            pop_bit(friendly, f);

            int victim = 0, attacker = 0;
            for (int i = 0; i < 12; i++) {
                if (get_bit(board->pieceBB[i], t)) victim = pieceValues[i];
                if (get_bit(board->pieceBB[i], f)) attacker = pieceValues[i];
            }

            /* Podas rápidas de captura ruim (centésimos de peão). */
            if (victim + 100 < attacker) continue;
            if (stand_pat + victim + 150 < alpha) continue;

            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                int score = -quiescence(board, -beta, -alpha, qply + 1);
                ChessBoard_unmakeMove(board, &state);

                if (score >= beta) return beta;
                if (score > alpha) alpha = score;
            }
        }
    }
    return alpha;
}

typedef struct {
    int from;
    int to;
    int score;
} ScoredMove;

int compare_moves(const void* a, const void* b) {
    int diff = ((ScoredMove*)b)->score - ((ScoredMove*)a)->score;
    return diff < 0 ? -1 : (diff > 0 ? 1 : 0);
}

const int futility_margins[] = { 0, 150, 300, 450, 600, 750, 900, 1050 };

int search(ChessBoard* board, int depth, int alpha, int beta, int ply, Move prev_move) {
    nodes_checked++;

    if (depth <= 0) {
        return quiescence(board, alpha, beta, 0);
    }

    Move tt_move = { 0, 0, ' ' };
    int tt_score = 0;
    if (tt_probe(board->hash, depth, alpha, beta, &tt_score, &tt_move)) {
        return tt_score;
    }

    int original_alpha = alpha;
    int original_beta = beta;

    ScoredMove moves[256];
    int movesCount = 0;
    int kingSq = get_lsb(board->pieceBB[board->side == WHITE ? K : k]);
    int side_in_check = is_square_attacked(board, kingSq, !board->side);

    uint64_t from_mask = board->occupancy[board->side];
    while (from_mask) {
        int f = get_lsb(from_mask);
        pop_bit(from_mask, f);
        for (int t = 0; t < 64; t++) {
            int is_capture = get_bit(board->occupancy[!board->side], t);
            int victim = 0;
            if (is_capture) {
                for (int i = 0; i < 12; i++) {
                    if (get_bit(board->pieceBB[i], t)) {
                        victim = pieceValues[i];
                        break;
                    }
                }
            }
            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                moves[movesCount].from = f;
                moves[movesCount].to = t;

                if (is_capture) {
                    moves[movesCount].score = 1000000 + victim;
                }
                else if (f == tt_move.from && t == tt_move.to) {
                    moves[movesCount].score = 900000;
                }
                else if (killer_moves[ply][0] == (f | (t << 6))) {
                    moves[movesCount].score = 800000;
                }
                else if (killer_moves[ply][1] == (f | (t << 6))) {
                    moves[movesCount].score = 700000;
                }
                else if (prev_move.from != -1 &&
                    countermove_table[!board->side][prev_move.from][prev_move.to] == (f | (t << 6))) {
                    moves[movesCount].score = 600000;
                }
                else {
                    moves[movesCount].score = history_table[board->side][f][t] >> 4;
                }

                ChessBoard_unmakeMove(board, &state);
                movesCount++;
            }
        }
    }

    if (movesCount == 0) {
        /* Sem jogadas: xeque-mate ou afogamento (eval relativo ao lado que joga). */
        if (is_square_attacked(board, kingSq, !board->side))
            return -(50000 - ply);
        return 0;
    }

    if (movesCount > 1) qsort(moves, movesCount, sizeof(ScoredMove), compare_moves);

    if (depth >= 3 && ply < 90 && original_beta - original_alpha == 1) {
        int non_pawn_material = popcount64(
            board->pieceBB[N] | board->pieceBB[B] | board->pieceBB[R] | board->pieceBB[Q] |
            board->pieceBB[n] | board->pieceBB[b] | board->pieceBB[r] | board->pieceBB[q]);

        if (non_pawn_material > 3) {
            board->side = !board->side;
            ChessBoard_updateOccupancy(board);

            Move null_move = { -1, -1, ' ' };
            int null_score = -search(board, depth - 3, -original_beta, -original_beta + 1, ply + 1, null_move);

            board->side = !board->side;
            ChessBoard_updateOccupancy(board);

            if (null_score >= original_beta) {
                cutoffs++;
                return original_beta;
            }
        }
    }

    int best_score = -50000;
    Move best_move = { -1, -1, ' ' };
    int moves_searched = 0;

    for (int i = 0; i < movesCount; i++) {
        if (depth <= 3 && moves_searched > 0 && moves[i].score < 700000) {
            int static_eval = evalRelative(board);
            if (static_eval + futility_margins[depth] < alpha) {
                futil_cuts++;
                continue;
            }
        }

        ChessMoveState state;
        ChessBoard_makeMove(board, moves[i].from, moves[i].to, ' ', &state);

        Move current_move;
        current_move.from = moves[i].from;
        current_move.to = moves[i].to;
        current_move.promoteTo = ' ';

        /* Extensão em xeque: mais um nível nas variantes forçadas */
        int chk_ext = (side_in_check && ply < 24) ? 1 : 0;
        int full_plies = depth - 1 + chk_ext;

        /* LMR: reduz profundidade nas jogadas tardias quietas; re-busca se melhora alpha */
        int search_plies = full_plies;
        int did_lmr = 0;
        if (moves_searched > 0 && depth >= 3 && !side_in_check && moves[i].score < 1000000) {
            int R = 1 + moves_searched / 12;
            if (R > full_plies - 1) R = full_plies > 1 ? full_plies - 1 : 0;
            search_plies = full_plies - R;
            if (search_plies < 1) search_plies = 1;
            did_lmr = (search_plies < full_plies);
        }

        int score = -search(board, search_plies, -beta, -alpha, ply + 1, current_move);
        if (score > alpha && did_lmr)
            score = -search(board, full_plies, -beta, -alpha, ply + 1, current_move);

        ChessBoard_unmakeMove(board, &state);
        moves_searched++;

        if (score > best_score) {
            best_score = score;
            best_move.from = moves[i].from;
            best_move.to = moves[i].to;
            best_move.promoteTo = ' ';
        }

        if (score >= beta) {
            cutoffs++;

            int is_capture = get_bit(board->occupancy[!board->side], moves[i].to);
            if (!is_capture) {
                killer_moves[ply][1] = killer_moves[ply][0];
                killer_moves[ply][0] = moves[i].from | (moves[i].to << 6);
                history_table[board->side][moves[i].from][moves[i].to] += depth * depth;

                if (prev_move.from != -1) {
                    countermove_table[!board->side][prev_move.from][prev_move.to] =
                        moves[i].from | (moves[i].to << 6);
                }
            }

            tt_store(board->hash, beta, depth, TT_LOWER, best_move);
            return beta;
        }

        if (score > alpha) alpha = score;
    }

    char flag = (best_score <= original_alpha) ? TT_UPPER : TT_EXACT;
    if (best_score >= original_beta) flag = TT_LOWER;

    tt_store(board->hash, best_score, depth, flag, best_move);

    return best_score;
}

Evaluate Evaluate_findBestMove(ChessBoard* board, int max_depth, int multiPvCount) {
    Evaluate eval;
    eval.multiPvCount = multiPvCount;
    eval.variations = malloc(sizeof(Variation) * multiPvCount);
    for (int i = 0; i < multiPvCount; i++) {
        eval.variations[i].line = NULL;
        eval.variations[i].score = -1000000;
    }

    tt_ensure();

    memset(killer_moves, 0, sizeof(killer_moves));
    search_start_time = clock();
    nodes_checked = 0;
    qnodes_checked = 0;
    tt_hits = 0;
    cutoffs = 0;
    futil_cuts = 0;

    int best_from = -1, best_to = -1;
    int root_side = board->side;
    /* Branco maximiza static eval (POV branco); Preto minimiza */
    int current_best_score = (root_side == WHITE) ? -1000000 : 1000000;

    ScoredMove moves[256];
    int movesCount = 0;

    // ROOT MOVES
    uint64_t root_froms = board->occupancy[board->side];
    while (root_froms) {
        int f = get_lsb(root_froms);
        pop_bit(root_froms, f);
        for (int t = 0; t < 64; t++) {
            int is_capture = get_bit(board->occupancy[!board->side], t);
            int victim = 0;
            if (is_capture) {
                for (int i = 0; i < 12; i++) {
                    if (get_bit(board->pieceBB[i], t)) {
                        victim = pieceValues[i];
                        break;
                    }
                }
            }
            ChessMoveState state;
            if (ChessBoard_makeMove(board, f, t, ' ', &state)) {
                moves[movesCount].from = f;
                moves[movesCount].to = t;
                if (is_capture) {
                    moves[movesCount].score = 1000000 + victim;
                }
                else {
                    moves[movesCount].score = 0;
                }

                ChessBoard_unmakeMove(board, &state);
                movesCount++;
            }
        }
    }

    if (movesCount > 1) qsort(moves, movesCount, sizeof(ScoredMove), compare_moves);

    int alpha = -50000, beta = 50000;
    Move null_move = { -1, -1, ' ' };

    for (int i = 0; i < movesCount; i++) {
        ChessMoveState state;
        ChessBoard_makeMove(board, moves[i].from, moves[i].to, ' ', &state);

        int score;
        if (root_side == WHITE) {
            // Root move foi WHITE, agora é BLACK
            score = -search(board, max_depth - 1, -beta, -alpha, 1, null_move);
        }
        else {
            // Root move foi BLACK, agora é WHITE
            score = search(board, max_depth - 1, -beta, -alpha, 1, null_move);
        }

        ChessBoard_unmakeMove(board, &state);

        if (root_side == WHITE) {
            if (score > current_best_score) {
                current_best_score = score;
                best_from = moves[i].from;
                best_to = moves[i].to;
            }
            if (score > alpha) alpha = score;
        }
        else {
            if (score < current_best_score) {
                current_best_score = score;
                best_from = moves[i].from;
                best_to = moves[i].to;
            }
            if (score < beta) beta = score;
        }
    }

    Move bestMoveObj;
    bestMoveObj.from = best_from;
    bestMoveObj.to = best_to;
    bestMoveObj.promoteTo = ' ';

    eval.variations[0].line = malloc(sizeof(Move));
    eval.variations[0].line[0] = bestMoveObj;
    eval.variations[0].score = (root_side == WHITE) ? current_best_score : -current_best_score;
    eval.variations[0].depth = max_depth;
    eval.variations[0].length = 1;

    double elapsed = (double)(clock() - search_start_time) / CLOCKS_PER_SEC;
    double cutoff_rate = cutoffs > 0 ? (cutoffs * 100.0 / nodes_checked) : 0;

    /* Score da perspectiva do lado pensando (+ é bom para o lado que pensa). */
    double display_score = (root_side == WHITE)
        ? (current_best_score / 100.0)
        : (-current_best_score / 100.0);

    printf("[D%d] Nodes: %d (Q:%d) CO: %d (%.1f%%) TT: %d (%.1f%%) FUT: %d Time: %.2fs NPS: %.0f Score: %.2f\n",
        max_depth, nodes_checked, qnodes_checked, cutoffs, cutoff_rate,
        tt_hits, tt_hits * 100.0 / nodes_checked, futil_cuts, elapsed,
        elapsed > 0 ? nodes_checked / elapsed : 0,
        display_score);

    return eval;
}