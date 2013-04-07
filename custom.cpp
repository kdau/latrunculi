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
#include <fcntl.h>
#include <winsock2.h>
#undef GetClassName // ugh, Windows...

#include <lg/lg/ai.h>
#include <lg/lg/propdefs.h>
#include <ScriptLib.h>
#include "ScriptModule.h"
#include "utils.h"

#include "custom.h"



// move event maximum durations

#define DUR_MOVE 10000
#define DUR_CASTLING_ROOK 3000
#define DUR_PROMOTION 3000
#define DUR_ATTACK 15000
#define DUR_DEATH 3000
#define DUR_CORPSING 250
#define DUR_BURIAL 1000



/* misc. utilities */

#define fclamp(val, min, max) (fmin (fmax ((val), (min)), (max)))

chess::Side
GetSide (object target)
{
	if (InheritsFrom ("M-ChessBlack", target)) return chess::SIDE_BLACK;
	if (InheritsFrom ("M-ChessWhite", target)) return chess::SIDE_WHITE;
	return chess::SIDE_NONE;
}

int
GetChessSet (chess::Side side)
{
	char set_qvar[] = "chess_set_\0";
	set_qvar[10] = SideCode (side);
	SService<IQuestSrv> pQS (g_pScriptManager);
	return SideValid (side) ? pQS->Get (set_qvar) : 0;
}

COLORREF
GetChessSetColor (int set)
{
	static COLORREF default_color = RGB (255, 255, 255);
	if (set < 1) return default_color;

	char _metaprop[128] = { 0 };
	snprintf (_metaprop, 128, "M-ChessSet%d", set);
	object metaprop = StrToObject (_metaprop);
	if (!metaprop) return default_color;

	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm color_name;
	pPS->Get (color_name, metaprop, "TrapQVar", "NULL");
	if (color_name.type == kMT_String)
		return strtocolor (color_name);
	else
		return default_color;
}

bool
PlayMotion (object ai, CustomMotion motion)
{
	static const char* schemas[] =
	{
		"humsalute3", // MOTION_BOW_TO_KING
		"humgulp", // MOTION_PLAY_HORN
		"bh112003", // MOTION_THINKING
		"bh112550", // MOTION_THOUGHT
		NULL // MOTION_FACE_ENEMY - done with signal
	};

	if (motion == MOTION_FACE_ENEMY)
	{
		SService<IAIScrSrv> pAIS (g_pScriptManager);
		pAIS->Signal (ai, "FaceEnemy");
		return true;
	}
	else if (motion <= MOTION_NONE || motion >= N_MOTIONS)
		return false;

	SService<IPuppetSrv> pPuS (g_pScriptManager);
	true_bool result;
	pPuS->PlayMotion (result, ai, schemas[motion]);
	return result;
}



/* callbacks for chess module */

