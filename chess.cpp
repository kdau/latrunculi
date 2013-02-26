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

Square::operator bool () const
{
	return file >= FILE_A && file <= FILE_H &&
		rank >= RANK_1 && rank <= RANK_8;
}

void
Square::GetCode (char* code) const
{
	assert (code != NULL);
	if (operator bool ())
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
	return result ? result : Square ();
}



/**
 ** Piece (Side, PieceType, PieceSquares)
 **/

#define NONE_CODE 0
#define WHITE_CODES "KQRBNP"
#define BLACK_CODES "kqrbnp"

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

Piece::operator bool () const
{
	return (side == SIDE_WHITE || side == SIDE_BLACK) &&
		type >= PIECE_KING && type <= PIECE_PAWN;
}

char
Piece::GetCode () const
{
	if (!operator bool ()) return NONE_CODE;
	return (side == SIDE_WHITE) ? WHITE_CODES[type] : BLACK_CODES[type];
}

void
Piece::SetCode (char code)
{
	side = SIDE_NONE;
	type = PIECE_NONE;

	for (uint i = PIECE_KING; i <= PIECE_PAWN; ++i)
		if (code == WHITE_CODES[i])
		{
			side = SIDE_WHITE;
			type = (PieceType) i;
			return;
		}

	for (uint i = PIECE_KING; i <= PIECE_PAWN; ++i)
		if (code == WHITE_CODES[i])
		{
			side = SIDE_BLACK;
			type = (PieceType) i;
			return;
		}
}

Rank
Piece::GetInitialRank () const
{
	if (!operator bool ())
		return RANK_NONE;
	else if (side == SIDE_WHITE)
		return type == PIECE_PAWN ? RANK_2 : RANK_1;
	else // side == SIDE_BLACK
		return type == PIECE_PAWN ? RANK_7 : RANK_8;
}



/**
 ** Move (MoveType, Moves)
 **/

Move::Move ()
	: type (MOVE_NONE), promotion (PIECE_NONE)
{}



/**
 ** Board (GameState, CastlingOptions)
 **/

// constructors and persistence

Board::Board ()
{
	Reset ();
}

Board::Board (const char* fen, int state_data)
{
	Restore (fen, state_data);
}



// basic data access

Piece
Board::GetPieceAt (const Square& square) const
{
	return square ? board[square.rank][square.file] : 0;
}

void
Board::GetSquaresWith (Squares& squares, const Piece& piece) const
{
	squares.clear ();
	auto range = piece_squares.equal_range (piece);
	for (auto iter = range.first; iter != range.second; ++iter)
		squares.push_back (iter->second);
}

CastlingOptions
Board::GetCastlingOptions (Side side) const
{
	return Piece (side, PIECE_KING)
		? castling_options[side] : CASTLING_NONE;
}



// game status and analysis

bool
Board::IsUnderAttack (const Square& square, Side attacker) const
{
	(void) square; (void) attacker; return false; //FIXME Determine.
}

bool
Board::IsInCheck (Side side) const
{
	return Piece (side, PIECE_KING) ? in_check[side] : false;
}



// movement and player actions

