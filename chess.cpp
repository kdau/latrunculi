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

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "ScriptModule.h" //FIXME FIXME debug
#include "chess.h"

namespace chess {



/**
 ** Square (File, Rank)
 **/

Square::Square (File _file, Rank _rank)
	: file (_file), rank (_rank)
{}

Square::Square (const std::string& code)
{
	if (code.length () == 2)
	{
		file = (File) tolower (code[0]);
		rank = (Rank) code[1];
	}
	else
		Clear ();
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
	return Valid () && rhs.Valid () && file == rhs.file && rank == rhs.rank;
}

std::string
Square::GetCode () const
{
	std::string result;
	if (Valid ())
	{
		result.push_back (file + 'a');
		result.push_back (rank + '1');
	}
	else
		result.push_back ('-');
	return result;
}

SquareColor
Square::GetColor () const
{
	if (!Valid ()) return SQUARE_NONE;
	return (file % 2 == rank % 2) ? SQUARE_DARK : SQUARE_LIGHT;
}

Square
Square::Offset (int delta_file, int delta_rank) const
{
	Square result (File (file + delta_file), Rank (rank + delta_rank));
	if (!Valid () || !result.Valid ()) result.Clear ();
	return result;
}

void
Square::Clear ()
{
	file = FILE_NONE;
	rank = RANK_NONE;
}



/**
 ** Piece (Side, PieceType)
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

std::string
SideName (Side side)
{
	return SideValid (side)
		? Translate (std::string ("side_") + SideCode (side))
		: Translate ("side_invalid");
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
	return Valid () && rhs.Valid () && side == rhs.side && type == rhs.type;
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

	for (uint _side = 0; _side < N_SIDES; ++_side)
		for (uint _type = 0; _type < N_PIECE_TYPES; ++_type)
			if (code == CODES[_side][_type])
			{
				side = (Side) _side;
				type = (PieceType) _type;
				return;
			}
}

const char
Piece::NONE_CODE = '\0';

const char
Piece::CODES[N_SIDES][N_PIECE_TYPES+1] = { "KQRBNP", "kqrbnp" };

std::string
Piece::GetName () const
{
	// translator's msgids may be case-insensitive, so give side separately
	return Valid ()
		? Translate ((std::string ("piece_") +
			SideCode (side)) + CODES[SIDE_BLACK][type])
		: Translate ("piece_invalid");
}

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
 ** Move (Resignation, Movement, PieceMove, Capture, EnPassantCapture,
 **       TwoSquarePawnMove, Castling)
 **/

#define DescribeMove(msgid, ...) \
{ \
	char result[128]; \
	snprintf (result, 128, Translate (msgid).data (), __VA_ARGS__); \
	return result; \
}

std::string
Resignation::GetUCICode () const
{
	return "0000"; //FIXME Is this right? Are there even UCI codes for non-PieceMoves?
}

std::string
Resignation::GetDescription () const
{
	DescribeMove ("move_resignation", SideName (GetSide ()).data ());
}

PieceMove::PieceMove (const Piece& _piece, const Square& _from,
                      const Square& _to)
	: piece (_piece),
	  from (_from),
	  to (_to)
{
	if (!piece.Valid () || !from.Valid () || !to.Valid () || from == to)
		Invalidate ();
}

std::string
PieceMove::GetUCICode () const
{
	return GetFrom ().GetCode () + GetTo ().GetCode ();
	//FIXME for promotions: + Piece (SIDE_BLACK, promotion).GetCode ()
}

std::string
PieceMove::GetDescription () const
{
	DescribeMove ("move_to_empty",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetTo ().GetCode ().data ());
	//FIXME describe promotions?
}

Capture::Capture (const Piece& piece, const Square& from,
                  const Square& to, const Piece& _captured_piece)
	: PieceMove (piece, from, to),
	  captured_piece (_captured_piece)
{
	if (!captured_piece.Valid () ||
	    piece.side == captured_piece.side)
		Invalidate ();
		
}

std::string
Capture::GetDescription () const
{
	DescribeMove ("move_capture",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName ().data (),
		GetTo ().GetCode ().data ());
}

EnPassantCapture::EnPassantCapture (Side side, File from, File to)
	: Capture (
		Piece (side, PIECE_PAWN),
		Square (from, RANK_NONE), //FIXME real rank
		Square (to, RANK_NONE), //FIXME real rank
		Piece (Opponent (side), PIECE_PAWN)
	  ),
	  captured_square (to, RANK_NONE) //FIXME real rank
{
	// Capture's validation will have failed on invalid side, from, or to
	if (!captured_square.Valid () ||
	    std::abs (from - to) != 1) // not an adjacent file
		Invalidate ();
}

std::string
EnPassantCapture::GetDescription () const
{
	DescribeMove ("move_en_passant",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName ().data (),
		GetCapturedSquare ().GetCode ().data (),
		GetTo ().GetCode ().data ());
}

TwoSquarePawnMove::TwoSquarePawnMove (Side side, File file)
	: PieceMove (
		Piece (side, PIECE_PAWN),
		Square (file, RANK_NONE), //FIXME real rank
		Square (file, RANK_NONE) //FIXME real rank
	  ),
	  passed_square (file, RANK_NONE) //FIXME real rank
{} // PieceMove's validation will have failed on invalid side or file

Castling::Castling (Side side, Type _type)
	: PieceMove (
		Piece (side, PIECE_KING),
		Square (FILE_E, RANK_NONE), //FIXME real rank
		Square ((_type == KINGSIDE) ? FILE_G : FILE_C, RANK_NONE) //FIXME real rank
	  ),
	  type (_type),
	  rook_piece (side, PIECE_ROOK)
{
	// PieceMove's validation will have failed on invalid side
	if (type != KINGSIDE && type != QUEENSIDE)
		Invalidate ();

	rook_from.rank = rook_to.rank = rook_piece.GetInitialRank ();
	rook_from.file = (type == KINGSIDE) ? FILE_H : FILE_A;
	rook_to.file = (type == KINGSIDE) ? FILE_F : FILE_D;
}

std::string
Castling::GetDescription () const
{
	DescribeMove ((type == KINGSIDE) ? "move_castle_k" : "move_castle_q",
		SideName (GetSide ()).data ());
}



/**
 ** Board
 **/

// constructors and persistence

Board::Board ()
	: active_side (INITIAL_SIDE),
	  fifty_move_clock (0),
	  fullmove_number (1),
	  game_state (GAME_ONGOING),
	  victor (SIDE_NONE)
{
	memcpy (board, INITIAL_BOARD, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::BOTH;
	CalculateData ();
}

Board::Board (const Board& _board)
	: active_side (_board.active_side),
	  en_passant_square (_board.en_passant_square),
	  fifty_move_clock (_board.fifty_move_clock),
	  fullmove_number (_board.fullmove_number),
	  game_state (_board.game_state),
	  victor (_board.victor)
{
	memcpy (board, _board.board, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = _board.castling_options[side];
	CalculateData ();
}

#define fen_throw_invalid(detail) \
	throw std::invalid_argument ("invalid FEN: " detail);
#define fen_expect_separator(separator) \
	{ if (pos == fen.end () || *pos++ != separator) \
		fen_throw_invalid ("missing separator"); }

Board::Board (const std::string& fen, int state_data) //FIXME Test this thoroughly.
{
	std::string::const_iterator pos = fen.begin ();

	int rank = N_RANKS - 1, file = 0; // FEN is backwards rank-wise
	while (pos != fen.end ())
	{
		if (file == N_FILES)
		{
			file = 0;
			if (--rank == -1)
				break;
			else
				fen_expect_separator ('/');
		}
		else if (Piece (*pos).Valid ())
		{
			board[rank][file++] = *pos++;
		}
		else if (*pos >= '1' && *pos <= '8' - int (file))
		{
			uint blank_count = *pos++ - '0';
			while (blank_count--)
				board[rank][file++] = Piece::NONE_CODE;
		}
		else
			fen_throw_invalid ("malformed piece placement");
	}
	if (rank != -1) fen_throw_invalid ("incomplete piece placement");

	fen_expect_separator (' ');
	if (pos == fen.end () || (*pos != 'w' && *pos != 'b'))
		fen_throw_invalid ("invalid active side");
	active_side = (*pos++ == 'w') ? SIDE_WHITE : SIDE_BLACK;

	fen_expect_separator (' ');
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::NONE;
	while (pos != fen.end ())
	{
		if (*pos == 'K')
			castling_options[SIDE_WHITE] |= Castling::KINGSIDE;
		else if (*pos == 'Q')
			castling_options[SIDE_WHITE] |= Castling::QUEENSIDE;
		else if (*pos == 'k')
			castling_options[SIDE_BLACK] |= Castling::KINGSIDE;
		else if (*pos == 'q')
			castling_options[SIDE_BLACK] |= Castling::QUEENSIDE;
		else if (*pos == '-')
			{}
		else
			break;
		++pos;
	}

	fen_expect_separator (' ');
	if (pos != fen.end () && *pos == '-')
	{
		en_passant_square.Clear ();
		++pos;
	}
	else
		{} //FIXME Restore en passant square.

	fen_expect_separator (' ');
	//FIXME Restore fifty move clock.

	fen_expect_separator (' ');
	//FIXME Restore fullmove number.

	game_state = GameState (state_data % 64); //FIXME Validate.

	Side _victor = Side (state_data / 64);
	if (!SideValid (_victor))
		throw std::invalid_argument ("invalid state data (victor)");
	else
		victor = _victor;

	CalculateData ();
}

#undef fen_expect_separator
#undef fen_throw_invalid



// basic data access

void
Board::GetSquaresWith (Squares& squares, const Piece& piece) const
{
	squares.clear ();
	auto range = piece_squares.equal_range (piece);
	for (auto square = range.first; square != range.second; ++square)
		squares.push_back (square->second);
}

std::string
Board::GetHalfmoveName () const
{
	char result[8];
	snprintf (result, 8, "%u%c", fullmove_number,
		(active_side == SIDE_WHITE) ? 'a' : 'b');
	return result;
}



// game status and analysis

bool
Board::IsUnderAttack (const Square& square, Side attacker) const
{
	(void) square; (void) attacker; return false; //FIXME Determine. This should be calculated and cached. As an infinite loop or circular dependency would result, needs to occur on a special board.
}



// movement and player actions

void
Board::MakeMove (const PieceMove& move)
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	if (!move.Valid ())
		throw std::runtime_error ("invalid move specified");

	//FIXME Confirm the move is possible on this board.

	// clear origin square
	(*this)[move.GetFrom ()] = Piece::NONE_CODE;

	// promote piece, if applicable
	Piece piece = move.GetPiece ();
	//FIXME identify and do the promotion

	// clear captured square, if applicable
	auto capture = dynamic_cast<const Capture*> (&move);
	if (capture)
		(*this)[capture->GetCapturedSquare ()] = Piece::NONE_CODE;

	// place piece in target square
	(*this)[move.GetTo ()] = piece.GetCode ();

	// move castling rook, if applicable
	if (auto castling = dynamic_cast<const Castling*> (&move))
	{
		(*this)[castling->GetRookFrom ()] = Piece::NONE_CODE;
		(*this)[castling->GetRookTo ()] =
			castling->GetRookPiece ().GetCode ();
	}

	// update castling options
	if (piece.type == PIECE_KING)
		castling_options[piece.side] = Castling::NONE;
	else if (move.GetPiece ().type == PIECE_ROOK && // not a new-promoted one
		 move.GetFrom ().rank == piece.GetInitialRank ())
	{
		if (move.GetFrom ().file == FILE_A)
			castling_options[piece.side] &= ~Castling::QUEENSIDE;
		else if (move.GetFrom ().file == FILE_H)
			castling_options[piece.side] &= ~Castling::KINGSIDE;
	}

	// move turn to opponent
	active_side = Opponent (move.GetSide ());

	// update en passant square
	if (auto two_square = dynamic_cast<const TwoSquarePawnMove*> (&move))
		en_passant_square = two_square->GetPassedSquare ();
	else
		en_passant_square.Clear ();

	// update fifty move clock
	if (piece.type == PIECE_PAWN || capture) //FIXME does a pawn being promoted count?
		fifty_move_clock = 0;
	else
		++fifty_move_clock;

	// update fullmove number
	if (move.GetSide () == SIDE_BLACK)
		++fullmove_number;

	//FIXME Record move in list.

	// update calculated data
	CalculateData ();
}

void
Board::Resign ()
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	//FIXME Record a Resignation (active_side) in list.

	EndGame (GAME_WON_RESIGNATION, Opponent (active_side));
}

void
Board::ExpireTime (Side against)
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	//FIXME Shouldn't this only apply to the active_side? How does its implementation contrast with the desired interface?

