/******************************************************************************
 *  custom.cpp
 *
 *  Custom scripts for A Nice Game of Chess
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  Adapted in part from Public Scripts
 *  Copyright (C) 2005-2011 Tom N Harris <telliamed@whoopdedo.org>
 *
 *  Adapted in part from GNU Chess 6
 *  Copyright (C) 2001-2012 Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <fcntl.h>
#include <io.h>
#include <process.h>
//FIXME FIXME #include <winsock2.h>

#include <ScriptLib.h>

#include "ScriptModule.h"
#include "utils.h"
#include "custom.h"



/* ChessEngine */

#define ENGINE_BUFSIZE 4096

// prototype for main loop of GNU Chess engine
namespace engine { void main_engine (int pipefd_a2e_0, int pipefd_e2a_1); }

ChessEngine::ChessEngine ()
{
	HANDLE read, write;
	int pipefd[2] = { 0 };

	if (!CreatePipe (&read, &write, NULL, 0))
		throw std::runtime_error ("could not create engine output pipe");
	pipefd[0] = _open_osfhandle ((intptr_t) read, _O_RDONLY);
	output_pipe = _fdopen (_open_osfhandle ((intptr_t) write, _O_APPEND), "a");
	if (pipefd[0] == -1 || output_pipe == NULL)
		throw std::runtime_error ("could not connect engine output pipe");

	if (!CreatePipe (&read, &write, NULL, 0))
		throw std::runtime_error ("could not create engine input pipe");
	pipefd[1] = _open_osfhandle ((intptr_t) write, _O_APPEND);
	input_pipe = _fdopen (_open_osfhandle ((intptr_t) read, _O_RDONLY), "r");
	if (pipefd[1] == -1 || input_pipe == NULL)
		throw std::runtime_error ("could not connect engine input pipe");

	engine_thread = _beginthread (EngineThread, 0, pipefd);
	if (engine_thread <= 0)
		throw std::runtime_error ("could not spawn engine thread");

	WriteCommand ("uci");
	WaitForReply ("uciok");

#ifdef DEBUG
	WriteCommand ("debug on");
#endif
}

ChessEngine::~ChessEngine ()
{
	try
	{
		WriteCommand ("quit");
	}
	catch (...) {}

	fclose (input_pipe);
	fclose (output_pipe);
}

void
ChessEngine::SetDifficulty (Difficulty difficulty)
{
	if (difficulty == DIFF_EASY)
		WriteCommand ("setoption name Ponder value false");
	else
		WriteCommand ("setoption name Ponder value true"); //FIXME Does pondering still need to be prompted?

	//FIXME Make additional difficulty-related changes.
}

void
ChessEngine::SetOpeningsBook (const char* book_file)
{
	if (book_file)
	{
		WriteCommand ("setoption name OwnBook value true");

		char book_command[ENGINE_BUFSIZE];
		snprintf (book_command, ENGINE_BUFSIZE,
			"setoption name BookFile value %s", book_file);
		WriteCommand (book_command);
	}
	else
		WriteCommand ("setoption name OwnBook value false");
}

void
ChessEngine::StartGame (const char* position)
{
	WriteCommand ("ucinewgame");
	if (position)
	{
		char position_command[ENGINE_BUFSIZE];
		snprintf (position_command, ENGINE_BUFSIZE,
			"position fen %s", position);
		WriteCommand (position_command);
	}
	else
		WriteCommand ("position startpos");
}

void
ChessEngine::WaitUntilReady ()
{
	WriteCommand ("isready");
	WaitForReply ("readyok");
}

bool
ChessEngine::HasReply (unsigned int wait_msec)
{
#if 0 //FIXME FIXME
	fd_set set[1];
	FD_ZERO (set);
	FD_SET (input_pipe_read, set);

	timeval wait { 0, wait_msec };
#endif //FIXME FIXME

	int fds = 0;//FIXME FIXME select (input_pipe + 1, set, NULL, NULL, &wait);
	if (fds == -1)
		throw std::runtime_error ("could not poll engine for reply");
	return fds > 0;
}

