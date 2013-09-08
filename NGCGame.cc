/******************************************************************************
 *  NGCGame.cc
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

#include "NGCGame.hh"

#define CATCH_ENGINE_FAILURE(where, retstmt) \
	catch (std::exception& e) { engine_failure (where, e.what ()); retstmt; }

#define CATCH_SCRIPT_FAILURE(where, retstmt) \
	catch (std::exception& e) { script_failure (where, e.what ()); retstmt; }


// GameMessage

GameMessage::GameMessage (Side side, float luminance_mult)
	: HUDMessage (10)
{
	position = Position::NORTH;
	offset = { PADDING, PADDING };
	set_color (ChessSet (side).get_color (), luminance_mult);
}



// NGCGame

NGCGame::NGCGame (const String& _name, const Object& _host)
	: Script (_name, _host),
	  PERSISTENT_ (record),
	  PARAMETER_ (good_side, "chess_side_good", Side::NONE),
	  PERSISTENT (evil_side, Side::NONE),
	  PERSISTENT (state, State::NONE),
	  PARAMETER (luminance_mult, 1.0f)
{
	listen_message ("PostSim", &NGCGame::start_game);

	listen_timer ("TickTock", &NGCGame::tick_tock);

	listen_message ("SelectFrom", &NGCGame::select_from);
	listen_message ("SelectTo", &NGCGame::select_to);

	listen_timer ("HaltComputing", &NGCGame::halt_computing);
	listen_timer ("CheckEngine", &NGCGame::check_engine);

	listen_message ("FinishMove", &NGCGame::finish_move);

	listen_message ("Resign", &NGCGame::record_resignation);
	listen_message ("TimeControl", &NGCGame::record_time_control);
	listen_message ("Draw", &NGCGame::record_draw);
	listen_message ("FinishEndgame", &NGCGame::finish_endgame);

	listen_timer ("EndAnnouncement", &NGCGame::end_announcement);

	listen_message ("TurnOn", &NGCGame::show_logbook);

	listen_timer ("EndMission", &NGCGame::end_mission);
	listen_timer ("EarlyEngineFailure", &NGCGame::early_engine_failure);
}

NGCGame::~NGCGame ()
{}



Object
NGCGame::get_square (const Square& square, bool proxy)
{
	return Object ((proxy ? "Proxy" : "Square") + square.get_code ());
}

Square
NGCGame::get_square (const Object& square)
{
	String square_name = square.get_name ();
	if (square_name.length () == 8u &&
	    square_name.substr (0u, 6u) == "Square")
		return Square (square_name.substr (6u, 2u));
	else
		return Square ();
}

Object
NGCGame::get_piece_at (const Square& square, bool proxy)
{
	return get_piece_at (get_square (square, proxy));
}

Object
NGCGame::get_piece_at (const Object& square)
{
	return (square != Object::NONE)
		? Link::get_one ("Population", square).get_dest ()
		: Object::NONE;
}



// Game and board state

void
NGCGame::initialize ()
{
	bool resume_computing = false;

	if (record.exists ()) // existing game
	{
		std::istringstream _record (record);
		try { game.reset (new Game (_record)); }
		CATCH_SCRIPT_FAILURE ("initialize", return)

		if (game->get_result () != Game::Result::ONGOING)
			return; // Don't start the engine at all.
		else if (state == State::COMPUTING)
			resume_computing = true;
		else if (state == State::NONE) // ???
			state = State::INTERACTIVE;
	}
	else // new game
	{
		game.reset (new Game ());
		update_record ();
		// Remainder of preparations will occur post-Sim.
	}

	prepare_engine (resume_computing);
}

Message::Result
NGCGame::start_game (Message&)
{
	if (!game) return Message::ERROR;

	if (good_side == Side::NONE)
		good_side = Side::Value (Thief::Engine::random_int
			(Side::WHITE, Side::BLACK));
	QuestVar ("chess_side_good").set (good_side->value);

	evil_side = good_side->get_opponent ();
	QuestVar ("chess_side_evil").set (evil_side->value);

	Object good_mp ((good_side == Side::WHITE)
		? "M-ChessWhite" : "M-ChessBlack");
	Object evil_mp ((good_side == Side::BLACK)
		? "M-ChessWhite" : "M-ChessBlack");
	for (auto& link : ScriptParamsLink::get_all (host ()))
	{
		Parameter<Team> team (link.get_dest (), "chess_team",
			{ Team::NEUTRAL });
		switch (team)
		{
		case Team::GOOD: link.get_dest ().add_metaprop (good_mp); break;
		case Team::BAD_1: link.get_dest ().add_metaprop (evil_mp); break;
		default: break;
		}
	}

	Object board_origin = ScriptParamsLink::get_one_by_data
		(host (), "BoardOrigin").get_dest ();
	if (board_origin != Object::NONE)
		arrange_board (board_origin, false);
	else
	{
		script_failure ("start_game", "missing board");
		return Message::ERROR;
	}

	Object proxy_origin = ScriptParamsLink::get_one_by_data
		(host (), "ProxyOrigin").get_dest ();
	if (proxy_origin != Object::NONE)
		arrange_board (proxy_origin, true);

	good_check.reset (new GameMessage (good_side, luminance_mult));
	good_check->enabled = false;
	good_check->position = HUDMessage::Position::NW;
	good_check->set_text (Check (good_side).get_description ());

	evil_check.reset (new GameMessage (evil_side, luminance_mult));
	evil_check->enabled = false;
	evil_check->position = HUDMessage::Position::NE;
	evil_check->set_text (Check (evil_side).get_description ());

	update_sim ();
	start_timer ("TickTock", 1000ul, true);
	start_timer ("CheckEngine", 250ul, true);

	// Announce the beginning of the game.
	announce_event (std::make_shared<StartGame> ());

	// Prepare for the "next" (first) move. If playing as black, delay it
	// until after the opening herald announcements.
	state = State::MOVING; // required by finish_move
	GenericMessage ("FinishMove").schedule (host (), host (),
		(good_side == Side::WHITE) ? 250ul : 13250ul, false);

	return Message::HALT;
}

void
NGCGame::arrange_board (const Object& origin, bool proxy)
{
	Object archetype (proxy ? "ChessProxySquare" : "ChessSquare");
	if (origin == Object::NONE || archetype == Object::NONE) return;

	Vector origin_location = origin.get_location (),
		origin_rotation = origin.get_rotation (),
		rank_offset = Parameter<Vector> (origin, "rank_offset"),
		file_offset = Parameter<Vector> (origin, "file_offset");

	bool reversed_board = (good_side == Side::BLACK);

	for (auto _square = Square::BEGIN; _square.is_valid (); ++_square)
	{
		Object square = Object::create (archetype);
		if (square == Object::NONE)
		{
			mono () << "Error: could not create a square."
				<< std::endl;
			continue;
		}

		square.set_name ((proxy ? "Proxy" : "Square")
			+ _square.get_code ());

		double rank = reversed_board
			? (N_RANKS - size_t (_square.rank) - 1u)
			: size_t (_square.rank);
		double file = reversed_board
			? (N_FILES - size_t (_square.file) - 1u)
			: size_t (_square.file);

		Vector location = origin_location
			+ rank_offset * rank + file_offset * file;
		square.set_position (location, origin_rotation);

		Piece _piece = game->get_piece_at (_square);
		if (_piece.is_valid ())
			create_piece (square, _piece, true, proxy);
	}
}

Object
NGCGame::create_piece (const Object& square, const Piece& _piece,
	bool start_positioned, bool proxy)
{
	std::ostringstream archetype_name;
	if (proxy)
		archetype_name << "ChessProxy" << _piece.get_code ();
	else
		archetype_name << "ChessPiece" << _piece.get_code ()
			<< ChessSet (_piece.side).number;

	Object archetype (archetype_name.str ());
	if (archetype == Object::NONE)
		return Object::NONE;

	AI piece = Object::create (archetype);
	if (piece == Object::NONE)
	{
		mono () << "Error: could not create a piece." << std::endl;
		return Object::NONE;
	}

	if (_piece.side == good_side)
		piece.add_metaprop (Object ("M-ChessGood"));
	else if (_piece.side == evil_side)
		piece.add_metaprop (Object ("M-ChessEvil"));

	if (_piece.side == Side::WHITE)
		piece.add_metaprop (Object ("M-ChessWhite"));
	else if (_piece.side == Side::BLACK)
		piece.add_metaprop (Object ("M-ChessBlack"));

	if (!proxy)
		piece.add_metaprop (Object ("M-ChessAlive"));

	Link::create ("Population", square, piece);

	if (proxy)
		place_proxy (piece, square);
	else if (start_positioned)
	{
		GenericMessage ("Reveal").send (host (), piece);
		GenericMessage::with_data ("Reposition", nullptr, true)
			.send (host (), piece);
	}
	else
		piece.send_signal ("FaceEnemy");

	return piece;
}

void
NGCGame::update_record ()
{
	if (game)
	{
		std::ostringstream _record;
		game->serialize (_record);
		record = _record.str ();

		// Update "moves made" statistic.
		QuestVar ("stat_moves").set (game->get_fullmove_number () - 1u);
	}
}

void
NGCGame::update_sim ()
{
	// Erase old possible-move links (all Route links in mission are ours).
	for (auto& old_move : Link::get_all ("Route"))
		old_move.destroy ();

	if (!game) return;

	if (state != State::MOVING &&
	    game->get_result () != Game::Result::ONGOING)
	{
		start_endgame ();
		return; // update_interface will be called from there.
	}

	// Create new possible-move links.
	for (auto& move : game->get_possible_moves ())
	{
		if (!move) continue;
		Object from = get_square (move->get_from ()),
			to = get_square (move->get_to ());
		if (from != Object::NONE && to != Object::NONE)
			Link::create ("Route", from, to);
	}

	update_interface ();
}

void
NGCGame::update_interface ()
{
	clear_selection ();

	bool have_ongoing_game =
		game && game->get_result () == Game::Result::ONGOING;
	bool can_resign = have_ongoing_game && state == State::INTERACTIVE &&
		game->get_active_side () == good_side;
	bool can_draw = can_resign && (game->get_fifty_move_clock () >= 50u ||
		game->is_third_repetition ());
	bool can_exit = state == State::NONE && !have_ongoing_game;

	// Enable or disable the flags based on game state.

	for (auto& flag : ScriptParamsLink::get_all_by_data
				(host (), "ResignFlag"))
		Rendered (flag.get_dest ()).render_type =
			(can_resign && !can_draw)
				? Rendered::RenderType::NORMAL
				: Rendered::RenderType::NONE;

	for (auto& flag : ScriptParamsLink::get_all_by_data
				(host (), "DrawFlag"))
		Rendered (flag.get_dest ()).render_type = can_draw
			? Rendered::RenderType::NORMAL
			: Rendered::RenderType::NONE;

	for (auto& flag : ScriptParamsLink::get_all_by_data
				(host (), "ExitFlag"))
		Rendered (flag.get_dest ()).render_type = can_exit
			? Rendered::RenderType::NORMAL
			: Rendered::RenderType::NONE;

	if (!game) return;

	// Update the squares interface (buttons and decals).
	for (auto _square = Square::BEGIN; _square.is_valid (); ++_square)
	{
		Object square = get_square (_square);
		if (square == Object::NONE) continue; // ???

		Piece piece = game->get_piece_at (_square);
		bool can_move = state == State::INTERACTIVE &&
			Link::any_exist ("Route", square);
		bool is_friendly = state == State::INTERACTIVE &&
			piece.side == game->get_active_side ();
		GenericMessage::with_data ("UpdateState",
			can_move ? NGCSquare::State::CAN_MOVE_FROM :
				is_friendly ? NGCSquare::State::FRIENDLY_INERT :
					NGCSquare::State::EMPTY,
			piece).send (host (), square);
	}
}

Message::Result
NGCGame::tick_tock (TimerMessage& message)
{
	if (state == State::NONE) return Message::HALT;

	// Update time-played statistic.
	QuestVar ("stat_time").set (message.get_time ());

	// Inform chess clocks.
	for (auto& clock : ScriptParamsLink::get_all_by_data (host (), "Clock"))
		GenericMessage ("TickTock").send (host (), clock.get_dest ());

	return Message::HALT;
}



// Player moves

Message::Result
NGCGame::select_from (Message& message)
{
	Object from = message.get_from ();
	if (state != State::INTERACTIVE) return Message::ERROR;

	clear_selection ();

	ScriptParamsLink::create (host (), from, "SelectedSquare");
	GenericMessage ("Select").send (host (), from);

	return Message::HALT;
}

Message::Result
NGCGame::select_to (Message& message)
{
	Object to = message.get_from ();
	if (state != State::INTERACTIVE || !game) return Message::ERROR;

	Object from = ScriptParamsLink::get_one_by_data
		(host (), "SelectedSquare").get_dest ();
	clear_selection ();

	auto move = game->find_possible_move
		(get_square (from), get_square (to));
	if (!move)
	{
		script_failure ("select_to", "move not possible");
		return Message::ERROR;
	}

	start_move (move, false);
	return Message::HALT;
}

void
NGCGame::clear_selection ()
{
	for (auto& old : ScriptParamsLink::get_all_by_data
				(host (), "SelectedSquare"))
	{
		GenericMessage ("Deselect").send (host (), old.get_dest ());
		old.destroy ();
	}
}



// Engine moves

void
NGCGame::prepare_engine (bool resume_computing)
{
	try
	{
		String engine_path = Thief::Engine::find_file_in_path
			("script_module_path", "engine.ose");
		if (engine_path.empty ())
			throw std::runtime_error ("could not find chess engine");
		engine.reset (new Chess::Engine (engine_path));

		String openings_path = Thief::Engine::find_file_in_path
			("script_module_path", "openings.bin");
		if (!openings_path.empty ())
			engine->set_openings_book (openings_path);
		else
			engine->clear_openings_book ();

		engine->set_difficulty (Mission::get_difficulty ());
		engine->start_game (game.get ());

		if (resume_computing)
			start_computing ();
	}
	catch (std::exception& e)
	{
		engine.reset ();
		start_timer ("EarlyEngineFailure", 10ul, false,
			String (e.what ()));
	}
}

void
NGCGame::start_computing ()
{
	if (!engine || state == State::NONE) return;
	state = State::COMPUTING;
	update_interface ();

	Time comp_time;
	try { comp_time = engine->start_calculation (); }
	CATCH_ENGINE_FAILURE ("start_computing", return)

	start_timer ("HaltComputing", comp_time, false);

	for (auto& opponent : ScriptParamsLink::get_all_by_data
				(host (), "Opponent"))
		GenericMessage ("StartThinking").send (host (),
			opponent.get_dest ());
}

Message::Result
NGCGame::halt_computing (TimerMessage&)
{
	try { if (engine) engine->stop_calculation (); }
	CATCH_ENGINE_FAILURE ("halt_computing",)
	// The next check_engine cycle will pick up the move.
	return Message::HALT;
}

Message::Result
NGCGame::check_engine (TimerMessage&)
{
	if (!engine) return Message::HALT;

	try { engine->wait_until_ready (); }
	CATCH_ENGINE_FAILURE ("check_engine",)

	if (state == State::COMPUTING && !engine->is_calculating ())
		finish_computing ();

	return Message::HALT;
}

void
NGCGame::finish_computing ()
{
	if (!engine || !game || state != State::COMPUTING) return;
	state = State::NONE;

	for (auto& opponent : ScriptParamsLink::get_all_by_data
				(host (), "Opponent"))
		GenericMessage ("FinishThinking").send (host (),
			opponent.get_dest ());

	if (engine->has_resigned ())
	{
		if (game->get_result () == Game::Result::ONGOING)
			try
			{
				game->record_loss
					(Loss::Type::RESIGNATION, evil_side);
				start_endgame ();
			}
			CATCH_SCRIPT_FAILURE ("finish_computing",)
		return;
	}

	auto move = game->find_possible_move (engine->take_best_move ());
	if (move)
		start_move (move, true);
	else
		engine_failure ("finish_computing", "no best move");
}

void
NGCGame::engine_failure (const String& where, const String& what)
{
	mono () << "Error: Engine failure in " << where << ": " << what
		<< std::endl;

	engine.reset ();
	if (state == State::COMPUTING)
		state = State::INTERACTIVE;

	// Inform the player that both sides will be interactive.
	Mission::show_book ("engine-problem", "parch");

	// Eliminate objects associated with the computer opponent.
	for (auto& fence : ScriptParamsLink::get_all_by_data
			(host (), "OpponentFence"))
		fence.get_dest ().destroy ();
	for (auto& _opponent : ScriptParamsLink::get_all_by_data
			(host (), "Opponent"))
	{
		Damageable opponent = _opponent.get_dest ();
		opponent.remove_metaprop (Object ("M-ChessAlive"));
		opponent.slay (host ());
	}

	update_interface ();
}

Message::Result
NGCGame::early_engine_failure (TimerMessage& message)
{
	// This timer is only set from the initialize method.
	engine_failure ("initialize",
		message.get_data (Message::DATA1, String ()));
	return Message::HALT;
}



// All moves

void
NGCGame::start_move (const Move::Ptr& move, bool from_engine)
{
	if (!game || !move) return;
	state = State::MOVING;
	clear_selection ();

	try { game->make_move (move); }
	CATCH_SCRIPT_FAILURE ("start_move", return)
	update_record ();

	// Inform engine of player move, unless the game is now over.
	if (engine && !from_engine &&
	    game->get_result () == Game::Result::ONGOING)
	{
		try { engine->set_position (*game); }
		CATCH_ENGINE_FAILURE ("start_move",)
	}

	// Announce the move and clear any check indicator.
	announce_event (move);
	good_check->enabled = false;
	evil_check->enabled = false;

	// Identify the moving piece and squares.
	Object piece = get_piece_at (move->get_from ()),
		from = get_square (move->get_from ()),
		to = get_square (move->get_to ());
	if (piece == Object::NONE || from == Object::NONE || to == Object::NONE)
	{
		script_failure ("start_move", "moving objects not found");
		return;
	}

	// Identify any capture, updating the proxy board and statistics.
	Object captured_piece;
	if (auto capture = std::dynamic_pointer_cast<const Capture> (move))
	{
		captured_piece = get_piece_at (capture->get_captured_square ());

		Object captured_proxy =
			get_piece_at (capture->get_captured_square (), true);
		if (captured_proxy != Object::NONE)
			captured_proxy.destroy ();

		// Increment the pieces-taken statistics.
		QuestVar statistic ((move->get_side () == good_side)
			? "stat_enemy_pieces" : "stat_own_pieces");
		statistic.set (statistic.get () + 1);
	}

	// Set up a castling sequence, if applicable.
	auto castling = std::dynamic_pointer_cast<const Castling> (move);
	if (castling)
	{
		Object rook = get_piece_at (castling->get_rook_from ()),
			rook_from = get_square (castling->get_rook_from ()),
			rook_to = get_square (castling->get_rook_to ());

		Link::get_one ("Population", rook_from, rook).destroy ();
		Link::create ("Population", rook_to, rook);

		// The king will prompt the rook to move after he does.
		ScriptParamsLink::create (piece, rook, "ComovingRook");
		ScriptParamsLink::create (piece, rook_to, "RookTo");

		// The rook will bow to the king after they're in place.
		ScriptParamsLink::create (rook, piece, "MyLiege");

		Object rook_proxy =
			get_piece_at (castling->get_rook_from (), true);
		Object rook_to_proxy =
			get_square (castling->get_rook_to (), true);
		if (rook_proxy != Object::NONE && rook_to_proxy != Object::NONE)
			place_proxy (rook_proxy, rook_to_proxy);
	}

	// Update the Population links on the main board.
	Link::get_one ("Population", from, piece).destroy ();
	Link::create ("Population", to, piece);

	// Move the piece on the proxy board, placing marker decals.
	Object piece_proxy = get_piece_at (move->get_from (), true),
		from_proxy = get_square (move->get_from (), true),
		to_proxy = get_square (move->get_to (), true);
	if (piece_proxy != Object::NONE && to_proxy != Object::NONE)
	{
		place_proxy (piece_proxy, to_proxy);
		GenericMessage::with_data ("UpdateState",
			NGCSquare::State::PROXY_WAS_TO, move->get_piece ())
				.send (host (), to_proxy);
	}
	if (from_proxy != Object::NONE)
		GenericMessage::with_data ("UpdateState",
			NGCSquare::State::PROXY_WAS_FROM, move->get_piece ())
				.send (host (), from_proxy);

	// Start an attack sequence, if any, else go to the square.
	if (captured_piece != Object::NONE)
	{
		Link::get_one ("~Population", captured_piece).destroy ();
		GenericMessage ("BecomeVictim").send (piece, captured_piece);

		GenericMessage::with_data ("AttackPiece", captured_piece).send
			(host (), piece);
		// The piece will proceed to its final square after the attack.
	}
	else
		GenericMessage::with_data ("GoToSquare", to).send
			(host (), piece);

	// Promote the piece, if applicable.
	Piece promoted_piece = move->get_promoted_piece ();
	if (promoted_piece.is_valid ())
	{
		ScriptParamsLink::create (to, piece, "ExPopulation");
		Link::get_one ("Population", to, piece).destroy ();
		Object promotion = create_piece (to, promoted_piece, false);
		GenericMessage::with_data ("BePromoted", promotion).send
			(host (), piece);

		if (piece_proxy != Object::NONE && to_proxy != Object::NONE)
		{
			piece_proxy.destroy ();
			create_piece (to_proxy, promoted_piece, true, true);
		}
	}

	update_sim ();
}

Message::Result
NGCGame::finish_move (Message&)
{
	if (!game || state != State::MOVING) return Message::ERROR;

	if (game->get_result () != Game::Result::ONGOING)
	{
		start_endgame ();
		return Message::HALT;
	}

	// Announce check, if any.
	if (game->is_in_check ())
	{
		if (good_check && game->get_active_side () == good_side)
			good_check->enabled = true;
		else if (evil_check && game->get_active_side () == evil_side)
			evil_check->enabled = true;
		announce_event (std::make_shared<Check>
			(game->get_active_side ()));
	}

	// Prepare for the next move.
	if (engine && game->get_active_side () != good_side)
		start_computing ();
	else
	{
		state = State::INTERACTIVE;
		update_interface ();
	}

	return Message::HALT;
}

void
NGCGame::place_proxy (Rendered proxy, const Object& square)
{
	if (proxy == Object::NONE || square == Object::NONE) return;
	Parameter<Side> proxy_side (proxy, "chess_side", { Side::NONE });

	Link::get_one ("~Population", proxy).destroy ();
	Link::create ("Population", square, proxy);

	Vector location = square.get_location ();
	location.z += Parameter<float> (proxy, "height", 0.0f) / 2.0f
		* Vector (proxy.model_scale).z;

	Vector rotation = { 0.0f, 0.0f, 180.0f };
	// Proxy boards are mirror images of real boards, so subtract.
	rotation.z -= 90.0f * get_facing_direction (proxy_side);

	proxy.set_position (location, rotation);
}



// Endgame

Message::Result
NGCGame::record_resignation (Message&)
{
	if (!game || game->get_result () != Game::Result::ONGOING)
		return Message::ERROR;
	try
	{
		game->record_loss (Loss::Type::RESIGNATION, good_side);
		start_endgame ();
	}
	CATCH_SCRIPT_FAILURE ("record_resignation",)
	return Message::HALT;
}

Message::Result
NGCGame::record_time_control (Message&)
{
	if (!game || game->get_result () != Game::Result::ONGOING)
		return Message::ERROR;
	try
	{
		game->record_loss (Loss::Type::TIME_CONTROL, good_side);
		start_endgame ();
	}
	CATCH_SCRIPT_FAILURE ("record_time_control",)
	return Message::HALT;
}

Message::Result
NGCGame::record_draw (Message&)
{
	if (!game || game->get_result () != Game::Result::ONGOING)
		return Message::ERROR;
	try
	{
		if (game->get_fifty_move_clock () >= 50u)
			game->record_draw (Draw::Type::FIFTY_MOVE);
		else if (game->is_third_repetition ())
			game->record_draw (Draw::Type::THREEFOLD_REPETITION);
		else
			return Message::ERROR;
		start_endgame ();
	}
	CATCH_SCRIPT_FAILURE ("record_draw",)
	return Message::HALT;
}

void
NGCGame::start_endgame ()
{
	if (!game || game->get_result () == Game::Result::ONGOING ||
	    state == State::NONE)
		return;

	state = State::NONE;

	// Don't need the engine anymore.
	engine.reset ();

	update_sim ();
	update_interface ();

	// Stop the clocks.
	for (auto& clock : ScriptParamsLink::get_all_by_data (host (), "Clock"))
		GenericMessage ("StopTheClock").send (host (), clock.get_dest ());

	// Have the heralds announce the result.
	announce_event (game->get_last_event ());
}

Message::Result
NGCGame::finish_endgame (Message& message)
{
	// Destroy the end-review gem.
	message.get_from ().destroy ();

	if (!game) return Message::ERROR;

	auto event = game->get_last_event ();
	auto loss = std::dynamic_pointer_cast<const Loss> (event);

	Objective objective;
	switch (game->get_result ())
	{
	case Game::Result::WON:
		switch (loss ? loss->get_type () : Loss::Type::NONE)
		{
		case Loss::Type::RESIGNATION:
			objective.number = 3; // don't resign
			break;
		case Loss::Type::TIME_CONTROL:
			objective.number = 2; // don't run out of time
			break;
		case Loss::Type::CHECKMATE:
		default:
			objective.number = (game->get_victor () == good_side)
				? 0 // checkmate opponent
				: 1; // keep self out of checkmate
			break;
		}
		break;
	case Game::Result::DRAWN:
		objective.number = 4; // don't draw
		break;
	default: // Game::Result::ONGOING - ???
		return Message::ERROR;
	}

	objective.set_visible (true);
	objective.set_state ((objective.number == 0)
		? Objective::State::COMPLETE : Objective::State::FAILED);
	return Message::HALT;
}



// Heraldry

void
NGCGame::announce_event (const Event::ConstPtr& event)
{
	if (!event) return;

	// Display the description on screen, if appropriate.
	String description = event->get_description (),
		identifier = event->serialize ();
	if (!description.empty () && !identifier.empty ())
	{
		description.front () = std::toupper (description.front ());
		announcement.reset (new GameMessage
			(event->get_side (), luminance_mult));
		announcement->identifier = identifier;
		announcement->set_text (description);
		start_timer ("EndAnnouncement", std::max (5000ul,
			Mission::calc_text_duration (description, 1000ul).value),
			false, identifier);
	}

	// Play the heralds' sounds/motions. Both sides for the start of the game
	// or a draw; the event's side's opponent for a check; and the event's
	// side for anything else. Delay a check briefly to avoid overlap.
	if (event->get_side () == Side::NONE)
	{
		herald_concept (Side::WHITE, event->get_concept (), 250ul);
		herald_concept (Side::BLACK, event->get_concept (), 6750ul);
	}
	else if (std::dynamic_pointer_cast<const Check> (event))
		herald_concept (event->get_side ().get_opponent (),
			event->get_concept (), 500ul);
	else
		herald_concept (event->get_side (), event->get_concept ());

	// If it's a Loss, have the winning side celebrate their victory.
	if (std::dynamic_pointer_cast<const Loss> (event))
	{
		herald_concept (game->get_victor (), "win", 6500ul);

		// Have the winning side's remaining pieces cheer.
		for (auto square = Square::BEGIN; square.is_valid (); ++square)
		{
			Object piece = get_piece_at (square);
			if (game->get_piece_at (square).side ==
			    game->get_victor () && piece != Object::NONE)
				GenericMessage ("Celebrate").schedule
					(host (), piece,
					 Thief::Engine::random_int (6000, 7000),
					 false);
		}

		// Play any scripted victory events (such as fireworks).
		for (auto& victory : ScriptParamsLink::get_all_by_data
					(host (), "Victory"))
			if (Parameter<Side> (victory.get_dest (), "chess_side",
			    { Side::NONE }) == game->get_victor ())
				GenericMessage ("TurnOn").schedule (host (),
					victory.get_dest (), 12000ul, false);
	}
}

void
NGCGame::herald_concept (Side side, const String& concept, Time delay)
{
	for (auto& _herald : ScriptParamsLink::get_all_by_data
				(host (), "Herald"))
	{
		Object herald = _herald.get_dest ();
		Time my_delay = delay + Thief::Engine::random_int (0, 50);
		if (side == Side::NONE || side ==
		    Parameter<Side> (herald, "chess_side", { Side::NONE }))
			GenericMessage::with_data ("HeraldConcept", concept)
				.schedule (host (), herald, my_delay, false);
	}
}

Message::Result
NGCGame::end_announcement (TimerMessage& message)
{
	String identifier = message.get_data (Message::DATA1, String ());
	if (announcement && announcement->identifier == identifier)
		announcement.reset ();
	return Message::HALT;
}



// Miscellaneous

Message::Result
NGCGame::show_logbook (Message& message)
{
	if (!game) return Message::ERROR;

	Readable readable = message.get_from ();
	if (!readable.inherits_from (Object ("Book"))) return Message::HALT;

	try
	{
		String book_path = Thief::Engine::find_file_in_path
			("resname_base", "books\\logbook.str");
		if (book_path.empty ())
			throw std::runtime_error ("missing logbook file");
		String plain_path = Mission::get_path_in_fm ("logbook.txt");
		std::ofstream book (book_path), plain (plain_path);

		plain << Game::get_logbook_heading (1u)
			<< std::endl << std::endl;

		unsigned halfmove = 0u, page = 0u;
		for (auto& entry : game->get_history ())
		{
			if (!entry.second) continue;
			if (halfmove % 9u == 0u)
			{
				if (halfmove != 0u)
					book << "...\"" << std::endl;
				book << "page_" << page++ << ": \"";
				book << Game::get_logbook_heading (page)
					<< std::endl << std::endl;
			}
			String description = entry.second->get_description ();
			if (!description.empty ())
				description.front () =
					std::toupper (description.front ());
			book << Game::get_halfmove_prefix (halfmove)
				<< description << std::endl << std::endl;
			plain << Game::get_halfmove_prefix (halfmove)
				<< description << std::endl;
			++halfmove;
		}
		if (game->get_history ().empty ())
			book << "page_0: \""
				<< Game::get_logbook_heading (1u);
		book << "\"" << std::endl;
	}
	catch (std::exception& e)
	{
		mono () << "Warning: Failed to prepare logbook: " << e.what ()
			<< std::endl;
		Mission::show_text (Chess::translate ("logbook_problem"));
		return Message::ERROR;
	}

	if (!readable.book_art.exists ())
		readable.book_art = "pbook";
	Mission::show_book ("logbook", readable.book_art, true);
	return Message::HALT;
}

void
NGCGame::script_failure (const String& where, const String& what)
{
	mono () << "Error: Script failure in " << where << ": " << what
		<< std::endl;

	game.reset ();
	state = State::NONE;

	// Inform the player that we are about to die.
	Mission::show_book ("script-problem", "parch");

	Mission::fade_to_black (100ul);
	start_timer ("EndMission", 100ul, false);
}

Message::Result
NGCGame::end_mission (TimerMessage&)
{
	announcement.reset ();
	good_check.reset ();
	evil_check.reset ();
	Mission::end ();
	return Message::HALT;
}

