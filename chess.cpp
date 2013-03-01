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
		: std::string ();
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
		: std::string ();
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
 ** Event (Loss, Draw, Move, Capture, EnPassantCapture,
 **        TwoSquarePawnMove, Castling)
 **/

bool
operator == (const Event& lhs, const Event& rhs)
{
	return typeid (lhs) == typeid (rhs) &&
		lhs.Valid () && rhs.Valid () && lhs.Equals (rhs);
}

#define DescribeEvent(msgid, ...) \
{ \
	char result[128]; \
	snprintf (result, 128, Translate (msgid).data (), __VA_ARGS__); \
	return result; \
}

Loss::Loss (Type _type, Side _side)
	: type (_type),
	  side (_side)
{
	switch (type)
	{
	case CHECKMATE:
	case RESIGNATION:
	case TIME_CONTROL:
		if (!SideValid (side))
			Invalidate ();
		break;
	default:
		Invalidate ();
	}
}

std::string
Loss::GetDescription () const
{
	const char* msgid = NULL;
	switch (type)
	{
	case CHECKMATE: msgid = "loss_checkmate"; break;
	case RESIGNATION: msgid = "loss_resignation"; break;
	case TIME_CONTROL: msgid = "loss_time_control"; break;
	}
	if (msgid)
		DescribeEvent (msgid, SideName (side).data ())
	else
		return std::string ();
}

bool
Loss::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Loss&> (_rhs);
	return type == rhs.type && side == rhs.side;
}

Draw::Draw (Type _type)
	: type (_type)
{
	switch (type)
	{
	case STALEMATE:
	case DEAD_POSITION:
	case FIFTY_MOVE:
	case THREEFOLD_REPITITION:
	case BY_AGREEMENT:
		break;
	default:
		Invalidate ();
	}
}

std::string
Draw::GetDescription () const
{
	const char* msgid = NULL;
	switch (type)
	{
	case STALEMATE: msgid = "draw_stalemate"; break;
	case DEAD_POSITION: msgid = "draw_dead_position"; break;
	case FIFTY_MOVE: msgid = "draw_fifty_move"; break;
	case THREEFOLD_REPITITION: msgid = "draw_threefold_repitition"; break;
	case BY_AGREEMENT: msgid = "draw_by_agreement"; break;
	}
	if (msgid)
		DescribeEvent (msgid,
			SideName (SIDE_WHITE).data (),
			SideName (SIDE_BLACK).data ())
	else
		return std::string ();
}

bool
Draw::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Draw&> (_rhs);
	return type == rhs.type;
}

Move::Move (const Piece& _piece, const Square& _from, const Square& _to)
	: piece (_piece),
	  from (_from),
	  to (_to),
	  promotion (PIECE_NONE)
{
	if (!piece.Valid () || !from.Valid () || !to.Valid () || from == to)
		Invalidate ();

	// identify promotion if pawn reaches opposing king's rank
	if (piece.type == PIECE_PAWN && to.rank ==
		Piece (Opponent (GetSide ()), PIECE_KING).GetInitialRank ())
		promotion = PIECE_QUEEN; //FIXME Support under-promotions.
}

std::string
Move::GetUCICode () const
{
	return from.GetCode () + to.GetCode () +
		Piece (SIDE_BLACK, promotion).GetCode ();
}

std::string
Move::GetDescription () const
{
	DescribeEvent ("move_to_empty",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetTo ().GetCode ().data ());
	//FIXME Describe promotions? Would also need to be in Capture.
}

bool
Move::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Move&> (_rhs);
	return piece == rhs.piece && from == rhs.from && to == rhs.to;
}

Capture::Capture (const Piece& piece, const Square& from,
                  const Square& to, const Piece& _captured_piece)
	: Move (piece, from, to),
	  captured_piece (_captured_piece)
{
	if (!captured_piece.Valid () ||
	    piece.side == captured_piece.side)
		Invalidate ();
		
}

std::string
Capture::GetDescription () const
{
	DescribeEvent ("move_capture",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName ().data (),
		GetTo ().GetCode ().data ());
}

bool
Capture::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Capture&> (_rhs);
	return Move::Equals (_rhs) && captured_piece == rhs.captured_piece;
}

EnPassantCapture::EnPassantCapture (Side side, File from, File to)
	: Capture (
		Piece (side, PIECE_PAWN),
		Square (from, (side == SIDE_WHITE) ? RANK_5 : RANK_4),
		Square (to, (side == SIDE_WHITE) ? RANK_6 : RANK_3),
		Piece (Opponent (side), PIECE_PAWN)
	  ),
	  captured_square (to, (side == SIDE_WHITE) ? RANK_5 : RANK_4)
{
	// Capture's validation will have failed on invalid side, from, or to
	if (!captured_square.Valid () ||
	    std::abs (from - to) != 1) // not an adjacent file
		Invalidate ();
}

