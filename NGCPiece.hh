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

#include <Thief/Thief.hh>
using namespace Thief;

#include "Chess.hh"

class NGCPiece : public Script //FIXME inherit from Fade or replace functionality
{
public:
	NGCPiece (const String& name, const Object& host);

private:
	void initialize ();

	Parameter<Chess::Side> side;
	Parameter<int> set;

	void tell_game (const char* message);
	Persistent<Object> game;

	// Interface

	Message::Result select (Message&);
	Message::Result deselect (Message&);

	// Movement

	Message::Result reposition (Message&);
	bool reposition_step ();
	Persistent<Vector> reposition_start, reposition_end;
	Transition reposition_trans;

	Message::Result go_to_square (Message&);
	Message::Result arrive_at_square (AIActionResultMessage&);
	Persistent<Object> target_square;

	Message::Result bow_to_king (TimerMessage&);

	// Combat

	Message::Result start_attack (Message&);
	Message::Result maintain_attack (TimerMessage&);
	void finish_attack ();
	Persistent<AI> victim;

	Message::Result become_victim (Message&);
	Message::Result be_attacked (Message&);
	Persistent<AI> attacker;

	AIAwarenessLink create_awareness (const Object& target, Time);

	// Death and burial

	Message::Result force_death (TimerMessage&);
	Message::Result check_ai_mode (AIModeChangeMessage&);
	Message::Result check_death_stage (PropertyChangeMessage&);
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
	Object get_heraldry_source ();

	// "Player"s

	Message::Result start_thinking (Message&);
	Message::Result finish_thinking (Message&);
};

#endif // NGCPIECE_HH

