#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum {
	BOARD_WIDTH = 8,
	BOARD_HEIGHT = 8,
};

/* Piece values:
 * knight 1/32 = 7/224
 * bishop 1/14 = 16/224
 * rook 1/8    = 28/224
 * king 1/16   = 14/224
 *
 * loose upper bound = 64*28 = 1792 fits in int
 */
typedef int_fast8_t index_t;
typedef uint_fast16_t score_t;
typedef uint_fast8_t population_t;

_Static_assert((index_t)(-1) < 0, "easy row + dy comparisons must be possible");
_Static_assert((population_t)(BOARD_WIDTH * BOARD_HEIGHT) == BOARD_WIDTH * BOARD_HEIGHT, "population type must suit the board");
_Static_assert((index_t)(BOARD_WIDTH * BOARD_HEIGHT) == BOARD_WIDTH * BOARD_HEIGHT, "index type must be able to represent one past the last board index");

#define PRIscore PRIuFAST16
#define SCORE_MAX UINT_FAST16_MAX

enum {
	KNIGHT_VALUE = 7,
	BISHOP_VALUE = 16,
	ROOK_VALUE = 28,
	KING_VALUE = 14,
};

enum piece {
	EMPTY = 0,
	KNIGHT,
	BISHOP,
	ROOK,
	KING,
};

struct board {
	enum piece pieces[BOARD_HEIGHT][BOARD_WIDTH];

	bool downward_diagonals_attacked[BOARD_HEIGHT + BOARD_WIDTH - 1];
	population_t downward_diagonal_population[BOARD_HEIGHT + BOARD_WIDTH - 1];
	bool upward_diagonals_attacked[BOARD_HEIGHT + BOARD_WIDTH - 1];
	population_t upward_diagonal_population[BOARD_HEIGHT + BOARD_WIDTH - 1];
	bool rows_attacked[BOARD_HEIGHT];
	population_t row_population[BOARD_HEIGHT];
	bool columns_attacked[BOARD_WIDTH];
	population_t column_population[BOARD_WIDTH];
	population_t spot_attacks[BOARD_HEIGHT][BOARD_WIDTH];

	/* number of pieces that would be attacked by a knight in a given square */
	population_t knight_population[BOARD_HEIGHT][BOARD_WIDTH];

	/* number of pieces that would be attacked by a king in a given square */
	population_t king_population[BOARD_HEIGHT][BOARD_WIDTH];

	score_t score;
};

_Static_assert(EMPTY == 0, "empty piece must be zero");
static struct board EMPTY_BOARD;

__attribute__ ((nonnull))
static void print_board(struct board const* const board) {
	for (index_t row = 0; row < BOARD_HEIGHT; row++) {
		for (index_t column = 0; column < BOARD_WIDTH; column++) {
			char c;

			switch (board->pieces[row][column]) {
			case EMPTY:  c = '.'; break;
			case KNIGHT: c = 'N'; break;
			case BISHOP: c = 'B'; break;
			case ROOK:   c = 'R'; break;
			case KING:   c = 'K'; break;
			}

			putchar(c);
			putchar(' ');
		}

		putchar('\n');
	}

	score_t const score = board->score;
	printf("\nScores %" PRIscore "/224 = %.3f\n", score, score / 224.0);
}

__attribute__ ((const))
static index_t downward_diagonal(index_t const row, index_t const column) {
	return (BOARD_WIDTH - 1) + row - column;
}

__attribute__ ((const))
static index_t upward_diagonal(index_t const row, index_t const column) {
	return row + column;
}

