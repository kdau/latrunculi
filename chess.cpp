/******************************************************************************
 *  chess.cpp
 *
 *  Custom scripts for A Nice Game of Chess
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "chess.h"

namespace chess {



/**
 ** Square (File, Rank)
 **/

Square::Square (File _file, Rank _rank)
	: file (_file), rank (_rank)
{}

Square::Square (const char* position)
	: file (FILE_NONE), rank (RANK_NONE)
{
	if (strlen (position) >= 2)
	{
		file = (File) tolower (position[0]);
		rank = (Rank) position[1];
	}
}

bool
Square::Valid () const
{
	return file > FILE_NONE && file < N_FILES &&
		rank > RANK_NONE && rank < N_RANKS;
}

bool
Square::operator == (const Square& rhs) const
{
	return file == rhs.file && rank == rhs.rank;
}

void
Square::GetCode (char* code) const
{
	assert (code != NULL);
	if (Valid ())
	{
		code[0] = file + 'a';
		code[1] = rank + '1';
	}
	else
	{
		code[0] = code[1] = '-';
	}
}

Square
Square::Offset (int delta_file, int delta_rank) const
{
	Square result;
	result.file = File (file + delta_file);
	result.rank = Rank (rank + delta_rank);
	return result.Valid () ? result : Square ();
}



/**
 ** Piece (Side, PieceType, PieceSquares)
 **/

bool
SideValid (Side side)
{
	return side > SIDE_NONE && side < N_SIDES;
}

char
SideCode (Side side)
{
	switch (side)
	{
		case SIDE_WHITE: return 'w';
		case SIDE_BLACK: return 'b';
		default: return '-';
	}
}

int
SideFacing (Side side)
{
	switch (side)
	{
		case SIDE_WHITE: return 1;
		case SIDE_BLACK: return -1;
		default: return 0;
	}
}

Side
Opponent (Side side)
{
	switch (side)
	{
		case SIDE_WHITE: return SIDE_BLACK;
		case SIDE_BLACK: return SIDE_WHITE;
		default: return SIDE_NONE;
	}
}

Piece::Piece (Side _side, PieceType _type)
	: side (_side), type (_type)
{}

Piece::Piece (char code)
	: side (SIDE_NONE), type (PIECE_NONE)
{
	SetCode (code);
}

bool
Piece::Valid () const
{
	return SideValid (side) && type > PIECE_NONE && type < N_PIECE_TYPES;
}

bool
Piece::operator == (const Piece& rhs) const
{
	return side == rhs.side && type == rhs.type;
}

char
Piece::GetCode () const
{
	return Valid () ? CODES[side][type] : NONE_CODE;
}

void
Piece::SetCode (char code)
{
	side = SIDE_NONE;
	type = PIECE_NONE;

	for (uint side = 0; side < N_SIDES; ++side)
		for (uint type = 0; type < N_PIECE_TYPES; ++type)
			if (code == CODES[side][type])
			{
				side = (Side) side;
				type = (PieceType) type;
				return;
			}
}

const char
Piece::NONE_CODE = '\0';

const char*
Piece::CODES[N_SIDES] = { "KQRBNP", "kqrbnp" };

Rank
Piece::GetInitialRank () const
{
	if (!Valid ())
		return RANK_NONE;
	else if (side == SIDE_WHITE)
		return type == PIECE_PAWN ? RANK_2 : RANK_1;
	else // side == SIDE_BLACK
		return type == PIECE_PAWN ? RANK_7 : RANK_8;
}



/**
 ** Move (MoveBase, MoveType, Moves)
 **/

Move::Move (const Piece& _piece, const Square& _from, const Square& _to)
	: MoveBase ({ _piece, _from, _to }),
	  type (MOVE_NONE),
	  promotion (PIECE_NONE)
{}



/**
 ** Board (GameState, CastlingOptions)
 **/

// constructors and persistence