namespace chess {

void
DebugMessage (const std::string& message)
{
	g_pfnMPrintf (message.data ());
}

std::string
Translate (const std::string& _msgid, Side side)
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



/* LinkIter */

LinkIter::LinkIter (object source, object dest, const char* flavor)
{
	SService<ILinkSrv> pLS (g_pScriptManager);
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	pLS->GetAll (links, flavor ? pLTS->LinkKindNamed (flavor) : 0,
		source, dest);
}

LinkIter::~LinkIter () noexcept
{}

LinkIter::operator bool () const
{
	return links.AnyLinksLeft ();
}

LinkIter&
LinkIter::operator++ ()
{
	links.NextLink ();
	AdvanceToMatch ();
	return *this;
}

LinkIter::operator link () const
{
	return links.AnyLinksLeft () ? links.Link () : link ();
}

object
LinkIter::Source () const
{
	return links.AnyLinksLeft () ? links.Get ().source : object ();
}

object
LinkIter::Destination () const
{
	return links.AnyLinksLeft () ? links.Get ().dest : object ();
}

const void*
LinkIter::GetData () const
{
	return links.AnyLinksLeft () ? links.Data () : NULL;
}

void
LinkIter::GetDataField (const char* field, cMultiParm& value) const
{
	if (!field)
		throw std::invalid_argument ("invalid link data field");
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	pLTS->LinkGetData (value, links.Link (), field);
}

void
LinkIter::AdvanceToMatch ()
{
	while (links.AnyLinksLeft () && !Matches ())
		links.NextLink ();
}



/* ScriptParamsIter */

ScriptParamsIter::ScriptParamsIter (object source, const char* _data,
                                    object destination)
	: LinkIter (source, destination, "ScriptParams"),
	  data (_data), only ()
{
	if (!source && !destination)
		throw std::invalid_argument ("invalid source/destination");
	if (_data && !strcmp (_data, "Self"))
		only = source;
	else if (_data && !strcmp (_data, "Player"))
		only = StrToObject ("Player");
	AdvanceToMatch ();
}

ScriptParamsIter::~ScriptParamsIter () noexcept
{}

ScriptParamsIter::operator bool () const
{
	return only ? true : LinkIter::operator bool ();
}

ScriptParamsIter&
ScriptParamsIter::operator++ ()
{
	if (only)
		only = 0;
	else
		LinkIter::operator++ ();
	return *this;
}

ScriptParamsIter::operator object () const
{
	return only ? only : Destination ();
}

bool
ScriptParamsIter::Matches ()
{
	if (!LinkIter::operator bool ())
		return false;
	else if (data)
		return !strnicmp (data, (const char*) GetData (),
			data.GetLength () + 1);
	else
		return true;
}



/* ChessEngine */

ChessEngine::ChessEngine (const std::string& executable)
	: ein_buf (NULL), ein (NULL), ein_h (NULL),
	  eout_buf (NULL), eout (NULL),
	  difficulty (DIFF_NORMAL), started (false), calculating (false)
{
	LaunchEngine (executable);

	WriteCommand ("uci");
	ReadReplies ("uciok");

#ifdef DEBUG
	WriteCommand ("debug on");
#endif
}

ChessEngine::~ChessEngine ()
{
	try { WriteCommand ("stop"); } catch (...) {}
	try { WriteCommand ("quit"); } catch (...) {}

	try
	{
		if (eout) delete (eout);
		if (eout_buf) delete (eout_buf);
		if (ein) delete (ein);
		if (ein_buf) delete (ein_buf);
	}
	catch (...) {}
}

void
ChessEngine::SetDifficulty (Difficulty _difficulty)
{
	difficulty = _difficulty;
}

void
ChessEngine::SetOpeningsBook (const std::string& book_file)
{
	WriteCommand ("setoption name OwnBook value true");

	// Fruit non-portable
	WriteCommand ("setoption name BookFile value " + book_file);
}

void
ChessEngine::ClearOpeningsBook ()
{
	WriteCommand ("setoption name OwnBook value false");
}

void
ChessEngine::StartGame (const chess::Position* initial_position)
{
	started = true;
	WaitUntilReady ();
	WriteCommand ("ucinewgame");
	if (initial_position)
		UpdatePosition (*initial_position);
	else
		WriteCommand ("position startpos");
}

void
ChessEngine::UpdatePosition (const chess::Position& position)
{
	if (started)
	{
		std::ostringstream fen;
		fen << "position fen ";
		position.Serialize (fen);
		WriteCommand (fen.str ());
	}
	else
		StartGame (&position);
}

unsigned
ChessEngine::StartCalculation ()
{
	static const unsigned comp_time[] = { 2500, 5000, 7500 };
	static const unsigned depth[] = { 1, 4, 9 };

	std::stringstream go_command;
	go_command << "go depth " << depth[difficulty]
		<< " movetime " << comp_time[difficulty];

	WaitUntilReady ();
	WriteCommand (go_command.str ());

	calculating = true;
	return comp_time[difficulty];
}

void
ChessEngine::StopCalculation ()
{
	WaitUntilReady ();
	WriteCommand ("stop");
	calculating = false;
}

std::string
ChessEngine::TakeBestMove ()
{
	std::string result = best_move;
	best_move.clear ();
	return result;
}

void
ChessEngine::WaitUntilReady ()
{
	WriteCommand ("isready");
	ReadReplies ("readyok");
}

void
ChessEngine::LaunchEngine (const std::string& executable)
{
#define lnchstep(x) if (!(x)) goto launch_problem

	HANDLE engine_stdin_r, engine_stdin_w,
		engine_stdout_r, engine_stdout_w;
	int eout_fd, ein_fd;
	FILE *eout_file, *ein_file;

	SECURITY_ATTRIBUTES attrs;
	attrs.nLength = sizeof (SECURITY_ATTRIBUTES);
	attrs.bInheritHandle = true;
	attrs.lpSecurityDescriptor = NULL;

	lnchstep (CreatePipe (&engine_stdin_r, &engine_stdin_w, &attrs, 0));
	lnchstep (SetHandleInformation (engine_stdin_w, HANDLE_FLAG_INHERIT, 0));
	eout_fd = _open_osfhandle ((intptr_t) engine_stdin_w, _O_APPEND);
	lnchstep (eout_fd != -1);
	eout_file = _fdopen (eout_fd, "w");
	lnchstep (eout_file != NULL);
	eout_buf = new EngineBuf (eout_file, std::ios::out, 1);
	eout = new std::ostream (eout_buf);

	lnchstep (CreatePipe (&engine_stdout_r, &engine_stdout_w, &attrs, 0));
	lnchstep (SetHandleInformation (engine_stdout_r, HANDLE_FLAG_INHERIT, 0));
	ein_fd = _open_osfhandle ((intptr_t) engine_stdout_r, _O_RDONLY);
	lnchstep (ein_fd != -1);
	ein_file = _fdopen (ein_fd, "r");
	lnchstep (ein_file != NULL);
	ein_buf = new EngineBuf (ein_file, std::ios::in, 1);
	ein = new std::istream (ein_buf);
	ein_h = engine_stdout_r;

	STARTUPINFO start_info;
	ZeroMemory (&start_info, sizeof (STARTUPINFO));
	start_info.cb = sizeof (STARTUPINFO);
	start_info.hStdError = engine_stdout_w;
	start_info.hStdOutput = engine_stdout_w;
	start_info.hStdInput = engine_stdin_r;
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION proc_info;
	ZeroMemory (&proc_info, sizeof (PROCESS_INFORMATION));

	lnchstep (CreateProcess (executable.data (), NULL, NULL, NULL, true,
		CREATE_NO_WINDOW, NULL, NULL, &start_info, &proc_info));

	CloseHandle (proc_info.hProcess);
	CloseHandle (proc_info.hThread);

#ifdef DEBUG
	g_pfnMPrintf ("ChessEngine == %s\n", executable.data ());
#endif

	return;
#undef lnchstep
launch_problem:
	throw std::runtime_error ("could not launch chess engine");
}

void
ChessEngine::ReadReplies (const std::string& desired_reply)
{
	if (!ein) throw std::runtime_error ("no pipe from engine");
	std::string last_reply;
	unsigned wait_count = 0;

	while (last_reply.compare (desired_reply) != 0)
	{
		while (!HasReply ())
			if (wait_count++ < 250)
				usleep (1000);
			else // time out after 250ms
				throw std::runtime_error
					("engine took too long to reply");

		std::string full_reply;
		std::getline (*ein, full_reply);
		if (full_reply.empty ())
			continue;
		if (full_reply.back () == '\r')
			full_reply.erase (full_reply.size () - 1, 1);

#ifdef DEBUG
		if (full_reply.compare ("readyok") != 0)
			g_pfnMPrintf ("ChessEngine -> %s\n", full_reply.data ());
#endif

		std::size_t pos = full_reply.find_first_of (" \t");
		last_reply = full_reply.substr (0, pos);
		full_reply.erase (0, pos + 1);
		pos = full_reply.find_first_of (" \t");

		if (last_reply.compare ("id") == 0)
		{
			std::string field = full_reply.substr (0, pos);
			full_reply.erase (0, pos + 1);

			if (field.compare ("name") == 0)
				g_pfnMPrintf ("INFO: The chess engine is %s.\n",
					full_reply.data ());
			else if (field.compare ("author") == 0)
				g_pfnMPrintf ("INFO: The chess engine was written by %s.\n",
					full_reply.data ());
		}

		else if (last_reply.compare ("bestmove") == 0)
			best_move = full_reply.substr (0, pos);
			// ponder moves are ignored
	}
}

bool
ChessEngine::HasReply ()
{
	if (!ein) throw std::runtime_error ("no pipe from engine");

	DWORD val;
	if (!PeekNamedPipe (ein_h, NULL, 0, NULL, &val, NULL))
		throw std::runtime_error ("could not check for engine reply");

	return val > 0;
}

void
ChessEngine::WriteCommand (const std::string& command)
{
	if (!eout) throw std::runtime_error ("no pipe to engine");
	*eout << command << std::endl;
#ifdef DEBUG
	if (command.compare ("isready") != 0)
		g_pfnMPrintf ("ChessEngine <- %s\n", command.data ());
#endif
}



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
		GetChessSetColor (GetObjectParamInt (ObjId (), "WhiteSet")));
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



/* ChessGame */

#define CATCH_ENGINE_FAILURE(where, retstmt) \
	catch (std::exception& e) { EngineFailure (where, e.what ()); retstmt; }

#define CATCH_SCRIPT_FAILURE(where, retstmt) \
	catch (std::exception& e) { ScriptFailure (where, e.what ()); retstmt; }

cScr_ChessGame::cScr_ChessGame (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  game (NULL), engine (NULL),
	  SCRIPT_VAROBJ (ChessGame, record, iHostObjId),
	  SCRIPT_VAROBJ (ChessGame, state, iHostObjId)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	state.Init (NONE);

	if (record.Valid ()) // existing game
	{
		std::istringstream _record ((const char*) record);
		try { game = new chess::Game (_record); }
		CATCH_SCRIPT_FAILURE ("BeginScript", game = NULL; return 0)

		if (state == COMPUTING) // need to prompt the engine
			SetTimedMessage ("BeginComputing", 10, kSTM_OneShot);
		else if (game->GetResult () != chess::Game::ONGOING)
			return 0; // don't start the engine at all
		else if (state == NONE) // ???
			state = INTERACTIVE;
	}
	else // new game
	{
		game = new chess::Game ();
		UpdateRecord ();
		state = INTERACTIVE;
	}

	try
	{
		SService<IEngineSrv> pES (g_pScriptManager);
		cScrStr path;

		if (!pES->FindFileInPath ("script_module_path", "engine.ose", path))
			throw std::runtime_error ("could not find chess engine");
		engine = new ChessEngine ((const char*) path);

		if (pES->FindFileInPath ("script_module_path", "openings.bin", path))
			engine->SetOpeningsBook ((const char*) path);
		else
			engine->SetOpeningsBook (NULL);

		SService<IQuestSrv> pQS (g_pScriptManager);
		engine->SetDifficulty
			((ChessEngine::Difficulty) pQS->Get ("difficulty"));

		engine->StartGame (game);
	}
	catch (std::exception& e)
	{
		engine = NULL;
		SetTimedMessage ("EngineFailure", 10, kSTM_OneShot, e.what ());
	}

	return 0;
}