/* only have to do forwards attacks, because only future pieces are up for consideration */
#define FOR_EACH_KING_ATTACK(row, column, attack_row, attack_column) \
	for (index_t dy = 0; dy <= 1; dy++) { \
		index_t const attack_row = (row) + dy; \
		if (attack_row < BOARD_HEIGHT) { \
			for (index_t dx = -dy; dx <= 1; dx++) { \
				index_t const attack_column = (column) + dx; \
				if (attack_column >= 0 && attack_column < BOARD_WIDTH) {

#define END_FOR_EACH_KING_ATTACK }}}}

#define FOR_EACH_KNIGHT_ATTACK(row, column, attack_row, attack_column) \
	for (index_t dy = 1; dy <= 2; dy++) { \
		index_t const attack_row = (row) + dy; \
		if (attack_row < BOARD_HEIGHT) { \
			index_t const dxd = 1 + (dy & 1); \
			for (index_t dx = -dxd; dx <= dxd; dx += 2 * dxd) { \
				index_t const attack_column = (column) + dx; \
				if (attack_column >= 0 && attack_column < BOARD_WIDTH) {

#define END_FOR_EACH_KNIGHT_ATTACK }}}}

__attribute__ ((nonnull))
static void place_piece(struct board* const board, index_t const row, index_t const column, enum piece const piece, score_t const value) {
	board->row_population[row]++;
	board->column_population[column]++;
	board->downward_diagonal_population[downward_diagonal(row, column)]++;
	board->upward_diagonal_population[upward_diagonal(row, column)]++;
	board->pieces[row][column] = piece;

	FOR_EACH_KING_ATTACK(row, column, attack_row, attack_column)
		board->king_population[attack_row][attack_column]++;
	END_FOR_EACH_KING_ATTACK

	FOR_EACH_KNIGHT_ATTACK(row, column, attack_row, attack_column)
		board->knight_population[attack_row][attack_column]++;
	END_FOR_EACH_KNIGHT_ATTACK

	board->score += value;
}

__attribute__ ((nonnull))
static void unplace_piece(struct board* const board, index_t const row, index_t const column, score_t const value) {
	board->row_population[row]--;
	board->column_population[column]--;
	board->downward_diagonal_population[downward_diagonal(row, column)]--;
	board->upward_diagonal_population[upward_diagonal(row, column)]--;
	board->pieces[row][column] = EMPTY;

	FOR_EACH_KING_ATTACK(row, column, attack_row, attack_column)
		board->king_population[attack_row][attack_column]--;
	END_FOR_EACH_KING_ATTACK

	FOR_EACH_KNIGHT_ATTACK(row, column, attack_row, attack_column)
		board->knight_population[attack_row][attack_column]--;
	END_FOR_EACH_KNIGHT_ATTACK

	board->score -= value;
}

__attribute__ ((nonnull))
static void place_rook(struct board* const board, index_t const row, index_t const column) {
	place_piece(board, row, column, ROOK, ROOK_VALUE);
	board->rows_attacked[row] = true;
	board->columns_attacked[column] = true;
}

__attribute__ ((nonnull))
static void unplace_rook(struct board* const board, index_t const row, index_t const column) {
	unplace_piece(board, row, column, ROOK_VALUE);
	board->rows_attacked[row] = false;
	board->columns_attacked[column] = false;
}

__attribute__ ((nonnull))
static void place_bishop(struct board* const board, index_t const row, index_t const column) {
	place_piece(board, row, column, BISHOP, BISHOP_VALUE);
	board->upward_diagonals_attacked[upward_diagonal(row, column)] = true;
	board->downward_diagonals_attacked[downward_diagonal(row, column)] = true;
}

__attribute__ ((nonnull))
static void unplace_bishop(struct board* const board, index_t const row, index_t const column) {
	unplace_piece(board, row, column, BISHOP_VALUE);
	board->upward_diagonals_attacked[upward_diagonal(row, column)] = false;
	board->downward_diagonals_attacked[downward_diagonal(row, column)] = false;
}

__attribute__ ((nonnull))
static void place_king(struct board* const board, index_t const row, index_t const column) {
	place_piece(board, row, column, KING, KING_VALUE);

	FOR_EACH_KING_ATTACK(row, column, attack_row, attack_column)
		board->spot_attacks[attack_row][attack_column]++;
	END_FOR_EACH_KING_ATTACK
}

__attribute__ ((nonnull))
static void unplace_king(struct board* const board, index_t const row, index_t const column) {
	unplace_piece(board, row, column, KING_VALUE);

	FOR_EACH_KING_ATTACK(row, column, attack_row, attack_column)
		board->spot_attacks[attack_row][attack_column]--;
	END_FOR_EACH_KING_ATTACK
}

__attribute__ ((nonnull))
static void place_knight(struct board* const board, index_t const row, index_t const column) {
	place_piece(board, row, column, KNIGHT, KNIGHT_VALUE);

	FOR_EACH_KNIGHT_ATTACK(row, column, attack_row, attack_column)
		board->spot_attacks[attack_row][attack_column]++;
	END_FOR_EACH_KNIGHT_ATTACK
}

__attribute__ ((nonnull))
static void unplace_knight(struct board* const board, index_t const row, index_t const column) {
	unplace_piece(board, row, column, KNIGHT_VALUE);

	FOR_EACH_KNIGHT_ATTACK(row, column, attack_row, attack_column)
		board->spot_attacks[attack_row][attack_column]--;
	END_FOR_EACH_KNIGHT_ATTACK
}

__attribute__ ((nonnull))
static void maximize(struct board* const board, struct board* const maximum_board, score_t const* const limits, index_t index) {
	if (board->score > maximum_board->score) {
		*maximum_board = *board;
		print_board(board);
	}

	if (index == BOARD_WIDTH * BOARD_HEIGHT) {
		return;
	}

	if (board->score + limits[index] <= maximum_board->score) {
		return;
	}

	index_t const row = index / BOARD_WIDTH;
	index_t const column = index % BOARD_WIDTH;
	index++;

	bool const attacked = (
		board->rows_attacked[row] ||
		board->columns_attacked[column] ||
		board->downward_diagonals_attacked[downward_diagonal(row, column)] ||
		board->upward_diagonals_attacked[upward_diagonal(row, column)] ||
		board->spot_attacks[row][column] != 0
	);

	if (!attacked) {
		if (board->row_population[row] == 0 && board->column_population[column] == 0) {
			place_rook(board, row, column);
			maximize(board, maximum_board, limits, index);
			unplace_rook(board, row, column);
		}

		if (board->downward_diagonal_population[downward_diagonal(row, column)] == 0 && board->upward_diagonal_population[upward_diagonal(row, column)] == 0) {
			place_bishop(board, row, column);
			maximize(board, maximum_board, limits, index);
			unplace_bishop(board, row, column);
		}

		if (board->king_population[row][column] == 0) {
			place_king(board, row, column);
			maximize(board, maximum_board, limits, index);
			unplace_king(board, row, column);
		}

		if (board->knight_population[row][column] == 0) {
			place_knight(board, row, column);
			maximize(board, maximum_board, limits, index);
			unplace_knight(board, row, column);
		}
	}

	maximize(board, maximum_board, limits, index);
}

int main(void) {
	struct board board = EMPTY_BOARD;
	struct board maximum_board = EMPTY_BOARD;

	/* the maximum number of points achievable using the squares from that index to the last */
	score_t limits[BOARD_WIDTH * BOARD_HEIGHT];

	for (index_t i = 0; i < BOARD_WIDTH * BOARD_HEIGHT; i++) {
		limits[i] = SCORE_MAX;
	}

	for (index_t i = BOARD_WIDTH * BOARD_HEIGHT; i--;) {
		maximize(&board, &maximum_board, limits, i);
		limits[i] = maximum_board.score;
	}
}
