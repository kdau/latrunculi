/******************************************************************************
 *  Chess.cc
 *
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include "Chess.hh"
#include <cstdarg> //FIXME Remove once translate_format is replaced.
#include <cstdio> //FIXME Remove once translate_format is replaced.
#include <cstdlib>

namespace Chess {



String
translate_format (const String& format_msgid, ...)
{
	std::va_list va;
	char result[1024];
	va_start (va, format_msgid);
	std::vsnprintf (result, 1024, translate (format_msgid).data (), va);
	va_end (va);
	return result;
}



// Square

const Square
Square::BEGIN = { File::A, Rank::R1 };

const size_t
Square::COUNT = size_t (Rank::_COUNT) * size_t (File::_COUNT);

Square::Square (const String& code)
{
	if (code.length () == 2u)
	{
		file = File (tolower (code [0u]) - 'a');
		rank = Rank (code [1u] - '1');
	}
	else
		clear ();
}

String
Square::get_code () const
{
	char code[3u] = "-\0";
	if (is_valid ())
	{
		code[0u] = char (file) + 'a';
		code[1u] = char (rank) + '1';
	}
	return code;
}

Square::Color
Square::get_color () const
{
	return is_valid ()
		? ((char (file) % 2u == char (rank) % 2u)
			? Color::DARK : Color::LIGHT)
		: Color::NONE;
}

Square
Square::offset (Delta delta) const
{
	Square result (File (char (file) + delta.file),
		Rank (char (rank) + delta.rank));
	if (!is_valid () || !result.is_valid ()) result.clear ();
	return result;
}

Square&
Square::operator ++ ()
{
	file = File (char (file) + 1u);
	if (file == File::_COUNT)
	{
		rank = Rank (char (rank) + 1u);
		file = File::A;
	}
	return *this;
}

void
Square::clear ()
{
	file = File::NONE;
	rank = Rank::NONE;
}



// Side

Side::Side (char code)
	: value (NONE)
{
	switch (code)
	{
	case 'W': case 'w':
		value = WHITE;
		break;
	case 'B': case 'b':
		value = BLACK;
		break;
	default:
		break;
	}
}

char
Side::get_code () const
{
	switch (value)
	{
	case WHITE: return 'w';
	case BLACK: return 'b';
	default: return '-';
	}
}

String
Side::get_name (Case name_case) const
{
	if (!is_valid ()) return String ();

	const char* msgid = nullptr;
	switch (name_case)
	{
	case Case::NOMINATIVE: msgid = "side_nom_"; break;
	case Case::DATIVE: msgid = "side_dat_"; break;
	default: return String ();
	}

	return translate (msgid, *this);
}

int
Side::get_facing_direction () const
{
	switch (value)
	{
	case WHITE: return 1;
	case BLACK: return -1;
	default: return 0;
	}
}

Side
Side::get_opponent () const
{
	switch (value)
	{
	case WHITE: return BLACK;
	case BLACK: return WHITE;
	default: return NONE;
	}
}



// Piece

const char
Piece::NONE_CODE = '\0';

char
Piece::get_code () const
{
	return is_valid ()
		? get_codes (side) [size_t (type)]
		: NONE_CODE;
}

void
Piece::set_code (char code)
{
	side = Side::NONE;
	type = Type::NONE;

	for (Side _side : { Side::WHITE, Side::BLACK })
		for (int _type = 0; _type < int (Type::_COUNT); ++_type)
			if (code == get_codes (_side) [_type])
			{
				side = Side (_side);
				type = Type (_type);
				return;
			}
}

String
Piece::get_name (Case name_case) const
{
	if (!is_valid ()) return String ();

	String msgid;
	switch (name_case)
	{
	case Case::NOMINATIVE: msgid = "piece_nom_"; break;
	case Case::ACCUSATIVE: msgid = "piece_acc_"; break;
	case Case::TRANSLATIVE: msgid = "piece_tra_"; break;
	default: return String ();
	}

	msgid += get_codes (Side::BLACK) [size_t (type)];
	return translate (msgid, side);
}

Rank
Piece::get_initial_rank () const
{
	if (!is_valid ())
		return Rank::NONE;
	else if (side == Side::WHITE)
		return (type == Type::PAWN) ? Rank::R2 : Rank::R1;
	else // side == Side::BLACK
		return (type == Type::PAWN) ? Rank::R7 : Rank::R8;
}

const char*
Piece::get_codes (Side side)
{
	switch (side.value)
	{
	case Side::WHITE: return "KQRBNP";
	case Side::BLACK: return "kqrbnp";
	default: return "\0\0\0\0\0\0";
	}
}



// Event

Event::~Event () {}

Event::Ptr
Event::deserialize (const MLAN& mlan, Side active_side)
{
	Event::Ptr result;

#define TRY_PARSE(type) \
	{ result = type::deserialize (mlan, active_side); \
	if (result && result->is_valid ()) return result; }

	TRY_PARSE (Loss);
	TRY_PARSE (Draw);
		TRY_PARSE (Castling);
		TRY_PARSE (TwoSquarePawnMove);
			TRY_PARSE (EnPassantCapture);
		TRY_PARSE (Capture);
	TRY_PARSE (Move);

	result.reset ();
	return result;
}

bool
operator == (const Event& lhs, const Event& rhs)
{
	return typeid (lhs) == typeid (rhs) &&
		lhs.is_valid () && rhs.is_valid () && lhs.equals (rhs);
}

bool
operator != (const Event& lhs, const Event& rhs)
{
	return !(lhs == rhs);
}



// Loss

Loss::Loss (Type _type, Side _side)
	: type (_type), side (_side)
{
	switch (type)
	{
	case Type::CHECKMATE:
	case Type::RESIGNATION:
	case Type::TIME_CONTROL:
		if (!side.is_valid ())
			invalidate ();
		break;
	default:
		invalidate ();
	}
}

Event::Ptr
Loss::deserialize (const MLAN& mlan, Side active_side)
{
	Type type = Type::NONE;
	Side side = active_side;
	if (mlan == "#")
		type = Type::CHECKMATE;
	else if (mlan == "0")
		type = Type::RESIGNATION;
	else if (mlan.length () == 3u && mlan.substr (0u, 2u) == "TC")
	{
		type = Type::TIME_CONTROL;
		side = Side (mlan [2u]);
	}
	else
		return nullptr;
	return std::make_shared<Loss> (type, side);
}

MLAN
Loss::serialize () const
{
	switch (type)
	{
	case Type::CHECKMATE: return "#";
	case Type::RESIGNATION: return "0";
	case Type::TIME_CONTROL: return String ("TC") + side.get_code ();
	default: return String ();
	}
}

Side
Loss::get_side () const
	{ return side; }

String
Loss::get_description () const
{
	const char* msgid = nullptr;
	switch (type)
	{
	case Type::CHECKMATE: msgid = "loss_checkmate"; break;
	case Type::RESIGNATION: msgid = "loss_resignation"; break;
	case Type::TIME_CONTROL: msgid = "loss_time_control"; break;
	default: return String ();
	}

	return translate_format (msgid,
		side.get_name (Case::NOMINATIVE).data (),
		side.get_opponent ().get_name (Case::DATIVE).data ());
}

String
Loss::get_concept () const
{
	switch (type)
	{
	case Type::CHECKMATE: return "mate"; break;
	case Type::RESIGNATION: return "resign"; break;
	case Type::TIME_CONTROL: return "time"; break;
	default: return String ();
	}
}

bool
Loss::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Loss&> (_rhs);
	return type == rhs.type && side == rhs.side;
}



// Draw

Draw::Draw (Type _type)
	: type (_type)
{
	switch (type)
	{
	case Type::STALEMATE:
	case Type::DEAD_POSITION:
	case Type::FIFTY_MOVE:
	case Type::THREEFOLD_REPETITION:
	case Type::BY_AGREEMENT:
		break;
	default:
		invalidate ();
	}
}

Event::Ptr
Draw::deserialize (const MLAN& mlan, Side)
{
	Type type = Type::NONE;
	if (mlan == "SM")
		type = Type::STALEMATE;
	else if (mlan == "DP")
		type = Type::DEAD_POSITION;
	else if (mlan == "50M")
		type = Type::FIFTY_MOVE;
	else if (mlan == "3FR")
		type = Type::THREEFOLD_REPETITION;
	else if (mlan == "=")
		type = Type::BY_AGREEMENT;
	else
		return nullptr;
	return std::make_shared<Draw> (type);
}

MLAN
Draw::serialize () const
{
	switch (type)
	{
	case Type::STALEMATE: return "SM";
	case Type::DEAD_POSITION: return "DP";
	case Type::FIFTY_MOVE: return "50M";
	case Type::THREEFOLD_REPETITION: return "3FR";
	case Type::BY_AGREEMENT: return "=";
	default: return String ();
	}
}

Side
Draw::get_side () const
{
	return Side::NONE;
}

String
Draw::get_description () const
{
	const char* msgid = nullptr;
	switch (type)
	{
	case Type::STALEMATE: msgid = "draw_stalemate"; break;
	case Type::DEAD_POSITION: msgid = "draw_dead_position"; break;
	case Type::FIFTY_MOVE: msgid = "draw_fifty_move"; break;
	case Type::THREEFOLD_REPETITION:
		msgid = "draw_threefold_repetition"; break;
	case Type::BY_AGREEMENT: msgid = "draw_by_agreement"; break;
	default: return String ();
	}

	return translate_format (msgid,
		Side (Side::WHITE).get_name (Case::NOMINATIVE).data (),
		Side (Side::BLACK).get_name (Case::NOMINATIVE).data ());
}

String
Draw::get_concept () const
{
	return "draw";
}

bool
Draw::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Draw&> (_rhs);
	return type == rhs.type;
}



// Move

Move::Move (const Piece& _piece, const Square& _from, const Square& _to)
	: piece (_piece), from (_from), to (_to),
	  promotion (Piece::Type::NONE)
{
	if (!piece.is_valid () || !from.is_valid () || !to.is_valid () ||
	    from == to)
		invalidate ();

	// Identify the promotion if a pawn reaches the opposing king's rank.
	Piece opposing_king (piece.side.get_opponent (), Piece::Type::KING);
	if (piece.type == Piece::Type::PAWN &&
	    to.rank == opposing_king.get_initial_rank ())
		promotion = Piece::Type::QUEEN; // Always promote to queen.
}

Event::Ptr
Move::deserialize (const MLAN& mlan, Side active_side)
{
	if (mlan.length () < 6u || mlan.length () > 7u || mlan [3u] != '-')
		return nullptr;

	auto move = std::make_shared<Move> (Piece (mlan [0u]),
		Square (mlan.substr (1u, 2u)), Square (mlan.substr (4u, 2u)));
	if (!move || !move->is_valid () || move->get_side () != active_side)
		return nullptr;

	if ((mlan.length () == 7u &&
		move->get_promoted_piece ().get_code () != mlan [6u]) ||
	    (mlan.length () == 6u && move->get_promoted_piece ().is_valid ()))
		return nullptr;

	return move;
}

MLAN
Move::serialize () const
{
	String result;
	if (!is_valid ()) return result;
	result = piece.get_code () + from.get_code () + '-' + to.get_code ();
	if (get_promoted_piece ().is_valid ())
		result += get_promoted_piece ().get_code ();
	return result;
}

Side
Move::get_side () const
{
	return piece.side;
}

String
Move::get_uci_code () const
{
	String result = from.get_code () + to.get_code ();
	if (get_promoted_piece ().is_valid ())
		result += Piece (Side::BLACK, promotion).get_code ();
	return result;
}

String
Move::get_description () const
{
	return translate_format (
		get_promoted_piece ().is_valid ()
			? "move_empty_promotion" : "move_empty",
		piece.get_name (Case::NOMINATIVE).data (),
		from.get_code ().data (),
		to.get_code ().data (),
		get_promoted_piece ().get_name (Case::TRANSLATIVE).data ());
}

String
Move::get_concept () const
{
	return "move";
}

bool
Move::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Move&> (_rhs);
	return piece == rhs.piece && from == rhs.from && to == rhs.to;
}



// Capture

Capture::Capture (const Piece& _piece, const Square& _from,
                  const Square& _to, const Piece& _captured_piece)
	: Move (_piece, _from, _to),
	  captured_piece (_captured_piece)
{
	if (!captured_piece.is_valid () ||
	    get_piece ().side == captured_piece.side)
		invalidate ();
		
}

Event::Ptr
Capture::deserialize (const MLAN& mlan, Side active_side)
{
	if (mlan.length () < 7u || mlan.length () > 8u || mlan [3u] != 'x')
		return nullptr;

	auto capture = std::make_shared<Capture> (Piece (mlan [0u]),
		Square (mlan.substr (1u, 2u)), Square (mlan.substr (5u, 2u)),
		Piece (mlan [4u]));
	if (!capture || !capture->is_valid () ||
	    capture->get_side () != active_side)
		return nullptr;

	if ((mlan.length () == 8u &&
		capture->get_promoted_piece ().get_code () != mlan [7u]) ||
	    (mlan.length () == 7u && capture->get_promoted_piece ().is_valid ()))
		return nullptr;

	return capture;
}

MLAN
Capture::serialize () const
{
	String result;
	if (!is_valid ()) return result;
	result = get_piece ().get_code () + get_from ().get_code () + 'x' +
		captured_piece.get_code () + get_to ().get_code ();
	if (get_promoted_piece ().is_valid ())
		result += get_promoted_piece ().get_code ();
	return result;
}

Square
Capture::get_captured_square () const
{
	return get_to ();
}

String
Capture::get_description () const
{
	return translate_format (
		get_promoted_piece ().is_valid ()
			? "move_capture_promotion" : "move_capture",
		get_piece ().get_name (Case::NOMINATIVE).data (),
		get_from ().get_code ().data (),
		get_captured_piece ().get_name (Case::ACCUSATIVE).data (),
		get_to ().get_code ().data (),
		get_promoted_piece ().get_name (Case::TRANSLATIVE).data ());
}

bool
Capture::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Capture&> (_rhs);
	return Move::equals (_rhs) && captured_piece == rhs.captured_piece;
}



// EnPassantCapture

EnPassantCapture::EnPassantCapture (Side side, File _from, File _to)
	: Capture (
		Piece (side, Piece::Type::PAWN),
		Square (_from, (side == Side::WHITE) ? Rank::R5 : Rank::R4),
		Square (_to, (side == Side::WHITE) ? Rank::R6 : Rank::R3),
		Piece (side.get_opponent (), Piece::Type::PAWN)
	  ),
	  captured_square (_to, (side == Side::WHITE) ? Rank::R5 : Rank::R4)
{
	// Capture's validation will have failed on invalid side, from, or to.
	if (!captured_square.is_valid () ||
	    std::abs (char (_from) - char (_to)) != 1) // not an adjacent file
		invalidate ();
}

Event::Ptr
EnPassantCapture::deserialize (const MLAN& mlan, Side active_side)
{
	size_t pos = mlan.length ();
	if (pos < 4u || mlan.substr (pos - 4u) != "e.p.")
		return nullptr;

	auto capture = std::dynamic_pointer_cast<Capture>
		(Capture::deserialize (mlan.substr (0u, pos - 4u), active_side));
	if (!capture || !capture->is_valid ())
		return nullptr;

	return std::make_shared<EnPassantCapture> (capture->get_side (),
		capture->get_from ().file, capture->get_to ().file);
}

MLAN
EnPassantCapture::serialize () const
{
	return is_valid () ? (Capture::serialize () + "e.p.") : String ();
}

Square
EnPassantCapture::get_captured_square () const
{
	return captured_square;
}

String
EnPassantCapture::get_description () const
{
	return translate_format ("move_en_passant",
		get_piece ().get_name (Case::NOMINATIVE).data (),
		get_from ().get_code ().data (),
		get_captured_piece ().get_name (Case::ACCUSATIVE).data (),
		get_captured_square ().get_code ().data (),
		get_to ().get_code ().data ());
}

bool
EnPassantCapture::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const EnPassantCapture&> (_rhs);
	return Capture::equals (rhs) && captured_square == rhs.captured_square;
}



// TwoSquarePawnMove

TwoSquarePawnMove::TwoSquarePawnMove (Side side, File file)
	: Move (Piece (side, Piece::Type::PAWN),
		Square (file, (side == Side::WHITE) ? Rank::R2 : Rank::R7),
		Square (file, (side == Side::WHITE) ? Rank::R4 : Rank::R5)),
	  passed_square (file, (side == Side::WHITE) ? Rank::R3 : Rank::R6)
{} // Move's validation will have failed on invalid side or file.

Event::Ptr
TwoSquarePawnMove::deserialize (const MLAN& mlan, Side active_side)
{
	size_t pos = mlan.length ();
	if (pos < 4u || mlan.substr (pos - 4u) != "t.s.")
		return nullptr;

	auto move = std::dynamic_pointer_cast<Move>
		(Move::deserialize (mlan.substr (0u, pos - 4u), active_side));
	if (!move || !move->is_valid ())
		return nullptr;

	return std::make_shared<TwoSquarePawnMove>
		(move->get_side (), move->get_from ().file);
}

MLAN
TwoSquarePawnMove::serialize () const
{
	return is_valid () ? Move::serialize () + "t.s." : String ();
}

bool
TwoSquarePawnMove::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const TwoSquarePawnMove&> (_rhs);
	return Move::equals (rhs) && passed_square == rhs.passed_square;
}



// Castling

Castling::Castling (Side side, Type _type)
	: Move (Piece (side, Piece::Type::KING),
		Square (File::E, (side == Side::WHITE) ? Rank::R1 : Rank::R8),
		Square ((_type == Type::KINGSIDE) ? File::G : File::C,
			(side == Side::WHITE) ? Rank::R1 : Rank::R8)),
	  type (_type),
	  rook_piece (side, Piece::Type::ROOK)
{
	// Move's validation will have failed on invalid side
	if (type != Type::KINGSIDE && type != Type::QUEENSIDE)
		invalidate ();

	rook_from.rank = rook_to.rank =
		(side == Side::WHITE) ? Rank::R1 : Rank::R8;
	rook_from.file = (type == Type::KINGSIDE) ? File::H : File::A;
	rook_to.file = (type == Type::KINGSIDE) ? File::F : File::D;
}

Event::Ptr
Castling::deserialize (const MLAN& mlan, Side active_side)
{
	Type type = Type::NONE;
	if (mlan == "0-0")
		type = Type::KINGSIDE;
	else if (mlan == "0-0-0")
		type = Type::QUEENSIDE;
	else
		return nullptr;
	return std::make_shared<Castling> (active_side, type);
}

MLAN
Castling::serialize () const
{
	switch (type)
	{
	case Type::KINGSIDE: return "0-0";
	case Type::QUEENSIDE: return "0-0-0";
	default: return String ();
	}
}

String
Castling::get_description () const
{
	return translate_format
		((type == Type::KINGSIDE) ? "move_castle_ks" : "move_castle_qs",
		 get_side ().get_name (Case::NOMINATIVE).data ());
}

bool
Castling::equals (const Event& _rhs) const
{
	auto& rhs = dynamic_cast<const Castling&> (_rhs);
	return Move::equals (rhs) &&
		type == rhs.type &&
		rook_piece == rhs.rook_piece &&
		rook_from == rhs.rook_from &&
		rook_to == rhs.rook_to;
}



} // namespace Chess