long
cScr_ChessGame::OnEndScript (sScrMsg*, cMultiParm&)
{
	if (engine)
	{
		try { delete engine; } catch (...) {}
		engine = NULL;
	}
	if (game)
	{
		try { delete game; } catch (...) {}
		game = NULL;
	}
	return 0;
}

long
cScr_ChessGame::OnSim (sSimMsg* pMsg, cMultiParm&)
{
	if (!pMsg->fStarting || !game) return 0;

	if (object board_origin = StrToObject ("BoardOrigin"))
		ArrangeBoard (board_origin, false);
	else
	{
		ScriptFailure ("OnSim", "missing board");
		return 0;
	}

	if (object proxy_origin = StrToObject ("ProxyOrigin"))
		ArrangeBoard (proxy_origin, true);

	UpdateSim ();
	SetTimedMessage ("HeraldBegin", 250, kSTM_OneShot);
	SetTimedMessage ("TickTock", 1000, kSTM_Periodic);
	SetTimedMessage ("EngineCheck", 250, kSTM_Periodic);
	return 0;
}

long
cScr_ChessGame::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "SelectFrom"))
		SelectFrom (pMsg->from);

	else if (!strcmp (pMsg->message, "SelectTo"))
		SelectTo (pMsg->from);

	else if (!strcmp (pMsg->message, "FinishMove"))
		FinishMove ();

	else if (!strcmp (pMsg->message, "Resign"))
	{
		if (game && game->GetResult () == chess::Game::ONGOING)
			try
			{
				game->RecordLoss (chess::Loss::RESIGNATION,
					chess::SIDE_WHITE);
				BeginEndgame ();
			}
			CATCH_SCRIPT_FAILURE ("Resign",)
	}

	else if (!strcmp (pMsg->message, "TimeControl") &&
	         pMsg->data.type == kMT_Int)
	{
		if (game && game->GetResult () == chess::Game::ONGOING)
			try
			{
				game->RecordLoss (chess::Loss::TIME_CONTROL,
					(chess::Side) (int) pMsg->data);
				BeginEndgame ();
			}
			CATCH_SCRIPT_FAILURE ("TimeControl",)
	}

	else if (!strcmp (pMsg->message, "FiftyMove"))
	{
		if (game && game->GetResult () == chess::Game::ONGOING &&
		    game->GetFiftyMoveClock () >= 50)
			try
			{
				game->RecordDraw (chess::Draw::FIFTY_MOVE);
				BeginEndgame ();
			}
			CATCH_SCRIPT_FAILURE ("FiftyMove",)
	}

	else if (!strcmp (pMsg->message, "FinishEndgame"))
	{
		DestroyObject (pMsg->from);
		FinishEndgame ();
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessGame::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (state != NONE && !strcmp (pMsg->name, "TickTock"))
	{
		SService<IQuestSrv> pQS (g_pScriptManager);
		int time = pQS->Get ("stat_time") + 1000;
		pQS->Set ("stat_time", time, kQuestDataMission);
		for (ScriptParamsIter clock (ObjId (), "Clock"); clock; ++clock)
			SimpleSend (ObjId (), clock.Destination (), "TickTock");
	}

	else if (engine && !strcmp (pMsg->name, "EngineCheck"))
	{
		try { engine->WaitUntilReady (); }
		CATCH_ENGINE_FAILURE ("EngineCheck",)

		if (state == COMPUTING && !engine->IsCalculating ())
			FinishComputing ();
	}

	else if (engine && !strcmp (pMsg->name, "BeginComputing"))
		BeginComputing ();

	else if (engine && !strcmp (pMsg->name, "StopCalculation"))
		try { engine->StopCalculation (); }
		CATCH_ENGINE_FAILURE ("StopCalculation",)
		// the next EngineCheck will pick up the move

	else if (!strcmp (pMsg->name, "HeraldBegin"))
	{
		HeraldConcept (chess::SIDE_WHITE, "begin");
		HeraldConcept (chess::SIDE_BLACK, "begin", 6500);
	}

	else if (!strcmp (pMsg->name, "EngineFailure"))
	{
		// this timer is only set from OnBeginScript
		std::string what;
		if (pMsg->data.type == kMT_String)
			what = (const char*) pMsg->data;
		EngineFailure ("BeginScript", what);
	}

	else if (!strcmp (pMsg->name, "EndMission"))
	{
		SService<IDarkGameSrv> pDGS (g_pScriptManager);
		pDGS->EndMission ();
	}

	return cBaseScript::OnTimer (pMsg, mpReply);
}

long
cScr_ChessGame::OnTurnOn (sScrMsg* pMsg, cMultiParm&)
{
	if (InheritsFrom ("Book", pMsg->from))
	{
		SService<IPropertySrv> pPS (g_pScriptManager);
		cMultiParm art; pPS->Get (art, pMsg->from, "BookArt", NULL);
		ShowLogbook ((art.type == kMT_String)
			? (const char*) art : "pbook");
	}

	return 0;
}

object
cScr_ChessGame::GetSquare (const chess::Square& square, bool proxy)
{
	std::string square_name =
		(proxy ? "Proxy" : "Square") + square.GetCode ();
	return StrToObject (square_name.data ());
}

chess::Square
cScr_ChessGame::GetSquare (object square)
{
	std::string square_name = (const char*) ObjectToStr (square);
	if (square_name.length () == 8 &&
			square_name.substr (0, 6).compare ("Square") == 0)
		return chess::Square (square_name.substr (6, 2));
	else
		return chess::Square ();
}

object
cScr_ChessGame::GetPieceAt (const chess::Square& square, bool proxy)
{
	return GetPieceAt (GetSquare (square, proxy));
}

object
cScr_ChessGame::GetPieceAt (object square)
{
	return square
		? LinkIter (square, 0, "Population").Destination ()
		: object ();
}

void
cScr_ChessGame::ArrangeBoard (object origin, bool proxy)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	object archetype = StrToObject ("ChessSquare");
	if (!origin || !archetype) return;

	cScrVec origin_position, origin_facing, rank_offset, file_offset;
	pOS->Position (origin_position, origin);
	pOS->Facing (origin_facing, origin);
	rank_offset.x = GetObjectParamFloat (origin, "RankX", 0.0);
	rank_offset.y = GetObjectParamFloat (origin, "RankY", 0.0);
	file_offset.x = GetObjectParamFloat (origin, "FileX", 0.0);
	file_offset.y = GetObjectParamFloat (origin, "FileY", 0.0);

	for (auto _square = chess::Square::BEGIN; _square.Valid (); ++_square)
	{
		object square; pOS->Create (square, archetype);
		if (!square) continue; // ???

		std::string square_name = (proxy ? "Proxy" : "Square")
			+ _square.GetCode ();
		pOS->SetName (square, square_name.data ());

		cScrVec position = origin_position
			+ rank_offset * float (_square.rank)
			+ file_offset * float (_square.file);
		pOS->Teleport (square, position, origin_facing, 0);

		chess::Piece _piece = game->GetPieceAt (_square);
		if (_piece.Valid () && square)
			CreatePiece (square, _piece, true, proxy);
	}
}

object
cScr_ChessGame::CreatePiece (object square, const chess::Piece& _piece,
	bool start_positioned, bool proxy)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	char _archetype[128] = { 0 };
	if (proxy)
		snprintf (_archetype, 128, "ChessProxy%c", _piece.GetCode ());
	else
		snprintf (_archetype, 128, "ChessPiece%c%d", _piece.GetCode (),
			GetChessSet (_piece.side));
	object archetype = StrToObject (_archetype);
	if (!archetype) return object ();

	object piece; pOS->Create (piece, archetype);
	if (!piece) return piece; // ???

	if (_piece.side == chess::SIDE_WHITE)
		AddSingleMetaProperty ("M-ChessWhite", piece);
	else if (_piece.side == chess::SIDE_BLACK)
		AddSingleMetaProperty ("M-ChessBlack", piece);

	AddSingleMetaProperty ("M-ChessAlive", piece);

	CreateLink ("Population", square, piece);

	if (proxy)
		PlaceProxy (piece, square);
	else if (start_positioned)
	{
		SimpleSend (ObjId (), piece, "FadeIn");
		SimpleSend (ObjId (), piece, "Position");
	}
	else
		PlayMotion (piece, MOTION_FACE_ENEMY);

	return piece;
}