std::string
EnPassantCapture::GetDescription () const
{
	DescribeEvent ("move_en_passant",
		GetPiece ().GetName ().data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName ().data (),
		GetCapturedSquare ().GetCode ().data (),
		GetTo ().GetCode ().data ());
}

bool
EnPassantCapture::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const EnPassantCapture&> (_rhs);
	return Capture::Equals (rhs) && captured_square == rhs.captured_square;
}

TwoSquarePawnMove::TwoSquarePawnMove (Side side, File file)
	: Move (Piece (side, PIECE_PAWN),
		Square (file, (side == SIDE_WHITE) ? RANK_2 : RANK_7),
		Square (file, (side == SIDE_WHITE) ? RANK_4 : RANK_5)),
	  passed_square (file, (side == SIDE_WHITE) ? RANK_3 : RANK_6)
{} // Move's validation will have failed on invalid side or file

bool
TwoSquarePawnMove::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const TwoSquarePawnMove&> (_rhs);
	return Move::Equals (rhs) && passed_square == rhs.passed_square;
}

Castling::Castling (Side side, Type _type)
	: Move (Piece (side, PIECE_KING),
		Square (FILE_E, (side == SIDE_WHITE) ? RANK_1 : RANK_8),
		Square ((_type == KINGSIDE) ? FILE_G : FILE_C,
			(side == SIDE_WHITE) ? RANK_1 : RANK_8)),
	  type (_type),
	  rook_piece (side, PIECE_ROOK)
{
	// Move's validation will have failed on invalid side
	if (type != KINGSIDE && type != QUEENSIDE)
		Invalidate ();

	rook_from.rank = rook_to.rank = (side == SIDE_WHITE) ? RANK_1 : RANK_8;
	rook_from.file = (type == KINGSIDE) ? FILE_H : FILE_A;
	rook_to.file = (type == KINGSIDE) ? FILE_F : FILE_D;
}

std::string
Castling::GetDescription () const
{
	DescribeEvent ((type == KINGSIDE) ? "move_castle_k" : "move_castle_q",
		SideName (GetSide ()).data ());
}

bool
Castling::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Castling&> (_rhs);
	return Move::Equals (rhs) &&
		type == rhs.type &&
		rook_piece == rhs.rook_piece &&
		rook_from == rhs.rook_from &&
		rook_to == rhs.rook_to;
}



/**
 ** Position
 **/

Position::Position ()
	: active_side (INITIAL_SIDE),
	  en_passant_square (),
	  fifty_move_clock (0),
	  fullmove_number (1)
{
	memcpy (board, INITIAL_BOARD, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::BOTH;
	UpdateTransient ();
}

Position::Position (const Position& position)
	: active_side (position.active_side),
	  en_passant_square (position.en_passant_square),
	  fifty_move_clock (position.fifty_move_clock),
	  fullmove_number (position.fullmove_number)
{
	memcpy (board, position.board, N_RANKS * N_FILES);
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = position.castling_options[side];
	UpdateTransient ();
}

#define fen_throw_invalid(detail) \
	throw std::invalid_argument ("invalid FEN: " detail);

#define fen_expect_separator(separator) \
	{ if (pos == fen.end () || *pos++ != separator) \
		fen_throw_invalid ("missing separator"); }

Position::Position (const std::string& fen) //FIXME Test this thoroughly.
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

	UpdateTransient ();
}

bool
Position::IsEmpty (const Square& square) const
{
	return !GetPieceAt (square).Valid ();
}

Piece
Position::GetPieceAt (const Square& square) const
{
	return square.Valid () ? (*this)[square] : Piece::NONE_CODE;
}

uint
Position::GetCastlingOptions (Side side) const
{
	return SideValid (side)
		? castling_options[side] : uint (Castling::NONE);
}

std::string
Position::GetHalfmoveName () const
{
	char result[8];
	snprintf (result, 8, "%u%c", fullmove_number,
		(active_side == SIDE_WHITE) ? 'a' : 'b');
	return result;
}

