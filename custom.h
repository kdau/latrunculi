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
#include <ext/stdio_filebuf.h>

#include "BaseScript.h"
#include "scriptvars.h"
#include "chess.h"

chess::Side GetSide (object target);
int GetChessSet (chess::Side side);

typedef unsigned long COLORREF;
COLORREF GetChessSetColor (int set);

enum CustomMotion
{
	MOTION_NONE = -1,
	MOTION_BOW_TO_KING,	// rook - bipeds and pseudo-bipeds only
	MOTION_PLAY_HORN,	// herald - bipeds only
	MOTION_THINKING,	// herald - bipeds only
	MOTION_THOUGHT,		// herald - bipeds only
	MOTION_FACE_ENEMY,	// all - all types
	N_MOTIONS
};
bool PlayMotion (object ai, CustomMotion motion);

#endif // SCR_GENSCRIPTS



/**
 * Iterator: LinkIter
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
	linkset links;
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
	cAnsiStr data;
	object only;
};
#endif // SCR_GENSCRIPTS



#if !SCR_GENSCRIPTS
/**
 * Utility Class: ChessEngine
 *
 * Manages a connection to a UCI chess engine (customized for Fruit).
 */
class ChessEngine
{
public:
	ChessEngine (const std::string& executable);
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

	void StartGame (const chess::Position* initial_position);
	void UpdatePosition (const chess::Position& position);

	bool IsCalculating () const { return calculating; }
	unsigned StartCalculation (); // returns expected calculation time in ms
	void StopCalculation ();

	const std::string& PeekBestMove () const { return best_move; }
	std::string TakeBestMove ();

	void WaitUntilReady ();

private:
	void LaunchEngine (const std::string& executable);

	void ReadReplies (const std::string& desired_reply);
	bool HasReply ();

	void WriteCommand (const std::string& command);

	typedef __gnu_cxx::stdio_filebuf<char> EngineBuf;
	EngineBuf* ein_buf; std::istream* ein; HANDLE ein_h;
	EngineBuf* eout_buf; std::ostream* eout;

	Difficulty difficulty;
	std::string best_move;
	bool started, calculating;
};
#endif // !SCR_GENSCRIPTS



/**
 * Script: Fade
 * Inherits: BaseScript
 *
 * Fades an object in and out (opacity) in response to the FadeIn and FadeOut
 * messages. FadeAway works like FadeOut except that the object is destroyed
 * after it is fully transparent.
 */
#if !SCR_GENSCRIPTS
class cScr_Fade : public virtual cBaseScript
{
public:
	cScr_Fade (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);

private:
	void Fade ();