	//FIXME Record event in list.

	EndGame (GAME_WON_TIME_CONTROL, Opponent (against));
}

void
Board::Draw (GameState draw_type)
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	switch (draw_type)
	{
	case GAME_DRAWN_AGREEMENT:
	case GAME_DRAWN_THREEFOLD:
		// accept unconditionally; UI must detect/confirm conditions
		break;
	case GAME_DRAWN_FIFTY_MOVE:
		if (fifty_move_clock < 50)
			throw std::runtime_error ("fifty move rule not in effect");
		break;
	default:
		throw std::invalid_argument ("invalid draw type");
	}

	//FIXME Record event in list.

	EndGame (draw_type, SIDE_NONE);
}

void
Board::EndGame (GameState result, Side _victor)
{
	game_state = result;
	victor = _victor;

	// clear now-invalid state
	active_side = SIDE_NONE;
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::NONE;
	en_passant_square.Clear ();
	fifty_move_clock = 0;

	CalculateData ();
}



// persistent data and initial values

char&
Board::operator [] (const Square& square)
{
	if (!square.Valid ())
		throw std::invalid_argument ("invalid square specified");
	return board[square.rank][square.file];
}

const char&
Board::operator [] (const Square& square) const
{
	if (!square.Valid ())
		throw std::invalid_argument ("invalid square specified");
	return board[square.rank][square.file];
}

