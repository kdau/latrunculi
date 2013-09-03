/******************************************************************************
 *  custom.cpp
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

#include <cmath>
#include <ctime>
#include <sstream>
#include <unistd.h>

#include <lg/lg/ai.h>
#include <lg/lg/propdefs.h>
#include <ScriptLib.h>
#include "ScriptModule.h"
#include "utils.h"

#include "custom.h"



/* misc. utilities */

#define fclamp(val, min, max) (fmin (fmax ((val), (min)), (max)))

int
get_chess_set (Side side)
{
	return side.is_valid ()
		? QuestVar (String ("chess_set_") + side.get_code ()).get ()
		: 0;
}

Color
get_chess_set_color (int set_number)
{
	static const Color DEFAULT_COLOR (255, 255, 255);
	if (set_number < 1) return DEFAULT_COLOR;

	std::ostringstream set_name;
	set_name << "M-ChessSet" << set_number;

	return Parameter<Color> (Object (set_name.str ()), "chess_color",
		DEFAULT_COLOR);
}



/* callbacks for chess module */

namespace Chess {

String
translate (const String& _msgid, Side side)
{
	char msgid[128] = { 0 };
	if (SideValid (side))
		snprintf (msgid, 128, "%s%d", _msgid.data (),
			GetChessSet (side));
	else
		strncpy (msgid, _msgid.data (), 128);

	cScrStr buffer;
	SService<IDataSrv> pDS (g_pScriptManager);
	pDS->GetString (buffer, "chess", msgid, "", "strings");

	std::string result = (const char*) buffer;
	buffer.Free ();
	return result;

}

} // namespace chess



/* Fade */

cScr_Fade::cScr_Fade (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  SCRIPT_VAROBJ (Fade, state, iHostObjId)
{}

long
cScr_Fade::OnBeginScript (sScrMsg*, cMultiParm&)
{
	state.Init (NONE);
	return 0;
}

long
cScr_Fade::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "FadeIn"))
	{
		state = FADING_IN;
		Fade ();
	}

	else if (!strcmp (pMsg->message, "FadeOut"))
	{
		state = FADING_OUT;
		Fade ();
	}

	else if (!strcmp (pMsg->message, "FadeAway"))
	{
		state = FADING_AWAY;
		Fade ();
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_Fade::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "Fade"))
		Fade ();

	return cBaseScript::OnTimer (pMsg, mpReply);
}

void
cScr_Fade::Fade ()
{
	if (state == NONE) return;

	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm _opacity;
	pPS->Get (_opacity, ObjId (), "RenderAlpha", NULL);
	if (_opacity.type != kMT_Float) return;

	float opacity = float (_opacity) +
		((state == FADING_IN) ? 0.05 : -0.05);
	opacity = fclamp (opacity, 0.0, 1.0);

	pPS->SetSimple (ObjId (), "RenderAlpha", opacity);

	for (LinkIter cont (ObjId (), 0, "Contains"); cont; ++cont)
		pPS->SetSimple (cont.Destination (), "RenderAlpha", opacity);
	for (LinkIter detl (0, ObjId (), "DetailAttachement"); detl; ++detl)
		pPS->SetSimple (detl.Source (), "RenderAlpha", opacity);
	for (LinkIter part (0, ObjId (), "ParticleAttachement"); part; ++part)
		pPS->SetSimple (part.Source (), "RenderAlpha", opacity);
	for (LinkIter phys (0, ObjId (), "PhysAttach"); phys; ++phys)
		pPS->SetSimple (phys.Source (), "RenderAlpha", opacity);

	float max_opacity = GetObjectParamFloat (ObjId (), "MaxOpacity", 1.0);
	if ((state == FADING_IN)
		? (opacity < max_opacity - 0.04)
		: (opacity > 0.04))
	{
		unsigned delay =
			GetObjectParamInt (ObjId (), "FadeTime", 1000) / 20;
		SetTimedMessage ("Fade", delay, kSTM_OneShot);
	}
	else
	{
		if (state == FADING_AWAY)
			DestroyObject (ObjId ());
		else
			state = NONE;
	}
}



