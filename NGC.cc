/******************************************************************************
 *  NGC.cc
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

#include "NGC.hh"



// Callback for Chess module

namespace Chess {

String
translate (const String& msgid, Side side)
{
	return Interface::get_text ("strings", "chess", side.is_valid ()
		? (msgid + std::to_string (ChessSet (side).number))
		: msgid);

}

} // namespace Chess



// Team

Team
get_chess_team (Side side)
{
	if (side.value == QuestVar ("chess_side_good"))
		return Team::GOOD;
	else if (side.value == QuestVar ("chess_side_evil"))
		return Team::BAD_1;
	else
		return Team::NEUTRAL;
}

Side
get_chess_side (Team team)
{
	switch (team)
	{
	case Team::GOOD:
		return Side::Value (int (QuestVar ("chess_side_good")));
	case Team::BAD_1:
		return Side::Value (int (QuestVar ("chess_side_evil")));
	default:
		return Side::NONE;
	}
}

int
get_facing_direction (Side side)
{
	return side.get_facing_direction () *
		((QuestVar ("chess_side_good") == 0) ? 1 : -1);
}



// ChessSet

ChessSet::ChessSet (int _number)
	: number (_number)
{}

ChessSet::ChessSet (Team team)
	: number (QuestVar (team == Team::GOOD
		? "chess_set_good" : "chess_set_evil"))
{}

Team
ChessSet::get_team () const
{
	if (QuestVar ("chess_set_good") == number)
		return Team::GOOD;
	else if (QuestVar ("chess_set_evil") == number)
		return Team::BAD_1;
	else
		return Team::NEUTRAL;
}

ChessSet::ChessSet (Side side)
	: number (QuestVar (get_chess_team (side) == Team::GOOD
		? "chess_set_good" : "chess_set_evil"))
{}

Side
ChessSet::get_side () const
{
	return get_chess_side (get_team ());
}

Object
ChessSet::get_metaprop () const
{
	return Object ("M-ChessSet" + std::to_string (number));
}

Color
ChessSet::get_color () const
{
	return Parameter<Color> (get_metaprop (), "chess_color",
		Color (0xffffff));
}



// HUDMessage

const int
HUDMessage::BORDER = 2;

const int
HUDMessage::PADDING = 12;

const CanvasPoint
HUDMessage::DEFAULT_OFFSET = { 0, 2 * PADDING };

HUDMessage::HUDMessage (ZIndex priority)
	: HUDElement (), enabled (true), position (Position::TOPIC),
	  offset (DEFAULT_OFFSET)
{
	initialize (priority);
	set_color (Color (0xffffff));
}

HUDMessage::~HUDMessage ()
{}

void
HUDMessage::set_text (const String& _text)
{
	text = _text;
	schedule_redraw ();
}

void
HUDMessage::set_color (const Color& color, float luminance_mult)
{
	LabColor _color_fg (color);
	_color_fg.L *= luminance_mult;
	color_fg = _color_fg;

	// The background is black.
	color_bg = Color (0x000000);

	// The border is slightly darker than the foreground.
	LabColor _color_border (_color_fg);
	_color_border.L /= 2.0;
	color_border = _color_border;
}

bool
HUDMessage::prepare ()
{
	if (!enabled) return false;
	CanvasRect elem_area;

	// Get the canvas and text size and calculate the element size.
	CanvasSize canvas = Engine::get_canvas_size (),
		text_size = get_text_size (text);
	elem_area.w = BORDER + PADDING + text_size.w + PADDING + BORDER;
	elem_area.h = BORDER + PADDING + text_size.h + PADDING + BORDER;

	// Get the topic's position in canvas coordinates.
	CanvasPoint topic_pos = CanvasPoint::OFFSCREEN;
	Player player;
	if (topic != Object::NONE && topic != player &&
	    !player.is_in_inventory (topic))
	{
		topic_pos = centroid_to_canvas (topic);
		if (!topic_pos.valid ()) return false;
	}

	// Calculate the element position.
	switch (position)
	{
	case Position::TOPIC:
		if (topic_pos.valid ())
		{
			elem_area.x = topic_pos.x - elem_area.w / 2 + offset.x;
			elem_area.y = topic_pos.y + offset.y;
			break;
		}
	case Position::CENTER:
		elem_area.x = (canvas.w - elem_area.w) / 2 + offset.x;
		elem_area.y = canvas.h / 2 + offset.y;
		break;
	case Position::NW:
		elem_area.x = offset.x;
		elem_area.y = offset.y;
		break;
	case Position::NORTH:
		elem_area.x = (canvas.w - elem_area.w) / 2 + offset.x;
		elem_area.y = offset.y;
		break;
	case Position::NE:
		elem_area.x = canvas.w - elem_area.w - offset.x;
		elem_area.y = offset.y;
		break;
	}
	elem_area.x = std::max (0, std::min (canvas.w - elem_area.w, elem_area.x));
	elem_area.y = std::max (0, std::min (canvas.h - elem_area.h, elem_area.y));

	set_position (elem_area);
	set_size (elem_area);
	return true;
}

void
HUDMessage::redraw ()
{
	// draw background
	set_drawing_color (color_bg);
	fill_area ();

	// draw border
	set_drawing_color (color_border);
	CanvasSize elem_size = get_size ();
	for (int i = 0; i < BORDER; ++i)
		draw_box ({ i, i, elem_size.w - 2 * i, elem_size.h - 2 * i });

	// draw text
	set_drawing_color (color_fg);
	draw_text (text, { BORDER + PADDING, BORDER + PADDING });
}



// NGCTitled

NGCTitled::NGCTitled (const String& _name, const Object& _host,
		const CIString& _title_msgid)
	: Script (_name, _host),
	  THIEF_PARAMETER_FULL (title_msgid, _title_msgid, "")
{
	listen_message ("WorldSelect", &NGCTitled::on_world_select);
	listen_message ("WorldDeSelect", &NGCTitled::on_world_deselect);
	listen_message ("FrobWorldBegin", &NGCTitled::on_frob_world_begin);
}

void
NGCTitled::initialize ()
{
	Script::initialize ();
	title.reset (new HUDMessage ());
	title->enabled = false;
	title->topic = host ();
	if (title_msgid.exists () && title->get_text ().empty ())
		title->set_text (Chess::translate (title_msgid));
}

Message::Result
NGCTitled::on_world_select (Message&)
{
	if (title) title->enabled = true;
	return Message::CONTINUE;
}

Message::Result
NGCTitled::on_world_deselect (Message&)
{
	if (title) title->enabled = false;
	return Message::CONTINUE;
}

Message::Result
NGCTitled::on_frob_world_begin (FrobMessage&)
{
	if (title) title->enabled = false;
	return Message::CONTINUE;
}



// NGCIntro

NGCIntro::NGCIntro (const String& _name, const Object& _host)
	: Script (_name, _host)
{
	listen_message ("PostSim", &NGCIntro::prepare_mission);
	listen_message ("ChooseScenario", &NGCIntro::choose_scenario);
	listen_message ("StartBriefing", &NGCIntro::start_briefing);
	listen_message ("ConversationEnd", &NGCIntro::finish_briefing);
}

void
NGCIntro::initialize ()
{
	Script::initialize ();
	host_as<Conversation> ().subscribe ();
}

Message::Result
NGCIntro::prepare_mission (Message&)
{
	// Destroy the doors blocking off the scenario alcoves.
	for (auto& door : ScriptParamsLink::get_all_by_data
				(host (), "ScenarioDoor"))
		door.get_dest ().destroy ();

	// Switch the scroll (this object) from "script-problem" to "welcome".
	host_as<Readable> ().book_name = "welcome";

	// Create a goto target for the heralds and attach to the player.
	Object target = Object::create (Object ("Marker"));
	target.set_name ("BeforePlayer");
	DetailAttachementLink::create (target, Player (),
		DetailAttachementLink::Type::OBJECT, 0, AI::Joint::NONE,
		{ 4.0f, 0.0f, 0.0f });

	return Message::HALT;
}

Message::Result
NGCIntro::choose_scenario (Message& message)
{
	Object scenario = message.get_from ();

	// Disable the other scenario gems.
	for (auto& gem : ScriptParamsLink::get_all_by_data (host (), "Scenario"))
		if (gem.get_dest () != scenario)
			GenericMessage ("Disable").send (host (), gem.get_dest ());

	// Make this scenario's herald the actor in the briefing conversation.
	Object herald = ScriptParamsLink::get_one_by_data
		(scenario, "Herald").get_dest ();
	if (herald != Object::NONE)
		host_as<Conversation> ().set_actor (1, herald);

	return Message::HALT;
}

Message::Result
NGCIntro::start_briefing (Message&)
{
	host_as<Conversation> ().start_conversation ();
	return Message::HALT;
}

Message::Result
NGCIntro::finish_briefing (ConversationMessage& message)
{
	if (message.conversation == host ())
		Objective (0).state = Objective::State::COMPLETE;
	return Message::CONTINUE;
}



// NGCScenario

NGCScenario::NGCScenario (const String& _name, const Object& _host)
	: NGCTitled (_name, _host),
	  disable_trans (*this, &NGCScenario::disable_step, "Disable", 50ul,
		1000ul, Curve::LINEAR, "fade_time", "fade_curve"),
	  THIEF_PARAMETER (mission, 0),
	  THIEF_PARAMETER (chess_set, 0),
	  THIEF_PERSISTENT_FULL (state, State::NONE)
{
	listen_message ("FrobWorldEnd", &NGCScenario::select);
	listen_message ("Disable", &NGCScenario::disable);
	listen_message ("TurnOn", &NGCScenario::enter_environment);
}

void
NGCScenario::initialize ()
{
	NGCTitled::initialize ();
	if (title)
	{
		title->set_text (Interface::get_text ("strings", "titles",
			"title_" + std::to_string (mission)));
		title->set_color (ChessSet (chess_set).get_color ());
	}
}

Message::Result
NGCScenario::select (FrobMessage& message)
{
	// Actually choose the scenario.
	state = State::SELECTED;
	Mission::set_next (mission);

	// Notify NGCIntro.
	Object intro = ScriptParamsLink::get_one_by_data
		(host (), "Scenario", true).get_dest ();
	GenericMessage ("ChooseScenario").send (host (), intro);

	// Play a frob sound and disable the gem.
	SoundSchema ("pickup_gem").play_ambient ();
	disable (message);

	// Lower the chess table into the floor.
	Object table = ScriptParamsLink::get_one_by_data
		(host (), "Table").get_dest ();
	GenericMessage ("Open").send (host (), table);

	// Raise the false wall into the ceiling.
	Object wall = ScriptParamsLink::get_one_by_data
		(host (), "Wall").get_dest ();
	GenericMessage ("Open").send (host (), wall);

	return Message::HALT;
}

Message::Result
NGCScenario::disable (Message&)
{
	// Disable the gem itself.
	host ().add_metaprop (Object ("FrobInert"));
	host_as<AmbientHacked> ().active = false;
	host_as<AnimLight> ().light_mode = AnimLight::Mode::SMOOTH_DIM;

	// Turn off the fill lighting (on the wall object).
	AnimLight wall = ScriptParamsLink::get_one_by_data
		(host (), "Wall").get_dest ();
	wall.light_mode = AnimLight::Mode::SMOOTH_DIM;

	// Fade out the gem and darken the preview image.
	disable_trans.start ();

	return Message::HALT;
}

bool
NGCScenario::disable_step ()
{
	float level = disable_trans.interpolate (1.0f, 0.0f);

	host_as<Rendered> ().opacity = level;

	Rendered wall = ScriptParamsLink::get_one_by_data
		(host (), "Wall").get_dest ();
	wall.self_illumination = level;

	return true;
}

Message::Result
NGCScenario::enter_environment (Message& message)
{
	if (state == State::NONE)
		return Message::ERROR; // ???

	if (state == State::BRIEFING)
		return Message::HALT; // already done

	// Prevent a bounding box trigger from re-triggering, since TrigOBB
	// doesn't support the Once flag.
	Physical (message.get_from ()).remove_physics ();

	if (state == State::SELECTED)
	{
		// Close the false wall again, quickly.
		TranslatingDoor wall = ScriptParamsLink::get_one_by_data
			(host (), "Wall").get_dest ();
		wall.speed = wall.speed * 3.0f;
		wall.close_door ();

		state = State::ENTERED;
		if (mission == 22) // Wait for a separate briefing trigger.
			return Message::HALT;
	}

	if (state == State::ENTERED)
	{
		// 22's briefing area is not designed for its herald to move.
		if (mission == 22)
			Object ("BeforePlayer").destroy ();

		// Start the briefing now.
		state = State::BRIEFING;
		Object intro = ScriptParamsLink::get_one_by_data
			(host (), "Scenario", true).get_dest ();
		GenericMessage ("StartBriefing").send (host (), intro);
	}

	return Message::HALT;
}



// NGCClock

NGCClock::NGCClock (const String& _name, const Object& _host)
	: NGCTitled (_name, _host),
	  THIEF_PARAMETER_FULL (time_control, "clock_time", 0ul),
	  THIEF_PERSISTENT_FULL (running, false),
	  THIEF_PARAMETER_FULL (joint, "clock_joint", 0),
	  THIEF_PARAMETER_FULL (joint_low, "clock_low", 0.0f),
	  THIEF_PARAMETER_FULL (joint_high, "clock_high", 0.0f)
{
	listen_message ("TickTock", &NGCClock::tick_tock);
	listen_message ("StopTheClock", &NGCClock::stop_the_clock);
}

void
NGCClock::initialize ()
{
	NGCTitled::initialize ();
	if (!running.exists ())
		running = (time_control != 0ul);
	update_display ();
}

Message::Result
NGCClock::tick_tock (Message& message)
{
	if (!running)
		return Message::HALT;

	// Check the time.
	Time remaining = get_time_remaining ();
	float pct_remaining = float (remaining) / float (Time (time_control));

	// Update the clock joint.
	if (joint >= 1 && joint <= 6 && joint_high > joint_low)
		host_as<Rendered> ().joint_position [joint - 1]
			= joint_low + pct_remaining * (joint_high - joint_low);

	// Notify the game if time has run out.
	if (remaining == 0ul)
	{
		stop_the_clock (message);
		SoundSchema ("dinner_bell").play (host ());

		Object game = ScriptParamsLink::get_one_by_data
			(host (), "Clock", true).get_dest ();
		GenericMessage ("TimeControl").send (host (), game);
	}

	// Update the time display.
	else
		update_display ();

	return Message::HALT;
}

Message::Result
NGCClock::stop_the_clock (Message&)
{
	running = false;
	update_display ();
	host ().add_metaprop (Object ("FrobInertFocusable"));
	host_as<AmbientHacked> ().active = false;
	SoundSchema ("button_rmz").play (host ());
	return Message::HALT;
}

Time
NGCClock::get_time_remaining () const
{
	return std::max (0l, long (Time (time_control)) - QuestVar ("stat_time"));
}

void
NGCClock::update_display ()
{
	if (!title) return;
	Time remaining = get_time_remaining ();
	bool last_minute = (remaining <= 60000ul);
	const char* msgid = last_minute ? "time_seconds" : "time_minutes";
	unsigned time = last_minute ? remaining.seconds () : remaining.minutes ();
	title->set_text (Chess::translate_format (msgid, time));
}



// NGCFlag

NGCFlag::NGCFlag (const String& _name, const Object& _host)
	: NGCTitled (_name, _host),
	  THIEF_PARAMETER (question, String ()),
	  THIEF_PARAMETER_FULL (message_name, "message", "TurnOn"),
	  THIEF_PERSISTENT_FULL (orig_loc, Vector ()),
	  THIEF_PERSISTENT_FULL (orig_rot, Vector ()),
	  THIEF_PERSISTENT_FULL (drop_loc, Vector ()),
	  THIEF_PERSISTENT_FULL (drop_rot, Vector ()),
	  boomerang (*this, &NGCFlag::boomerang_step, "Boomerang", 20ul, 320ul)
{
	listen_message ("FrobWorldEnd", &NGCFlag::ask_question);
	listen_message ("WorldDeSelect", &NGCFlag::intercept_deselect);
	listen_message ("FrobInvEnd", &NGCFlag::answered_yes);
	listen_message ("Contained", &NGCFlag::answered_no);
}

Message::Result
NGCFlag::ask_question (FrobMessage&)
{
	if (!title) return Message::ERROR;

	SoundSchema ("bow_begin").play_ambient ();

	orig_loc = host ().get_location ();
	orig_rot = host ().get_rotation ();

	Player player;
	player.add_to_inventory (host ());
	player.select_item (host ());

	title->enabled = true;
	title->set_text (Chess::translate (question));
	title->offset = { 0, 64 };

	return Message::HALT;
}

Message::Result
NGCFlag::intercept_deselect (Message&)
{
	if (!title) return Message::ERROR;
	if (Player ().is_in_inventory (host ()))
		title->enabled = true;
	return Message::HALT;
}

Message::Result
NGCFlag::answered_yes (FrobMessage&)
{
	if (!title) return Message::ERROR;
	if (title->enabled)
	{
		SoundSchema ("pickup_gem").play_ambient ();
		GenericMessage (message_name->data ()).broadcast
			(host (), "ControlDevice");
		end_question ();
	}
	return Message::HALT;
}

Message::Result
NGCFlag::answered_no (ContainmentMessage& message)
{
	if (!title) return Message::ERROR;
	if (title->enabled && message.event == ContainmentMessage::REMOVE)
	{
		SoundSchema ("bow_abort").play_ambient ();
		end_question ();
	}
	return Message::HALT;
}

void
NGCFlag::end_question ()
{
	if (title)
	{
		title->enabled = false;
		title->set_text (Chess::translate (title_msgid));
		title->offset = HUDMessage::DEFAULT_OFFSET;
	}

	Player player;
	player.remove_from_inventory (host ());
	drop_loc = player.object_to_world ({ 4.0f, 0.0f, 2.0f });
	drop_rot = player.get_rotation ();
	boomerang.start ();
}

bool
NGCFlag::boomerang_step ()
{
	auto self = host_as<Physical> ();
	if (self.is_physical ())
		self.physics_type = Physical::PhysicsType::NONE;
	self.set_position (boomerang.interpolate (drop_loc, orig_loc),
		boomerang.interpolate (drop_rot, orig_rot));
	return true;
}



// NGCSquare

NGCSquare::NGCSquare (const String& _name, const Object& _host)
	: Script (_name, _host),
	  THIEF_PERSISTENT_FULL (state, State::EMPTY),
	  THIEF_PERSISTENT_FULL (piece, Piece ()),
	  THIEF_PARAMETER (is_proxy, false),
	  decal_fade (*this, &NGCSquare::decal_fade_step, "DecalFade"),
	  THIEF_PARAMETER (decal_offset, Vector ()),
	  THIEF_PARAMETER (luminance_mult, 1.0f),
	  button_fade (*this, &NGCSquare::button_fade_step, "ButtonFade"),
	  THIEF_PARAMETER (button_offset, Vector ())
{
	listen_message ("UpdateState", &NGCSquare::update_state);
	listen_message ("Select", &NGCSquare::select);
	listen_message ("Deselect", &NGCSquare::deselect);
	listen_message ("TurnOn", &NGCSquare::on_turn_on);
}

Message::Result
NGCSquare::update_state (Message& message)
{
	state = message.get_data (Message::DATA1, State::EMPTY);

	if (message.has_data (Message::DATA2))
		piece = message.get_data (Message::DATA2, Piece ());
	// Otherwise, the piece hasn't changed.

	// Get link data for singleton states.
	CIString singleton;
	switch (state)
	{
	case State::PROXY_WAS_FROM:
		singleton = "ProxyFrom";
		singleton += piece->side.get_code ();
		break;
	case State::PROXY_WAS_TO:
		singleton = "ProxyTo";
		singleton += piece->side.get_code ();
		break;
	default:
		break;
	}

	// If in a singleton state, empty the previous square in the state.
	if (!singleton.empty ())
	{
		Object game ("TheGame");
		auto old_singleton = ScriptParamsLink::get_one_by_data
			(game, singleton);
		if (old_singleton.exists ())
		{
			GenericMessage::with_data ("UpdateState", State::EMPTY,
				Piece ()).send (game, old_singleton.get_dest ());
			old_singleton.destroy ();
		}
		ScriptParamsLink::create (game, host (), singleton);
	}

	// Update the decal and button.
	update_decal ();
	update_button ();

	return Message::HALT;
}

Bitmapped
NGCSquare::get_decal () const
{
	return ScriptParamsLink::get_one_by_data (host (), "Decal").get_dest ();
}

void
NGCSquare::update_decal ()
{
	Bitmapped decal = get_decal ();

	Piece display_piece = piece;
	switch (state)
	{
	case State::FRIENDLY_INERT:
	case State::CAN_MOVE_FROM:
	case State::PROXY_WAS_FROM:
		break;
	case State::CAN_MOVE_TO:
	case State::PROXY_WAS_TO:
		display_piece.type = Piece::Type::NONE;
		if (!is_proxy)
			display_piece.side = piece->side.get_opponent ();
		break;
	default:
		if (decal != Object::NONE)
		{
			Time fade = Parameter<Time>
				(decal, "fade_time", { 500ul });
			decal.schedule_destruction (fade);
			decal_fade.length = fade;
			decal_fade.start ();
		}
		return;
	}

	if (decal != Object::NONE)
		decal.destroy ();

	decal = Object::start_create
		(Object (is_proxy ? "ChessProxyDecal" : "ChessDecal"));
	if (decal == Object::NONE)
	{
		log (Log::ERROR, "Could not create a decal.");
		return;
	}

	ScriptParamsLink::create (host (), decal, "Decal");

	decal.model = String ("decal-") +
		(display_piece.is_valid () ? display_piece.get_code () : 'z');

	if (state == State::FRIENDLY_INERT)
		decal.bitmap_color = Color (0x606060);
	else
	{
		LabColor decal_color = ChessSet (display_piece.side).get_color ();
		decal_color.L *= std::max (0.8f, std::min (1.2f,
			float (luminance_mult)));
		decal.bitmap_color = decal_color;
	}

	Vector location = host ().get_location () + decal_offset,
		rotation (0.0f, 0.0f, 180.0f + 90.0f * (is_proxy ? 1 : -1) *
			get_facing_direction (display_piece.side));
	decal.set_position (location, rotation);

	decal.finish_create ();

	Time fade = Parameter<Time> (decal, "fade_time", { 500ul });
	decal_fade.length = fade;
	decal_fade.start ();
}

bool
NGCSquare::decal_fade_step ()
{
	Rendered decal = get_decal ();
	if (decal == Object::NONE) return false;
	bool fade_in = state != State::EMPTY;
	decal.opacity = decal_fade.interpolate
		(fade_in ? 0.0f : 0.75f, fade_in ? 0.75f : 0.0f);
	return true;
}

Interactive
NGCSquare::get_button () const
{
	return ScriptParamsLink::get_one_by_data (host (), "Button").get_dest ();
}

void
NGCSquare::update_button ()
{
	Interactive button = get_button ();

	Side side = piece->side;
	Vector rotation;
	switch (state)
	{
	case State::CAN_MOVE_FROM:
		rotation.z = 180.0f + 90.0f * get_facing_direction (side);
		break;
	case State::CAN_MOVE_TO:
		side = side.get_opponent ();
		rotation.y = 90.0f;
		break;
	default:
		if (button != Object::NONE)
		{
			button.add_metaprop (Object ("FrobInert"));
			Time fade = Parameter<Time>
				(button, "fade_time", { 500ul });
			button.schedule_destruction (fade);
			button_fade.length = fade;
			button_fade.start ();
		}
		return;
	}

	if (button != Object::NONE)
		button.destroy ();

	Object archetype ("ChessButton" +
		std::to_string (ChessSet (side).number));
	button = Object::start_create (archetype);
	if (button == Object::NONE)
	{
		log (Log::ERROR, "Could not create a button.");
		return;
	}

	ScriptParamsLink::create (host (), button, "Button");
	Link::create ("ControlDevice", button, host ());

	Vector location = host ().get_location () + button_offset;
	button.set_position (location, rotation);

	button.finish_create ();

	Time fade = Parameter<Time> (button, "fade_time", { 500ul });
	button_fade.length = fade;
	button_fade.start ();
}

bool
NGCSquare::button_fade_step ()
{
	Interactive button = get_button ();
	if (button == Object::NONE) return false;
	bool fade_in = (state != State::EMPTY && state != State::FRIENDLY_INERT);
	button.opacity = button_fade.interpolate
		(fade_in ? 0.0f : 1.0f, fade_in ? 1.0f : 0.0f);
	return true;
}

Message::Result
NGCSquare::select (Message&)
{
	Interactive button = get_button ();
	if (button != Object::NONE)
	{
		button.add_metaprop (Object ("M-SelectedSquare"));
		GenericMessage ("TurnOn").send (host (), button);
	}

	Object piece_obj = Link::get_one ("Population", host ()).get_dest ();
	if (piece_obj != Object::NONE)
		GenericMessage ("Select").send (host (), piece_obj);

	for (auto& move : Link::get_all ("Route", host ()))
		GenericMessage::with_data ("UpdateState", State::CAN_MOVE_TO,
			Piece (piece)).send (host (), move.get_dest ());

	return Message::HALT;
}

Message::Result
NGCSquare::deselect (Message&)
{
	Interactive button = get_button ();
	if (button != Object::NONE)
	{
		GenericMessage ("TurnOff").send (host (), button);
		button.remove_metaprop (Object ("M-SelectedSquare"));
	}

	Object piece_obj = Link::get_one ("Population", host ()).get_dest ();
	if (piece_obj != Object::NONE)
		GenericMessage ("Deselect").send (host (), piece_obj);

	for (auto& move : Link::get_all ("Route", host ()))
		GenericMessage::with_data ("UpdateState", State::EMPTY,
			Piece ()).send (host (), move.get_dest ());

	return Message::HALT;
}

Message::Result
NGCSquare::on_turn_on (Message&)
{
	switch (state)
	{
	case State::CAN_MOVE_FROM:
		SoundSchema ("bow_begin").play_ambient ();
		GenericMessage ("SelectFrom").send (host (), Object ("TheGame"));
		return Message::HALT;
	case State::CAN_MOVE_TO:
		SoundSchema ("pickup_gem").play_ambient ();
		GenericMessage ("SelectTo").send (host (), Object ("TheGame"));
		return Message::HALT;
	default:
		return Message::ERROR;
	}
}



// NGCFireworks

NGCFireworks::NGCFireworks (const String& _name, const Object& _host)
	: TrapTrigger (_name, _host),
	  THIEF_PARAMETER_FULL (count, "firework_count", 12),
	  THIEF_PARAMETER_FULL (spread, "firework_spread", 300)
{
	listen_timer ("LaunchOne", &NGCFireworks::launch_one);
}

Message::Result
NGCFireworks::on_trap (bool on, Message&)
{
	if (!on) return Message::HALT;

	Object firework = ScriptParamsLink::get_one_by_data
		(host (), "Firework").get_dest ();
	if (firework == Object::NONE)
		firework = Object ("firearr");

	for (int i = 0; i < count; ++i)
		start_timer ("LaunchOne", Engine::random_int (0, count * spread),
			false, firework);

	return Message::HALT;
}

Message::Result
NGCFireworks::launch_one (TimerMessage& message)
{
	Projectile::launch (message.get_data<Object> (Message::DATA1), host (),
		0.0f, { Engine::random_float (-20.0f, 20.0f),
			Engine::random_float (-20.0f, 20.0f), 40.0f });
	return Message::HALT;
}

