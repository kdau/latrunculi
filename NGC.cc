/******************************************************************************
 *  NGC.cc
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

#include "NGC.hh"



// Callback for Chess module

namespace Chess {

String
translate (const String& _msgid, Side side)
{
	std::ostringstream msgid;
	msgid << _msgid;
	if (side.is_valid ())
		msgid << ChessSet (side).number;
	return Mission::get_text ("strings", "chess", msgid.str ());

}

} // namespace Chess




// ChessSet

ChessSet::ChessSet (int _number)
	: number (_number)
{}

ChessSet::ChessSet (Side side)
	: number (side.is_valid ()
		? QuestVar (String ("chess_set_") + side.get_code ()).get ()
		: 0)
{}

Side
ChessSet::get_side () const
{
	for (Side side : { Side::WHITE, Side::BLACK })
		if (QuestVar (String ("chess_set_") + side.get_code ()).get ()
		    == number)
			return side;
	return Side::NONE;
}

Object
ChessSet::get_metaprop () const
{
	std::ostringstream name;
	name << "M-ChessSet" << number;
	return Object (name.str ());
}

Color
ChessSet::get_color () const
{
	return Parameter<Color> (get_metaprop (), "chess_color",
		Color (0xffffff));
}



// HUDMessage

const HUD::ZIndex
HUDMessage::PRIORITY = 20;

const int
HUDMessage::BORDER = 2;

const int
HUDMessage::PADDING = 12;

HUDMessage::HUDMessage (const Object& _topic, const String& _text,
		const Color& color, bool _enabled)
	: HUDElement (), enabled (_enabled), topic (_topic), text (_text)
{
	initialize (PRIORITY);
	set_color (color);
}

HUDMessage::~HUDMessage ()
{}

void
HUDMessage::set_topic (const Object& _topic)
{
	topic = _topic;
	schedule_redraw ();
}

String
HUDMessage::get_text () const
{
	return text;
}

void
HUDMessage::set_text (const String& _text)
{
	text = _text;
	schedule_redraw ();
}

void
HUDMessage::set_color (const Color& color)
{
	color_fg = color;

	// The background is a near-black version of the foreground.
	LabColor _color_bg (color);
	_color_bg.L = -0.2 * (100.0 - _color_bg.L);
	color_bg = _color_bg;

	// The border is slightly darker than the foreground.
	LabColor _color_border (color);
	_color_border.L /= 3.0;
	color_border = _color_border;
}

bool
HUDMessage::prepare ()
{
	if (!enabled) return false;

	// Get the canvas and text size and calculate the element size.
	CanvasSize canvas = Engine::get_canvas_size (),
		text_size = get_text_size (text),
		elem_size;
	elem_size.w = BORDER + PADDING + text_size.w + PADDING + BORDER;
	elem_size.h = BORDER + PADDING + text_size.h + PADDING + BORDER;

	// Get the topic's position in canvas coordinates.
	CanvasPoint topic_pos;
	Player player;
	if (topic == Object::NONE || topic == player ||
	    player.is_in_inventory (topic))
		topic_pos = CanvasPoint (canvas.w / 2, canvas.h / 2);
	else
	{
		topic_pos = centroid_to_canvas (topic);
		if (!topic_pos.valid ()) return false;
	}

	// Calculate the element position.
	CanvasPoint elem_pos;
	elem_pos.x = std::max (0, std::min (canvas.w - elem_size.w,
		topic_pos.x - elem_size.w / 2));
	elem_pos.y = std::max (0, std::min (canvas.h - elem_size.h,
		topic_pos.y + 2 * PADDING)); // slightly below center

	set_position (elem_pos);
	set_size (elem_size);
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
	draw_box ();

	// draw text
	set_drawing_color (color_fg);
	draw_text (text, CanvasPoint (BORDER+PADDING, BORDER+PADDING));
}



// NGCTitled

NGCTitled::NGCTitled (const String& _name, const Object& _host,
		const CIString& _title_msgid)
	: Script (_name, _host),
	  PARAMETER_ (title_msgid, _title_msgid, "")
{
	listen_message ("WorldSelect", &NGCTitled::on_world_select);
	listen_message ("WorldDeSelect", &NGCTitled::on_world_deselect);
	listen_message ("FrobWorldBegin", &NGCTitled::on_frob_world_begin);
}

void
NGCTitled::initialize ()
{
	title.reset (new HUDMessage (host ()));
	title->enabled = false;
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
	if (message.get_conversation () == host ())
		Objective (0).set_state (Objective::State::COMPLETE);
	return Message::CONTINUE;
}



// NGCScenario

NGCScenario::NGCScenario (const String& _name, const Object& _host)
	: NGCTitled (_name, _host),
	  disable_trans (*this, &NGCScenario::disable_step, "Disable", 50ul,
		1000ul, Curve::LINEAR, "fade_time", "fade_curve"),
	  PARAMETER (mission, 0),
	  PARAMETER (chess_set_white, 0),
	  PARAMETER (chess_set_black, 0),
	  PERSISTENT (entered, false)
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
		std::ostringstream msgid;
		msgid << "title_" << mission;
		title->set_text (Mission::get_text ("strings", "titles",
			msgid.str ()));
		title->set_color (ChessSet (chess_set_white).get_color ()); //FIXME assumes side?
	}
}

Message::Result
NGCScenario::select (FrobMessage& message)
{
	// Actually choose the scenario.
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
	host_as<AnimLight> ().set_light_mode (AnimLight::Mode::SMOOTH_DIM);
	ObjectProperty ("StTweqRotate", host ()).set_field ("AnimS", 0); //TODO Use RotateTweq once available.
	ObjectProperty ("StTweqScale", host ()).set_field ("AnimS", 0); //TODO Use ScaleTweq once available.

	// Turn off the fill lighting (on the wall object).
	AnimLight wall = ScriptParamsLink::get_one_by_data
		(host (), "Wall").get_dest ();
	wall.set_light_mode (AnimLight::Mode::SMOOTH_DIM);

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
NGCScenario::enter_environment (Message&)
{
	// Only proceed once.
	if (entered)
		return Message::HALT;
	else
		entered = true;

	// Close the false wall again.
	TranslatingDoor wall = ScriptParamsLink::get_one_by_data
		(host (), "Wall").get_dest ();
	wall.base_speed = wall.base_speed * 2.0f;
	wall.close_door ();

	// Start the briefing.
	Object intro = ScriptParamsLink::get_one_by_data
		(host (), "Scenario", true).get_dest ();
	GenericMessage ("StartBriefing").send (host (), intro);

	return Message::HALT;
}



// NGCClock //FIXME per side

NGCClock::NGCClock (const String& _name, const Object& _host)
	: NGCTitled (_name, _host),
	  PARAMETER_ (time_control, "clock_time", 0ul),
	  PARAMETER_ (side, "chess_side", Side::NONE),
	  PARAMETER_ (joint, "clock_joint", 0),
	  PARAMETER_ (joint_low, "clock_low", 0.0f),
	  PARAMETER_ (joint_high, "clock_high", 0.0f),
	  PERSISTENT (running, false)
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
	if (title)
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
		GenericMessage::with_data ("TimeControl", Side (side)).send
			(host (), game);
	}

	// Update the time display.
	else if (title)
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
	return std::max (0l, long (Time (time_control)) -
		QuestVar ("stat_time").get ());
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
	  PARAMETER_ (message_name, "message", "TurnOn")
{
	listen_message ("FrobWorldEnd", &NGCFlag::ask_question);
	listen_message ("FrobInvEnd", &NGCFlag::answered_yes);
	listen_message ("Contained", &NGCFlag::answered_no);
}

Message::Result
NGCFlag::ask_question (FrobMessage&)
{
	Object clone = host ().clone ();
	Link::create ("Owns", host (), clone);

	Player player;
	player.add_to_inventory (clone);
	player.select_item (clone);

	host_as<Rendered> ().render_type = Rendered::RenderType::NONE;

	return Message::HALT;
}

Message::Result
NGCFlag::answered_yes (FrobMessage&)
{
	Rendered owner = Link::get_one ("~Owns", host ()).get_dest ();
	owner.render_type = Rendered::RenderType::NORMAL;
	GenericMessage (message_name->data ()).broadcast
		(owner, "ControlDevice");
	// This object (the clone) will be destroyed by the FrobInfo.
	return Message::HALT;
}

Message::Result
NGCFlag::answered_no (ContainmentMessage& message)
{
	if (message.get_event () == ContainmentMessage::REMOVE)
	{
		Rendered owner = Link::get_one ("~Owns", host ()).get_dest ();
		owner.render_type = Rendered::RenderType::NORMAL;
		host ().schedule_destruction (10ul);
	}
	return Message::HALT;
}



// NGCSquare

NGCSquare::NGCSquare (const String& _name, const Object& _host)
	: Script (_name, _host),
	  PERSISTENT (state, State::EMPTY),
	  PERSISTENT (piece, Piece ()),
	  decal_fade (*this, &NGCSquare::decal_fade_step, "DecalFade"),
	  PARAMETER (decal_offset, Vector ()),
	  button_fade (*this, &NGCSquare::button_fade_step, "ButtonFade"),
	  PARAMETER (button_offset, Vector ())
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

	update_decal ();
	update_button ();

	return Message::HALT;
}

Rendered
NGCSquare::get_decal () const
{
	return ScriptParamsLink::get_one_by_data (host (), "Decal").get_dest ();
}

void
NGCSquare::update_decal ()
{
	Rendered decal = get_decal ();

	Piece display_piece = piece;
	switch (state)
	{
	case State::FRIENDLY_INERT:
	case State::CAN_MOVE_FROM:
		break;
	case State::CAN_MOVE_TO:
		display_piece.type = Piece::Type::NONE;
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

	decal = Object::create (Object ("ChessDecal"));
	if (decal == Object::NONE) return; // ???

	ScriptParamsLink::create (host (), decal, "Decal");

	Vector location = host ().get_location () + decal_offset,
		rotation (0.0f, 0.0f, 180.0f + 90.0f *
			display_piece.side.get_facing_direction ());
	decal.set_position (location, rotation);

	std::ostringstream texture;
	texture << "obj\\txt16\\decal-"
		<< (display_piece.is_valid () ? display_piece.get_code () : 'z')
		<< (state == State::FRIENDLY_INERT
			? 0 : ChessSet (display_piece.side).number);
	decal.replacement_texture [0u] = texture.str ();

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
		(fade_in ? 0.0f : 1.0f, fade_in ? 1.0f : 0.0f);
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
		rotation.z = 180.0f + 90.0f * side.get_facing_direction ();
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

	std::ostringstream archetype_name;
	archetype_name << "ChessButton" << ChessSet (side).number;
	Object archetype (archetype_name.str ());
	if (archetype == Object::NONE) return; // ???

	button = Object::create (archetype);
	if (button == Object::NONE) return; // ???

	ScriptParamsLink::create (host (), button, "Button");
	Link::create ("ControlDevice", button, host ());

	Vector location = host ().get_location () + button_offset;
	button.set_position (location, rotation);

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