Board::Board ()
	: active_side (INITIAL_SIDE),
	  halfmove_clock (0),
	  fullmove_number (1),
	  game_state (GAME_ONGOING),
	  victor (SIDE_NONE)
{
	memcpy (board, INITIAL_BOARD, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = CASTLING_ALL;
	CalculateData ();
}

Board::Board (const Board& _board)
	: active_side (_board.active_side),
	  en_passant_square (_board.en_passant_square),
	  halfmove_clock (_board.halfmove_clock),
	  fullmove_number (_board.fullmove_number),
	  game_state (_board.game_state),
	  victor (_board.victor)
{
	memcpy (board, _board.board, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = _board.castling_options[side];
	CalculateData ();
}

Board::Board (const char* fen, int state_data)
	: game_state (GameState (state_data % 64)), //FIXME Okay math? Check?
	  victor (Side (state_data / 64)) //FIXME Okay math? Check?
{
	(void) fen; //FIXME Restore from FEN.

	CalculateData ();
}



// basic data access

void
Board::GetSquaresWith (Squares& squares, const Piece& piece) const
{
	squares.clear ();
	auto range = piece_squares.equal_range (piece);
	for (auto square = range.first; square != range.second; ++square)
		squares.push_back (square->second);
}




// game status and analysis

bool
Board::IsUnderAttack (const Square& square, Side attacker) const
{
	(void) square; (void) attacker; return false; //FIXME Determine.
}



// movement and player actions

bool
Board::MakeMove (const MovePtr& move)
{
	if (!move) return false;

	//FIXME Confirm the move comes from this board?

	// clear origin square
	(*this)[move->from] = Piece::NONE_CODE;

	// promote piece, if applicable
	Piece piece = move->piece;
	if (move->type & MOVE_PROMOTION)
		piece.type = move->promotion;

	// place piece in target square
	(*this)[move->to] = piece.GetCode ();

	// capture en passant, if applicable
	if (move->type & MOVE_EN_PASSANT)
		(*this)[move->passed_square] = Piece::NONE_CODE;

	// move castling rook, if applicable
	if (move->type & MOVE_CASTLING)
	{
		(*this)[move->comoving_rook.from] = Piece::NONE_CODE;
		(*this)[move->comoving_rook.to] =
			move->comoving_rook.piece.GetCode ();
	}

	// move turn to opponent
	active_side = Opponent (active_side);

	// update en passant square
	if (move->type & MOVE_TWO_SQUARE)
		en_passant_square = move->passed_square;
	else
		en_passant_square = Square ();

	// update castling options
	if (move->piece.type == PIECE_KING)
		castling_options[move->piece.side] = CASTLING_NONE;
	else if (move->piece.type == PIECE_ROOK &&
		 move->from.rank == move->piece.GetInitialRank ())
	{
		if (move->from.file == FILE_A)
			castling_options[move->piece.side] &= ~CASTLING_QUEENSIDE;
		else if (move->from.file == FILE_H)
			castling_options[move->piece.side] &= ~CASTLING_KINGSIDE;
	}

	// update halfmove clock
	if (move->piece.type == PIECE_PAWN || move->type & MOVE_CAPTURE)
		halfmove_clock = 0;
	else
		++halfmove_clock;

	// update fullmove number
	if (move->piece.side == SIDE_BLACK)
		++fullmove_number;

	// update calculated data
	CalculateData ();
	return true;
}

bool
Board::Resign ()
{
	if (game_state != GAME_ONGOING) return false;
	game_state = GAME_WON_RESIGNATION;
	victor = Opponent (active_side);
	CalculateData ();
	return true;
}

bool
Board::ExpireTime (Side against)
{
	if (game_state != GAME_ONGOING) return false;
	game_state = GAME_WON_TIME_CONTROL;
	victor = Opponent (against);
	CalculateData ();
	return true;
}

bool
Board::Draw (GameState draw_type)
{
	switch (draw_type)
	{
	case GAME_DRAWN_AGREEMENT:
	case GAME_DRAWN_THREEFOLD:
		// accept unconditionally; UI must detect/confirm conditions
		break;
	case GAME_DRAWN_FIFTY_MOVE:
		if (halfmove_clock < 50) return false;
		break;
	default:
		return false;
	}

	CalculateData ();
	return true;
}



// persistent data and initial values

char&
Board::operator[] (const Square& square)
{
	assert (square.Valid ());
	return board[square.rank][square.file];
}

const char&
Board::operator[] (const Square& square) const
{
	assert (square.Valid ());
	return board[square.rank][square.file];
}

const char
Board::INITIAL_BOARD[N_RANKS * N_FILES + 1] =
	"RNBQKBNRPPPPPPPP                                pppppppprnbqkbnr";

const Side
Board::INITIAL_SIDE = SIDE_WHITE;



// calculated data

void
Board::CalculateData ()
{
	piece_squares.clear ();
	possible_moves.clear ();

	// update fen and state_data
	UpdatePersistence ();

	// update piece_squares
	for (uint rank = 0; rank < N_RANKS; ++rank)
	for (uint file = 0; file < N_FILES; ++file)
	{
		Square square ((File) file, (Rank) rank);
		Piece piece = GetPieceAt (square);
		if (piece.Valid ())
			piece_squares.insert ({ piece, square });
	}

	// bail out if game is over
	if (game_state != GAME_ONGOING)
	{
		//FIXME If we're restoring a finished game, this could be wrong. But why would we be?
		for (uint side = 0; side < N_SIDES; ++side)
			in_check[side] = false;
		return;
	}

	// update possible_moves
	EnumerateMoves ();

	// update in_check
	Squares king;
	for (uint side = 0; side < N_SIDES; ++side)
	{
		GetSquaresWith (king, Piece ((Side) side, PIECE_KING));
		Side opponent = Opponent ((Side) side);
		in_check[side] = (king.size () == 1)
			? IsUnderAttack (king.front (), opponent) : true;
	}

	// detect checkmate or stalemate
	if (possible_moves.empty ())
	{
		if (in_check[active_side])
		{
			game_state = GAME_WON_CHECKMATE;
			victor = Opponent (active_side);
		}
		else
		{
			game_state = GAME_DRAWN_STALEMATE;
			victor = SIDE_NONE;
		}
		return;
	}

	//FIXME Detect dead positions.
}

void
Board::UpdatePersistence ()
{
	char* ptr = fen;

	for (uint rank = 0; rank < N_RANKS; ++rank)
	{
		//FIXME Write the rank's piece placement.
		*ptr++ = '/';
	}

	*ptr++ = ' ';
	*ptr++ = SideCode (active_side);

	*ptr++ = ' ';
	if (castling_options[SIDE_WHITE] & CASTLING_KINGSIDE) *ptr++ = 'K';
	if (castling_options[SIDE_WHITE] & CASTLING_QUEENSIDE) *ptr++ = 'Q';
	if (castling_options[SIDE_BLACK] & CASTLING_KINGSIDE) *ptr++ = 'k';
	if (castling_options[SIDE_BLACK] & CASTLING_QUEENSIDE) *ptr++ = 'q';
	if (castling_options[SIDE_WHITE] == CASTLING_NONE &&
	    castling_options[SIDE_BLACK] == CASTLING_NONE)
		*ptr++ = '-';

	*ptr++ = ' ';
	if (en_passant_square.Valid ())
	{
		en_passant_square.GetCode (ptr);
		ptr += 2;
	}
	else
		*ptr++ = '-';

	snprintf (ptr, FEN_BUFSIZE - (ptr - fen),
		" %u %u", halfmove_clock, fullmove_number);

	state_data = victor * 64 + game_state;
}

void
Board::EnumerateMoves ()
{
	// caller must clear possible_moves first

	for (uint rank = 0; rank < N_RANKS; ++rank)
	for (uint file = 0; file < N_FILES; ++file)
	{
		Square from ((File) file, (Rank) rank);
		Piece piece = (*this)[from];
		if (piece.side != active_side) continue;
		switch (piece.type)
		{
		case PIECE_KING:
			EnumerateKingMoves (piece, from);
			break;
		case PIECE_QUEEN:
		case PIECE_ROOK:
		case PIECE_BISHOP:
			EnumerateRangedMoves (piece, from);
			break;
		case PIECE_KNIGHT:
			EnumerateKnightMoves (piece, from);
			break;
		case PIECE_PAWN:
			EnumeratePawnMoves (piece, from);
			break;
		default:
			break;
		}
	}
}

void
Board::EnumerateKingMoves (const Piece& piece, const Square& from)
{
	// basic moves

	static const int OFFSETS[][2] =
		{ {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}, {0,1} };
	for (auto& offset : OFFSETS)
		ConfirmPossibleMove (new Move (piece, from,
			from.Offset (offset[0], offset[1])));

	// castling

	if (IsInCheck (piece.side)) return;

	if (castling_options[piece.side] & CASTLING_KINGSIDE)
	{
		//FIXME Consider it and record if possible.
	}

	if (castling_options[piece.side] & CASTLING_QUEENSIDE)
	{
		//FIXME Consider it and record if possible.
	}
}

void
Board::EnumerateRangedMoves (const Piece& piece, const Square& from)
{
	static const int PATHS[][2] =
	{
		{1,1}, {1,-1}, {-1,-1}, {-1,1}, // rook and queen
		{0,1}, {0,-1}, {1,0}, {-1,0} // bishop and queen
	};

	uint imin = (piece.type == PIECE_BISHOP) ? 4 : 0;
	uint imax = (piece.type == PIECE_ROOK) ? 4 : 8;
	for (uint i = imin; i < imax; ++i)
		for (Square to = from; to.Valid ();
			to = to.Offset (PATHS[i][0], PATHS[i][1]))
		{
			if (to == from) continue;
			ConfirmPossibleMove (new Move (piece, from, to));
			if (!IsEmpty (to))
				break; // can't pass an occupied square
		}
}

void
Board::EnumerateKnightMoves (const Piece& piece, const Square& from)
{
	static const int OFFSETS[][2] =
		{ {1,2}, {2,1}, {-1,2}, {-2,1}, {-1,-2}, {-2,-1}, {1,-2}, {2,-1} };
	for (auto& offset : OFFSETS)
		ConfirmPossibleMove (new Move (piece, from,
			from.Offset (offset[0], offset[1])));
}

void
Board::EnumeratePawnMoves (const Piece& piece, const Square& from)
{
	int facing = SideFacing (piece.side);

	// forward moves

	Square one_square = from.Offset (0, facing);
	if (IsEmpty (one_square))
	{
		ConfirmPossibleMove (new Move (piece, from, one_square));

		Square two_square = one_square.Offset (0, facing);
		if (IsEmpty (two_square) && from.rank == piece.GetInitialRank ())
		{
			Move* move = new Move (piece, from, two_square);
			move->type |= MOVE_TWO_SQUARE;
			move->passed_square = one_square;
			ConfirmPossibleMove (move);
		}
	}

	// captures

	for (int delta_file : { -1, 1 })
	{
		Square capture = from.Offset (delta_file, facing);
		if (!IsEmpty (capture))
			ConfirmPossibleMove (new Move (piece, from, capture));
		else if (capture == en_passant_square)
		{
			Move* move = new Move (piece, from, capture);
			move->type |= MOVE_EN_PASSANT;
			//FIXME Set move->passed_square.
			ConfirmPossibleMove (move);
		}
	}
}

bool
Board::ConfirmPossibleMove (Move* _move)
{
	// caller must set piece; from; to; and as applicable: MOVE_TWO_SQUARE,
	// MOVE_EN_PASSANT, MOVE_CASTLING, passed_square, and comoving_rook.

	MovePtr move (_move);
	if (!move) return false;

	// move cannot be off board
	if (!move->to.Valid ()) return false;

	// move cannot be to friendly-occupied square
	if (GetPieceAt (move->to).side == move->piece.side) return false;

	// identify captured piece and set related flags
	move->captured_piece = GetPieceAt ((move->type & MOVE_EN_PASSANT)
		? move->passed_square : move->to);
	if (move->type & MOVE_EN_PASSANT || !move->captured_piece.Valid ())
		move->type |= MOVE_TO_EMPTY;
	if (move->captured_piece.Valid ())
		move->type |= MOVE_CAPTURE;

	// identify promotion if pawn reaches opposing king's rank
	if (move->piece.type == PIECE_PAWN && move->to.rank ==
		Piece (Opponent (move->piece.side), PIECE_KING).GetInitialRank ())
	{
		move->type |= MOVE_PROMOTION;
		move->promotion = PIECE_QUEEN; //FIXME Support other promotions.
	}

	// move cannot place piece's side in check
	Board check_test (*this); //FIXME Will the CalculateData calls in the constructor and MakeMove cause an infinite loop?
	check_test.MakeMove (move);
	if (check_test.IsInCheck (move->piece.side)) return false;

	possible_moves.push_back (std::move (move));
	return true;
}



} // namespace chess