void
cScr_ChessGame::UpdateRecord ()
{
	if (game)
	{
		std::ostringstream _record;
		game->Serialize (_record);
		record = _record.str ().data ();

		// update "moves made" statistic
		SService<IQuestSrv> pQS (g_pScriptManager);
		pQS->Set ("stat_moves", game->GetFullmoveNumber () - 1,
			kQuestDataMission);
	}
}

void
cScr_ChessGame::UpdateSim ()
{
	// erase old possible-move links (all Route links in mission are ours)
	for (LinkIter old_move (0, 0, "Route"); old_move; ++old_move)
		DestroyLink (old_move);

	if (!game) return;

	if (state != MOVING && game->GetResult () != chess::Game::ONGOING)
	{
		BeginEndgame ();
		return; // UpdateInterface will be called from there
	}

	// create new possible-move links
	for (auto& move : game->GetPossibleMoves ())
	{
		if (!move) continue;
		object from = GetSquare (move->GetFrom ()),
			to = GetSquare (move->GetTo ());
		if (from && to) CreateLink ("Route", from, to);
	}

	UpdateInterface ();
}

void
cScr_ChessGame::UpdateInterface ()
{
	ClearSelection ();

	bool have_ongoing_game =
		game && game->GetResult () == chess::Game::ONGOING;
	bool can_resign = state == INTERACTIVE && have_ongoing_game
		&& game->GetActiveSide () == chess::SIDE_WHITE;
	bool fifty_move = can_resign && game->GetFiftyMoveClock () >= 50;

	for (ScriptParamsIter flag (ObjId (), "ResignFlag"); flag; ++flag)
	{
		if (can_resign && !fifty_move)
			RemoveMetaProperty ("M-Transparent", flag.Destination ());
		else
			AddMetaProperty ("M-Transparent", flag.Destination ());
	}

	for (ScriptParamsIter flag (ObjId (), "FiftyMoveFlag"); flag; ++flag)
	{
		if (fifty_move)
			RemoveMetaProperty ("M-Transparent", flag.Destination ());
		else
			AddMetaProperty ("M-Transparent", flag.Destination ());
	}

	bool can_exit = state == NONE && !have_ongoing_game;
	for (ScriptParamsIter flag (ObjId (), "ExitFlag"); flag; ++flag)
	{
		if (can_exit)
			RemoveMetaProperty ("M-Transparent", flag.Destination ());
		else
			AddMetaProperty ("M-Transparent", flag.Destination ());
	}

	if (!game) return;

	for (auto _square = chess::Square::BEGIN; _square.Valid (); ++_square)
	{
		object square = GetSquare (_square);
		if (!square) continue; // ???

		chess::Piece piece = game->GetPieceAt (_square);
		bool can_move = state == INTERACTIVE
			&& LinkIter (square, 0, "Route");
		bool is_friendly = state == INTERACTIVE
			&& piece.side == game->GetActiveSide ();
		SimpleSend (ObjId (), square, "UpdateState",
			can_move ? cScr_ChessSquare::CAN_MOVE_FROM :
				is_friendly ? cScr_ChessSquare::FRIENDLY_INERT :
					cScr_ChessSquare::INACTIVE,
			piece.GetCode ());
	}
}

void
cScr_ChessGame::SelectFrom (object from)
{
	if (state != INTERACTIVE) return;
	ClearSelection ();
	CreateLink ("ScriptParams", ObjId (), from, "SelectedSquare");
	SimpleSend (ObjId (), from, "Select");
}

void
cScr_ChessGame::SelectTo (object _to)
{
	if (state != INTERACTIVE || !game) return;

	object _from = ScriptParamsIter (ObjId (), "SelectedSquare");
	ClearSelection ();

	auto move = game->FindPossibleMove (GetSquare (_from), GetSquare (_to));
	if (!move)
	{
		ScriptFailure ("SelectTo", "move not possible");
		return;
	}

	BeginMove (move, false);
}

void
cScr_ChessGame::ClearSelection ()
{
	for (ScriptParamsIter old (ObjId (), "SelectedSquare"); old; ++old)
	{
		SimpleSend (ObjId (), old.Destination (), "Deselect");
		DestroyLink (old);
	}
}

void
cScr_ChessGame::BeginComputing ()
{
	if (!engine || state == NONE) return;
	state = COMPUTING;
	UpdateInterface ();

	unsigned comp_time = 0;
	try { comp_time = engine->StartCalculation (); }
	CATCH_ENGINE_FAILURE ("BeginComputing", return)

	SetTimedMessage ("StopCalculation", comp_time, kSTM_OneShot);

	if (game->IsInCheck ())
		AnnounceCheck ();

	for (ScriptParamsIter opp (ObjId (), "Opponent"); opp; ++opp)
		SimpleSend (ObjId (), opp.Destination (), "BeginThinking");
}

void
cScr_ChessGame::FinishComputing ()
{
	if (!engine || !game || state != COMPUTING) return;
	state = NONE;

	for (ScriptParamsIter opp (ObjId (), "Opponent"); opp; ++opp)
		SimpleSend (ObjId (), opp.Destination (), "FinishThinking");

	std::string best_move = engine->TakeBestMove ();

	// Fruit non-portable
	if (best_move.compare ("a1a1") == 0)
	{
		// the computer has resigned
		if (game->GetResult () == chess::Game::ONGOING)
			try
			{
				game->RecordLoss (chess::Loss::RESIGNATION,
					chess::SIDE_BLACK);
				BeginEndgame ();
			}
			CATCH_SCRIPT_FAILURE ("FinishComputing",)
		return;
	}

	auto move = game->FindPossibleMove (best_move);
	if (move)
		BeginMove (move, true);
	else
		EngineFailure ("FinishComputing", "no best move");
}