const char
Board::INITIAL_BOARD[N_RANKS * N_FILES + 1] =
	"RNBQKBNRPPPPPPPP\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0pppppppprnbqkbnr";

const Side
Board::INITIAL_SIDE = SIDE_WHITE;



// calculated data

void
Board::CalculateData ()
{
	piece_squares.clear ();
	possible_moves.clear ();
	for (uint side = 0; side < N_SIDES; ++side)
		in_check[side] = false;

	// update fen and state_data
	UpdatePersistence ();

	// bail out if game is over (only persistence is relevant)
	if (game_state != GAME_ONGOING) return;

	// update piece_squares
	for (uint rank = 0; rank < N_RANKS; ++rank)
	for (uint file = 0; file < N_FILES; ++file)
	{
		Square square ((File) file, (Rank) rank);
		Piece piece = GetPieceAt (square);
		if (piece.Valid ())
			piece_squares.insert ({ piece, square });
	}

	// detect dead position
	if (IsDeadPosition ())
		EndGame (GAME_DRAWN_DEAD_POSITION, SIDE_NONE);

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
			EndGame (GAME_WON_CHECKMATE, Opponent (active_side));
		else
			EndGame (GAME_DRAWN_STALEMATE, SIDE_NONE);
	}
}

void
Board::UpdatePersistence () //FIXME Test this thoroughly.
{
	std::stringstream new_fen;

	for (int rank = N_RANKS - 1 ; rank >= 0; --rank) // FEN is backwards
	{
		uint file = 0;
		while (file < N_FILES)
		{
			uint blank_count = 0;
			while (board[rank][file] == Piece::NONE_CODE &&
				++blank_count && ++file < N_FILES);
			if (blank_count > 0)
				new_fen << blank_count;
			if (file < N_FILES)
				new_fen << board[rank][file++];
		}
		if (rank > 0 ) new_fen << '/'; // don't delimit the last one
	}

	new_fen << ' ' << SideCode (active_side);

	new_fen << ' ';
	if (castling_options[SIDE_WHITE] & Castling::KINGSIDE)	new_fen << 'K';
	if (castling_options[SIDE_WHITE] & Castling::QUEENSIDE)	new_fen << 'Q';
	if (castling_options[SIDE_BLACK] & Castling::KINGSIDE)	new_fen << 'k';
	if (castling_options[SIDE_BLACK] & Castling::QUEENSIDE)	new_fen << 'q';
	if (castling_options[SIDE_WHITE] == Castling::NONE &&
	    castling_options[SIDE_BLACK] == Castling::NONE)	new_fen << '-';

	new_fen << ' ' << en_passant_square.GetCode ();
	new_fen << ' ' << fifty_move_clock;
	new_fen << ' ' << fullmove_number;

	fen = new_fen.str ();
	state_data = victor * 64 + game_state;
}