bool
Position::IsDead () const
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
Position::MakeMove (const Move& move)
{
	if (!move.Valid ())
		throw std::runtime_error ("invalid move specified");

	// clear origin square
	(*this)[move.GetFrom ()] = Piece::NONE_CODE;

	// promote piece, if applicable
	Piece piece = move.GetPiece ();
	PieceType orig_type = piece.type;
	if (move.GetPromotion () != PIECE_NONE)
		piece.type = move.GetPromotion ();

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
	else if (orig_type == PIECE_ROOK && // not a new-promoted one
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
	if (orig_type == PIECE_PAWN || capture)
		fifty_move_clock = 0;
	else
		++fifty_move_clock;

	// update fullmove number
	if (move.GetSide () == SIDE_BLACK)
		++fullmove_number;

	// update transient data
	UpdateTransient ();
}

char&
Position::operator [] (const Square& square)
{
	if (!square.Valid ())
		throw std::invalid_argument ("invalid square specified");
	return board[square.rank][square.file];
}

const char&
Position::operator [] (const Square& square) const
{
	if (!square.Valid ())
		throw std::invalid_argument ("invalid square specified");
	return board[square.rank][square.file];
}

PieceSquaresRange
Position::GetSquaresWith (const Piece& piece) const
{
	return piece_squares.equal_range (piece);
}

void
Position::EndGame ()
{
	active_side = SIDE_NONE;
	for (uint side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::NONE;
	en_passant_square.Clear ();
	fifty_move_clock = 0;
	// fullmove_number remains valid
	UpdateTransient ();
}

const char
Position::INITIAL_BOARD[N_RANKS * N_FILES + 1] =
	"RNBQKBNRPPPPPPPP\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0pppppppprnbqkbnr";

const Side
Position::INITIAL_SIDE = SIDE_WHITE;

void
Position::UpdateTransient ()
{
	// update fen //FIXME Test this thoroughly.

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

	// update piece_squares

	piece_squares.clear ();
	for (uint rank = 0; rank < N_RANKS; ++rank)
	for (uint file = 0; file < N_FILES; ++file)
	{
		Square square ((File) file, (Rank) rank);
		Piece piece = (*this)[square];
		if (piece.Valid ())
			piece_squares.insert ({ piece, square });
	}
}



/**
 ** Game
 **/

Game::Game ()
	: result (ONGOING),
	  victor (SIDE_NONE)
{
	UpdateTransient ();
}

Game::Game (const std::string& fen, int state_data) //FIXME Test the state_data math.
	: Position (fen)
{
	Result _result = Result (state_data % 64);
	if (result != ONGOING && result != WON && result != DRAWN)
		throw std::invalid_argument ("invalid state data (result)");
	else
		result = _result;

	Side _victor = Side (state_data / 64);
	if (!SideValid (_victor))
		throw std::invalid_argument ("invalid state data (victor)");
	else
		victor = _victor;

	UpdateTransient ();
}

int
Game::GetStateData () const
{
	return victor * 64 + result;
}

// status and analysis

const Move*
Game::FindPossibleMove (const Square& from, const Square& to) const
{
	for (auto& event : possible_moves)
	{
		auto move = dynamic_cast<Move*> (&*event);
		if (move && move->GetFrom () == from && move->GetTo () == to)
			return move;
	}
	return NULL;
}

bool
Game::IsUnderAttack (const Square& square) const
{
	return square.Valid () ? false : false; //FIXME Read from under_attack.
}

// movement and player actions

void
Game::MakeMove (const Move& move)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");

	bool possible = false;
	for (auto& possible_move : possible_moves)
		if (move == *possible_move)
		{
			possible = true;
			break;
		}

	if (!possible)
		throw std::runtime_error ("move not currently possible");

	Position::MakeMove (move);
	//FIXME Copy move and RecordEvent() it.
	UpdateTransient ();
}

void
Game::RecordLoss (const Loss::Type& type)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");

	if (!Loss (type, GetActiveSide ()).Valid ())
		throw std::runtime_error ("invalid loss type");

	if (type == Loss::CHECKMATE)
		throw std::runtime_error ("loss type must be automatically detected");

	RecordEvent (*new Loss (type, GetActiveSide ()));
	EndGame (WON, Opponent (GetActiveSide ()));
}

void
Game::RecordDraw (const Draw::Type& type)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");

	if (!Draw (type).Valid ())
		throw std::runtime_error ("invalid draw type");

	switch (type)
	{
	case Draw::STALEMATE:
	case Draw::DEAD_POSITION:
		throw std::invalid_argument ("draw type must be automatically detected");
	case Draw::FIFTY_MOVE:
		if (GetFiftyMoveClock () < 50)
			throw std::runtime_error ("fifty move rule not in effect");
		break;
	case Draw::THREEFOLD_REPITITION:
		//FIXME Store positions as well as moves and check this for validity?
	case Draw::BY_AGREEMENT:
		// accept unconditionally; UI must detect/confirm conditions
		break;
	}

	RecordEvent (*new Draw (type));
	EndGame (DRAWN, SIDE_NONE);
}

void
Game::RecordEvent (Event& event)
{
	history.push_back (std::move (EventPtr (&event)));
}

void
Game::EndGame (Result result, Side _victor)
{
	result = result;
	victor = _victor;
	Position::EndGame ();
	UpdateTransient ();
}

// transient data