void
cScr_ChessGame::BeginMove (const chess::MovePtr& move, bool from_engine)
{
	if (!game || !move) return;
	state = MOVING;
	ClearSelection ();

	try { game->MakeMove (move); }
	CATCH_SCRIPT_FAILURE ("BeginMove", return)
	UpdateRecord ();

	// inform engine of player move, unless the game is now over
	if (engine && !from_engine && game->GetResult () == chess::Game::ONGOING)
	{
		try { engine->UpdatePosition (*game); }
		CATCH_ENGINE_FAILURE ("BeginMove",)
	}

	if (from_engine)
		AnnounceEvent (move);

	object piece = GetPieceAt (move->GetFrom ()),
		from = GetSquare (move->GetFrom ()),
		to = GetSquare (move->GetTo ());
	if (!piece || !from || !to) {
		ScriptFailure ("BeginMove", "moving objects not found");
		return;
	}

	object captured_square, captured_piece;
	if (auto capture = std::dynamic_pointer_cast<const chess::Capture> (move))
	{
		captured_square = GetSquare (capture->GetCapturedSquare ());
		captured_piece = GetPieceAt (captured_square);

		object captured_proxy =
			GetPieceAt (capture->GetCapturedSquare (), true);
		if (captured_proxy)
		{
			DestroyLink (LinkIter (0, captured_proxy, "Population"));
			SimpleSend (ObjId (), captured_proxy, "FadeAway");
		}

		// increment the pieces-taken statistics
		const char* statistic = (move->GetSide () == chess::SIDE_WHITE)
			? "stat_enemy_pieces" : "stat_own_pieces";
		SService<IQuestSrv> pQS (g_pScriptManager);
		pQS->Set (statistic, pQS->Get (statistic) + 1, kQuestDataMission);
	}

	auto castling = std::dynamic_pointer_cast<const chess::Castling> (move);
	if (castling)
	{
		object rook = GetPieceAt (castling->GetRookFrom ()),
			rook_from = GetSquare (castling->GetRookFrom ()),
			rook_to = GetSquare (castling->GetRookTo ());

		DestroyLink (LinkIter (rook_from, rook, "Population"));
		CreateLink ("Population", rook_to, rook);

		// the king will prompt the rook to move after he does
		CreateLink ("ScriptParams", piece, rook, "ComovingRook");
		CreateLink ("ScriptParams", piece, rook_to, "RookTo");

		// the rook will bow to the king after they're in place
		CreateLink ("ScriptParams", rook, piece, "MyLiege");

		object rook_proxy = GetPieceAt (castling->GetRookFrom (), true),
			rook_to_proxy = GetSquare (castling->GetRookTo (), true);
		if (rook_proxy && rook_to_proxy)
			PlaceProxy (rook_proxy, rook_to_proxy);
	}

	DestroyLink (LinkIter (from, piece, "Population"));
	CreateLink ("Population", to, piece);

	object piece_proxy = GetPieceAt (move->GetFrom (), true),
		to_proxy = GetSquare (move->GetTo (), true);
	if (piece_proxy && to_proxy)
		PlaceProxy (piece_proxy, to_proxy);

	if (captured_piece)
	{
		DestroyLink (LinkIter (0, captured_piece, "Population"));
		SimpleSend (piece, captured_piece, "BecomeVictim");

		SimpleSend (ObjId (), piece, "AttackPiece", captured_piece);
		// The piece will proceed to its final square after the attack.
	}
	else
		SimpleSend (ObjId (), piece, "GoToSquare", to);

	chess::Piece _promotion = move->GetPromotion ();
	if (_promotion.Valid ())
	{
		CreateLink ("ScriptParams", to, piece, "ExPopulation");
		DestroyLink (LinkIter (to, piece, "Population"));
		object promotion = CreatePiece (to, _promotion, false);
		SimpleSend (ObjId (), piece, "BePromoted", promotion);

		if (piece_proxy && to_proxy)
		{
			DestroyLink (LinkIter (0, piece_proxy, "Population"));
			SimpleSend (ObjId (), piece_proxy, "FadeAway");
			CreatePiece (to_proxy, _promotion, true, true);
		}
	}

	UpdateSim ();
}

void
cScr_ChessGame::FinishMove ()
{
	if (!game || state != MOVING) return;

	if (game->GetResult () != chess::Game::ONGOING)
		BeginEndgame ();

	else if (engine && game->GetActiveSide () == chess::SIDE_BLACK)
		BeginComputing ();

	else
	{
		state = INTERACTIVE;
		if (game->IsInCheck ())
			AnnounceCheck ();
		UpdateInterface ();
	}
}

void
cScr_ChessGame::PlaceProxy (object proxy, object square)
{
	if (!proxy || !square) return;
	SService<IObjectSrv> pOS (g_pScriptManager);

	DestroyLink (LinkIter (0, proxy, "Population"));
	CreateLink ("Population", square, proxy);

	static const float SCALE_Z = 0.07; // ugh, the property won't read
	cScrVec position; pOS->Position (position, square);
	position.z += GetObjectParamFloat (proxy, "Height", 0.0) / 2.0 * SCALE_Z;

	cScrVec facing = { 0.0, 0.0, 180.0 };
	// proxy boards are mirror images of real boards, so subtract
	facing.z -= 90.0 * chess::SideFacing (GetSide (proxy));

	pOS->Teleport (proxy, position, facing, 0);
}

void
cScr_ChessGame::BeginEndgame ()
{
	if (!game || game->GetResult () == chess::Game::ONGOING ||
	    state == NONE)
		return;

	state = NONE;

	// don't need the engine anymore
	if (engine)
	{
		try { delete engine; } catch (...) {}
		engine = NULL;
	}

	UpdateSim ();
	UpdateInterface ();

	for (ScriptParamsIter clock (ObjId (), "Clock"); clock; ++clock)
		SimpleSend (ObjId (), clock.Destination (), "StopTheClock");

	if (!game->GetHistory ().empty ())
		AnnounceEvent (game->GetHistory ().back ());
}

void
cScr_ChessGame::FinishEndgame ()
{
	if (!game) return;
	int goal = 1;

	chess::EventConstPtr event = game->GetHistory ().empty ()
		? NULL : game->GetHistory ().back ();
	auto loss = std::dynamic_pointer_cast<const chess::Loss> (event);

	switch (game->GetResult ())
	{
	case chess::Game::WON:
		switch (loss ? loss->GetType () : chess::Loss::NONE)
		{
		case chess::Loss::CHECKMATE:
			goal = (game->GetVictor () == chess::SIDE_WHITE)
				? 0 // checkmate black
				: 1; // keep white out of checkmate
			break;
		case chess::Loss::RESIGNATION:
			goal = 3; // don't resign
			break;
		case chess::Loss::TIME_CONTROL:
			goal = 2; // don't run out of time
			break;
		default:
			return; // ???
		}
		break;
	case chess::Game::DRAWN:
		goal = 4; // don't draw
		break;
	default: // chess::Game::ONGOING - ???
		return;
	}

	SService<IQuestSrv> pQS (g_pScriptManager);
	char goal_qvar[128] = { 0 };

	snprintf (goal_qvar, 128, "goal_visible_%d", goal);
	pQS->Set (goal_qvar, 1, kQuestDataMission);

	snprintf (goal_qvar, 128, "goal_state_%d", goal);
	pQS->Set (goal_qvar, (goal == 0) ? 1 : 3, kQuestDataMission);
}

void
cScr_ChessGame::AnnounceEvent (const chess::EventConstPtr& event)
{
	if (!event) return;

	std::string desc = event->GetDescription ();
	if (desc.length () > 0) desc[0] = std::toupper (desc[0]);
	ShowString (desc.data (), std::max (CalcTextTime (desc.data ()), 4000),
		GetChessSetColor (GetChessSet (event->GetSide ())));

	if (std::dynamic_pointer_cast<const chess::Draw> (event))
	{
		// can't be simultanous as the lines may vary
		HeraldConcept (chess::SIDE_WHITE, event->GetConcept (), 0);
		HeraldConcept (chess::SIDE_BLACK, event->GetConcept (), 6500);
	}
	else
	{
		HeraldConcept (event->GetSide (), event->GetConcept ());

		// if a Loss, announce the opposing side's win
		if (std::dynamic_pointer_cast<const chess::Loss> (event))
			HeraldConcept (game->GetVictor (), "win", 6500);
	}
}

void
cScr_ChessGame::HeraldConcept (chess::Side side, const std::string& concept,
	ulong delay)
{
	for (ScriptParamsIter herald (ObjId (), "Herald"); herald; ++herald)
		if (side == chess::SIDE_NONE || side == GetSide (herald))
		{
			sScrMsg* message = new sScrMsg ();
			message->from = ObjId ();
			message->to = herald.Destination ();
			message->message = "HeraldConcept";
			message->data = concept.data ();
			g_pScriptManager->SetTimedMessage
				(message, delay, kSTM_OneShot);
		}
}