	enum State
	{
		NONE,
		FADING_IN,
		FADING_OUT,
		FADING_AWAY
	};
	script_int state; // State
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("Fade","BaseScript",cScr_Fade)
#endif // SCR_GENSCRIPTS



/**
 * Script: ConfirmVerb
 * Inherits: BaseScript
 *
 * When frobbed, displays an on-screen confirmation prompt. If confirmed, sends
 * the message in the @Message parameter along ControlDevice links.
 */
#if !SCR_GENSCRIPTS
class cScr_ConfirmVerb : public virtual cBaseScript
{
public:
	cScr_ConfirmVerb (const char* pszName, int iHostObjId);

protected:
	virtual long OnFrobWorldEnd (sFrobMsg* pMsg, cMultiParm& mpReply);
	virtual long OnFrobInvEnd (sFrobMsg* pMsg, cMultiParm& mpReply);
	virtual long OnContained (sContainedScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ConfirmVerb","BaseScript",cScr_ConfirmVerb)
#endif // SCR_GENSCRIPTS



/**
 * Script: Titled
 * Inherits: BaseScript
 *
 * Displays the translated msgid named in the @Title parameter when the object
 * is focused in-world.
 */
#if !SCR_GENSCRIPTS
class cScr_Titled : public virtual cBaseScript
{
public:
	cScr_Titled (const char* pszName, int iHostObjId);

protected:
	virtual long OnWorldSelect (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnFrobWorldBegin (sFrobMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("Titled","BaseScript",cScr_Titled)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessIntro
 * Inherits: BaseScript
 *
 * Sets up the introduction/scenario selection mission.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessIntro : public virtual cBaseScript
{
public:
	cScr_ChessIntro (const char* pszName, int iHostObjId);

protected:
	virtual long OnSim (sSimMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTurnOn (sScrMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessIntro","BaseScript",cScr_ChessIntro)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessScenario
 * Inherits: BaseScript
 *
 * Selects one of the scenarios and proceeds to its mission.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessScenario : public virtual cBaseScript
{
public:
	cScr_ChessScenario (const char* pszName, int iHostObjId);

protected:
	virtual long OnWorldSelect (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnFrobWorldEnd (sFrobMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessScenario","BaseScript",cScr_ChessScenario)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessGame
 * Inherits: BaseScript
 *
 * Coordinates the chess gameplay, managing the game model, engine connection,
 * player controls, and effects for moves/captures, key events, etc.
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
	virtual long OnTurnOn (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);

private:
	object GetSquare (const chess::Square& square, bool proxy = false);
	chess::Square GetSquare (object square);

	object GetPieceAt (const chess::Square& square, bool proxy = false);
	object GetPieceAt (object square);

	void ArrangeBoard (object origin, bool proxy);
	object CreatePiece (object square, const chess::Piece& piece,
		bool start_positioned, bool proxy = false);

	void UpdateRecord ();
	void UpdateSim ();
	void UpdateInterface ();

	void SelectFrom (object square);
	void SelectTo (object square);
	void ClearSelection ();

	void BeginComputing ();
	void FinishComputing ();

	void BeginMove (const chess::MovePtr& move, bool from_engine);
	void FinishMove ();
	void PlaceProxy (object proxy, object square);

	void BeginEndgame ();
	void FinishEndgame ();

	void AnnounceCheck ();
	void ShowEventMessage (const chess::EventConstPtr& event);
	void HeraldEvent (chess::Side side, const char* event);
	void ShowLogbook (const std::string& art);

	void EngineFailure (const std::string& where, const std::string& what);
	void ScriptFailure (const std::string& where, const std::string& what);

	chess::Game* game;
	ChessEngine* engine;

	script_str record;

	enum State
	{
		NONE = 0,
		INTERACTIVE,
		COMPUTING,
		MOVING
	};
	script_int state; // State
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessGame","BaseScript",cScr_ChessGame)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessClock
 * Inherits: BaseScript
 *
 * Handles time control and the game clock interface.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessClock : public virtual cBaseScript
{
public:
	cScr_ChessClock (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnSim (sSimMsg* pMsg, cMultiParm& mpReply);
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);
	virtual long OnWorldSelect (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnWorldDeSelect (sScrMsg* pMsg, cMultiParm& mpReply);

	void ShowTimeRemaining ();

	script_int time_remaining, time_total; // in seconds
	bool focused; // don't persist
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessClock","BaseScript",cScr_ChessClock)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessSquare
 * Inherits: BaseScript
 *
 * Handles interactions with a single square on the chess board.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessSquare : public virtual cBaseScript
{
public:
	enum State
	{
		INACTIVE,
		FRIENDLY_INERT,
		CAN_MOVE_FROM,
		CAN_MOVE_TO
	};

	cScr_ChessSquare (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTurnOn (sScrMsg* pMsg, cMultiParm& mpReply);

private:
	object CreateDecal ();
	object CreateButton ();
	void DestroyAttachments ();

	script_int state; // State
	script_int piece; // chess::Piece.GetCode ()
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessSquare","BaseScript",cScr_ChessSquare)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessPiece
 * Inherits: BaseAIScript, Fade
 *
 * Coordinates the activity of an AI chess piece with the ChessGame ("TheGame").
 */
#if !SCR_GENSCRIPTS
class cScr_ChessPiece : public virtual cBaseAIScript, public cScr_Fade
{
public:
	cScr_ChessPiece (const char* pszName, int iHostObjId);

protected:
	virtual long OnBeginScript (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply);
	virtual long OnObjActResult (sAIObjActResultMsg* pMsg,
		cMultiParm& mpReply);
	virtual long OnSlain (sSlayMsg* pMsg, cMultiParm& mpReply);
	virtual long OnAIModeChange (sAIModeChangeMsg* pMsg,
		cMultiParm& mpReply);

private:
	void Reposition (object square = object ());
	void CreateAwareness (object target, uint time);

	void GoToSquare (object square);
	void ArriveAtSquare (uint time);
	script_int going_to_square; // object

	void AttackPiece (object piece, uint time);
	void MaintainAttack (uint time);
	void FinishAttack ();
	script_int attacking_piece; // object

	void BecomeVictim (object attacker, uint time);
	void BeAttacked (object attacker, uint time);
	void Die ();
	void BeginBurial ();
	void FinishBurial ();
	script_int being_attacked_by; // object

	void BePromoted (object promotion);
	void RevealPromotion ();
	void FinishPromotion ();
	script_int being_promoted_to; // object

	// for special "piece" types
	void HeraldEvent (const std::string& event);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessPiece","Fade",cScr_ChessPiece)
#endif // SCR_GENSCRIPTS



/**
 * Script: ChessCorpse
 * Inherits: ChessPiece
 *
 * Buries a chess piece's corpse.
 */
#if !SCR_GENSCRIPTS
class cScr_ChessCorpse : public virtual cScr_ChessPiece
{
public:
	cScr_ChessCorpse (const char* pszName, int iHostObjId);

protected:
	virtual long OnCreate (sScrMsg* pMsg, cMultiParm& mpReply);
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessCorpse","ChessPiece",cScr_ChessCorpse)
#endif // SCR_GENSCRIPTS



#endif // CUSTOM_H

