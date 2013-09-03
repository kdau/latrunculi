/******************************************************************************
 *  NGCPiece.cc
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

#include "NGCPiece.hh"

namespace Duration
{
	static const Time MOVE = 10000ul;
	static const Time CASTLING_ROOK = 3000ul;
	static const Time PROMOTION = 3000ul;
	static const Time ATTACK = 15000ul;
	static const Time DEATH = 3000ul;
	static const Time CORPSING = 250ul;
	static const Time BURIAL = 1000ul;
}


//FIXME Removed support for TellIntro message, which was supporting subtitles and convo-done notification in the briefing conversation. Replace those functions.



NGCPiece::NGCPiece (const String& _name, const Object& _host)
	: Script (_name, _host),
	  PARAMETER_ (side, "chess_side", Side::NONE),
	  PARAMETER_ (set, "chess_set", 0),
	  PERSISTENT (game, Object::NONE),
	  PERSISTENT_ (reposition_start), PERSISTENT_ (reposition_end),
	  reposition_trans (*this, &NGCPiece::reposition_step, "Reposition",
	  	10ul, 500ul, Curve::LOG_10, "slide_time", "slide_curve"),
	  PERSISTENT (target_square, Object::NONE),
	  PERSISTENT (victim, Object::NONE),
	  PERSISTENT (attacker, Object::NONE),
	  PARAMETER_ (is_corpse, "chess_corpse", false),
	  PERSISTENT (promotion, Object::NONE)
{
	listen_message ("Select", &NGCPiece::select);
	listen_message ("Deselect", &NGCPiece::deselect);

	listen_message ("Reposition", &NGCPiece::reposition);
	listen_message ("GoToSquare", &NGCPiece::go_to_square);
	listen_message ("ObjActResult", &NGCPiece::arrive_at_square);
	listen_timer ("BowToKing", &NGCPiece::bow_to_king);

	listen_message ("AttackPiece", &NGCPiece::start_attack);
	listen_timer ("MaintainAttack", &NGCPiece::maintain_attack);
	listen_message ("BecomeVictim", &NGCPiece::become_victim);
	listen_message ("BeAttacked", &NGCPiece::be_attacked);

	listen_timer ("ForceDeath", &NGCPiece::force_death);
	listen_message ("AIModeChange", &NGCPiece::check_ai_mode);
	listen_message ("PropertyChange", &NGCPiece::check_death_stage);
	listen_message ("Slain", &NGCPiece::die);

	listen_timer ("StartBurial", &NGCPiece::start_burial);
	listen_timer ("FinishBurial", &NGCPiece::finish_burial);
	listen_message ("Create", &NGCPiece::bury_corpse);

	listen_message ("BePromoted", &NGCPiece::start_promotion);
	listen_timer ("RevealPromotion", &NGCPiece::reveal_promotion);
	listen_timer ("FinishPromotion", &NGCPiece::finish_promotion);

	listen_message ("HeraldConcept", &NGCPiece::herald_concept);

	listen_message ("StartThinking", &NGCPiece::start_thinking);
	listen_message ("FinishThinking", &NGCPiece::finish_thinking);
}

void
NGCPiece::initialize ()
{
	game = Object ("TheGame");
	ObjectProperty::subscribe ("DeathStage", host ());
}



void
NGCPiece::tell_game (const char* message)
{
	GenericMessage (message).send (host (), game);
}



// Interface

Message::Result
NGCPiece::select (Message&)
{
	host ().add_metaprop (Object ("M-SelectedPiece"));
	host_as<DynamicLight> ().brightness.instantiate ();
	return Message::CONTINUE;
}

Message::Result
NGCPiece::deselect (Message&)
{
	host ().remove_metaprop (Object ("M-SelectedPiece"));
	host_as<DynamicLight> ().brightness.remove ();
	return Message::CONTINUE;
}



// Movement

Message::Result
NGCPiece::reposition (Message& message)
{
	auto square = message.get_data (Message::DATA1, Object ());
	auto direct = message.get_data (Message::DATA2, false);

	if (square == Object::NONE)
		square = Link::get_one ("~Population", host ()).get_dest ();
	if (square == Object::NONE)
		return Message::HALT;

	Vector origin = host ().get_location ();
	Vector target = square.get_location ();

	if (direct)
		target.z += 0.5f; // don't be stuck in ground
	else
		target.z = origin.z; // don't move on z axis

	Vector distance = target - origin;
	if (direct || distance.magnitude () < 0.25f)
		host ().set_location (target);
	else
	{
		reposition_start = origin;
		reposition_end = target;
		reposition_trans.start ();
	}

	return Message::CONTINUE;
}

bool
NGCPiece::reposition_step ()
{
	host ().set_location (reposition_trans.interpolate
		(reposition_start, reposition_end));
	if (reposition_trans.is_finished ())
		host_as<AI> ().send_signal ("FaceEnemy"); //FIXME Okay to only do it once?
	return true;
}

Message::Result
NGCPiece::go_to_square (Message& message)
{
	target_square = message.get_data (Message::DATA1, Object::NONE);
	if (target_square == Object::NONE) return Message::HALT;

	bool castling_king = ScriptParamsLink::get_one_by_data
		(host (), "ComovingRook").exists ();

	host_as<AI> ().go_to_location (target_square,
		castling_king ? AI::Speed::FAST : AI::Speed::NORMAL,
		AI::ActionPriority::HIGH, String ("ArriveAtSquare"));
	return Message::CONTINUE;
}

Message::Result
NGCPiece::arrive_at_square (AIActionResultMessage& message)
{
	if (message.get_result_data<String> () != "ArriveAtSquare")
		return Message::CONTINUE;

	if (target_square == Object::NONE)
		return Message::HALT;
	target_square = Object::NONE;

	auto king_link = ScriptParamsLink::get_one_by_data (host (), "MyLiege");

	if (victim != Object::NONE)
		GenericMessage::with_data ("AttackPiece", Object (victim)).send
			(host (), host ());

	else if (promotion != Object::NONE)
		GenericMessage::with_data ("BePromoted", Object (promotion))
			.send (host (), host ());

	else if (king_link.exists ()) // This is a castling rook.
	{
		tell_game ("FinishMove");
		host_as<AI> ().face_object (king_link.get_dest ());
		king_link.destroy ();
		start_timer ("BowToKing", 500ul, false);
	}

	else
	{
		start_timer ("Reposition", 500ul, false);

		auto rook_link = ScriptParamsLink::get_one_by_data
			(host (), "ComovingRook");
		auto rook_to_link = ScriptParamsLink::get_one_by_data
			(host (), "RookTo");
		if (rook_link.exists () && rook_to_link.exists ())
		{
			GenericMessage::with_data ("GoToSquare",
				rook_to_link.get_dest ()).send
					(host (), rook_link.get_dest ());

			rook_link.destroy ();
			rook_to_link.destroy ();
		}
		else
			tell_game ("FinishMove");
	}

	return Message::CONTINUE;
}

Message::Result
NGCPiece::bow_to_king (TimerMessage&)
{
	host_as<AI> ().play_motion ("humsalute3"); // (pseudo-)bipeds only
	start_timer ("Reposition", 3000ul, false);
	return Message::CONTINUE;
}



// Combat

Message::Result
NGCPiece::start_attack (Message& message)
{
	victim = message.get_data (Message::DATA1, Object::NONE);
	if (victim == Object::NONE) return Message::HALT;

	host ().add_metaprop (Object ("M-ChessAttacker"));
	create_awareness (victim, message.get_time ());

	if (target_square != Object::NONE)
		// The attack will begin upon arrival at the square.
		return Message::HALT;

	host_as<AI> ().set_minimum_alert (AI::Alert::HIGH);

	GenericMessage ("BeAttacked").send (host (), victim);
	start_timer ("MaintainAttack", 1ul, false);

	// If needed, the victim's death timer will lead to a finish_attack call.
	return Message::CONTINUE;
}

Message::Result
NGCPiece::maintain_attack (TimerMessage& message)
{
	if (victim == Object::NONE)
		return Message::HALT;

	if (!victim->exists () || victim->mode == AI::Mode::DEAD ||
	    victim->hit_points <= 0)
	{
		finish_attack ();
		return Message::HALT;
	}

	create_awareness (victim, message.get_time ());

	bool have_attack_link = false;
	for (auto& attack_link : AIAttackLink::get_all (host ()))
		if (attack_link.get_dest () == victim)
			have_attack_link = true;
		else
			attack_link.destroy ();

	if (!have_attack_link)
		AIAttackLink::create (host (), victim, AI::Priority::VERY_HIGH);

	host_as<AI> ().mode = AI::Mode::COMBAT;
	start_timer ("MaintainAttack", 125ul, false);
	return Message::CONTINUE;
}

void
NGCPiece::finish_attack ()
{
	if (victim == Object::NONE) return;
	victim = Object::NONE;

	auto self = host_as<AI> ();

	self.remove_metaprop (Object ("M-ChessAttacker"));

	// Notify AttackActivate to turn off weapons/particles.
	GenericMessage ("AbortAttack").send (host (), host ());

	// Break off all attacks and potentially hostile awarenesses.
	for (auto& attack_link : AIAttackLink::get_all (host ()))
		attack_link.destroy ();
	for (auto& aware_link : AIAwarenessLink::get_all (host ()))
		aware_link.destroy ();

	self.set_minimum_alert (AI::Alert::NONE);
	self.clear_alertness ();

	// Prevent some non-human AIs from continuing first alert barks.
	self.halt_speech ();

	// Go to the final destination. FinishMove will be sent from there.
	Object square = Link::get_one ("~Population", self).get_dest ();
	if (square == Object::NONE)
		square = ScriptParamsLink::get_one_by_data
			(host (), "ExPopulation", true).get_dest ();
	if (square != Object::NONE)
		GenericMessage::with_data ("GoToSquare", square).send
			(host (), host ());
}

Message::Result
NGCPiece::become_victim (Message& message)
{
	host ().remove_metaprop (Object ("M-ChessAlive"));
	host ().add_metaprop (Object ("M-ChessAttackee"));

	create_awareness (message.get_from (), message.get_time ());
	host_as<AI> ().face_object (message.get_from ());

	// Don't set attacker variable until it's official.
	return Message::CONTINUE;
}

Message::Result
NGCPiece::be_attacked (Message& message)
{
	attacker = message.get_from ();
	create_awareness (attacker, message.get_time ());
	start_timer ("ForceDeath", Duration::ATTACK, false);
	return Message::CONTINUE;
}

AIAwarenessLink
NGCPiece::create_awareness (const Object& target, Time time)
{
	AIAwarenessLink aware = Link::get_one ("AIAwareness", host (), target);
	if (aware.exists ())
	{
		aware.seen = true;
		aware.update_level (AIAwarenessLink::Level::HIGH, time);
		aware.update_contact (target.get_location (), time, true);
		aware.update (time, true);
		return aware;
	}
	else
		return AIAwarenessLink::create (host (), target,
			AIAwarenessLink::SEEN | AIAwarenessLink::CAN_RAYCAST |
			AIAwarenessLink::HAVE_LOS | AIAwarenessLink::FIRSTHAND,
			AIAwarenessLink::Level::HIGH, time,
			target.get_location (), 0);
}



// Death and burial

Message::Result
NGCPiece::force_death (TimerMessage&)
{
	host_as<Damageable> ().slay (attacker);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::check_ai_mode (AIModeChangeMessage& message)
{
        if (message.get_new_mode () == AI::Mode::DEAD)
		die (message);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::check_death_stage (PropertyChangeMessage& message)
{
	if (message.get_property () == Property ("DeathStage") &&
	    host_as<Damageable> ().death_stage == 12) // being slain
		die (message);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::die (Message&)
{
	if (attacker == Object::NONE) return Message::HALT;
	attacker = Object::NONE;

	// Ensure that any corpses will bury themselves appropriately.
	QuestVar ("chess_corpse_side").set (side->value);

	// Set timer to do it on ourself, if we are not replaced.
	start_timer ("StartBurial", Duration::DEATH, false);

	return Message::CONTINUE;
}

Message::Result
NGCPiece::start_burial (TimerMessage&)
{
	if (!side->is_valid ())
		side = Side::Value (QuestVar ("chess_corpse_side").get ());

	// Create a smoke puff at the site of death.
	Object puff_type = Object ("ChessBurialPuff"),
		puff = Object::create (puff_type);
	puff.set_location (host ().get_location ());

	// Create a smoke puff at the gravesite, if any.
	String grave_name = "ChessGrave";
	grave_name += side->get_code ();
	Object grave = Object (grave_name);
	if (grave != Object::NONE)
	{
		ScriptParamsLink::create (host (), grave, "Grave");
		puff = Object::create (puff_type);
		puff.set_location (grave.get_location ());
	}

	GenericMessage ("FadeOut").send (host (), host ()); //FIXME
	start_timer ("FinishBurial", Duration::BURIAL, false);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::finish_burial (TimerMessage&)
{
	Object grave = ScriptParamsLink::get_one_by_data (host (),
		"Grave").get_dest ();
	if (grave != Object::NONE)
	{
		Vector location = grave.get_location ();
		host ().set_location (location);
		GenericMessage ("FadeIn").send (host (), host ()); //FIXME

		// Displace the grave marker (for rows instead of piles).
		location.x += Parameter<float> (grave, "XIncrement", {0.0f});
		location.y += Parameter<float> (grave, "YIncrement", {0.0f});
		location.z += Parameter<float> (grave, "ZIncrement", {0.0f});
		grave.set_location (location);
	}
	else
		host ().destroy ();
	return Message::CONTINUE;
}

Message::Result
NGCPiece::bury_corpse (Message&)
{
	if (is_corpse)
		start_timer ("StartBurial", Duration::CORPSING, false);
	return Message::CONTINUE;
}



// Promotion

Message::Result
NGCPiece::start_promotion (Message& message)
{
	promotion = message.get_data (Message::DATA1, Object::NONE);
	if (promotion == Object::NONE) return Message::HALT;
	if (target_square != Object::NONE || victim != Object::NONE)
		// The promotion will begin when the piece has arrived/captured.
		return Message::HALT;

	Object square = Link::get_one ("~Population", host ()).get_dest ();
	if (square != Object::NONE)
		GenericMessage::with_data ("Reposition", square, true).send
			(host (), host ());

	std::ostringstream effect_name;
	effect_name << "ChessPromotion" << set;
	Object effect = Object::create (Object (effect_name.str ()));
	if (effect != Object::NONE)
	{
		// Don't ParticleAttach, so that the FX can outlive us.
		ScriptParamsLink::create (host (), effect, "PromoEffect");
		effect.set_location (host ().get_location ());
	}

	start_timer ("RevealPromotion", Duration::PROMOTION / 2, false);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::reveal_promotion (TimerMessage&)
{
	host_as<Physical> ().collides_with_ai = false;

	GenericMessage ("FadeOut").send (host (), host ()); //FIXME

	GenericMessage::with_data ("Reposition", Object::SELF, true).send
		(host (), promotion);

	GenericMessage ("FadeIn").send (host (), promotion); //FIXME

	start_timer ("FinishPromotion", Duration::PROMOTION / 2, false);
	return Message::CONTINUE;
}

Message::Result
NGCPiece::finish_promotion (TimerMessage&)
{
	promotion = Object::NONE;
	tell_game ("FinishMove");

	Object effect = ScriptParamsLink::get_one_by_data (host (),
		"PromoEffect").get_dest ();
	if (effect != Object::NONE)
		GenericMessage ("TurnOff").send (host (), effect);

	host ().destroy ();
	return Message::HALT;
}



// Heralds

Message::Result
NGCPiece::herald_concept (Message& message)
{
	String concept = message.get_data (Message::DATA1, String ());

	// Play the trumpeting motion (bipeds only).
	host_as<AI> ().play_motion ("humgulp");

	// Play the announcement sound (fanfare and/or speech).
	std::ostringstream tags;
	tags << "ChessSet set" << set << ", ChessConcept " << concept;
	SoundSchema::play_by_tags (tags.str (),
		SoundSchema::Tagged::AT_OBJECT_LOCATION,
		get_heraldry_source (), Object::NONE, host ());

	return Message::CONTINUE;
}

Object
NGCPiece::get_heraldry_source () //TODO Replace this with proper attenuation factors on schemas.
{
	Object source = ScriptParamsLink::get_one_by_data (host (),
		"HeraldrySource").get_dest ();

	if (!source.exists ())
	{
		source = Object::create (Object ("ambientSound"));
		if (source == Object::NONE) return source;
		ScriptParamsLink::create (host (), source, "HeraldrySource");
	}

	Vector herald_loc = host ().get_location (),
		player_loc = Player ().get_location ();

	// If the player is out of earshot, dislocate herald sounds toward them.
	static const float EARSHOT = 25.0f;
	Vector source_loc = herald_loc;
	if (player_loc.x < herald_loc.x - EARSHOT)
		source_loc.x = player_loc.x + EARSHOT;
	else if (player_loc.x > herald_loc.x + EARSHOT)
		source_loc.x = player_loc.x - EARSHOT;
	if (player_loc.y < herald_loc.y - EARSHOT)
		source_loc.y = player_loc.y + EARSHOT;
	else if (player_loc.y > herald_loc.y + EARSHOT)
		source_loc.y = player_loc.y - EARSHOT;

	source.set_location (source_loc);
	return source;
}



// "Player"s

Message::Result
NGCPiece::start_thinking (Message&)
{
	host_as<AI> ().play_motion ("bh112003"); // bipeds only
	return Message::CONTINUE;
}

Message::Result
NGCPiece::finish_thinking (Message&)
{
	host_as<AI> ().play_motion ("bh112550"); // bipeds only
	return Message::CONTINUE;
}
