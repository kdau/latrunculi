/******************************************************************************
 *  Chess.inl
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
#error "This file should only be included from \"Chess.hh\"."
#endif

#ifndef CHESS_INL
#define CHESS_INL



namespace Thief {

THIEF_LGMULTI_SPECIALIZE_ (Chess::Piece, LGMulti<int>, Chess::Piece ())
THIEF_LGMULTI_SPECIALIZE_ (Chess::Side, LGMulti<int>, Chess::Side ())

template<> bool Parameter<Chess::Side>::decode (const String& raw) const;
template<> String Parameter<Chess::Side>::encode () const;

} // namespace Thief



namespace Chess {



// Square

inline
Square::Square ()
	: file (File::NONE), rank (Rank::NONE)
{}

inline
Square::Square (File _file, Rank _rank)
	: file (_file), rank (_rank)
{}

inline bool
Square::is_valid () const
{
	return file > File::NONE && file < File::_COUNT &&
		rank > Rank::NONE && rank < Rank::_COUNT;
}

inline bool
Square::operator == (const Square& rhs) const
{
	return is_valid () && rhs.is_valid () &&
		file == rhs.file && rank == rhs.rank;
}

inline bool
Square::operator != (const Square& rhs) const
{
	return !is_valid () || !rhs.is_valid () ||
		file != rhs.file || rank != rhs.rank;
}



// Side

inline
Side::Side (Value _value)
	: value (_value)
{}

inline bool
Side::is_valid () const
{
	return value == WHITE || value == BLACK;
}

inline bool
Side::operator == (const Side& rhs) const
{
	return value == rhs.value;
}

inline bool
Side::operator != (const Side& rhs) const
{
	return value != rhs.value;
}



// Piece

inline
Piece::Piece ()
	: side (Side::NONE), type (Type::NONE)
{}

inline
Piece::Piece (Side _side, Type _type)
	: side (_side), type (_type)
{}

inline
Piece::Piece (char code)
	: side (Side::NONE), type (Type::NONE)
{
	set_code (code);
}

inline bool
Piece::is_valid () const
{
	return side.is_valid () && type > Type::NONE && type < Type::_COUNT;
}

inline bool
Piece::operator == (const Piece& rhs) const
{
	return is_valid () && rhs.is_valid () &&
		side == rhs.side && type == rhs.type;
}

inline bool
Piece::operator != (const Piece& rhs) const
{
	return !is_valid () || !rhs.is_valid () ||
		side != rhs.side || type != rhs.type;
}



// Event

inline
Event::Event ()
	: valid (true)
{}

inline void
Event::invalidate ()
{
	valid = false;
}



// Move

inline Piece
Move::get_promoted_piece () const
{
	return Piece (piece.side, promotion);
}



} // namespace Chess

#endif // CHESS_INL