bool
ChessEngine::ReadReply (const char* desired_reply)
{
	//FIXME FIXME if (!HasReply ()) return false;

	char full_reply[ENGINE_BUFSIZE] = { 0 };
	if (fgets (full_reply, ENGINE_BUFSIZE, input_pipe) == NULL)
		throw std::runtime_error ("could not read reply from engine");

#ifdef DEBUG
	g_pfnMPrintf ("ChessEngine -> %s", full_reply);
#endif

	char* remainder = full_reply;
	char* reply = strsep (&remainder, " \t\n");
	if (!reply) return false;

	if (!strcmp (reply, "id"))
	{
		char* field = strsep (&remainder, " \t\n");
		char* value = strsep (&remainder, "\n");
		if (!field || !value)
			{}
		else if (!strcmp (field, "name"))
			g_pfnMPrintf ("The chess engine is %s.\n", value);
		else if (!strcmp (field, "author"))
			g_pfnMPrintf ("The chess engine was written by %s.\n", value);
	}
	else if (!strcmp (reply, "bestmove"))
	{
		char* move = strsep (&remainder, " \t\n");
		if (move)
			g_pfnMPrintf ("The chess engine suggests moving %s.\n", move); //FIXME Record move properly and notify the owning script.
	}
	//FIXME Handle any other replies?

	return !desired_reply || !strcmp (reply, desired_reply);
}

bool
ChessEngine::ReadReplies (const char* desired_reply)
{
	while (HasReply ())
		if (ReadReply (desired_reply))
			return true;
	return false;
}

void
ChessEngine::WaitForReply (const char* reply)
{
	int cycle_count = 0;
	while (!ReadReply (reply))
		if (!HasReply (100) && ++cycle_count > 100) //FIXME max(cycle_count)=10
			throw std::runtime_error ("engine took too long to reply");
}

void
ChessEngine::WriteCommand (const char* command)
{
#ifdef DEBUG
	g_pfnMPrintf ("ChessEngine <- %s\n", command);
#endif
	if (fputs (command, output_pipe) == EOF ||
	    fputc ('\n', output_pipe) == EOF)
		throw std::runtime_error ("could not send command to engine");
}

void
ChessEngine::EngineThread (void* _pipefd)
{
	int* pipefd = reinterpret_cast<int*> (_pipefd);
	engine::main_engine (pipefd[0], pipefd[1]);
}



/* ChessGame */

#define STARTING_POSITION \
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

cScr_ChessGame::cScr_ChessGame (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  engine (NULL),
	  SCRIPT_VAROBJ (ChessGame, position, iHostObjId)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	//FIXME Handle any exceptions nicely.

	engine = new ChessEngine ();

	SService<IEngineSrv> pES (g_pScriptManager);
	cScrStr book;
	if (pES->FindFileInPath ("script_module_path", "openings.dat", book)) //FIXME This still isn't an absolute file name.
		engine->SetOpeningsBook (book); //FIXME Should the book string be freed?
	else
		engine->SetOpeningsBook (NULL);

	SService<IQuestSrv> pQS (g_pScriptManager);
	int difficulty = pQS->Get ("difficulty");
	engine->SetDifficulty ((ChessEngine::Difficulty) difficulty);

	engine->WaitUntilReady ();

	if (position.Valid ()) // not a new game (which is started in OnSim)
	{
		engine->StartGame (position);
		//FIXME Anything else? What if it's the computer's move?
	}

	return 0;
}

long
cScr_ChessGame::OnEndScript (sScrMsg*, cMultiParm&)
{
	if (engine)
	{
		delete engine;
		engine = NULL;
	}
	return 0;
}

long
cScr_ChessGame::OnSim (sSimMsg* pMsg, cMultiParm&)
{
	if (pMsg->fStarting && engine)
	{
		engine->StartGame (NULL);
		position = STARTING_POSITION; //FIXME Read off the objects instead?
		//FIXME Begin the game.
	}
	return 0;
}

//FIXME Crashing on DromEd exit.