void
Game::UpdateTransient ()
{
	possible_moves.clear ();
	in_check = false;

	// bail out if game is over (only persistence is relevant)
	if (result != ONGOING) return;

	// detect dead position
	if (IsDead ())
	{
		RecordEvent (*new Draw (Draw::DEAD_POSITION));
		EndGame (DRAWN, SIDE_NONE);
	}

	// update possible_moves
	EnumerateMoves ();

	// update under_attack
	//FIXME Do. As an infinite loop or circular dependency would result, needs to occur on a special board.

	// update in_check
	auto range = GetSquaresWith (Piece (GetActiveSide (), PIECE_KING));
	for (auto square = range.first; square != range.second; ++square)
		if (IsUnderAttack (square->second))
			in_check = true;

	// detect checkmate or stalemate
	if (possible_moves.empty () && in_check)
	{
		RecordEvent (*new Loss (Loss::CHECKMATE, GetActiveSide ()));
		EndGame (WON, Opponent (GetActiveSide ()));
	}
	else if (possible_moves.empty ())
	{
		RecordEvent (*new Draw (Draw::STALEMATE));
		EndGame (DRAWN, SIDE_NONE);
	}
}

void
Game::EnumerateMoves ()
{
	possible_moves.clear ();

	for (uint rank = 0; rank < N_RANKS; ++rank)
	for (uint file = 0; file < N_FILES; ++file)
	{
		Square from ((File) file, (Rank) rank);
		Piece piece = (*this)[from];
		if (piece.side != GetActiveSide ()) continue;
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
Game::EnumerateKingMoves (const Piece& piece, const Square& from)
{
	// basic moves

	static const int OFFSETS[][2] =
		{ {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}, {0,1} };
	for (auto& offset : OFFSETS)
		ConfirmPossibleCapture (piece, from,
			from.Offset (offset[0], offset[1]));

	// castling

	if (IsInCheck ()) return;

	if (GetCastlingOptions (piece.side) & Castling::KINGSIDE)
	{
		Square rook_to = from.Offset (1, 0),
			king_to = from.Offset (2, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to) &&
				IsEmpty (king_to) && !IsUnderAttack (king_to))
			ConfirmPossibleMove (*new Castling
				(piece.side, Castling::KINGSIDE));
	}

	if (GetCastlingOptions (piece.side) & Castling::QUEENSIDE)
	{
		Square rook_to = from.Offset (-1, 0),
			king_to = from.Offset (-2, 0),
			rook_pass = from.Offset (-3, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to) &&
				IsEmpty (king_to) && !IsUnderAttack (king_to)
				&& IsEmpty (rook_pass))
			ConfirmPossibleMove (*new Castling
				(piece.side, Castling::QUEENSIDE));
	}
}

void
Game::EnumerateRangedMoves (const Piece& piece, const Square& from)
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
			ConfirmPossibleCapture (piece, from, to);
			if (!IsEmpty (to))
				break; // can't pass an occupied square
		}
}

void
Game::EnumerateKnightMoves (const Piece& piece, const Square& from)
{
	static const int OFFSETS[][2] =
		{ {1,2}, {2,1}, {-1,2}, {-2,1}, {-1,-2}, {-2,-1}, {1,-2}, {2,-1} };
	for (auto& offset : OFFSETS)
		ConfirmPossibleCapture (piece, from,
			from.Offset (offset[0], offset[1]));
}

void
Game::EnumeratePawnMoves (const Piece& piece, const Square& from)
{
	int facing = SideFacing (piece.side);

	// forward moves

	Square one_square = from.Offset (0, facing);
	if (IsEmpty (one_square))
	{
		ConfirmPossibleMove (*new Move (piece, from, one_square));

		Square two_square = one_square.Offset (0, facing);
		if (IsEmpty (two_square) && from.rank == piece.GetInitialRank ())
			ConfirmPossibleMove
				(*new TwoSquarePawnMove (piece.side, from.file));
	}

	// captures

	for (int delta_file : { -1, 1 })
	{
		Square to = from.Offset (delta_file, facing);
		if (GetPieceAt (to).side == Opponent (piece.side))
			ConfirmPossibleMove (*new Capture
				(piece, from, to, (*this)[to]));
		else if (to == GetEnPassantSquare ())
			ConfirmPossibleMove (*new EnPassantCapture
				(piece.side, from.file, to.file));
	}
}

bool
Game::ConfirmPossibleCapture (const Piece& piece, const Square& from,
                              const Square& to)
{
	Piece to_occupant = GetPieceAt (to);
		
	if (piece.side == to_occupant.side)
		return false; // move cannot be to friendly-occupied square

	else if (to_occupant.Valid ())
		return ConfirmPossibleMove
			(*new Capture (piece, from, to, to_occupant));

	else
		return ConfirmPossibleMove (*new Move (piece, from, to));
}

bool
Game::ConfirmPossibleMove (Move& move)
{
	EventPtr move_ptr (&move);

	// move must be fundamentally valid
	if (!move.Valid ()) return false;

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

