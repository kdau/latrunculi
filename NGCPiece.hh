/******************************************************************************
 *  NGCPiece.hh
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

#ifndef NGCPIECE_HH
#define NGCPIECE_HH

#include "NGC.hh"

class NGCPiece : public Script
{
public:
	NGCPiece (const String& name, const Object& host);

private:
	virtual void initialize ();

	Parameter<Team> team;
	Parameter<int> set;

	void tell_game (const char* message);
	Persistent<Object> game;

	// Interface

	Message::Result select (Message&);
	Message::Result deselect (Message&);

	Message::Result reveal (Message&);
	enum class Fade { NONE, IN, OUT };
	bool fade_step ();
	Parameter<float> max_opacity;
	Persistent<Fade> fade_state;
	Transition fade_trans;

	// Movement

	Message::Result reposition (Message&);
	bool reposition_step ();
	Persistent<Vector> reposition_start, reposition_end;
	Transition reposition_trans;

	Message::Result go_to_square (Message&);
	Message::Result arrive_at_square (AIActionResultMessage&);
	Persistent<Object> target_square;

	bool is_biped () const;
	Message::Result bow_to_king (TimerMessage&);
	Message::Result celebrate (Message&);

	// Combat

	Message::Result start_attack (Message&);
	Message::Result maintain_attack (TimerMessage&);
	void finish_attack ();
	Persistent<AI> victim;

	Message::Result become_victim (Message&);
	Message::Result be_attacked (Message&);
	Persistent<AI> attacker;

	AIAwarenessLink create_awareness (const Object& target, Time);

	Message::Result start_war (Message&);
	Message::Result finish_war (Message&);

	// Death and burial

	Message::Result force_death (TimerMessage&);
	Message::Result check_ai_mode (AIModeMessage&);
	Message::Result die (Message&);

	Message::Result start_burial (TimerMessage&);
	Message::Result finish_burial (TimerMessage&);

	Message::Result bury_corpse (Message&);
	Parameter<bool> is_corpse;

	// Promotion

	Message::Result start_promotion (Message&);
	Message::Result reveal_promotion (TimerMessage&);
	Message::Result finish_promotion (TimerMessage&);
	Persistent<AI> promotion;

	// Heralds

	Message::Result herald_concept (Message&);
	Message::Result subtitle_speech (PropertyMessage&);
	Message::Result finish_subtitle (TimerMessage&);
	HUDMessage::Ptr subtitle;

	// "Player"s

	Message::Result start_thinking (Message&);
	Message::Result finish_thinking (Message&);
};

#endif // NGCPIECE_HH

