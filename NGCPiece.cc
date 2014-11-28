/******************************************************************************
 *  NGCPiece.cc
 *
 *  Copyright (C) 2013-2014 Kevin Daughtridge <kevin@kdau.com>
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



NGCPiece::NGCPiece (const String& _name, const Object& _host)
	: Script (_name, _host),
	  THIEF_PARAMETER_FULL (team, "chess_team", Team::NEUTRAL),
	  THIEF_PARAMETER_FULL (set, "chess_set", 0),
	  THIEF_PERSISTENT_FULL (game, Object::NONE),
	  THIEF_PARAMETER (max_opacity, 1.0f),
	  THIEF_PERSISTENT_FULL (fade_state, Fade::NONE),
	  fade_trans (*this, &NGCPiece::fade_step, "Fade", 50ul, 1000ul,
		Curve::LINEAR, "fade_time", "fade_curve"),
	  THIEF_PERSISTENT (reposition_start),
	  THIEF_PERSISTENT (reposition_end),
	  reposition_trans (*this, &NGCPiece::reposition_step, "Reposition",
	  	10ul, 500ul, Curve::LOG_10, "slide_time", "slide_curve"),
	  THIEF_PERSISTENT_FULL (target_square, Object::NONE),
	  THIEF_PERSISTENT_FULL (victim, Object::NONE),
	  THIEF_PERSISTENT_FULL (attacker, Object::NONE),
	  THIEF_PARAMETER_FULL (is_corpse, "chess_corpse", false),
	  THIEF_PERSISTENT_FULL (promotion, Object::NONE)
{
	listen_message ("Select", &NGCPiece::select);
	listen_message ("Deselect", &NGCPiece::deselect);
	listen_message ("Reveal", &NGCPiece::reveal);

	listen_message ("Reposition", &NGCPiece::reposition);
	listen_timer ("Reposition", &NGCPiece::reposition);
	listen_message ("GoToSquare", &NGCPiece::go_to_square);
	listen_message ("ObjActResult", &NGCPiece::arrive_at_square);
	listen_timer ("BowToKing", &NGCPiece::bow_to_king);
	listen_message ("Celebrate", &NGCPiece::celebrate);

	listen_message ("AttackPiece", &NGCPiece::start_attack);
	listen_timer ("MaintainAttack", &NGCPiece::maintain_attack);
	listen_message ("BecomeVictim", &NGCPiece::become_victim);
	listen_message ("BeAttacked", &NGCPiece::be_attacked);

	listen_message ("StartWar", &NGCPiece::start_war);
	listen_message ("FinishWar", &NGCPiece::finish_war);

	listen_timer ("ForceDeath", &NGCPiece::force_death);
	listen_message ("AIModeChange", &NGCPiece::check_ai_mode);
	listen_message ("Slain", &NGCPiece::die);

	listen_timer ("StartBurial", &NGCPiece::start_burial);
	listen_timer ("FinishBurial", &NGCPiece::finish_burial);
	listen_message ("Create", &NGCPiece::bury_corpse);

	listen_message ("BePromoted", &NGCPiece::start_promotion);
	listen_timer ("RevealPromotion", &NGCPiece::reveal_promotion);
	listen_timer ("FinishPromotion", &NGCPiece::finish_promotion);

	listen_message ("HeraldConcept", &NGCPiece::herald_concept);
	listen_message ("PropertyChange", &NGCPiece::subtitle_speech);
	listen_timer ("FinishSubtitle", &NGCPiece::finish_subtitle);

	listen_message ("StartThinking", &NGCPiece::start_thinking);
	listen_message ("FinishThinking", &NGCPiece::finish_thinking);
}

void
NGCPiece::initialize ()
{
	Script::initialize ();
	game = Object ("TheGame");
	ObjectProperty::subscribe ("Speech", host ());
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
	return Message::HALT;
}

Message::Result
NGCPiece::deselect (Message&)
{
	host ().remove_metaprop (Object ("M-SelectedPiece"));
	host_as<DynamicLight> ().brightness.remove ();
	return Message::HALT;
}

Message::Result
NGCPiece::reveal (Message&)
{
	fade_state = Fade::IN;
	fade_trans.start ();
	return Message::HALT;
}

bool
NGCPiece::fade_step ()
{
	float new_opacity;
	switch (fade_state)
	{
	case Fade::IN:
		new_opacity = fade_trans.interpolate (0.0f, float (max_opacity));
		break;
	case Fade::OUT:
		new_opacity = fade_trans.interpolate (float (max_opacity), 0.0f);
		break;
	default:
		return false;
	}

	host_as<Rendered> ().opacity = new_opacity;

	for (auto& link : Link::get_all ("Contains", host ()))
		Rendered (link.get_dest ()).opacity = new_opacity;
	for (auto& link : Link::get_all ("~DetailAttachement", host ()))
		Rendered (link.get_dest ()).opacity = new_opacity;
	for (auto& link : Link::get_all ("~ParticleAttachement", host ()))
		Rendered (link.get_dest ()).opacity = new_opacity;

	if (fade_trans.is_finished ())
		fade_state = Fade::NONE;
	return true;
}



// Movement

Message::Result
NGCPiece::reposition (Message& message)
{
	auto square = message.get_data (Message::DATA1, Object ());
	auto direct = message.get_data (Message::DATA2, false);

	if (!square.exists ())
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

	host_as<AI> ().send_signal ("FaceEnemy");

	return Message::HALT;
}

bool
NGCPiece::reposition_step ()
{
	host ().set_location (reposition_trans.interpolate
		(reposition_start, reposition_end));
	if (reposition_trans.is_finished ())
		host_as<AI> ().send_signal ("FaceEnemy"); // just in case
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
	return Message::HALT;
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

	return Message::HALT;
}

bool
NGCPiece::is_biped () const
{
	switch (host_as<AI> ().creature_type)
	{
	case AI::CreatureType::HUMANOID:
	case AI::CreatureType::BUGBEAST:
	case AI::CreatureType::CRAYMAN:
	case AI::CreatureType::CONSTANTINE: // ???
	case AI::CreatureType::APPARITION: // ???
	case AI::CreatureType::ZOMBIE:
	case AI::CreatureType::CUTTY:
	case AI::CreatureType::AVATAR:
		return true;
	default:
		return false;
	}
}

Message::Result
NGCPiece::bow_to_king (TimerMessage&)
{
	if (is_biped ())
	{
		host_as<AI> ().play_motion ("humsalute3");
		start_timer ("Reposition", 3000ul, false);
	}
	else
		GenericMessage ("Reposition").send (host (), host ());
	return Message::HALT;
}

Message::Result
NGCPiece::celebrate (Message&)
{
	if (is_biped ())
		host_as<AI> ().play_motion (Thief::Engine::random_int (0, 1)
			? "humairpt2" : "humpshbt1");
	return Message::HALT;
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

	GenericMessage ("BeAttacked").send (host (), victim);
	start_timer ("MaintainAttack", 1ul, false);

	// If needed, the victim's death timer will lead to a finish_attack call.
	return Message::HALT;
}

Message::Result
NGCPiece::maintain_attack (TimerMessage& message)
{
	if (victim == Object::NONE)
		return Message::HALT;

	if (!victim->exists () || victim->mode == AI::Mode::DEAD ||
	    victim->death_stage == 12 || victim->hit_points <= 0)
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
	return Message::HALT;
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
	// Don't set the attacker variable until it's official (be_attacked).
	RangedCombatant _attacker = message.get_from (),
		self = host_as<RangedCombatant> ();

	self.remove_metaprop (Object ("M-ChessAlive"));
	self.add_metaprop (Object ("M-ChessVictim"));

	// The combination of a ranged attacker and melee victim results in
	// awkward attack sequences. In this case, don't let the victim fight
	// back until they have been hit.
	if (_attacker.is_ranged_combatant () && !self.is_ranged_combatant ())
		self.non_hostile = Combatant::NonHostile::UNTIL_DAMAGED;

	create_awareness (_attacker, message.get_time ());
	host_as<AI> ().face_object (_attacker);

	return Message::HALT;
}

Message::Result
NGCPiece::be_attacked (Message& message)
{
	attacker = message.get_from ();
	create_awareness (attacker, message.get_time ());
	start_timer ("ForceDeath", Duration::ATTACK, false);
	return Message::HALT;
}

AIAwarenessLink
NGCPiece::create_awareness (const Object& target, Time time)
{
	AIAwarenessLink aware = Link::get_one ("AIAwareness", host (), target);
	if (aware.exists ())
	{
		aware.seen = true;
		aware.can_raycast = true;
		aware.have_los = true;
		aware.firsthand = true;
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

Message::Result
NGCPiece::start_war (Message& message)
{
	auto self = host_as<AI> ();
	self.remove_metaprop (Object ("M-ChessAlive"));
	self.add_metaprop (message.get_data<Object> (Message::DATA1));
	self.add_metaprop (Object ("M-ChessWarrior"));
	attacker = self; // to allow die method to proceed
	return Message::HALT;
}

Message::Result
NGCPiece::finish_war (Message&)
{
	auto self = host_as<AI> ();
	self.remove_metaprop (Object ("M-ChessWarrior"));
	self.remove_metaprop (Object ("M-ChessAttacker"));
	self.remove_metaprop (Object ("M-ChessVictim"));
	self.add_metaprop (Object ("M-ChessAlive"));
	self.clear_alertness ();
	self.halt_speech ();
	self.send_signal ("FaceEnemy");
	return Message::HALT;
}



// Death and burial

Message::Result
NGCPiece::force_death (TimerMessage&)
{
	host_as<Damageable> ().slay (attacker);
	return Message::HALT;
}

Message::Result
NGCPiece::check_ai_mode (AIModeMessage& message)
{
        if (message.new_mode == AI::Mode::DEAD)
		die (message);
	return Message::HALT;
}

Message::Result
NGCPiece::die (Message&)
{
	if (attacker == Object::NONE) return Message::HALT;
	attacker = Object::NONE;

	// Stop any current subtitle, just in case.
	subtitle.reset ();

	// Ensure that any corpses will bury themselves appropriately.
	QuestVar ("chess_corpse_team") = int (Team (team));

	// Set timer to do it on ourself, if we are not replaced.
	start_timer ("StartBurial", Duration::DEATH, false);

	return Message::HALT;
}

Message::Result
NGCPiece::start_burial (TimerMessage&)
{
	if (team == Team::NEUTRAL)
		team = Team (int (QuestVar ("chess_corpse_team")));

	// Create a smoke puff at the site of death.
	Object puff_archetype = Object ("ChessBurialPuff"),
		puff = Object::create (puff_archetype);
	puff.set_location (host ().get_location ());

	// Create a smoke puff at the gravesite, if any.
	Object grave = Object ((team == Team::GOOD)
		? "ChessGraveGood" : "ChessGraveEvil");
	if (grave != Object::NONE)
	{
		ScriptParamsLink::create (host (), grave, "Grave");
		puff = Object::create (puff_archetype);
		puff.set_location (grave.get_location ());
	}

	fade_state = Fade::OUT;
	fade_trans.start ();
	start_timer ("FinishBurial", Duration::BURIAL, false);
	return Message::HALT;
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

		fade_state = Fade::IN;
		fade_trans.start ();

		// Displace the grave marker (for rows instead of piles).
		location += Parameter<Vector> (grave, "grave_offset");
		grave.set_location (location);
	}
	else
		host ().destroy ();
	return Message::HALT;
}

Message::Result
NGCPiece::bury_corpse (Message&)
{
	if (is_corpse)
		start_timer ("StartBurial", Duration::CORPSING, false);
	return Message::HALT;
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

	Object square = ScriptParamsLink::get_one_by_data (host (),
		"ExPopulation", true).get_dest ();
	if (square != Object::NONE)
		GenericMessage::with_data ("Reposition", square, true).send
			(host (), host ());

	Object effect_archetype ("ChessPromotion" + std::to_string (set));
	Object effect = Object::create (effect_archetype);
	if (effect != Object::NONE)
	{
		// Don't ParticleAttach, so that the FX can outlive us.
		ScriptParamsLink::create (host (), effect, "PromoEffect");
		effect.set_location (host ().get_location ());
	}

	start_timer ("RevealPromotion", Duration::PROMOTION / 2, false);
	return Message::HALT;
}

Message::Result
NGCPiece::reveal_promotion (TimerMessage&)
{
	host_as<Physical> ().collides_with_ai = false;

	fade_state = Fade::OUT;
	fade_trans.start ();

	GenericMessage::with_data ("Reposition", Object::SELF, true).send
		(host (), promotion);

	GenericMessage ("Reveal").send (host (), promotion);

	start_timer ("FinishPromotion", Duration::PROMOTION / 2, false);
	return Message::HALT;
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



// Heralds (Only bipeds are supported as heralds.)

Message::Result
NGCPiece::herald_concept (Message& message)
{
	String concept = message.get_data (Message::DATA1, String ());

	// Play the trumpeting motion.
	host_as<AI> ().play_motion ("hrldhorn");

	// Play the announcement sound (fanfare and/or speech).
	boost::format tags ("ChessSet set%||, ChessConcept %||");
	tags % set % concept;
	SoundSchema::play_by_tags (tags.str (), SoundSchema::Tagged::ON_OBJECT,
		host ());

	return Message::HALT;
}

Message::Result
NGCPiece::subtitle_speech (PropertyMessage& message)
{
	AI ai = host_as<AI> ();

	// Confirm that the relevant property has changed.
	if (message.object != ai || message.property != Property ("Speech"))
		return Message::CONTINUE;

	// Confirm that the speech schema is valid.
	SoundSchema schema = ai.last_speech_schema;
	if (!schema.exists ()) return Message::HALT;

	// If this is the end of a speech schema, finish the subtitle instead.
	if (!ai.is_speaking)
	{
		subtitle.reset ();
		return Message::HALT;
	}

	// Get subtitle text.
	String schema_name = schema.get_name ();
	String text = Chess::translate (schema_name);
	if (text.empty ()) return Message::HALT;

	// Get or calculate the schema duration.
	Time duration = ScriptHost (schema).script_timing.exists ()
		? ScriptHost (schema).script_timing
		: Interface::calc_text_duration (text, 700ul);

	// Start the subtitle and schedule its end.
	subtitle.reset (new HUDMessage ());
	subtitle->topic = ai;
	subtitle->identifier = schema_name;
	subtitle->set_text (text);
	subtitle->set_color (ChessSet (set).get_color ());
	start_timer ("FinishSubtitle", duration, false, schema_name);

	return Message::HALT;
}

Message::Result
NGCPiece::finish_subtitle (TimerMessage& message)
{
	if (subtitle && subtitle->identifier ==
	    message.get_data (Message::DATA1, String ()))
		subtitle.reset ();
	return Message::HALT;
}



// "Player"s (Only bipeds are supported as opponent "player"s.)

Message::Result
NGCPiece::start_thinking (Message&)
{
	host_as<AI> ().play_motion ("bh112003");
	return Message::HALT;
}

Message::Result
NGCPiece::finish_thinking (Message&)
{
	host_as<AI> ().play_motion ("bh112550");
	return Message::HALT;
}