bool
Board::IsDeadPosition () const
{
	// dead positions detected, based on remaining non-king material:
	//   none; one knight; any number of bishops of same square color

	uint n_knights = 0, n_bishops[2] = { 0, 0 };
	for (auto& piece : piece_squares)
		switch (piece.first.type)
		{
		case PIECE_KING:
			break; // kings don't count here
		case PIECE_KNIGHT:
			++n_knights;
			break;
		case PIECE_BISHOP:
			++n_bishops[piece.second.GetColor ()];
			break;
		default:
			return false; // pawn, rook, or queen = not dead
		}

	if (n_knights <= 1 && n_bishops[0] == 0 && n_bishops[1] == 0)
		return true; // none, or one knight

	if (n_knights > 1)
		return false; // knights and bishops

	// any number of bishops of same square color
	return (n_bishops[0] == 0) || (n_bishops[1] == 0);
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
		g_pfnMPrintf ("enumerating moves of a %s on %s\n",
			piece.GetName ().data (), from.GetCode ().data ()); //FIXME FIXME debug
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
		ConfirmPossibleMove (*new PieceMove (piece, from,
			from.Offset (offset[0], offset[1])));

	// castling

	if (IsInCheck (piece.side)) return;
	Side opponent = Opponent (piece.side);

	if (castling_options[piece.side] & Castling::KINGSIDE)
	{
		Square rook_to = from.Offset (1, 0),
			king_to = from.Offset (2, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
			IsEmpty (king_to) && !IsUnderAttack (king_to, opponent))
			ConfirmPossibleMove (*new Castling
				(piece.side, Castling::KINGSIDE));
	}

	if (castling_options[piece.side] & Castling::QUEENSIDE)
	{
		Square rook_to = from.Offset (-1, 0),
			king_to = from.Offset (-2, 0),
			rook_pass = from.Offset (-3, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
			IsEmpty (king_to) && !IsUnderAttack (king_to, opponent)
			&& IsEmpty (rook_pass))
			ConfirmPossibleMove (*new Castling
				(piece.side, Castling::QUEENSIDE));
	}
}

