/******************************************************************************
 *  ChessEngine.cc
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

#include "ChessEngine.hh"

#include <fcntl.h>
#include <winsock2.h>
#undef GetClassName // ugh, Windows...

namespace Chess {

#ifdef DEBUG
#define ENGINE_DEBUG_DEFAULT 1
#else
#define ENGINE_DEBUG_DEFAULT 0
#endif



Engine::Engine (const String& program_path)
	: ein_buf (nullptr), ein (nullptr), ein_h (nullptr),
	  eout_buf (nullptr), eout (nullptr),
	  difficulty (Thief::Difficulty::HARD),
	  debug (Thief::QuestVar ("debug_engine").get (ENGINE_DEBUG_DEFAULT)),
	  started (false), calculating (false)
{
	launch (program_path);

	write_command ("uci");
	read_replies ("uciok");

	if (debug) write_command ("debug on");
}

Engine::~Engine ()
{
	try { write_command ("stop"); } catch (...) {}
	try { write_command ("quit"); } catch (...) {}

	try { if (eout) delete (eout); } catch (...) {}
	try { if (eout_buf) delete (eout_buf); } catch (...) {}
	try { if (ein) delete (ein); } catch (...) {}
	try { if (ein_buf) delete (ein_buf); } catch (...) {}
}



void
Engine::set_difficulty (Thief::Difficulty _difficulty)
{
	difficulty = _difficulty;
}

void
Engine::set_openings_book (const String& book_path)
{
	write_command ("setoption name OwnBook value true");

	// for Fruit family (not portable UCI)
	write_command ("setoption name BookFile value " + book_path);
}

void
Engine::clear_openings_book ()
{
	write_command ("setoption name OwnBook value false");
}



void
Engine::start_game (const Position* initial)
{
	started = true;
	wait_until_ready ();
	write_command ("ucinewgame");
	if (initial)
		set_position (*initial);
	else
		write_command ("position startpos");
}

void
Engine::set_position (const Position& position)
{
	if (started)
	{
		std::ostringstream fen;
		fen << "position fen ";
		position.serialize (fen);
		write_command (fen.str ());
	}
	else
		start_game (&position);
}



Thief::Time
Engine::start_calculation ()
{
	static const Thief::Time comp_time[] = { 2500ul, 5000ul, 7500ul };
	static const unsigned depth[] = { 1u, 4u, 9u };

	std::stringstream go_command;
	go_command << "go depth " << depth [size_t (difficulty)]
		<< " movetime " << comp_time [size_t (difficulty)];

	wait_until_ready ();
	write_command (go_command.str ());

	calculating = true;
	return comp_time [size_t (difficulty)];
}

void
Engine::stop_calculation ()
{
	wait_until_ready ();
	write_command ("stop");
	calculating = false;
}



bool
Engine::has_resigned () const
{
	// for Fruit family (not portable UCI)
	return best_move == "a1a1";
}

String
Engine::take_best_move ()
{
	String result = std::move (best_move);
	best_move.clear ();
	return result;
}

void
Engine::wait_until_ready ()
{
	write_command ("isready");
	read_replies ("readyok");
}



void
Engine::launch (const String& program_path)
{
#define LAUNCH_CHECK(x) if (!(x)) goto launch_problem

	HANDLE engine_stdin_r, engine_stdin_w,
		engine_stdout_r, engine_stdout_w;
	int eout_fd, ein_fd;
	FILE *eout_file, *ein_file;

	SECURITY_ATTRIBUTES attrs;
	attrs.nLength = sizeof (SECURITY_ATTRIBUTES);
	attrs.bInheritHandle = true;
	attrs.lpSecurityDescriptor = nullptr;

	LAUNCH_CHECK (::CreatePipe (&engine_stdin_r, &engine_stdin_w,
		&attrs, 0));
	LAUNCH_CHECK (::SetHandleInformation (engine_stdin_w,
		HANDLE_FLAG_INHERIT, 0));
	eout_fd = ::_open_osfhandle ((intptr_t) engine_stdin_w, _O_APPEND);
	LAUNCH_CHECK (eout_fd != -1);
	eout_file = _fdopen (eout_fd, "w");
	LAUNCH_CHECK (eout_file != nullptr);
	eout_buf = new Buffer (eout_file, std::ios::out, 1);
	eout = new std::ostream (eout_buf);

	LAUNCH_CHECK (::CreatePipe (&engine_stdout_r, &engine_stdout_w,
		&attrs, 0));
	LAUNCH_CHECK (::SetHandleInformation (engine_stdout_r,
		HANDLE_FLAG_INHERIT, 0));
	ein_fd = ::_open_osfhandle ((intptr_t) engine_stdout_r, _O_RDONLY);
	LAUNCH_CHECK (ein_fd != -1);
	ein_file = _fdopen (ein_fd, "r");
	LAUNCH_CHECK (ein_file != nullptr);
	ein_buf = new Buffer (ein_file, std::ios::in, 1);
	ein = new std::istream (ein_buf);
	ein_h = engine_stdout_r;

	STARTUPINFO start_info;
	::ZeroMemory (&start_info, sizeof (STARTUPINFO));
	start_info.cb = sizeof (STARTUPINFO);
	start_info.hStdError = engine_stdout_w;
	start_info.hStdOutput = engine_stdout_w;
	start_info.hStdInput = engine_stdin_r;
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION proc_info;
	::ZeroMemory (&proc_info, sizeof (PROCESS_INFORMATION));

	LAUNCH_CHECK (::CreateProcess (program_path.data (), nullptr, nullptr,
		nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr, &start_info,
		&proc_info));

	::CloseHandle (proc_info.hProcess);
	::CloseHandle (proc_info.hThread);

	if (debug)
		Thief::mono << "Chess::Engine: Info: The engine has been "
			"loaded from \"" << program_path << "\"." << std::endl;

	return;
#undef LAUNCH_CHECK
launch_problem:
	throw std::runtime_error ("could not launch chess engine");
}



void
Engine::read_replies (const String& desired_reply)
{
	if (!ein) throw std::runtime_error ("no pipe from engine");
	String last_reply;
	unsigned wait_count = 0u;

	while (last_reply != desired_reply)
	{
		while (!has_reply ())
			if (wait_count++ < 250u)
				::Sleep (1u);
			else // time out after ~250ms
				throw std::runtime_error ("engine took too "
					"long to reply with " + desired_reply);

		String full_reply;
		std::getline (*ein, full_reply);
		if (full_reply.empty ())
			continue;
		if (full_reply.back () == '\r')
			full_reply.erase (full_reply.size () - 1u, 1u);

		if (debug && full_reply != "readyok")
			Thief::mono << "Chess::Engine -> " << full_reply
				<< std::endl;

		size_t pos = full_reply.find_first_of (" \t");
		last_reply = full_reply.substr (0u, pos);
		full_reply.erase (0u, pos + 1u);
		pos = full_reply.find_first_of (" \t");

		if (last_reply == "id")
		{
			String field = full_reply.substr (0u, pos);
			full_reply.erase (0u, pos + 1u);

			if (field == "name")
				Thief::mono << "Chess::Engine: Info: The engine "
					"is " << full_reply << "." << std::endl;
			else if (field == "author")
				Thief::mono << "Chess::Engine: Info: The engine "
					"was written by " << full_reply << "."
					<< std::endl;
		}

		else if (last_reply == "bestmove")
			best_move = full_reply.substr (0u, pos);
			// Ponder moves are ignored.
	}
}

bool
Engine::has_reply () const
{
	if (!ein) throw std::runtime_error ("no pipe from engine");

	DWORD val;
	if (!::PeekNamedPipe (ein_h, nullptr, 0, nullptr, &val, nullptr))
		throw std::runtime_error ("could not check for engine reply");

	return val > 0u;
}

void
Engine::write_command (const String& command)
{
	if (!eout) throw std::runtime_error ("no pipe to engine");
	*eout << command << std::endl;
	if (debug && command != "isready")
		Thief::mono << "Chess::Engine <- " << command << std::endl;
}



} // namespace Chess

