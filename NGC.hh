/******************************************************************************
 *  NGC.hh
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

#ifndef NGC_HH
#define NGC_HH

#include <Thief/Thief.hh>
using namespace Thief;

#include "Chess.hh"
using namespace Chess;


// Team: separation of Side (white vs. black) from player (good) vs. engine (bad)

typedef Being::Team Team;

Team get_chess_team (Side);
Side get_chess_side (Team);
int get_facing_direction (Side);



// ChessSet: wrapper for identifying and accessing chess sets in the gamesys.

struct ChessSet
{
	ChessSet (int number);
	int number;

	ChessSet (Team);
	Team get_team () const;

	ChessSet (Side);
	Side get_side () const;

	Object get_metaprop () const;
	Color get_color () const;
};



// HUDMessage: HUD element to display text messages associated with objects

class HUDMessage : public HUDElement
{
public:
	typedef std::unique_ptr<HUDMessage> Ptr;

	HUDMessage (HUD::ZIndex priority = 20);
	virtual ~HUDMessage ();

	bool enabled;
	enum class Position { TOPIC, CENTER, NW, NORTH, NE } position;
	CanvasPoint offset;
	static const CanvasPoint DEFAULT_OFFSET;

	Object topic;
	String identifier;

	String get_text () const { return text; }
	void set_text (const String&);

	Color get_color () const { return color_fg; }
	void set_color (const Color&, float luminance_mult = 1.0f);

protected:
	virtual bool prepare ();
	virtual void redraw ();

	static const int BORDER, PADDING;

	String text;
	Color color_fg, color_bg, color_border;
};



// NGCTitled: Base class for objects whose titles are displayed while focused.

class NGCTitled : public Script
{
public:
	NGCTitled (const String& name, const Object& host,
		const CIString& title_msgid = "title");

protected:
	virtual void initialize ();
	HUDMessage::Ptr title;
	Parameter<String> title_msgid;

private:
	Message::Result on_world_select (Message&);
	Message::Result on_world_deselect (Message&);
	Message::Result on_frob_world_begin (FrobMessage&);
};



// NGCIntro: Manages the introduction/scenario selection mission.

class NGCIntro : public Script
{
public:
	NGCIntro (const String& name, const Object& host);

private:
	virtual void initialize ();
	Message::Result prepare_mission (Message&);
	Message::Result choose_scenario (Message&);
	Message::Result start_briefing (Message&);
	Message::Result finish_briefing (ConversationMessage&);
};



// NGCScenario: In the introductory mission, selects one of the scenarios and
//	signals NGCIntro to proceed.

class NGCScenario : public NGCTitled
{
public:
	NGCScenario (const String& name, const Object& host);

private:
	virtual void initialize ();

	Message::Result select (FrobMessage&);

	Message::Result disable (Message&);
	bool disable_step ();
	Transition disable_trans;

	Message::Result enter_environment (Message&);

	Parameter<int> mission, chess_set;
	Persistent<bool> entered;
};



// NGCClock: Handles time control and the game clock interface.

class NGCClock : public NGCTitled
{
public:
	NGCClock (const String& name, const Object& host);

private:
	virtual void initialize ();

	Message::Result tick_tock (Message&);
	Message::Result stop_the_clock (Message&);

	Time get_time_remaining () const;
	void update_display ();

	Parameter<Time> time_control;
	Persistent<bool> running;

	Parameter<int> joint;
	Parameter<float> joint_low, joint_high;
};



// NGCFlag: Handles the draw, resignation, and exit-mission interface items.

class NGCFlag : public NGCTitled
{
public:
	NGCFlag (const String& name, const Object& host);

private:
	Message::Result ask_question (FrobMessage&);
	Message::Result intercept_deselect (Message&);
	Parameter<String> question;

	Message::Result answered_yes (FrobMessage&);
	Parameter<String> message_name;

	Message::Result answered_no (ContainmentMessage&);

	void end_question ();
	bool boomerang_step ();
	Persistent<Vector> orig_loc, orig_rot, drop_loc, drop_rot;
	Transition boomerang;
};



// NGCSquare: Handles interactions with a single square on the chess board.

class NGCSquare : public Script
{
public:
	enum class State
	{
		EMPTY,
		FRIENDLY_INERT,
		CAN_MOVE_FROM,
		CAN_MOVE_TO
	};

	NGCSquare (const String& name, const Object& host);

private:
	Message::Result update_state (Message&);
	Persistent<State> state;
	Persistent<Piece> piece;

	Rendered get_decal () const;
	void update_decal ();
	bool decal_fade_step ();
	Transition decal_fade;
	Parameter<Vector> decal_offset;

	Interactive get_button () const;
	void update_button ();
	bool button_fade_step ();
	Transition button_fade;
	Parameter<Vector> button_offset;

	Message::Result select (Message&);
	Message::Result deselect (Message&);
	Message::Result on_turn_on (Message&);
};



// NGCFireworks: Creates a fireworks display for victory celebrations.

class NGCFireworks : public TrapTrigger
{
public:
	NGCFireworks (const String& name, const Object& host);

private:
	virtual Message::Result on_trap (bool on, Message&);
	Message::Result launch_one (TimerMessage&);

	Parameter<int> count, spread;
};



#endif // NGC_HH