void
Board::EnumerateRangedMoves (const Piece& piece, const Square& from)
{
	static const int PATHS[][2] =
	{
		{0,1}, {0,-1}, {1,0}, {-1,0}, // rook and queen
		{1,1}, {1,-1}, {-1,-1}, {-1,1} // bishop and queen
	};

	uint imin = (piece.type == PIECE_BISHOP) ? 4 : 0;
	uint imax = (piece.type == PIECE_ROOK) ? 4 : 8;
	for (uint i = imin; i < imax; ++i)
		for (Square to = from; to.Valid ();
			to = to.Offset (PATHS[i][0], PATHS[i][1]))
		{
			if (to == from) continue;
			ConfirmPossibleMove (*new PieceMove (piece, from, to));
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
		ConfirmPossibleMove (*new PieceMove (piece, from,
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
		ConfirmPossibleMove (*new PieceMove (piece, from, one_square));

		Square two_square = one_square.Offset (0, facing);
		if (IsEmpty (two_square) && from.rank == piece.GetInitialRank ())
			ConfirmPossibleMove
				(*new TwoSquarePawnMove (piece.side, from.file));
	}

	// captures

	for (int delta_file : { -1, 1 })
	{
		Square to = from.Offset (delta_file, facing);
		if (!IsEmpty (to))
			ConfirmPossibleMove (*new Capture (piece, from, to,
				GetPieceAt (to)));
		else if (to == en_passant_square)
			ConfirmPossibleMove (*new EnPassantCapture
				(piece.side, from.file, to.file));
	}
}

bool
Board::ConfirmPossibleMove (PieceMove& move)
{
	// Caller must set piece; from; to; and as applicable: MOVE_TWO_SQUARE,
	// MOVE_EN_PASSANT, MOVE_CASTLING, passed_square, and comoving_rook.
	// These checks are not performed for the comoving rook's movement.

	MovePtr move_ptr (&move);

	// move must be fundamentally valid
	if (!move.Valid ()) return false;

	// identify unmarked simple captures
	Piece to_occupant = GetPieceAt (move.GetTo ());
	if (!dynamic_cast<Capture*> (&move) && to_occupant.Valid ())
	{
		// move cannot be to friendly-occupied square
		if (move.GetPiece ().side == to_occupant.side)
			return false;
		//FIXME Replace move with a Capture of the to_occupant.
	}

	// identify promotion if pawn reaches opposing king's rank //FIXME Should PieceMove do this? Probably.
	if (move.GetPiece ().type == PIECE_PAWN && move.GetTo ().rank ==
		Piece (Opponent (move.GetSide ()), PIECE_KING).GetInitialRank ())
	{
		//FIXME Mark the move as a promotion to PIECE_QUEEN.
		//FIXME Support under-promotions.
	}

	g_pfnMPrintf ("...confirming a move: %s\n", move.GetDescription ().data ()); //FIXME FIXME debug

	// move cannot place piece's side in check
/*FIXME This creates an infinite loop. Check testing needs to either (1) use this board, requiring a private ReverseMove method; or (2) use a Board that doesn't calculate moves.
	Board check_test (*this);
	check_test.MakeMove (move);
	if (check_test.IsInCheck (move.piece.side)) return false;
*/

	possible_moves.push_back (std::move (move_ptr));
	return true;
}



} // namespace chess