// for AnnounceCheck below
	class Check : public chess::Event
	{
	public:
		Check (chess::Side _side)
			: side (_side)
		{
			if (!chess::SideValid (side))
				Invalidate ();
		}

		virtual chess::Side GetSide () const { return side; }
		virtual std::string GetMLAN () const { return std::string (); }
		virtual bool Equals (const Event&) const { return false; }

		virtual std::string GetDescription () const
		{
			return chess::TranslateFormat ("in_check",
				chess::SideName (side, chess::NOMINATIVE).data ());
		}

		virtual std::string GetConcept () const { return "check"; }

	private:
		chess::Side side;
	};

void
cScr_ChessGame::AnnounceCheck ()
{
	if (game && game->IsInCheck ())
		AnnounceEvent
			(std::make_shared<Check> (game->GetActiveSide ()));
}

void
cScr_ChessGame::ShowLogbook (const std::string& art)
{
	if (!game) return;

	SService<IEngineSrv> pES (g_pScriptManager);
	cScrStr path;

	try
	{
		if (!pES->FindFileInPath ("resname_base", "books\\logbook.str",
		    path))
			throw std::runtime_error ("missing logbook file");
		std::ofstream logbook (path);

		unsigned halfmove = 0, page = 0;
		for (auto& event : game->GetHistory ())
		{
			if (!event) continue;
			if (halfmove % 9 == 0)
			{
				if (halfmove > 0)
					logbook << "...\"" << std::endl;
				logbook << "page_" << page++ << ": \"";
				logbook << chess::Game::GetLogbookHeading (page)
					<< std::endl << std::endl;
			}
			std::string desc = event->GetDescription ();
			if (desc.length () > 0) desc[0] = std::toupper (desc[0]);
			logbook << chess::Game::GetHalfmovePrefix (halfmove)
				<< desc << std::endl << std::endl;
			++halfmove;
		}
		if (game->GetHistory ().empty ())
			logbook << "page_0: \""
				<< chess::Game::GetLogbookHeading (1);
		logbook << "\"" << std::endl;
	}
	catch (std::exception& e)
	{
		DebugString ("failed to prepare logbook: ", e.what ());
		std::string message = chess::Translate ("logbook_problem");
		ShowString (message.data (), CalcTextTime (message.data ()));
		return;
	}

	SService<IDebugScrSrv> pDbS (g_pScriptManager);
	pDbS->Command ("test_book_ex", "logbook,", art.data (), "", "", "", "", "");
}

void
cScr_ChessGame::EngineFailure (const std::string& where,
	const std::string& what)
{
	DebugPrintf ("engine failure detected in %s: %s",
		where.data (), what.data ());

	if (engine)
	{
		try { delete engine; } catch (...) {}
		engine = NULL;
	}

	if (state == COMPUTING)
		state = INTERACTIVE;

	// inform the player that both sides will be interactive
	SService<IDarkUISrv> pDUIS (g_pScriptManager);
	pDUIS->ReadBook ("engine-problem", "parch");

	// eliminate objects associated with computer opponent
	for (ScriptParamsIter fence (ObjId (), "OpponentFence"); fence; ++fence)
		DestroyObject (fence.Destination ());
	for (ScriptParamsIter opp (ObjId (), "Opponent"); opp; ++opp)
	{
		RemoveSingleMetaProperty ("M-ChessAlive", opp.Destination ());
		SService<IDamageSrv> pDaS (g_pScriptManager);
		pDaS->Slay (opp.Destination (), ObjId ());
	}

	UpdateInterface ();
}

void
cScr_ChessGame::ScriptFailure (const std::string& where,
	const std::string& what)
{
	DebugPrintf ("script failure detected in %s: %s",
		where.data (), what.data ());

	if (game)
	{
		try { delete game; } catch (...) {}
		game = NULL;
	}

	state = NONE;

	// inform the player that we are about to die
	SService<IDarkUISrv> pDUIS (g_pScriptManager);
	pDUIS->ReadBook ("script-problem", "parch");

	SetTimedMessage ("EndMission", 10, kSTM_OneShot);
}



/* ChessClock */

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



/* ChessPiece */

cScr_ChessPiece::cScr_ChessPiece (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseAIScript (pszName, iHostObjId),
	  cScr_Fade (pszName, iHostObjId),
	  SCRIPT_VAROBJ (ChessPiece, going_to_square, iHostObjId),
	  SCRIPT_VAROBJ (ChessPiece, attacking_piece, iHostObjId),
	  SCRIPT_VAROBJ (ChessPiece, being_attacked_by, iHostObjId),
	  SCRIPT_VAROBJ (ChessPiece, being_promoted_to, iHostObjId)
{}

long
cScr_ChessPiece::OnBeginScript (sScrMsg*, cMultiParm&)
{
	going_to_square.Init (0);
	attacking_piece.Init (0);
	being_attacked_by.Init (0);
	being_promoted_to.Init (0);
	return 0;
}

long
cScr_ChessPiece::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "Position"))
		Reposition (true);

	else if (!strcmp (pMsg->message, "Reposition"))
		Reposition (false);

	else if (!strcmp (pMsg->message, "Select"))
	{
		AddSingleMetaProperty ("M-SelectedPiece", ObjId ());
		SService<IPropertySrv> pPS (g_pScriptManager);
		pPS->Add (ObjId (), "SelfLit"); // dynamic light
	}

	else if (!strcmp (pMsg->message, "Deselect"))
	{
		RemoveSingleMetaProperty ("M-SelectedPiece", ObjId ());
		SService<IPropertySrv> pPS (g_pScriptManager);
		pPS->Remove (ObjId (), "SelfLit"); // dynamic light
	}

	else if (!strcmp (pMsg->message, "GoToSquare") &&
			pMsg->data.type == kMT_Int)
		GoToSquare ((int) pMsg->data);

	else if (!strcmp (pMsg->message, "AttackPiece") &&
			pMsg->data.type == kMT_Int)
		AttackPiece ((int) pMsg->data, pMsg->time);

	else if (!strcmp (pMsg->message, "BecomeVictim"))
		BecomeVictim (pMsg->from, pMsg->time);

	else if (!strcmp (pMsg->message, "BeAttacked"))
		BeAttacked (pMsg->from, pMsg->time);

	else if (!strcmp (pMsg->message, "BePromoted") &&
			pMsg->data.type == kMT_Int)
		BePromoted ((int) pMsg->data);

	else if (!strcmp (pMsg->message, "HeraldConcept") &&
			pMsg->data.type == kMT_String)
		HeraldConcept ((const char*) pMsg->data);

	else if (!strcmp (pMsg->message, "BeginThinking"))
		PlayMotion (ObjId (), MOTION_THINKING);

	else if (!strcmp (pMsg->message, "FinishThinking"))
		PlayMotion (ObjId (), MOTION_THOUGHT);

	else if (!strcmp (pMsg->message, "TellIntro") &&
		pMsg->data.type == kMT_String)
	{
		object intro = StrToObject ("TheIntro");
		if (intro) SimpleSend (ObjId (), intro,
			(const char*) pMsg->data, pMsg->data2);
	}

	return cScr_Fade::OnMessage (pMsg, mpReply);
}

long
cScr_ChessPiece::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "Reposition"))
		Reposition (false);

	else if (!strcmp (pMsg->name, "MaintainAttack"))
		MaintainAttack (pMsg->time);

	else if (!strcmp (pMsg->name, "FinishAttack"))
		FinishAttack ();

	else if (!strcmp (pMsg->name, "ForceDie"))
	{
		SService<IDamageSrv> pDaS (g_pScriptManager);
		pDaS->Slay (ObjId (), (int) being_attacked_by);
	}

	else if (!strcmp (pMsg->name, "BeginBurial"))
		BeginBurial ();

	else if (!strcmp (pMsg->name, "FinishBurial"))
		FinishBurial ();

	else if (!strcmp (pMsg->name, "RevealPromotion"))
		RevealPromotion ();

	else if (!strcmp (pMsg->name, "FinishPromotion"))
		FinishPromotion ();

	else if (!strcmp (pMsg->name, "BowToKing"))
	{
		PlayMotion (ObjId (), MOTION_BOW_TO_KING);
		SetTimedMessage ("Reposition", 3000, kSTM_OneShot);
	}

	return cScr_Fade::OnTimer (pMsg, mpReply);
}

