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

#include "BaseScript.h"
#include "scriptvars.h"
#include "chess.h"

enum class SquareState
{
	INACTIVE,
	FRIENDLY_INERT,
	CAN_MOVE_FROM,
	CAN_MOVE_TO
};

int get_chess_set (Side side);
Color get_chess_set_color (int set_number);

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
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
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
	virtual long OnMessage (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnWorldSelect (sScrMsg* pMsg, cMultiParm& mpReply);
	virtual long OnWorldDeSelect (sScrMsg* pMsg, cMultiParm& mpReply);

	void TickTock ();
	void StopTheClock ();

	unsigned GetTimeRemaining ();
	void ShowTimeRemaining ();

	script_int time_control, running; // in seconds; bool
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



#endif // CUSTOM_H