/* ConfirmVerb */

cScr_ConfirmVerb::cScr_ConfirmVerb (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_ConfirmVerb::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	SInterface<ITraitManager> pTM (g_pScriptManager);
	SService<IObjectSrv> pOS (g_pScriptManager);
	object clone; pOS->Create (clone, pTM->GetArchetype (ObjId ()));
	CreateLink ("Owns", ObjId (), clone);

	SService<IContainSrv> pCS (g_pScriptManager);
	pCS->Add (clone, StrToObject ("Player"), kContainTypeGeneric, false);

	SService<IDarkUISrv> pDUIS (g_pScriptManager);
	pDUIS->InvSelect (clone);

	AddSingleMetaProperty ("M-Transparent", ObjId ());
	return 0;
}

long
cScr_ConfirmVerb::OnFrobInvEnd (sFrobMsg*, cMultiParm&)
{
	// player said yes
	object owner = LinkIter (0, ObjId (), "Owns").Source ();
	RemoveSingleMetaProperty ("M-Transparent", owner);
	char* message = GetObjectParamString (ObjId (), "Message", "TurnOn");
	CDSend (message, owner);
	g_pMalloc->Free (message);
	// we are destroyed automatically
	return 0;
}

long
cScr_ConfirmVerb::OnContained (sContainedScrMsg* pMsg, cMultiParm&)
{
	if (pMsg->event == kContainRemove) // player said no
	{
		object owner = LinkIter (0, ObjId (), "Owns").Source ();
		RemoveSingleMetaProperty ("M-Transparent", owner);
		SetTimedMessage ("Reject", 10, kSTM_OneShot);
	}
	return 0;
}

long
cScr_ConfirmVerb::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "Reject"))
	{
		DestroyObject (ObjId ());
		return 0;
	}
	else
		return cBaseScript::OnTimer (pMsg, mpReply);
}



/* Titled */

cScr_Titled::cScr_Titled (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_Titled::OnWorldSelect (sScrMsg*, cMultiParm&)
{
	char* msgid = GetObjectParamString (ObjId (), "Title");
	if (msgid != NULL)
	{
		std::string message = chess::Translate (msgid);
		g_pMalloc->Free (msgid);
		ShowString (message.data (),
			std::max (CalcTextTime (message.data ()), 2000));
	}
	return 0;
}

long
cScr_Titled::OnFrobWorldBegin (sFrobMsg*, cMultiParm&)
{
	ShowString ("", 1);
	return 0;
}



/* ChessIntro */

cScr_ChessIntro::cScr_ChessIntro (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_ChessIntro::OnSim (sSimMsg* pMsg, cMultiParm&)
{
	if (pMsg->fStarting)
	{
		for (ScriptParamsIter door (ObjId (), "ScenarioDoor");
		     door; ++door)
			DestroyObject (door);

		SService<IPropertySrv> pPS (g_pScriptManager);
		pPS->SetSimple (ObjId (), "Book", "welcome");
	}
	return 0;
}

long
cScr_ChessIntro::OnTurnOn (sScrMsg* pMsg, cMultiParm&)
{
	// send ourself away, but continue to exist for later events
	SService<IObjectSrv> pOS (g_pScriptManager);
	pOS->Teleport (ObjId (), { 0, 0, 0 }, { 0, 0, 0 }, 0);

	// make the gems inert
	for (ScriptParamsIter gem (ObjId (), "Scenario"); gem; ++gem)
	{
		if (gem.Destination () != pMsg->from)
			SimpleSend (ObjId (), gem.Destination (), "TurnOff");
		AddSingleMetaProperty ("FrobInert", gem.Destination ());
	}

	// delete all but the correct herald
	object herald = ScriptParamsIter (ObjId (), "TheHerald").Destination ();
	for (ScriptParamsIter other (ObjId (), "Herald"); other; ++other)
		if (other.Destination () != herald)
			DestroyObject (other);

	// begin the briefing (or skip if none)
	object briefing = StrToObject ("TheBriefing");
	if (briefing)
	{
		static const int actor = 1;
		CreateLink ("AIConversationActor", briefing, herald, &actor);
		SimpleSend (ObjId (), briefing, "TurnOn");
	}
	else
		SimpleSend (ObjId (), ObjId (), "DoneBriefing");

	return 0;
}

long
cScr_ChessIntro::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "SubtitleSpeech")
		&& pMsg->data.type == kMT_String)
	{
		SService<IQuestSrv> pQS (g_pScriptManager);
		char msgid[128] = { 0 };
		snprintf (msgid, 128, "m%d_%s",
			pQS->Get ("chess_mission"), (const char*) pMsg->data);

		std::string subtitle = chess::Translate (msgid);
		ShowString (subtitle.data (), CalcTextTime (subtitle.data ()));
	}

	else if (!strcmp (pMsg->message, "DoneBriefing"))
	{
		SService<IQuestSrv> pQS (g_pScriptManager);
		pQS->Set ("goal_state_0", 1, kQuestDataMission);
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}



