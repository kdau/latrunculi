/******************************************************************************
 *  ChessGame.hh
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

#ifndef CHESSGAME_HH
#define CHESSGAME_HH

#include "Chess.hh"

namespace Chess {



// Position

class Position
{
public:
	Position ();
	Position (const Position&);

	// serialize is intentionally non-virtual to allow FEN access for Game.
	Position (std::istream& fen);
	void serialize (std::ostream& fen) const;

	bool is_empty (const Square&) const;
	Piece get_piece_at (const Square&) const;

	Side get_active_side () const { return active_side; }
	Castling::Type get_castling_options (Side) const;
	Square get_en_passant_square () const { return en_passant_square; }

	unsigned get_fifty_move_clock () const { return fifty_move_clock; }
	unsigned get_fullmove_number () const { return fullmove_number; }

	bool is_under_attack (const Square&, Side attacker) const;
	bool is_in_check (Side = Side::NONE) const; // default: active side
	bool is_dead () const;

	// Compares positions according to threefold repetition rule.
	bool operator == (const Position&) const;

	virtual void make_move (const Move::Ptr&);

protected:
	char& operator [] (const Square&);
	const char& operator [] (const Square&) const;

	void end_game ();

private:
	char board [size_t (Rank::_COUNT)] [size_t (File::_COUNT)];
	Side active_side;
	unsigned castling_white, castling_black;
	Square en_passant_square;
	unsigned fifty_move_clock;
	unsigned fullmove_number;

	static const char* INITIAL_BOARD;
};



// Game

typedef std::pair<Position, Event::ConstPtr> HistoryEntry;
typedef std::vector<HistoryEntry> History;

class Game : public Position
{
public:
	Game ();
	Game (const Game&) = delete;

	Game (std::istream& record);
	void serialize (std::ostream& record);

	static String get_logbook_heading (unsigned page);
	static String get_halfmove_prefix (unsigned halfmove);

	// Status and analysis

	enum class Result
	{
		ONGOING,
		WON,
		DRAWN
	};

	Result get_result () const { return result; }
	Side get_victor () const { return victor; }

	const History& get_history () const { return history; }
	Event::ConstPtr get_last_event () const;
	bool is_third_repetition () const;

	const Moves& get_possibe_moves () const { return possible_moves; }
	Move::Ptr find_possible_move (const Square& from, const Square& to)
		const;
	Move::Ptr find_possible_move (const String& uci_code) const;

	// Movement and player actions

	virtual void make_move (const Move::Ptr&);
	void record_loss (Loss::Type, Side);
	void record_draw (Draw::Type);

private:
	void record_event (const Event::ConstPtr&);
	void end_game (Result, Side victor);
	void detect_endgames ();

	Result result;
	Side victor;
	History history;

	// Possible moves

	void update_possible_moves ();
	void enumerate_king_moves (const Piece&, const Square& from);
	void enumerate_rook_moves (const Piece&, const Square& from);
	void enumerate_bishop_moves (const Piece&, const Square& from);
	void enumerate_knight_moves (const Piece&, const Square& from);
	void enumerate_pawn_moves (const Piece&, const Square& from);
	bool confirm_possible_capture (const Piece&,
		const Square& from, const Square& to);
	bool confirm_possible_move (const Move::Ptr&);

	Moves possible_moves;
};



} // namespace Chess

#endif // CHESSGAME_HH

