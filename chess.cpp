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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "chess.h"

namespace chess {

static const Square::Delta KING_MOVES[] =
	{ {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}, {0,1} };
static const Square::Delta ROOK_MOVES[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };
static const Square::Delta BISHOP_MOVES[] = { {1,1}, {1,-1}, {-1,-1}, {-1,1} };
static const Square::Delta KNIGHT_MOVES[] =
	{ {1,2}, {2,1}, {-1,2}, {-2,1}, {-1,-2}, {-2,-1}, {1,-2}, {2,-1} };

std::string
TranslateFormat (const std::string& format_msgid, ...)
{
	std::va_list va;
	char result[1024];
	va_start (va, format_msgid);
	std::vsnprintf (result, 1024, Translate (format_msgid).data (), va);
	va_end (va);
	return result;
}



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
		file = File (tolower (code[0]) - 'a');
		rank = Rank (code[1] - '1');
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

bool
Square::operator != (const Square& rhs) const
{
	return !operator == (rhs);
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

Square::Color
Square::GetColor () const
{
	return Valid () ? ((file % 2 == rank % 2) ? DARK : LIGHT) : NONE;
}

Square
Square::Offset (Delta delta) const
{
	return Offset (delta.first, delta.second);
}

Square
Square::Offset (int delta_file, int delta_rank) const
{
	Square result (File (file + delta_file), Rank (rank + delta_rank));
	if (!Valid () || !result.Valid ()) result.Clear ();
	return result;
}

Square&
Square::operator ++ ()
{
	file = File (file + 1);
	if (file == N_FILES)
	{
		rank = Rank (rank + 1);
		file = FILE_A;
	}
	return *this;
}

const Square
Square::BEGIN = { FILE_A, RANK_1 };

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

Side
SideFromCode (char code)
{
	switch (code)
	{
		case 'W': case 'w': return SIDE_WHITE;
		case 'B': case 'b': return SIDE_BLACK;
		default: return SIDE_NONE;
	}
}

std::string
SideName (Side side, NameCase name_case)
{
	std::string msgid;
	switch (name_case)
	{
	case NOMINATIVE: msgid = "side_nom_"; break;
	case DATIVE: msgid = "side_dat_"; break;
	default: return std::string ();
	}

	return SideValid (side) ? Translate (msgid, side) : std::string ();
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

Piece::Piece (Side _side, Type _type)
	: side (_side), type (_type)
{}

Piece::Piece (char code)
	: side (SIDE_NONE), type (NONE)
{
	SetCode (code);
}

bool
Piece::Valid () const
{
	return SideValid (side) && type > NONE && type < N_TYPES;
}

bool
Piece::operator == (const Piece& rhs) const
{
	return Valid () && rhs.Valid () && side == rhs.side && type == rhs.type;
}

bool
Piece::operator != (const Piece& rhs) const
{
	return !operator == (rhs);
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
	type = NONE;

	for (unsigned _side = 0; _side < N_SIDES; ++_side)
		for (unsigned _type = 0; _type < N_TYPES; ++_type)
			if (code == CODES[_side][_type])
			{
				side = (Side) _side;
				type = (Type) _type;
				return;
			}
}

const char
Piece::NONE_CODE = '\0';

const char
Piece::CODES[N_SIDES][N_TYPES+1] = { "KQRBNP", "kqrbnp" };

std::string
Piece::GetName (NameCase name_case) const
{
	std::string msgid;
	switch (name_case)
	{
	case NOMINATIVE: msgid = "piece_nom_"; break;
	case ACCUSATIVE: msgid = "piece_acc_"; break;
	case TRANSLATIVE: msgid = "piece_tra_"; break;
	default: return std::string ();
	}

	if (!Valid ()) return std::string ();
	return Translate (msgid + CODES[SIDE_BLACK][type], side);
}

Rank
Piece::GetInitialRank () const
{
	if (!Valid ())
		return RANK_NONE;
	else if (side == SIDE_WHITE)
		return (type == PAWN) ? RANK_2 : RANK_1;
	else // side == SIDE_BLACK
		return (type == PAWN) ? RANK_7 : RANK_8;
}



/**
 ** Event (Loss, Draw, Move, Capture, EnPassantCapture,
 **        TwoSquarePawnMove, Castling)
 **/

EventPtr
Event::FromMLAN (const std::string& mlan, Side active_side)
{
	EventPtr result;

#define TryParse(type) \
	result = type::FromMLAN (mlan, active_side); \
	if (result && result->Valid ()) return result

	TryParse (Loss);
	TryParse (Draw);
		TryParse (Castling);
		TryParse (TwoSquarePawnMove);
			TryParse (EnPassantCapture);
		TryParse (Capture);
	TryParse (Move);

	return NULL;
}

bool
operator == (const Event& lhs, const Event& rhs)
{
	return typeid (lhs) == typeid (rhs) &&
		lhs.Valid () && rhs.Valid () && lhs.Equals (rhs);
}

bool
operator != (const Event& lhs, const Event& rhs)
{
	return !(lhs == rhs);
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

EventPtr
Loss::FromMLAN (const std::string& mlan, Side active_side)
{
	Type type = NONE;
	Side side = active_side;
	if (mlan.compare ("#") == 0)
		type = CHECKMATE;
	else if (mlan.compare ("0") == 0)
		type = RESIGNATION;
	else if (mlan.length () == 3 && mlan.substr (0, 2).compare ("TC") == 0)
	{
		type = TIME_CONTROL;
		side = SideFromCode (mlan[2]);
	}
	else
		return NULL;
	return std::make_shared<Loss> (type, side);
}

std::string
Loss::GetMLAN () const
{
	switch (type)
	{
	case CHECKMATE: return "#";
	case RESIGNATION: return "0";
	case TIME_CONTROL: return std::string ("TC") + SideCode (side);
	default: return std::string ();
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
	default: return std::string ();
	}
	return TranslateFormat (msgid,
		SideName (side, NOMINATIVE).data (),
		SideName (Opponent (side), DATIVE).data ());
}

std::string
Loss::GetConcept () const
{
	switch (type)
	{
	case CHECKMATE: return "mate"; break;
	case RESIGNATION: return "resign"; break;
	case TIME_CONTROL: return "time"; break;
	default: return std::string ();
	}
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
	case THREEFOLD_REPETITION:
	case BY_AGREEMENT:
		break;
	default:
		Invalidate ();
	}
}

EventPtr
Draw::FromMLAN (const std::string& mlan, Side)
{
	Type type = NONE;
	if (mlan.compare ("SM") == 0)
		type = STALEMATE;
	else if (mlan.compare ("DP") == 0)
		type = DEAD_POSITION;
	else if (mlan.compare ("50M") == 0)
		type = FIFTY_MOVE;
	else if (mlan.compare ("3FR") == 0)
		type = THREEFOLD_REPETITION;
	else if (mlan.compare ("=") == 0)
		type = BY_AGREEMENT;
	else
		return NULL;
	return std::make_shared<Draw> (type);
}

std::string
Draw::GetMLAN () const
{
	switch (type)
	{
	case STALEMATE: return "SM";
	case DEAD_POSITION: return "DP";
	case FIFTY_MOVE: return "50M";
	case THREEFOLD_REPETITION: return "3FR";
	case BY_AGREEMENT: return "=";
	default: return std::string ();
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
	case THREEFOLD_REPETITION: msgid = "draw_threefold_repetition"; break;
	case BY_AGREEMENT: msgid = "draw_by_agreement"; break;
	default: return std::string ();
	}
	return TranslateFormat (msgid,
		SideName (SIDE_WHITE, NOMINATIVE).data (),
		SideName (SIDE_BLACK, NOMINATIVE).data ());
}

std::string
Draw::GetConcept () const
{
	return "draw";
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
	  promotion (Piece::NONE)
{
	if (!piece.Valid () || !from.Valid () || !to.Valid () || from == to)
		Invalidate ();

	// identify promotion if pawn reaches opposing king's rank
	if (piece.type == Piece::PAWN && to.rank ==
		Piece (Opponent (GetSide ()), Piece::KING).GetInitialRank ())
		promotion = Piece::QUEEN; // always promote to queen
}

EventPtr
Move::FromMLAN (const std::string& mlan, Side active_side)
{
	if (mlan.length () < 6 || mlan.length () > 7 || mlan[3] != '-')
		return NULL;

	auto reference = std::make_shared<Move> (mlan[0], mlan.substr (1, 2),
		mlan.substr (4, 2));
	if (!reference || !reference->Valid () ||
	    reference->GetSide () != active_side)
		return NULL;

	if ((mlan.length () == 7 && reference->GetPromotion () != mlan[6]) ||
	    (mlan.length () == 6 && reference->GetPromotion ().Valid ()))
		return NULL;

	return reference;
}

std::string
Move::GetMLAN () const
{
	std::string result;
	if (!Valid ()) return result;
	result = GetPiece ().GetCode () + GetFrom ().GetCode () + '-' +
		GetTo ().GetCode ();
	if (GetPromotion ().Valid ())
		result += GetPromotion ().GetCode ();
	return result;
}

std::string
Move::GetUCICode () const
{
	std::string result = from.GetCode () + to.GetCode ();
	if (GetPromotion ().Valid ())
		result += Piece (SIDE_BLACK, promotion).GetCode ();
	return result;
}

std::string
Move::GetDescription () const
{
	return TranslateFormat (GetPromotion ().Valid ()
				? "move_empty_promotion" : "move_empty",
		GetPiece ().GetName (NOMINATIVE).data (),
		GetFrom ().GetCode ().data (),
		GetTo ().GetCode ().data (),
		GetPromotion ().GetName (TRANSLATIVE).data ());
}

std::string
Move::GetConcept () const
{
	return "move";
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

EventPtr
Capture::FromMLAN (const std::string& mlan, Side active_side)
{
	if (mlan.length () < 7 || mlan.length () > 8 || mlan[3] != 'x')
		return NULL;

	auto reference = std::make_shared<Capture> (mlan[0], mlan.substr (1, 2),
		mlan.substr (5, 2), mlan[4]);
	if (!reference || !reference->Valid () ||
	    reference->GetSide () != active_side)
		return NULL;

	if ((mlan.length () == 8 && reference->GetPromotion () != mlan[7]) ||
	    (mlan.length () == 7 && reference->GetPromotion ().Valid ()))
		return NULL;

	return reference;
}

std::string
Capture::GetMLAN () const
{
	std::string result;
	if (!Valid ()) return result;
	result = GetPiece ().GetCode () + GetFrom ().GetCode () + 'x' +
		captured_piece.GetCode () + GetTo ().GetCode ();
	if (GetPromotion ().Valid ())
		result += GetPromotion ().GetCode ();
	return result;
}

std::string
Capture::GetDescription () const
{
	return TranslateFormat (GetPromotion ().Valid ()
				? "move_capture_promotion" : "move_capture",
		GetPiece ().GetName (NOMINATIVE).data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName (ACCUSATIVE).data (),
		GetTo ().GetCode ().data (),
		GetPromotion ().GetName (TRANSLATIVE).data ());
}

bool
Capture::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Capture&> (_rhs);
	return Move::Equals (_rhs) && captured_piece == rhs.captured_piece;
}

EnPassantCapture::EnPassantCapture (Side side, File from, File to)
	: Capture (
		Piece (side, Piece::PAWN),
		Square (from, (side == SIDE_WHITE) ? RANK_5 : RANK_4),
		Square (to, (side == SIDE_WHITE) ? RANK_6 : RANK_3),
		Piece (Opponent (side), Piece::PAWN)
	  ),
	  captured_square (to, (side == SIDE_WHITE) ? RANK_5 : RANK_4)
{
	// Capture's validation will have failed on invalid side, from, or to
	if (!captured_square.Valid () ||
	    std::abs (from - to) != 1) // not an adjacent file
		Invalidate ();
}

EventPtr
EnPassantCapture::FromMLAN (const std::string& mlan, Side active_side)
{
	std::size_t pos = mlan.length ();
	if (pos < 4 || mlan.substr (pos - 4).compare ("e.p.") != 0)
		return NULL;

	auto reference = std::dynamic_pointer_cast<Capture>
		(Capture::FromMLAN (mlan.substr (0, pos - 4), active_side));
	if (reference && reference->Valid ())
		return std::make_shared<EnPassantCapture>
			(reference->GetSide (), reference->GetFrom ().file,
			 reference->GetTo ().file);
	else
		return NULL;
}

std::string
EnPassantCapture::GetMLAN () const
{
	return Valid () ? Capture::GetMLAN () + "e.p." : std::string ();
}

std::string
EnPassantCapture::GetDescription () const
{
	return TranslateFormat ("move_en_passant",
		GetPiece ().GetName (NOMINATIVE).data (),
		GetFrom ().GetCode ().data (),
		GetCapturedPiece ().GetName (ACCUSATIVE).data (),
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
	: Move (Piece (side, Piece::PAWN),
		Square (file, (side == SIDE_WHITE) ? RANK_2 : RANK_7),
		Square (file, (side == SIDE_WHITE) ? RANK_4 : RANK_5)),
	  passed_square (file, (side == SIDE_WHITE) ? RANK_3 : RANK_6)
{} // Move's validation will have failed on invalid side or file

EventPtr
TwoSquarePawnMove::FromMLAN (const std::string& mlan, Side active_side)
{
	std::size_t pos = mlan.length ();
	if (pos < 4 || mlan.substr (pos - 4).compare ("t.s.") != 0)
		return NULL;

	auto reference = std::dynamic_pointer_cast<Move>
		(Move::FromMLAN (mlan.substr (0, pos - 4), active_side));
	if (reference && reference->Valid ())
		return std::make_shared<TwoSquarePawnMove>
			(reference->GetSide (), reference->GetFrom ().file);
	else
		return NULL;
}

std::string
TwoSquarePawnMove::GetMLAN () const
{
	return Valid () ? Move::GetMLAN () + "t.s." : std::string ();
}

bool
TwoSquarePawnMove::Equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const TwoSquarePawnMove&> (_rhs);
	return Move::Equals (rhs) && passed_square == rhs.passed_square;
}

Castling::Castling (Side side, Type _type)
	: Move (Piece (side, Piece::KING),
		Square (FILE_E, (side == SIDE_WHITE) ? RANK_1 : RANK_8),
		Square ((_type == KINGSIDE) ? FILE_G : FILE_C,
			(side == SIDE_WHITE) ? RANK_1 : RANK_8)),
	  type (_type),
	  rook_piece (side, Piece::ROOK)
{
	// Move's validation will have failed on invalid side
	if (type != KINGSIDE && type != QUEENSIDE)
		Invalidate ();

	rook_from.rank = rook_to.rank = (side == SIDE_WHITE) ? RANK_1 : RANK_8;
	rook_from.file = (type == KINGSIDE) ? FILE_H : FILE_A;
	rook_to.file = (type == KINGSIDE) ? FILE_F : FILE_D;
}

EventPtr
Castling::FromMLAN (const std::string& mlan, Side active_side)
{
	Type type = NONE;
	if (mlan.compare ("0-0") == 0)
		type = KINGSIDE;
	else if (mlan.compare ("0-0-0") == 0)
		type = QUEENSIDE;
	else
		return NULL;
	return std::make_shared<Castling> (active_side, type);
}

std::string
Castling::GetMLAN () const
{
	switch (type)
	{
	case KINGSIDE: return "0-0";
	case QUEENSIDE: return "0-0-0";
	default: return std::string ();
	}
}

std::string
Castling::GetDescription () const
{
	return TranslateFormat
		((type == KINGSIDE) ? "move_castle_ks" : "move_castle_qs",
		 SideName (GetSide (), NOMINATIVE).data ());
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
	std::memcpy (board, INITIAL_BOARD, sizeof board);
	std::memcpy (castling_options, INITIAL_CASTLING,
		sizeof castling_options);
}

Position::Position (const Position& position)
	: active_side (position.active_side),
	  en_passant_square (position.en_passant_square),
	  fifty_move_clock (position.fifty_move_clock),
	  fullmove_number (position.fullmove_number)
{
	std::memcpy (board, position.board, sizeof board);
	std::memcpy (castling_options, position.castling_options,
		sizeof castling_options);
}

#define fen_throw_invalid(detail) \
	throw std::invalid_argument ("invalid FEN: " detail)

#define FEN_EXPECT_CHAR(expected, message) \
	if (c != expected) \
		fen_throw_invalid (message);

#define FEN_READ_CHAR(expected, message) \
	if (!(fen >> c) || c != expected) \
		fen_throw_invalid (message);

Position::Position (std::istream& fen)
{
	char c = '\0';

	std::noskipws (fen); // extract by character

	int rank = N_RANKS - 1, file = 0; // FEN is backwards rank-wise
	while (fen >> c)
	{
		if (file == N_FILES)
		{
			file = 0;
			if (--rank == -1)
			{
				FEN_EXPECT_CHAR (' ',
					"expected end of piece placement")
				break;
			}
			else
				FEN_EXPECT_CHAR ('/',
					"expected end of board rank")
		}
		else if (Piece (c).Valid ())
		{
			board[rank][file++] = c;
		}
		else if (c >= '1' && c <= '8' - int (file))
		{
			unsigned blank_count = c - '0';
			while (blank_count--)
				board[rank][file++] = Piece::NONE_CODE;
		}
		else
			fen_throw_invalid ("malformed piece placement");
	}
	if (rank != -1) fen_throw_invalid ("incomplete piece placement");

	bool got_side = false;
	while (fen >> c)
	{
		if (c == 'w')
			active_side = SIDE_WHITE;
		else if (c == 'b')
			active_side = SIDE_BLACK;
		else if (c == '-')
			active_side = SIDE_NONE; // for completed games
		else if (c == ' ')
			break;
		else
			fen_throw_invalid ("invalid active side");
		got_side = true;
	}
	if (!got_side) fen_throw_invalid ("missing active side");

	for (unsigned side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::NONE;
	bool got_castling = false;
	while (fen >> c)
	{
		if (c == 'K')
			castling_options[SIDE_WHITE] |= Castling::KINGSIDE;
		else if (c == 'Q')
			castling_options[SIDE_WHITE] |= Castling::QUEENSIDE;
		else if (c == 'k')
			castling_options[SIDE_BLACK] |= Castling::KINGSIDE;
		else if (c == 'q')
			castling_options[SIDE_BLACK] |= Castling::QUEENSIDE;
		else if (c == '-')
			{}
		else if (c == ' ')
			break;
		else
			fen_throw_invalid ("invalid castling options");
		got_castling = true;
	}
	if (!got_castling) fen_throw_invalid ("missing castling options");

	std::skipws (fen); // extract by token

	if (!fen) fen_throw_invalid ("missing en passant square");
	std::string _eps; fen >> _eps;
	if (_eps.compare ("-") == 0)
		en_passant_square.Clear ();
	else
	{
		Square eps (_eps);
		if (eps.Valid ())
			en_passant_square = eps;
		else
			fen_throw_invalid ("invalid en passant square");
	}

	if (!fen) fen_throw_invalid ("missing fifty move clock");
	fen >> fifty_move_clock;

	if (!fen) fen_throw_invalid ("missing fullmove number");
	fen >> fullmove_number;
}

void
Position::Serialize (std::ostream& fen) const
{
	for (int rank = N_RANKS - 1 ; rank >= 0; --rank) // FEN is backwards
	{
		unsigned file = 0;
		while (file < N_FILES)
		{
			unsigned blank_count = 0;
			while (board[rank][file] == Piece::NONE_CODE &&
				++blank_count && ++file < N_FILES);
			if (blank_count > 0)
				fen << blank_count;
			if (file < N_FILES)
				fen << board[rank][file++];
		}
		if (rank > 0 ) fen << '/'; // don't delimit the last one
	}

	fen << ' ' << SideCode (active_side);

	fen << ' ';
	if (castling_options[SIDE_WHITE] & Castling::KINGSIDE)	fen << 'K';
	if (castling_options[SIDE_WHITE] & Castling::QUEENSIDE)	fen << 'Q';
	if (castling_options[SIDE_BLACK] & Castling::KINGSIDE)	fen << 'k';
	if (castling_options[SIDE_BLACK] & Castling::QUEENSIDE)	fen << 'q';
	if (castling_options[SIDE_WHITE] == Castling::NONE &&
	    castling_options[SIDE_BLACK] == Castling::NONE)	fen << '-';

	fen << ' ' << en_passant_square.GetCode ();
	fen << ' ' << fifty_move_clock;
	fen << ' ' << fullmove_number;
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

unsigned
Position::GetCastlingOptions (Side side) const
{
	return SideValid (side)
		? castling_options[side] : unsigned (Castling::NONE);
}

bool
Position::IsUnderAttack (const Square& square, Side attacker) const
{
	if (!square.Valid () || !SideValid (attacker)) return false;

	// check for attacking kings
	for (auto& delta : KING_MOVES)
		if (GetPieceAt (square.Offset (delta)) ==
		    Piece (attacker, Piece::KING))
			return true;

	// check for attacking queens/rooks
	for (auto& delta : ROOK_MOVES)
		for (Square to = square.Offset (delta); to.Valid ();
		    to = to.Offset (delta))
			if (GetPieceAt (to) == Piece (attacker, Piece::QUEEN) ||
			    GetPieceAt (to) == Piece (attacker, Piece::ROOK))
				return true;
			else if (!IsEmpty (to))
				break; // can't pass an occupied square

	// check for attacking queens/bishops
	for (auto& delta : BISHOP_MOVES)
		for (Square to = square.Offset (delta); to.Valid ();
		    to = to.Offset (delta))
			if (GetPieceAt (to) == Piece (attacker, Piece::QUEEN) ||
			    GetPieceAt (to) == Piece (attacker, Piece::BISHOP))
				return true;
			else if (!IsEmpty (to))
				break; // can't pass an occupied square

	// check for attacking knights
	for (auto& delta : KNIGHT_MOVES)
		if (GetPieceAt (square.Offset (delta)) ==
		    Piece (attacker, Piece::KNIGHT))
			return true;

	// check for attacking pawns
	int facing = SideFacing (attacker);
	for (int delta_file : { -1, 1 })
	{
		if (GetPieceAt (square.Offset (delta_file, -facing)) ==
		    Piece (attacker, Piece::PAWN))
			return true;

		// check for en passant capture (behind EPS implies we're a pawn)
		if (square.Offset (0, facing) == GetEnPassantSquare ()
		    && GetPieceAt (square.Offset (delta_file, 0)) ==
		    Piece (attacker, Piece::PAWN))
		        return true;
	}

	return false;
}

bool
Position::IsInCheck (Side side) const
{
	if (side == SIDE_NONE) side = GetActiveSide ();
	Side opponent = Opponent (side);
	char king = Piece (side, Piece::KING).GetCode ();

	for (auto square = Square::BEGIN; square.Valid (); ++square)
		if ((*this)[square] == king && IsUnderAttack (square, opponent))
			return true;
	return false;
}

bool
Position::IsDead () const
{
	// dead positions detected, based on remaining non-king material:
	//   none; one knight; any number of bishops of same square color

	unsigned n_knights = 0, n_bishops[Square::N_COLORS] = { 0 };
	for (auto square = Square::BEGIN; square.Valid (); ++square)
		switch (GetPieceAt (square).type)
		{
		case Piece::KING: // kings don't count here
		case Piece::NONE:
			break;
		case Piece::KNIGHT:
			++n_knights;
			break;
		case Piece::BISHOP:
			++n_bishops[square.GetColor ()];
			break;
		default:
			return false; // pawn, rook, or queen = not dead
		}

	if (n_knights <= 1 &&
	    n_bishops[Square::LIGHT] == 0 &&
	    n_bishops[Square::DARK] == 0)
		return true; // none, or one knight

	if (n_knights > 1)
		return false; // knights and bishops

	// any number of bishops of same square color
	return (n_bishops[Square::LIGHT] == 0) ||
	       (n_bishops[Square::DARK] == 0);
}

bool
Position::operator == (const Position& rhs) const
{
	return std::memcmp (board, rhs.board, sizeof board) == 0 &&
		active_side == rhs.active_side &&
		std::memcmp (castling_options, rhs.castling_options,
			sizeof castling_options) == 0 &&
		(en_passant_square == rhs.en_passant_square ||
			(!en_passant_square.Valid () &&
			 !rhs.en_passant_square.Valid ()));
}

void
Position::MakeMove (const MovePtr& move)
{
	if (!move || !move->Valid ())
		throw std::runtime_error ("invalid move specified");

	// promote piece, if applicable
	Piece piece = move->GetPiece ();
	Piece::Type orig_type = piece.type;
	if (move->GetPromotion ().Valid ())
		piece.type = move->GetPromotion ().type;

	// move piece
	(*this)[move->GetFrom ()] = Piece::NONE_CODE;
	(*this)[move->GetTo ()] = piece.GetCode ();

	// clear captured square, if applicable
	auto capture = std::dynamic_pointer_cast<const Capture> (move);
	if (capture)
		(*this)[capture->GetCapturedSquare ()] = Piece::NONE_CODE;

	// move castling rook, if applicable
	if (auto castling = std::dynamic_pointer_cast<const Castling> (move))
	{
		(*this)[castling->GetRookFrom ()] = Piece::NONE_CODE;
		(*this)[castling->GetRookTo ()] =
			castling->GetRookPiece ().GetCode ();
	}

	// update castling options
	if (piece.type == Piece::KING)
		castling_options[piece.side] = Castling::NONE;
	else if (orig_type == Piece::ROOK && // not a new-promoted one
		 move->GetFrom ().rank == piece.GetInitialRank ())
	{
		if (move->GetFrom ().file == FILE_A)
			castling_options[piece.side] &= ~Castling::QUEENSIDE;
		else if (move->GetFrom ().file == FILE_H)
			castling_options[piece.side] &= ~Castling::KINGSIDE;
	}

	// move turn to opponent
	active_side = Opponent (move->GetSide ());

	// update en passant square
	if (auto two_square = std::dynamic_pointer_cast
	                        <const TwoSquarePawnMove> (move))
		en_passant_square = two_square->GetPassedSquare ();
	else
		en_passant_square.Clear ();

	// update fifty move clock
	if (orig_type == Piece::PAWN || capture)
		fifty_move_clock = 0;
	else
		++fifty_move_clock;

	// update fullmove number
	if (move->GetSide () == SIDE_BLACK)
		++fullmove_number;
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

void
Position::EndGame ()
{
	active_side = SIDE_NONE;
	for (unsigned side = 0; side < N_SIDES; ++side)
		castling_options[side] = Castling::NONE;
	en_passant_square.Clear ();
	fifty_move_clock = 0;
	// fullmove_number remains valid
}

const char
Position::INITIAL_BOARD[N_RANKS * N_FILES + 1] =
	"RNBQKBNRPPPPPPPP\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0pppppppprnbqkbnr";

const Side
Position::INITIAL_SIDE = SIDE_WHITE;

const unsigned
Position::INITIAL_CASTLING[N_SIDES] = { Castling::BOTH, Castling::BOTH };



/**
 ** Game
 **/

Game::Game ()
	: result (ONGOING),
	  victor (SIDE_NONE)
{
	UpdatePossibleMoves ();
}

Game::Game (std::istream& record)
	: Position (record),
	  result (ONGOING),
	  victor (SIDE_NONE)
{
	Side event_side = SIDE_WHITE;
	unsigned event_fullmove = 1;
	while (!record.eof ())
	{
		std::string token;
		record >> token;
		
		EventPtr event = Event::FromMLAN (token, event_side);
		if (!event)
			throw std::invalid_argument ("invalid event");
		RecordEvent (event);

		if (auto loss = std::dynamic_pointer_cast<Loss> (event))
		{
			result = WON;
			victor = Opponent (loss->GetSide ());
			event_side = SIDE_NONE;
		}
		else if (auto draw = std::dynamic_pointer_cast<Draw> (event))
		{
			result = DRAWN;
			event_side = SIDE_NONE;
		}
		else if (auto move = std::dynamic_pointer_cast<Move> (event))
		{
			if (event_side == SIDE_BLACK) ++event_fullmove;
			event_side = Opponent (event_side);
		}
	}
	if (GetFullmoveNumber () != event_fullmove)
		DebugMessage ("history inconsistent with position in game record");

	UpdatePossibleMoves ();
	DetectEndgames (); // just in case
}

void
Game::Serialize (std::ostream& record)
{
	Position::Serialize (record);
	for (auto& entry : history)
		if (entry.second)
			record << ' ' << entry.second->GetMLAN ();
}

std::string
Game::GetLogbookHeading (unsigned page)
{
	return TranslateFormat ("logbook_heading",
		SideName (SIDE_WHITE, DATIVE).data (),
		SideName (SIDE_BLACK, DATIVE).data (),
		page);
}

std::string
Game::GetHalfmovePrefix (unsigned halfmove)
{
	return TranslateFormat (
		(halfmove % 2 == 0) ? "event_prefix_a" : "event_prefix_b",
		halfmove / 2 + 1);
}

// status and analysis

EventConstPtr
Game::GetLastEvent () const
{
	return history.empty () ? EventConstPtr () : history.back ().second;
}

bool
Game::IsThirdRepetition () const
{
	unsigned repetitions = 1;
	for (auto& entry : history)
		if (*this == entry.first && ++repetitions == 3)
			return true;
	return false;
}

MovePtr
Game::FindPossibleMove (const Square& from, const Square& to) const
{
	for (auto& move : possible_moves)
		if (move && move->GetFrom () == from && move->GetTo () == to)
			return move;
	return NULL;
}

MovePtr
Game::FindPossibleMove (const std::string& uci_code) const
{
	Piece promotion;
	switch (uci_code.length ())
	{
	case 5:
		promotion.SetCode (uci_code.back ());
		if (!promotion.Valid ()) return NULL;
		// discard the promotion type; always promote to queen
	case 4:
		return FindPossibleMove (uci_code.substr (0, 2),
			uci_code.substr (2, 2));
	default:
		return NULL;
	}
}

// movement and player actions

void
Game::MakeMove (const MovePtr& move)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");
	if (!move) throw std::runtime_error ("no move specified");

	bool possible = false;
	for (auto& possible_move : possible_moves)
		if (*move == *possible_move)
		{
			possible = true;
			break;
		}

	if (!possible)
		throw std::runtime_error ("move not currently possible");

	RecordEvent (move);
	Position::MakeMove (move);
	UpdatePossibleMoves ();
	DetectEndgames ();
}

void
Game::RecordLoss (Loss::Type type, Side side)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");

	switch (type)
	{
	case Loss::CHECKMATE:
		throw std::runtime_error ("loss type must be automatically detected");
	case Loss::RESIGNATION:
		if (side != GetActiveSide ())
			throw std::runtime_error ("only active side may resign");
		break;
	case Loss::TIME_CONTROL:
		break;
	default:
		throw std::runtime_error ("invalid loss type");
	}

	RecordEvent (std::make_shared<Loss> (type, side));
	EndGame (WON, Opponent (side));
}

void
Game::RecordDraw (Draw::Type type)
{
	if (result != ONGOING) throw std::runtime_error ("game already ended");

	switch (type)
	{
	case Draw::STALEMATE:
	case Draw::DEAD_POSITION:
		throw std::invalid_argument ("draw type must be automatically detected");
	case Draw::FIFTY_MOVE:
		if (GetFiftyMoveClock () < 50)
			throw std::runtime_error ("fifty move rule not in effect");
		break;
	case Draw::THREEFOLD_REPETITION:
		if (!IsThirdRepetition ())
			throw std::runtime_error ("threefold repetition rule not in effect");
		break;
	case Draw::BY_AGREEMENT:
		// accept unconditionally; UI must broker
		break;
	default:
		throw std::runtime_error ("invalid draw type");
	}

	RecordEvent (std::make_shared<Draw> (type));
	EndGame (DRAWN, SIDE_NONE);
}

void
Game::RecordEvent (const EventConstPtr& event)
{
	history.push_back (HistoryEntry (*this, event));
}

void
Game::EndGame (Result _result, Side _victor)
{
	Position::EndGame ();
	result = _result;
	victor = _victor;
	possible_moves.clear ();
}

void
Game::DetectEndgames ()
{
	// bail out if game is already over
	if (result != ONGOING) return;

	// detect dead positions
	if (IsDead ())
	{
		RecordEvent (std::make_shared<Draw> (Draw::DEAD_POSITION));
		EndGame (DRAWN, SIDE_NONE);
	}

	// detect checkmate
	else if (possible_moves.empty () && IsInCheck ())
	{
		RecordEvent (std::make_shared<Loss>
			(Loss::CHECKMATE, GetActiveSide ()));
		EndGame (WON, Opponent (GetActiveSide ()));
	}

	// detect stalemate
	else if (possible_moves.empty ())
	{
		RecordEvent (std::make_shared<Draw> (Draw::STALEMATE));
		EndGame (DRAWN, SIDE_NONE);
	}
}

void
Game::UpdatePossibleMoves ()
{
	possible_moves.clear ();

	for (auto from = Square::BEGIN; from.Valid (); ++from)
	{
		Piece piece = (*this)[from];
		if (piece.side != GetActiveSide ()) continue;
		switch (piece.type)
		{
		case Piece::KING:
			EnumerateKingMoves (piece, from);
			break;
		case Piece::QUEEN:
			EnumerateRookMoves (piece, from);
			EnumerateBishopMoves (piece, from);
			break;
		case Piece::ROOK:
			EnumerateRookMoves (piece, from);
			break;
		case Piece::BISHOP:
			EnumerateBishopMoves (piece, from);
			break;
		case Piece::KNIGHT:
			EnumerateKnightMoves (piece, from);
			break;
		case Piece::PAWN:
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

	for (auto& delta : KING_MOVES)
		ConfirmPossibleCapture (piece, from, from.Offset (delta));

	// castling

	if (IsInCheck ()) return;
	Side opponent = Opponent (piece.side);

	if (GetCastlingOptions (piece.side) & Castling::KINGSIDE)
	{
		Square rook_to = from.Offset (1, 0),
			king_to = from.Offset (2, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
		    IsEmpty (king_to) && !IsUnderAttack (king_to, opponent))
			ConfirmPossibleMove (std::make_shared<Castling>
				(piece.side, Castling::KINGSIDE));
	}

	if (GetCastlingOptions (piece.side) & Castling::QUEENSIDE)
	{
		Square rook_to = from.Offset (-1, 0),
			king_to = from.Offset (-2, 0),
			rook_pass = from.Offset (-3, 0);
		if (IsEmpty (rook_to) && !IsUnderAttack (rook_to, opponent) &&
		    IsEmpty (king_to) && !IsUnderAttack (king_to, opponent)
		    && IsEmpty (rook_pass))
			ConfirmPossibleMove (std::make_shared<Castling>
				(piece.side, Castling::QUEENSIDE));
	}
}

void
Game::EnumerateRookMoves (const Piece& piece, const Square& from)
{
	for (auto& delta : ROOK_MOVES)
		for (Square to = from; to.Valid (); to = to.Offset (delta))
		{
			if (to == from) continue;
			ConfirmPossibleCapture (piece, from, to);
			if (!IsEmpty (to))
				break; // can't pass an occupied square
		}
}

void
Game::EnumerateBishopMoves (const Piece& piece, const Square& from)
{
	for (auto& delta : BISHOP_MOVES)
		for (Square to = from; to.Valid (); to = to.Offset (delta))
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
	for (auto& delta : KNIGHT_MOVES)
		ConfirmPossibleCapture (piece, from, from.Offset (delta));
}

void
Game::EnumeratePawnMoves (const Piece& piece, const Square& from)
{
	int facing = SideFacing (piece.side);

	// forward moves

	Square one_square = from.Offset (0, facing);
	if (IsEmpty (one_square))
	{
		ConfirmPossibleMove (std::make_shared<Move>
			(piece, from, one_square));

		Square two_square = one_square.Offset (0, facing);
		if (IsEmpty (two_square) && from.rank == piece.GetInitialRank ())
			ConfirmPossibleMove (std::make_shared<TwoSquarePawnMove>
				(piece.side, from.file));
	}

	// captures

	for (int delta_file : { -1, 1 })
	{
		Square to = from.Offset (delta_file, facing);
		if (GetPieceAt (to).side == Opponent (piece.side))
			ConfirmPossibleMove (std::make_shared<Capture>
				(piece, from, to, (*this)[to]));
		else if (to == GetEnPassantSquare ())
			ConfirmPossibleMove (std::make_shared<EnPassantCapture>
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
		return ConfirmPossibleMove (std::make_shared<Capture>
			(piece, from, to, to_occupant));

	else
		return ConfirmPossibleMove (std::make_shared<Move>
			(piece, from, to));
}

bool
Game::ConfirmPossibleMove (const MovePtr& move)
{
	// move must exist and be basically valid
	if (!move || !move->Valid ())
		return false;

	// move cannot place piece's side in check
	Position check_test (*this);
	check_test.MakeMove (move);
	if (check_test.IsInCheck (move->GetSide ()))
		return false;

	possible_moves.push_back (move);
	return true;
}



} // namespace chess