/* ChessScenario */

cScr_ChessScenario::cScr_ChessScenario (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_ChessScenario::OnWorldSelect (sScrMsg*, cMultiParm&)
{
	char msgid[128] = { 0 };
	snprintf (msgid, 128, "title_%d",
		GetObjectParamInt (ObjId (), "Mission"));

	cScrStr buffer;
	SService<IDataSrv> pDS (g_pScriptManager);
	pDS->GetString (buffer, "titles", msgid, "", "strings");

	ShowString (buffer, CalcTextTime (buffer),
		GetChessSetColor (GetObjectParamInt (ObjId (), "WhiteSet"))); //FIXME assumes side?
	buffer.Free ();

	return 0;
}

long
cScr_ChessScenario::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	object intro = StrToObject ("TheIntro");
	int mission = GetObjectParamInt (ObjId (), "Mission");

	PlaySchemaAmbient (ObjId (), StrToObject ("pickup_gem"));

	SService<IDarkGameSrv> pDGS (g_pScriptManager);
	pDGS->SetNextMission (mission);

	SService<IQuestSrv> pQS (g_pScriptManager);
	pQS->Set ("chess_mission", mission, kQuestDataMission);

	for (ScriptParamsIter herald (ObjId (), "Herald"); herald; ++herald)
		CreateLink ("ScriptParams", intro, herald.Destination (),
			"TheHerald");

	SimpleSend (ObjId (), intro, "TurnOn");
	return 0;
}



/* ChessClock */ //FIXME per side

cScr_ChessClock::cScr_ChessClock (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  SCRIPT_VAROBJ (ChessClock, time_control, iHostObjId),
	  SCRIPT_VAROBJ (ChessClock, running, iHostObjId),
	  focused (false)
{}

long
cScr_ChessClock::OnBeginScript (sScrMsg*, cMultiParm&)
{
	time_control.Init (GetObjectParamInt (ObjId (), "ClockTime"));
	running.Init (time_control > 0);
	return 0;
}

long
cScr_ChessClock::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "TickTock"))
		TickTock ();

	else if (!strcmp (pMsg->message, "StopTheClock"))
		StopTheClock ();

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessClock::OnWorldSelect (sScrMsg*, cMultiParm&)
{
	focused = true;
	ShowTimeRemaining ();
	return 0;
}

long
cScr_ChessClock::OnWorldDeSelect (sScrMsg*, cMultiParm&)
{
	focused = false;
	return 0;
}

