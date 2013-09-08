/******************************************************************************
 *  ChessEngine.hh
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

#ifndef CHESSENGINE_HH
#define CHESSENGINE_HH

#include "ChessGame.hh"
#include <iostream>
#include <ext/stdio_filebuf.h>

namespace Chess {

class Engine
{
public:
	typedef std::unique_ptr<Engine> Ptr;

	Engine (const String& program_path);
	~Engine ();

	void set_difficulty (Thief::Difficulty);

	void set_openings_book (const String& book_path);
	void clear_openings_book ();

	void start_game (const Position* initial);
	void set_position (const Position&);

	bool is_calculating () const { return calculating; }
	Thief::Time start_calculation (); // return: expected calculation time
	void stop_calculation ();

	const String& peek_best_move () const { return best_move; }
	bool has_resigned () const;
	String take_best_move ();

	void wait_until_ready ();

private:
	void launch (const String& program_path);

	void read_replies (const String& desired_reply);
	bool has_reply () const;

	void write_command (const String& command);

	typedef __gnu_cxx::stdio_filebuf<char> Buffer;
	std::unique_ptr<Buffer> ein_buf, eout_buf;
	std::unique_ptr<std::istream> ein;
	std::unique_ptr<std::ostream> eout;
	void* ein_handle;

	Thief::Difficulty difficulty;
	String best_move;
	bool debug, started, calculating;
};

} // namespace Chess

#endif // CHESSENGINE_HH

