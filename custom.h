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

#include <iostream>

#include "BaseScript.h"
#include "scriptvars.h"
#include "chess.h"

#endif // SCR_GENSCRIPTS



/**
 * Iterator: LinkIter //FIXME Be a real C++ iterator.
 *
 * Iterates through links that share a source, destination, and/or flavor.
 * Subclasses can further limit the set (for example, based on data) by
 * overriding the Matches function (and calling AdvanceToMatch in their ctors).
 */
#if !SCR_GENSCRIPTS
class LinkIter
{
public:
	LinkIter (object source, object dest, const char* flavor);
	virtual ~LinkIter () noexcept;

	virtual operator bool () const;
	virtual LinkIter& operator++ ();

	operator link () const;
	object Source () const;
	object Destination () const;

	const void* GetData () const;
	void GetDataField (const char* field, cMultiParm& value) const;

protected:
	void AdvanceToMatch ();
	virtual bool Matches () { return true; }

private:
	linkset m_links;
};
#endif // SCR_GENSCRIPTS



/**
 * Iterator: ScriptParamsIter
 *
 * Iterates through ScriptParams links from a source object that share a certain
 * link data string. If the string is "Self" or "Player", iterates over only the
 * source object or the object named Player, respectively.
 */
#if !SCR_GENSCRIPTS
class ScriptParamsIter : public LinkIter
{
public:
	ScriptParamsIter (object source, const char* data,
		object destination = object ());
	virtual ~ScriptParamsIter () noexcept;

	virtual operator bool () const;
	virtual ScriptParamsIter& operator++ ();

	operator object () const;

protected:
	virtual bool Matches ();

private:
	cAnsiStr m_data;
	object m_only;
};
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
	void SetOpeningsBook (const std::string& book_file);
	void ClearOpeningsBook ();

	void StartGame (const chess::Board* board);
	void RecordMove (const chess::MovePtr& move);

	uint BeginCalculation (); // returns allotted computation time in ms
	const std::string& EndCalculation (); // returns best computer move

	void WaitUntilReady ();

private:
	void ReadReplies (const std::string& desired_reply);
	bool ReadReply (const std::string& desired_reply);

	void WriteCommand (const std::string& command);

	void SetupPipe (int& read_fd, int& write_fd);
	static void EngineThread (void* pipefd);

	std::filebuf* ein_buf; std::istream* ein;
	std::filebuf* eout_buf; std::ostream* eout;
	uintptr_t engine_thread;

	Difficulty difficulty;
	std::string best_move;
};
#endif // !SCR_GENSCRIPTS



/**
 * Script: ChessGame
 * Inherits: BaseScript
 *
 * Coordinates the chess gameplay, managing the board state, player controls,
 * movement/capture/promotion effects, and ChessEngine connection.
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
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);

private:
	object GetSquare (const chess::Square& square);
	chess::Square GetSquare (object square);
	object GetPieceAt (const chess::Square& square);

	void AnalyzeBoard ();
	void UpdatePieceSelection ();

	void SelectPiece (object piece);
	void ClearSelection ();
	void SelectMove (object destination);

	void BeginComputerMove ();
	void FinishComputerMove ();

	void PerformMove (const chess::MovePtr& move);

	ChessEngine* engine;
	chess::Board* board;

	script_str fen;
	script_int state_data;

	enum PlayState
	{
		STATE_INTERACTIVE,
		STATE_ANIMATING,
		STATE_COMPUTING
	} play_state;
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessGame","BaseScript",cScr_ChessGame)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessSquare
 * Inherits: BaseScript
 *
 * Handles player interaction with a single square on the chess board.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessSquare : public virtual cBaseScript
{
public:
	cScr_ChessSquare (const char* pszName, int iHostObjId);

protected:
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnFrobWorldEnd (sFrobMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessSquare","BaseScript",cScr_ChessSquare)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessPiece
 * Inherits: BaseAIScript
 *
 * Coordinates the activity of an AI chess piece with the ChessGame ("TheGame").
 */
#if !SCR_GENSCRIPTS
class cScr_ChessPiece : public virtual cBaseAIScript
{
public:
	cScr_ChessPiece (const char* pszName, int iHostObjId);

protected:
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);
	virtual long OnFrobWorldEnd (sFrobMsg* pMsg, cMultiParm& mpReply);
	virtual long OnAIModeChange (sAIModeChangeMsg* pMsg,
		cMultiParm& mpReply);

private:
	void Select ();
	void Deselect ();

	void RestInPeace ();
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessPiece","BaseAIScript",cScr_ChessPiece)
#endif // SCR_GENSCRIPTS



#endif // CUSTOM_H