void
cScr_ChessClock::TickTock ()
{
	SService<IPropertySrv> pPS (g_pScriptManager);

	if (!running)
	{
		if (focused)
			ShowTimeRemaining ();
		return;
	}

	// check the time
	unsigned time_remaining = GetTimeRemaining ();
	float pct_remaining = float (time_remaining) / float (time_control);

	// update the clock joint
	int joint = GetObjectParamInt (ObjId (), "ClockJoint");
	float low = GetObjectParamFloat (ObjId (), "ClockLow"),
		high = GetObjectParamFloat (ObjId (), "ClockHigh"),
		range = high - low;
	if (joint >= 1 && joint <= 6 && range > 0.0)
	{
		char joint_name[] = "Joint \0";
		joint_name[6] = '0' + joint;
		pPS->Set (ObjId (), "JointPos", joint_name,
			low + pct_remaining * range);
	}

	// notify if time has run out
	if (time_remaining == 0)
	{
		StopTheClock ();
		PlaySchema (ObjId (), StrToObject ("dinner_bell"), ObjId ());
		SimpleSend (ObjId (), StrToObject ("TheGame"),
			"TimeControl", GetSide (ObjId ()));
	}

	// update the displayed time if focused
	else if (focused)
		ShowTimeRemaining ();
}

void
cScr_ChessClock::StopTheClock ()
{
	running = false;
	AddSingleMetaProperty ("FrobInertFocusable", ObjId ());
	SService<IPropertySrv> pPS (g_pScriptManager);
	pPS->Remove (ObjId (), "AmbientHacked");
	PlaySchema (ObjId (), StrToObject ("button_rmz"), ObjId ());
}

unsigned
cScr_ChessClock::GetTimeRemaining ()
{
	SService<IQuestSrv> pQS (g_pScriptManager);
	return std::max (time_control - pQS->Get ("stat_time") / 1000, 0);
}

void
cScr_ChessClock::ShowTimeRemaining ()
{
	unsigned time_remaining = GetTimeRemaining ();
	bool last_minute = (time_remaining <= 60);
	const char* msgid = last_minute ? "time_seconds" : "time_minutes";
	unsigned time = last_minute ? time_remaining : (time_remaining / 60);
	ShowString (chess::TranslateFormat (msgid, time).data (), 1010);
}



/* ChessSquare */