bool
Board::MakeMove (const Move& move)
{
	(void) move; //FIXME Evaluate move and enter if acceptable.

/*FIXME Import old PerformMove code.
	Piece mover = GetAt (from);
	if (!mover) return;

	SetAt (from); // remove mover from origin

	//FIXME Handle promotion.

	SetAt (to, mover); // place mover in destination

	//FIXME Handle rook portion of castling.

	//FIXME Handle en passant capture.

	active_camp = Enemy (active_camp);

	//FIXME Update en passant square.

	int& castling = (mover.camp == CAMP_WHITE)
		? castling_white : castling_black;
	if (mover.type == PIECE_KING)
		castling = CASTLE_NONE;
	else if (mover.type == PIECE_ROOK && from.rank ==
		(mover.camp == CAMP_WHITE) ? RANK_1 : RANK_8)
	{
		if (from.file == FILE_a)
			castling &= ~CASTLE_QUEENSIDE;
		else if (from.file == FILE_h)
			castling &= ~CASTLE_KINGSIDE;
	}

	//FIXME Update halfmove clock.

	if (mover.camp == CAMP_BLACK)
		++fullmove_number;
*/

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

void
Board::Restore (const char* fen, int state_data)
{
	(void) fen; //FIXME Restore from FEN.

	game_state = GameState (state_data % 64); //FIXME Okay math? Check?
	victor = Side (state_data / 64); //FIXME Okay math? Check?

	CalculateData ();
}

void
Board::Reset ()
{
	memcpy (board, INITIAL_BOARD, N_RANKS * N_FILES);
	active_side = INITIAL_SIDE;
	castling_options[SIDE_WHITE] = CASTLING_ALL;
	castling_options[SIDE_BLACK] = CASTLING_ALL;
	en_passant_square = Square ();
	halfmove_clock = 0;
	fullmove_number = 1;

	game_state = GAME_ONGOING;
	victor = SIDE_NONE;

	CalculateData ();
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
	for (Rank rank = RANK_1; rank <= RANK_8; rank = Rank (rank + 1))
	for (File file = FILE_A; file <= FILE_H; file = File (file + 1))
	{
		Square square (file, rank);
		Piece piece = GetPieceAt (square);
		if (piece)
			piece_squares.insert ({ piece, square });
	}

	// bail out if game is over
	if (game_state != GAME_ONGOING)
	{
		//FIXME If we're restoring a finished game, this could be wrong. But why would we be?
		in_check[SIDE_WHITE] = in_check[SIDE_BLACK] = false;
		return;
	}

	// update possible_moves
	EnumerateMoves ();

	// update in_check
	Squares king;
	GetSquaresWith (king, Piece (SIDE_WHITE, PIECE_KING));
	in_check[SIDE_WHITE] = (king.size () == 1)
		? IsUnderAttack (king.front (), SIDE_BLACK) : true;
	GetSquaresWith (king, Piece (SIDE_BLACK, PIECE_KING));
	in_check[SIDE_BLACK] = (king.size () == 1)
		? IsUnderAttack (king.front (), SIDE_WHITE) : true;

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

	for (Rank rank = RANK_1; rank <= RANK_8; rank = Rank (rank + 1))
	{
		//FIXME Write the rank's piece placement.
		*ptr++ = '/';
	}

	*ptr++ = ' ';
	*ptr++ = (active_side == SIDE_WHITE) ? 'w' : 'b';

	*ptr++ = ' ';
	if (castling_options[SIDE_WHITE] & CASTLING_KINGSIDE) *ptr++ = 'K';
	if (castling_options[SIDE_WHITE] & CASTLING_QUEENSIDE) *ptr++ = 'Q';
	if (castling_options[SIDE_BLACK] & CASTLING_KINGSIDE) *ptr++ = 'k';
	if (castling_options[SIDE_BLACK] & CASTLING_QUEENSIDE) *ptr++ = 'q';
	if (castling_options[SIDE_WHITE] == CASTLING_NONE &&
	    castling_options[SIDE_BLACK] == CASTLING_NONE)
		*ptr++ = '-';

	*ptr++ = ' ';
	if (en_passant_square)
	{
		en_passant_square.GetCode (ptr);
		ptr += 2;
	}
	else
		*ptr++ = '-';

	snprintf (ptr, 8, " %u %u", halfmove_clock, fullmove_number);

	state_data = victor * 64 + game_state;
}

void
Board::EnumerateMoves ()
{
	//FIXME Do.
}

#if 0 //FIXME Import old EnumerateMoves code.
void
ChessBoard::EnumerateMoves (MoveSet& set) const
{
	set.clear ();
	for (char rank = RANK_1; rank <= RANK_8; ++rank)
	for (char file = FILE_a; file <= FILE_h; ++file)
	{
		Square origin ((File) file, (Rank) rank);
		EnumeratePieceMoves (set, GetAt (origin), origin);
	}
}

void
ChessBoard::EnumeratePieceMoves (MoveSet& set, const Piece& piece,
	const Square& origin) const
{
	if (piece.camp != active_camp) return;

	switch (piece.type)
	{
	case PIECE_ROOK:
	case PIECE_BISHOP:
	case PIECE_QUEEN:
		if (piece.type != PIECE_ROOK)
		{
			TryMovePath (set, piece, origin,  1,  1);
			TryMovePath (set, piece, origin, -1,  1);
			TryMovePath (set, piece, origin,  1, -1);
			TryMovePath (set, piece, origin, -1, -1);
		}
		if (piece.type != PIECE_BISHOP)
		{
			TryMovePath (set, piece, origin,  0,  1);
			TryMovePath (set, piece, origin,  0, -1);
			TryMovePath (set, piece, origin,  1,  0);
			TryMovePath (set, piece, origin, -1,  0);
		}
		break;
	case PIECE_KNIGHT:
		TryMovePoint (set, piece, origin,  1,  2);
		TryMovePoint (set, piece, origin,  2,  1);
		TryMovePoint (set, piece, origin, -1,  2);
		TryMovePoint (set, piece, origin, -2,  1);
		TryMovePoint (set, piece, origin,  1, -2);
		TryMovePoint (set, piece, origin,  2, -1);
		TryMovePoint (set, piece, origin, -1, -2);
		TryMovePoint (set, piece, origin, -2, -1);
		break;
	case PIECE_KING:
	    {
		TryMovePoint (set, piece, origin,  1,  1);
		TryMovePoint (set, piece, origin,  1,  0);
		TryMovePoint (set, piece, origin,  1, -1);
		TryMovePoint (set, piece, origin,  0, -1);
		TryMovePoint (set, piece, origin, -1, -1);
		TryMovePoint (set, piece, origin, -1,  0);
		TryMovePoint (set, piece, origin, -1,  1);
		TryMovePoint (set, piece, origin,  0,  1);

		int castling = (piece.camp == CAMP_WHITE)
			? castling_white : castling_black;
		if (IsInCheck (piece.camp)) castling = CASTLE_NONE;
		if ((castling & CASTLE_KINGSIDE))
			{} //FIXME Consider kingside castling.
		if ((castling & CASTLE_QUEENSIDE))
			{} //FIXME Consider queenside castling.

		break;
	    }
	case PIECE_PAWN:
	    {
		char dirn = (piece.camp == CAMP_WHITE) ? 1 : -1;
		char init_rank = (piece.camp == CAMP_WHITE) ? RANK_2 : RANK_7;
		bool one_okay = TryMovePoint (set, piece, origin, 0, dirn, MOVE_EMPTY);
		if (one_okay && (origin.rank == init_rank))
			TryMovePoint (set, piece, origin, 0, 2 * dirn, MOVE_EMPTY);
		TryMovePoint (set, piece, origin, -1, dirn, MOVE_CAPTURE);
		TryMovePoint (set, piece, origin,  1, dirn, MOVE_CAPTURE);
		//FIXME Consider en passant capture.
		break;
	    }
	default:
		break;
	}
}

void
ChessBoard::TryMovePath (MoveSet& set, const Piece& piece, const Square& origin,
	char file_dirn, char rank_dirn) const
{
	char file_vect = 0, rank_vect = 0;
	while (true)
	{
		file_vect += file_dirn;
		rank_vect += rank_dirn;

		if (TryMovePoint (set, piece, origin,
				file_vect, rank_vect, MOVE_CAPTURE))
			return; // piece capture required, so can't go farther

		if (!TryMovePoint (set, piece, origin,
				file_vect, rank_vect, MOVE_EMPTY))
			return; // something else in the way
	}
}

bool
ChessBoard::TryMovePoint (MoveSet& set, const Piece& piece, const Square& origin,
	char file_vect, char rank_vect, int allowed_types) const
{
	Square offset ((File) file_vect, (Rank) rank_vect);
	Square destination = origin.Offset (offset);
	if (!destination) return false; // invalid position

	MoveType type = MOVE_NONE;
	Piece occupant = GetAt (destination);
	if (occupant)
	{
		if (occupant.camp == piece.camp)
			return false; // can't capture friendly occupant
		else if (!(allowed_types & MOVE_CAPTURE))
			return false; // this can't capture hostile occupant
		else
			type = MOVE_CAPTURE;
	}
	else if (!(allowed_types & MOVE_EMPTY))
		return false; // this can't move to empty square
	else
		type = MOVE_EMPTY;

	//FIXME Filter otherwise illegal moves. Mark castlings and en passants (and promotions?).

	Move move = { origin, destination, type };
	set.push_back (move);
	return true;
}
#endif



} // namespace chess