long
cScr_ChessPiece::OnObjActResult (sAIObjActResultMsg* pMsg, cMultiParm&)
{
	if (pMsg->result_data.type != kMT_String)
		{}

	else if (!strcmp (pMsg->result_data, "ArriveAtSquare"))
		ArriveAtSquare (pMsg->time);

	return 0;
}

long
cScr_ChessPiece::OnSlain (sSlayMsg*, cMultiParm&)
{
	if (being_attacked_by != 0)
		Die ();
	return 0;
}

long
cScr_ChessPiece::OnAIModeChange (sAIModeChangeMsg* pMsg, cMultiParm&)
{
	if (being_attacked_by != 0 && pMsg->mode == kAIM_Dead)
		Die ();
	return 0;
}

void
cScr_ChessPiece::Reposition (bool direct, object square)
{
	if (!square) square = LinkIter (0, ObjId (), "Population").Source ();
	if (!square) return;

	SService<IObjectSrv> pOS (g_pScriptManager);

	cScrVec old_pos; pOS->Position (old_pos, ObjId ());
	cScrVec facing; pOS->Facing (facing, ObjId ());

	cScrVec square_pos; pOS->Position (square_pos, square);
	if (direct)
		square_pos.z += 0.5; // don't be stuck in ground
	else
		square_pos.z = old_pos.z; // don't move on z axis

	cScrVec distance = square_pos - old_pos, position;
	if (direct || distance.Magnitude () < 0.25)
		position = square_pos;
	else
	{
		distance /= distance.Magnitude ();
		position = old_pos + distance * 0.2;
		SetTimedMessage ("Reposition", 10, kSTM_OneShot);
	}

	pOS->Teleport (ObjId (), position, facing, 0);

	PlayMotion (ObjId (), MOTION_FACE_ENEMY);
}

link
cScr_ChessPiece::CreateAwareness (object target, uint time)
{
	SService<IObjectSrv> pOS (g_pScriptManager);
	cScrVec target_pos; pOS->Position (target_pos, target);
	sAIAwareness data
	{
		target, kAIAware_Seen | kAIAware_Heard | kAIAware_CanRaycast
			| kAIAware_HaveLOS | kAIAware_FirstHand,
		kHighAwareness, kHighAwareness, time, time, target_pos,
		kHighAwareness, 0, time, time, 0, time, 0, 0
	};

	if (link aware = LinkIter (ObjId (), target, "AIAwareness"))
	{
		SInterface<ILinkManager> pLM (g_pScriptManager);
		data = *reinterpret_cast<sAIAwareness*> (pLM->GetData (aware));
		data.uFlags |= kAIAware_Seen | kAIAware_FirstHand;
		data.i2 = data.Level = data.PeakLevel = kHighAwareness;
		pLM->SetData (aware, &data);
		return aware;
	}
	else
		return CreateLink ("AIAwareness", ObjId (), target, &data);
}

void
cScr_ChessPiece::GoToSquare (object square)
{
	going_to_square = square;
	bool castling_king = ScriptParamsIter (ObjId (), "ComovingRook");

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	true_bool result;
	pAIS->MakeGotoObjLoc (result, ObjId (), square,
		castling_king ? kFastSpeed : kNormalSpeed,
		kHighPriorityAction, "ArriveAtSquare");
}

void
cScr_ChessPiece::ArriveAtSquare (uint time)
{
	if (going_to_square == 0) return;
	going_to_square = 0;
	ScriptParamsIter king (ObjId (), "MyLiege");

	if (attacking_piece != 0)
		AttackPiece ((int) attacking_piece, time);

	else if (being_promoted_to != 0)
		BePromoted ((int) being_promoted_to);

	else if (king) // castling rook
	{
		SimpleSend (ObjId (), StrToObject ("TheGame"), "FinishMove");
		FaceObject (ObjId (), king);
		DestroyLink (king);
		SetTimedMessage ("BowToKing", 500, kSTM_OneShot);
	}

	else
	{
		SetTimedMessage ("Reposition", 500, kSTM_OneShot);

		ScriptParamsIter rook (ObjId (), "ComovingRook"),
			rook_to (ObjId (), "RookTo");
		if (rook && rook_to)
		{
			SimpleSend (ObjId (), rook.Destination (), "GoToSquare",
				rook_to.Destination ());
			DestroyLink (rook);
			DestroyLink (rook_to);
		}
		else
			SimpleSend (ObjId (), StrToObject ("TheGame"),
				"FinishMove");
	}
}

void
cScr_ChessPiece::AttackPiece (object piece, uint time)
{
	attacking_piece = piece;

	AddSingleMetaProperty ("M-ChessAttacker", ObjId ());
	CreateAwareness (piece, time);

	if (going_to_square != 0)
		return; // the attack will commence when we arrive

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	pAIS->SetMinimumAlert (ObjId(), kHighAlert);

	SimpleSend (ObjId (), piece, "BeAttacked");
	MaintainAttack (time);
	// if needed, the @piece's Die timer will eventually call FinishAttack
}

void
cScr_ChessPiece::MaintainAttack (uint time)
{
	object target = (int) attacking_piece;
	if (!target) return;

	SService<IPropertySrv> pPS (g_pScriptManager);
	cMultiParm target_mode, target_hp;
	if (ObjectExists (target))
	{
		pPS->Get (target_mode, target, "AI_Mode", NULL);
		pPS->Get (target_hp, target, "HitPoints", NULL);
	}

	if (!ObjectExists (target) ||
	    (target_mode.type == kMT_Int && target_mode == kAIM_Dead) ||
	    (target_hp.type == kMT_Int && (int) target_hp <= 0))
	{
		FinishAttack ();
		return;
	}

	CreateAwareness (target, time);

	bool have_attack = false;
	for (LinkIter attack (ObjId (), 0, "AIAttack"); attack; ++attack)
		if (attack.Destination () == target)
			have_attack = true;
		else
			DestroyLink (attack);

	if (!have_attack)
	{
		static const eAIResponsePriority prio
			= kVeryHighPriorityResponse;
		CreateLink ("AIAttack", ObjId (), target, &prio);
	}

	pPS->Set (ObjId (), "AI_Mode", NULL, kAIM_Combat);

	SetTimedMessage ("MaintainAttack", 125, kSTM_OneShot);
}

void
cScr_ChessPiece::FinishAttack ()
{
	if (attacking_piece == 0) return;
	attacking_piece = 0;

	RemoveSingleMetaProperty ("M-ChessAttacker", ObjId ());
	SimpleSend (ObjId (), ObjId (), "AbortAttack");

	for (LinkIter attack (ObjId (), 0, "AIAttack"); attack; ++attack)
		DestroyLink (attack);
	for (LinkIter aware (ObjId (), 0, "AIAwareness"); aware; ++aware)
		DestroyLink (aware);

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	pAIS->SetMinimumAlert (ObjId (), kNoAlert);
	pAIS->ClearAlertness (ObjId ());

	// prevent some non-human AIs from continuing to play first alert speech
	HaltSpeech (ObjId ());

	// go to final destination; FinishMove will be called from there
	object square = LinkIter (0, ObjId (), "Population").Source ();
	if (!square)
		square = ScriptParamsIter (0, "ExPopulation", ObjId ()).Source ();
	if (square)
		GoToSquare (square);
}

