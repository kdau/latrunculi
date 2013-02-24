/******************************************************************************
 *  custom.h
 *
 *  Custom scripts for A Nice Game of Chess
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  Adapted in part from Public Scripts
 *  Copyright (C) 2005-2011 Tom N Harris <telliamed@whoopdedo.org>
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

#ifndef CUSTOM_H
#define CUSTOM_H

#if !SCR_GENSCRIPTS

#include <windows.h>
#undef GetClassName // ugh

#include "BaseScript.h"
#include "scriptvars.h"

#endif // SCR_GENSCRIPTS

#if !SCR_GENSCRIPTS
/**
 * Utility Class: ChessEngine
 *
 * Manages the connection to the GNU Chess engine.
 */
class ChessEngine
{
public:
	ChessEngine ();
	~ChessEngine ();

	enum Difficulty
	{
		DIFF_EASY = 0,
		DIFF_NORMAL,
		DIFF_HARD
	};
	void SetDifficulty (Difficulty difficulty);

	void SetOpeningsBook (const char* book_file);

	void StartGame (const char* position);

	void WaitUntilReady ();

private:
	bool ReadReply (const char* desired_reply = NULL);
	void ReadReplies (const char* desired_reply = NULL);
	void WriteCommand (const char* command);

	static void EngineThread (void* pipefd);

	uintptr_t engine_thread;
	FILE* output_pipe;
	FILE* input_pipe;
};
#endif // !SCR_GENSCRIPTS

/**
 * Script: ChessGame
 * Inherits: BaseScript
 *
 * Coordinates the chess gameplay with the GNU Chess engine, managing the Dark
 * Engine board representation (AI chess piece states and positions).
 */
#if !SCR_GENSCRIPTS
class cScr_ChessGame : public virtual cBaseScript
{
public:
	cScr_ChessGame (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnEndScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnSim (sSimMsg* pMsg, cMultiParm& mpReply);

private:
	ChessEngine* engine;
	script_str position;
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessGame","BaseScript",cScr_ChessGame)
#endif // SCR_GENSCRIPTS

#endif // CUSTOM_H