cScr_ChessSquare::cScr_ChessSquare (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  SCRIPT_VAROBJ (ChessSquare, state, iHostObjId),
	  SCRIPT_VAROBJ (ChessSquare, piece, iHostObjId)
{}

long
cScr_ChessSquare::OnBeginScript (sScrMsg*, cMultiParm&)
{
	state.Init (INACTIVE);
	piece.Init (chess::Piece::NONE_CODE);
	return 0;
}

long
cScr_ChessSquare::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "UpdateState") &&
		pMsg->data.type == kMT_Int)
	{
		state = (int) pMsg->data;

		if (pMsg->data2.type == kMT_Int)
			piece = (int) pMsg->data2;
		// otherwise the piece hasn't changed

		DestroyAttachments ();
		if (state != INACTIVE)
		{
			CreateDecal ();
			if (state != FRIENDLY_INERT)
				CreateButton ();
		}
	}

	else if (!strcmp (pMsg->message, "Select"))
	{
		for (ScriptParamsIter bttn (ObjId (), "Button"); bttn; ++bttn)
		{
			AddSingleMetaProperty ("M-SelectedSquare",
				bttn.Destination ());
			SimpleSend (ObjId (), bttn.Destination (), "TurnOn");
		}

		for (LinkIter piece (ObjId (), 0, "Population"); piece; ++piece)
			SimpleSend (ObjId (), piece.Destination (), "Select");

		for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
			SimpleSend (ObjId (), move.Destination (),
				"UpdateState", CAN_MOVE_TO,
				(int) piece);
	}

	else if (!strcmp (pMsg->message, "Deselect"))
	{
		for (ScriptParamsIter bttn (ObjId (), "Button"); bttn; ++bttn)
		{
			SimpleSend (ObjId (), bttn.Destination (), "TurnOff");
			RemoveSingleMetaProperty ("M-SelectedSquare",
				bttn.Destination ());
		}

		for (LinkIter piece (ObjId (), 0, "Population"); piece; ++piece)
			SimpleSend (ObjId (), piece.Destination (), "Deselect");

		for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
			SimpleSend (ObjId (), move.Destination (),
				"UpdateState", INACTIVE, chess::Piece::NONE_CODE);
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessSquare::OnTurnOn (sScrMsg*, cMultiParm&)
{
	if (state == CAN_MOVE_FROM)
	{
		PlaySchemaAmbient (ObjId (), StrToObject ("bow_begin"));
		SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectFrom");
	}

	else if (state == CAN_MOVE_TO)
	{
		PlaySchemaAmbient (ObjId (), StrToObject ("pickup_gem"));
		SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectTo");
	}

	return 0;
}

object
cScr_ChessSquare::CreateDecal ()
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	object decal; pOS->Create (decal, StrToObject ("ChessDecal"));
	if (!decal) return decal;

	chess::Piece _piece (piece);
	if (state == CAN_MOVE_TO)
	{
		_piece.type = chess::Piece::NONE;
		_piece.side = chess::Opponent (_piece.side);
	}

	char texture[128] = { 0 };
	snprintf (texture, 128, "obj/txt16/decal-%c%d",
		_piece.Valid () ? _piece.GetCode () : 'z',
		state == FRIENDLY_INERT ? 0 : GetChessSet (_piece.side));

	SService<IPropertySrv> pPS (g_pScriptManager);
	pPS->SetSimple (decal, "OTxtRepr0", texture);

	CreateLink ("ScriptParams", ObjId (), decal, "Decal");
	SimpleSend (ObjId (), decal, "FadeIn");

	cScrVec position, facing; pOS->Position (position, ObjId ());
	position.z += GetObjectParamFloat (ObjId (), "DecalZ");
	facing.z = 180.0 + 90.0 * chess::SideFacing (_piece.side);
	pOS->Teleport (decal, position, facing, 0);

	return decal;
}

object
cScr_ChessSquare::CreateButton ()
{
	chess::Piece _piece (piece);
	if (state == CAN_MOVE_TO)
	{
		_piece.type = chess::Piece::NONE;
		_piece.side = chess::Opponent (_piece.side);
	}

	char _archetype[128] = { 0 };
	snprintf (_archetype, 128, "ChessButton%d", GetChessSet (_piece.side));

	SService<IObjectSrv> pOS (g_pScriptManager);
	object archetype = StrToObject (_archetype);
	if (!archetype) return object ();

	object button; pOS->Create (button, archetype);
	if (!button) return button;

	CreateLink ("ScriptParams", ObjId (), button, "Button");
	CreateLink ("ControlDevice", button, ObjId ());
	SimpleSend (ObjId (), button, "FadeIn");

	cScrVec position, facing; pOS->Position (position, ObjId ());
	position.z += GetObjectParamFloat (ObjId (), "ButtonZ");
	if (state == CAN_MOVE_FROM)
		facing.z = 180.0 + 90.0 * chess::SideFacing (_piece.side);
	else // CAN_MOVE_TO
		facing.y = 90.0;
	pOS->Teleport (button, position, facing, 0);

	return button;
}

void
cScr_ChessSquare::DestroyAttachments ()
{
	for (ScriptParamsIter button (ObjId (), "Button"); button; ++button)
		SimpleSend (ObjId (), button.Destination (), "FadeAway");
	for (ScriptParamsIter decal (ObjId (), "Decal"); decal; ++decal)
		SimpleSend (ObjId (), decal.Destination (), "FadeAway");
}

