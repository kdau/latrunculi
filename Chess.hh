/******************************************************************************
 *  Chess.hh
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

#ifndef CHESS_HH
#define CHESS_HH

#include <Thief/Thief.hh>

namespace Chess {



// String utilities

typedef std::string String;

enum class Case
{
	NOMINATIVE,
	DATIVE,
	ACCUSATIVE,
	TRANSLATIVE
};

String translate_format (const String& format_msgid, ...); //FIXME Don't use printf.



// Modified long algebraic notation, created here for serialization of history.
typedef String MLAN;



// Square (File, Rank)

enum class File : char { NONE = -1, A, B, C, D, E, F, G, H, _COUNT };
static constexpr size_t N_FILES = size_t (File::_COUNT);

enum class Rank : char { NONE = -1, R1, R2, R3, R4, R5, R6, R7, R8, _COUNT };
static constexpr size_t N_RANKS = size_t (File::_COUNT);

struct Square
{
	Square ();
	Square (File, Rank);
	File file;
	Rank rank;

	bool is_valid () const;
	bool operator == (const Square&) const;
	bool operator != (const Square&) const;

	explicit Square (const String& code);
	String get_code () const;

	enum class Color { NONE = -1, LIGHT, DARK };
	Color get_color () const;

	struct Delta { int file, rank; };
	Square offset (Delta) const;

	static const Square BEGIN;
	static constexpr size_t COUNT = N_RANKS * N_FILES;
	Square& operator ++ ();

	void clear ();
};

typedef std::vector<Square> Squares;



// Side

struct Side
{
	enum Value { NONE = -1, WHITE, BLACK };

	Side (Value = NONE);
	Value value;

	bool is_valid () const;
	bool operator == (const Side&) const;
	bool operator != (const Side&) const;

	explicit Side (char code);
	char get_code () const;

	String get_name (Case) const;

	int get_facing_direction () const;

	Side get_opponent () const;
};



// Piece

struct Piece
{
	enum class Type
	{
		NONE = -1,
		KING,
		QUEEN,
		ROOK,
		BISHOP,
		KNIGHT,
		PAWN,
		_COUNT
	};
	static constexpr size_t N_TYPES = size_t (Type::_COUNT);

	Piece ();
	Piece (Side, Type);
	Side side;
	Type type;

	bool is_valid () const;
	bool operator == (const Piece&) const;
	bool operator != (const Piece&) const;

	explicit Piece (char code);
	char get_code () const;
	void set_code (char code);
	static const char NONE_CODE;

	String get_name (Case) const;

	Rank get_initial_rank () const;

private:
	static const char* get_codes (Side);
};



// Event

class Event
{
public:
	typedef std::shared_ptr<Event> Ptr;
	typedef std::shared_ptr<const Event> ConstPtr;

	virtual ~Event ();

	bool is_valid () const { return valid; }

	virtual Side get_side () const = 0;

	virtual MLAN serialize () const = 0;
	static Event::Ptr deserialize (const MLAN&, Side active_side);

	virtual String get_description () const = 0;
	virtual String get_concept () const = 0;

protected:
	Event ();
	Event (const Event&) = delete;

	void invalidate ();

	friend bool operator == (const Event&, const Event&);
	friend bool operator != (const Event&, const Event&);
	virtual bool equals (const Event&) const = 0;

private:
	bool valid;
};

bool operator == (const Event&, const Event&);
bool operator != (const Event&, const Event&);



// Loss

class Loss : public Event
{
public:
	enum class Type
	{
		NONE = -1,

		// Detected and entered automatically.
		CHECKMATE,

		// Entered manually; UI must broker or detect.
		RESIGNATION,
		TIME_CONTROL
	};

	Loss (Type, Side);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	virtual Side get_side () const;
	Type get_type () const { return type; }

	virtual String get_description () const;
	virtual String get_concept () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Type type;
	Side side;
};



// Draw

class Draw : public Event
{
public:
	enum class Type
	{
		NONE = -1,

		// Detected and entered automatically.
		STALEMATE,
		DEAD_POSITION,

		// Entered manually; only accepted if conditions are present.
		FIFTY_MOVE,
		THREEFOLD_REPETITION,

		// Entered manually; UI must broker.
		BY_AGREEMENT
	};

	explicit Draw (Type);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	virtual Side get_side () const;
	Type get_type () const { return type; }

	virtual String get_description () const;
	virtual String get_concept () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Type type;
};



// Move

class Move : public Event
{
public:
	typedef std::shared_ptr<const Move> Ptr;

	Move (const Piece&, const Square& from, const Square& to);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	virtual Side get_side () const;
	Piece get_piece () const { return piece; }
	Square get_from () const { return from; }
	Square get_to () const { return to; }
	Piece::Type get_promotion () const { return promotion; }

	Piece get_promoted_piece () const;

	String get_uci_code () const;
	virtual String get_description () const;
	virtual String get_concept () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Piece piece;
	Square from;
	Square to;
	Piece::Type promotion;
};

typedef std::vector<Move::Ptr> Moves;



// Move types: Move, Capture, EnPassantCapture, TwoSquarePawnMove, Castling

class Capture : public Move
{
public:
	Capture (const Piece&, const Square& from,
		const Square& to, const Piece& captured);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	const Piece& get_captured_piece () const { return captured_piece; }
	virtual Square get_captured_square () const;

	virtual String get_description () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Piece captured_piece;
};

class EnPassantCapture : public Capture
{
public:
	EnPassantCapture (Side, File from, File to);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	virtual Square get_captured_square () const;

	virtual String get_description () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Square captured_square;
};

class TwoSquarePawnMove : public Move
{
public:
	TwoSquarePawnMove (Side, File);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	const Square& get_passed_square () const { return passed_square; }

protected:
	virtual bool equals (const Event&) const;

private:
	Square passed_square;
};

class Castling : public Move
{
public:
	enum class Type : unsigned
	{
		NONE = 0,
		KINGSIDE = 1,
		QUEENSIDE = 2,
		BOTH = KINGSIDE | QUEENSIDE
	};

	Castling (Side, Type);

	static Event::Ptr deserialize (const MLAN&, Side active_side);
	virtual MLAN serialize () const;

	Type get_castling_type () const { return type; }
	Piece get_rook_piece () const { return rook_piece; }
	Square get_rook_from () const { return rook_from; }
	Square get_rook_to () const { return rook_to; }

	virtual String get_description () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Type type;
	Piece rook_piece;
	Square rook_from;
	Square rook_to;
};



// Check: unofficial (not in history) event type for downstream use

class Check : public Event
{
public:
	explicit Check (Side);

	virtual MLAN serialize () const;

	virtual Side get_side () const;

	virtual String get_description () const;
	virtual String get_concept () const;

protected:
	virtual bool equals (const Event&) const;

private:
	Side side;
};



// To be implemented appropriately by downstream user.
String translate (const String& msgid, Side = Side::NONE);



} // namespace Chess

#include "Chess.inl"

#endif // CHESS_HH

