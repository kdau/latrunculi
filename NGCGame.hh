/******************************************************************************
 *  NGCGame.hh
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

#ifndef NGCGAME_HH
#define NGCGAME_HH

#include "NGC.hh"
#include "ChessGame.hh"
#include "ChessEngine.hh"

class GameMessage : public HUDMessage
{
public:
	typedef std::unique_ptr<GameMessage> Ptr;

	GameMessage (Side, float luminance_mult);
};

class NGCGame : public Script
{
public:
	NGCGame (const String& name, const Object& host);
	virtual ~NGCGame ();

private:
	Object get_square (const Square&, bool proxy = false);
	Square get_square (const Object& square);

	Object get_piece_at (const Square&, bool proxy = false);
	Object get_piece_at (const Object& square);

	// Game and board state

	virtual void initialize ();

	Message::Result start_game (Message&);
	void arrange_board (const Object& origin, bool proxy);
	Object create_piece (const Object& square, const Piece&,
		bool start_positioned, bool proxy = false);

	void update_record ();
	void update_sim ();
	void update_interface ();
	Message::Result tick_tock (TimerMessage&);

	Chess::Game::Ptr game;
	Persistent<String> record;

	Parameter<Side> good_side;
	Persistent<Side> evil_side;

	enum class State
	{
		NONE = 0,
		INTERACTIVE,
		COMPUTING,
		MOVING
	};
	Persistent<State> state;

	// Player moves

	Message::Result select_from (Message&);
	Message::Result select_to (Message&);
	void clear_selection ();

	// Engine moves

	void prepare_engine (bool resume_computing);

	void start_computing ();
	Message::Result halt_computing (TimerMessage&);
	Message::Result check_engine (TimerMessage&);
	void finish_computing ();

	void engine_failure (const String& where, const String& what);
	Message::Result early_engine_failure (TimerMessage&);

	Chess::Engine::Ptr engine;

	// All moves

	void start_move (const Move::Ptr&, bool from_engine);
	Message::Result finish_move (Message&);
	void place_proxy (Rendered proxy, const Object& square);

	// Endgame

	Message::Result record_resignation (Message&);
	Message::Result record_time_control (Message&);
	Message::Result record_draw (Message&);
	void start_endgame ();
	Message::Result finish_endgame (Message&);

	// Announcements

	void announce_event (const Event::ConstPtr&);
	void herald_concept (Side, const String& concept, Time delay = 0ul);
	Message::Result end_announcement (TimerMessage&);
	GameMessage::Ptr announcement, good_check, evil_check;

	// Miscellaneous

	Message::Result show_logbook (Message&);

	void script_failure (const String& where, const String& what);
	Message::Result end_mission (TimerMessage&);

	Parameter<float> luminance_mult;
};

#endif // NGCGAME_HH