void
cScr_ChessPiece::BecomeVictim (object attacker, uint time)
{
	// don't set being_attacked_by until it's official
	RemoveSingleMetaProperty ("M-ChessAlive", ObjId ());
	AddSingleMetaProperty ("M-ChessAttackee", ObjId ());
	CreateAwareness (attacker, time);
	FaceObject (ObjId (), attacker);
}

void
cScr_ChessPiece::BeAttacked (object attacker, uint time)
{
	being_attacked_by = attacker;
	CreateAwareness (attacker, time);
	SetTimedMessage ("ForceDie", DUR_ATTACK, kSTM_OneShot);
}

void
cScr_ChessPiece::Die ()
{
	if (being_attacked_by == 0) return;
	being_attacked_by = 0;

	// ensure that any corpses will bury themselves appropriately
	SService<IQuestSrv> pQS (g_pScriptManager);
	pQS->Set ("chess_corpse_side", GetSide (ObjId ()), kQuestDataMission);

	// set timer to do it on ourself, if we are not replaced
	SetTimedMessage ("BeginBurial", DUR_DEATH, kSTM_OneShot);
}

void
cScr_ChessPiece::BeginBurial ()
{
	chess::Side side = GetSide (ObjId ());
	if (side == chess::SIDE_NONE)
	{
		SService<IQuestSrv> pQS (g_pScriptManager);
		side = (chess::Side) pQS->Get ("chess_corpse_side");
	}

	// create smoke puff at site of death
	SService<IObjectSrv> pOS (g_pScriptManager);
	object fx, fx_archetype = StrToObject ("ChessBurialPuff");
	cScrVec position; pOS->Position (position, ObjId ());
	pOS->Create (fx, fx_archetype);
	if (fx) pOS->Teleport (fx, position, cScrVec (), 0);

	// create smoke puff at gravesite, if any
	char grave_name[] = "ChessGrave\0";
	grave_name[10] = chess::SideCode (side);
	object grave = StrToObject (grave_name);
	if (grave)
	{
		CreateLink ("ScriptParams", ObjId (), grave, "Grave");
		pOS->Create (fx, fx_archetype);
		cScrVec position; pOS->Position (position, grave);
		if (fx) pOS->Teleport (fx, position, cScrVec (), 0);
	}

	SimpleSend (ObjId (), ObjId (), "FadeOut");
	SetTimedMessage ("FinishBurial", DUR_BURIAL, kSTM_OneShot);
}

void
cScr_ChessPiece::FinishBurial ()
{
	ScriptParamsIter _grave (ObjId (), "Grave");
	if (_grave)
	{
		object grave = _grave.Destination ();

		SService<IObjectSrv> pOS (g_pScriptManager);
		cScrVec position; pOS->Position (position, grave);
		pOS->Teleport (ObjId (), position, cScrVec (), 0);
		SimpleSend (ObjId (), ObjId (), "FadeIn");

		// displace the grave marker (for rows instead of piles)
		position.x += GetObjectParamFloat (grave, "XIncrement");
		position.y += GetObjectParamFloat (grave, "YIncrement");
		position.z += GetObjectParamFloat (grave, "ZIncrement");
		pOS->Teleport (grave, position, cScrVec (), 0);
	}
	else
		DestroyObject (ObjId ());
}

void
cScr_ChessPiece::BePromoted (object promotion)
{
	being_promoted_to = promotion;
	if (going_to_square != 0 || attacking_piece != 0)
		return; // the promotion will commence when we arrive/capture

	object square = LinkIter (0, promotion, "Population").Source ();
	if (square) Reposition (true, square);

	int set = GetChessSet (GetSide (ObjId ()));
	char fx_name[128] = { 0 };
	snprintf (fx_name, 128, "ChessPromotion%d", set);

	SService<IObjectSrv> pOS (g_pScriptManager);
	object fx; pOS->Create (fx, StrToObject (fx_name));
	if (fx)
	{
		// don't ParticleAttach, so that the FX can outlive us
		CreateLink ("Owns", ObjId (), fx);
		cScrVec position; pOS->Position (position, ObjId ());
		pOS->Teleport (fx, position, cScrVec (), 0);
	}

	SetTimedMessage ("RevealPromotion", DUR_PROMOTION / 2, kSTM_OneShot);
}

void
cScr_ChessPiece::RevealPromotion ()
{
	SService<IPropertySrv> pPS (g_pScriptManager);
	pPS->SetSimple (ObjId (), "PhysAIColl", false);

	SimpleSend (ObjId (), ObjId (), "FadeOut");
	SimpleSend (ObjId (), being_promoted_to, "Position");
	SimpleSend (ObjId (), being_promoted_to, "FadeIn");

	SetTimedMessage ("FinishPromotion", DUR_PROMOTION / 2, kSTM_OneShot);
}

void
cScr_ChessPiece::FinishPromotion ()
{
	being_promoted_to = 0;
	SimpleSend (ObjId (), StrToObject ("TheGame"), "FinishMove");

	object particles = LinkIter (ObjId (), 0, "Owns").Destination ();
	if (particles) SimpleSend (ObjId (), particles, "TurnOff");

	DestroyObject (ObjId ());
}

void
cScr_ChessPiece::HeraldConcept (const std::string& concept)
{
	// play the trumpeting motion
	PlayMotion (ObjId (), MOTION_PLAY_HORN);

	// play the announcement sound (fanfare and/or speech)
	char tags[128] = { 0 };
	snprintf (tags, 128, "ChessSet set%d, ChessConcept %s",
		GetChessSet (GetSide (ObjId ())), concept.data ());
	object source = GetHeraldrySource ();
	PlayEnvSchema (ObjId (), tags, source, source, kEnvSoundAtObjLoc);
}

object
cScr_ChessPiece::GetHeraldrySource ()
{
	SService<IObjectSrv> pOS (g_pScriptManager);

	object source = ScriptParamsIter (ObjId (), "HeraldrySource");
	if (!source)
	{
		pOS->Create (source, StrToObject ("ambientSound"));
		if (!source) return source;
		CreateLink ("ScriptParams", ObjId (), source, "HeraldrySource");
	}

	cScrVec herald_pos; pOS->Position (herald_pos, ObjId ());
	cScrVec player_pos; pOS->Position (player_pos, StrToObject ("Player"));

	// if the player is out of earshot, dislocate herald sounds toward them
	static const float EARSHOT = 25.0;
	cScrVec source_pos = herald_pos;
	if (player_pos.x < herald_pos.x - EARSHOT)
		source_pos.x = player_pos.x + EARSHOT;
	else if (player_pos.x > herald_pos.x + EARSHOT)
		source_pos.x = player_pos.x - EARSHOT;
	if (player_pos.y < herald_pos.y - EARSHOT)
		source_pos.y = player_pos.y + EARSHOT;
	else if (player_pos.y > herald_pos.y + EARSHOT)
		source_pos.y = player_pos.y - EARSHOT;

	pOS->Teleport (source, source_pos, cScrVec (), 0);
	return source;
}



/* ChessCorpse */

cScr_ChessCorpse::cScr_ChessCorpse (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseAIScript (pszName, iHostObjId),
	  cScr_ChessPiece (pszName, iHostObjId)
{}

long
cScr_ChessCorpse::OnCreate (sScrMsg* pMsg, cMultiParm& mpReply)
{
	SetTimedMessage ("BeginBurial", DUR_CORPSING, kSTM_OneShot);
	return cScr_ChessPiece::OnCreate (pMsg, mpReply);
}

