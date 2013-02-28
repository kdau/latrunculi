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

#include <sstream>
#include <stdexcept>

#include "chess.h"

namespace chess {



/**
 ** Square (File, Rank, SquareColor)
 **/

Square::Square (File _file, Rank _rank)
	: file (_file), rank (_rank)
{}

Square::Square (const std::string& code)
	: file (FILE_NONE), rank (RANK_NONE)
{
	if (code.length () == 2)
	{
		file = (File) tolower (code[0]);
		rank = (Rank) code[1];
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
	Square result;
	result.file = File (file + delta_file);
	result.rank = Rank (rank + delta_rank);
	return (Valid () && result.Valid ()) ? result : Square ();
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

const char
Piece::CODES[N_SIDES][N_PIECE_TYPES+1] = { "KQRBNP", "kqrbnp" };

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
 ** Move (MoveBase, MoveType, MovePtr, Moves)
 **/

Move::Move (const Piece& _piece, const Square& _from, const Square& _to)
	: MoveBase ({ _piece, _from, _to }),
	  type (MOVE_NONE),
	  promotion (PIECE_NONE)
{}

std::string
Move::GetUCICode () const
{
	std::string result = from.GetCode () + to.GetCode ();
	if (type & MOVE_PROMOTION)
		result.push_back (Piece (SIDE_BLACK, promotion).GetCode ());
	return result;
}

Square
Move::GetCapturedSquare () const
{
	if (!(type & MOVE_CAPTURE)) return Square ();
	return (type & MOVE_EN_PASSANT) ? passed_square : to;
}


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
	//FIXME Restore active side.

	fen_expect_separator (' ');
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = CASTLING_NONE;
	while (pos != fen.end ())
	{
		if (*pos == 'K')
			castling_options[SIDE_WHITE] |= CASTLING_KINGSIDE;
		else if (*pos == 'Q')
			castling_options[SIDE_WHITE] |= CASTLING_QUEENSIDE;
		else if (*pos == 'k')
			castling_options[SIDE_BLACK] |= CASTLING_KINGSIDE;
		else if (*pos == 'q')
			castling_options[SIDE_BLACK] |= CASTLING_QUEENSIDE;
		else if (*pos == '-')
			{}
		else
			break;
		++pos;
	}

	fen_expect_separator (' ');
	//FIXME Restore en passant square.

	fen_expect_separator (' ');
	//FIXME Restore halfmove clock.

	fen_expect_separator (' ');
	//FIXME Restore fullmove number.

	game_state = GameState (state_data % 64); //FIXME Validate.
	victor = Side (state_data / 64); //FIXME Validate.
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




// game status and analysis

bool
Board::IsUnderAttack (const Square& square, Side attacker) const
{
	(void) square; (void) attacker; return false; //FIXME Determine.
}



// movement and player actions

void
Board::MakeMove (const MovePtr& move)
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");
	if (!move)
		throw std::invalid_argument ("no move specified");

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
}

void
Board::Resign ()
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	game_state = GAME_WON_RESIGNATION;
	victor = Opponent (active_side);

	CalculateData ();
}

void
Board::ExpireTime (Side against)
{
	if (game_state != GAME_ONGOING)
		throw std::runtime_error ("game already ended");

	game_state = GAME_WON_TIME_CONTROL;
	victor = Opponent (against);

	CalculateData ();
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
		if (halfmove_clock < 50)
			throw std::runtime_error ("fifty move rule not in effect");
		break;
	default:
		throw std::invalid_argument ("invalid draw type");
	}

	game_state = draw_type;
	victor = SIDE_NONE;

	CalculateData ();
}



// persistent data and initial values

char&
Board::operator[] (const Square& square)
{
	if (!square.Valid ())
		throw std::invalid_argument ("invalid square specified");
	return board[square.rank][square.file];
}

const char&
Board::operator[] (const Square& square) const
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

	// detect dead position
	if (IsDeadPosition ())
	{
		game_state = GAME_DRAWN_DEAD_POSITION;
		victor = SIDE_NONE;
	}

	// bail out if game is over
	if (game_state != GAME_ONGOING)
	{
		// check status is irrelevant at this point
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
	if (castling_options[SIDE_WHITE] & CASTLING_KINGSIDE)	new_fen << 'K';
	if (castling_options[SIDE_WHITE] & CASTLING_QUEENSIDE)	new_fen << 'Q';
	if (castling_options[SIDE_BLACK] & CASTLING_KINGSIDE)	new_fen << 'k';
	if (castling_options[SIDE_BLACK] & CASTLING_QUEENSIDE)	new_fen << 'q';
	if (castling_options[SIDE_WHITE] == CASTLING_NONE &&
	    castling_options[SIDE_BLACK] == CASTLING_NONE)	new_fen << '-';

	new_fen << ' ' << en_passant_square.GetCode ();
	new_fen << ' ' << halfmove_clock;
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
	Side opponent = Opponent (piece.side);

	if (castling_options[piece.side] & CASTLING_KINGSIDE)
	{
		Square rook_to = from.Offset (1, 0),
			king_to = from.Offset (2, 0),
			rook_from = from.Offset (3, 0);
		Piece rook = GetPieceAt (rook_from);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
			IsEmpty (king_to) && !IsUnderAttack (king_to, opponent))
		{
			Move* move = new Move (piece, from, king_to);
			move->type |= MOVE_CASTLING;
			move->comoving_rook.piece = rook;
			move->comoving_rook.from = rook_from;
			move->comoving_rook.to = rook_to;
			ConfirmPossibleMove (move);
		}
	}

	if (castling_options[piece.side] & CASTLING_QUEENSIDE)
	{
		Square rook_to = from.Offset (-1, 0),
			king_to = from.Offset (-2, 0),
			rook_pass = from.Offset (-3, 0),
			rook_from = from.Offset (-4, 0);
		Piece rook = GetPieceAt (rook_from);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
			IsEmpty (king_to) && !IsUnderAttack (king_to, opponent)
			&& IsEmpty (rook_pass))
		{
			Move* move = new Move (piece, from, king_to);
			move->type |= MOVE_CASTLING;
			move->comoving_rook.piece = rook;
			move->comoving_rook.from = rook_from;
			move->comoving_rook.to = rook_to;
			ConfirmPossibleMove (move);
		}
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
			move->passed_square = capture.Offset (0, -facing);
			ConfirmPossibleMove (move);
		}
	}
}

bool
Board::ConfirmPossibleMove (Move* _move)
{
	// Caller must set piece; from; to; and as applicable: MOVE_TWO_SQUARE,
	// MOVE_EN_PASSANT, MOVE_CASTLING, passed_square, and comoving_rook.
	// These checks are not performed for the comoving rook's movement.

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

