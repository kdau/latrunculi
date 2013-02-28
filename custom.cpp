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

#include <ctime>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <winsock2.h>
#undef GetClassName // ugh, Windows...

#include <ScriptLib.h>
#include "ScriptModule.h"
#include "utils.h"

#include "custom.h"



/* translation for chess module */

namespace chess {

std::string
Translate (const std::string& msgid)
{
	SService<IDataSrv> pDS (g_pScriptManager);
	cScrStr buffer;
	pDS->GetString (buffer, "chess", msgid.data (), "", "strings");
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
	pLS->GetAll (m_links, flavor ? pLTS->LinkKindNamed (flavor) : 0,
		source, dest);
}

LinkIter::~LinkIter () noexcept
{}

LinkIter::operator bool () const
{
	return m_links.AnyLinksLeft ();
}

LinkIter&
LinkIter::operator++ ()
{
	m_links.NextLink ();
	AdvanceToMatch ();
	return *this;
}

LinkIter::operator link () const
{
	return m_links.AnyLinksLeft () ? m_links.Link () : link ();
}

object
LinkIter::Source () const
{
	return m_links.AnyLinksLeft () ? m_links.Get ().source : object ();
}

object
LinkIter::Destination () const
{
	return m_links.AnyLinksLeft () ? m_links.Get ().dest : object ();
}

const void*
LinkIter::GetData () const
{
	return m_links.AnyLinksLeft () ? m_links.Data () : NULL;
}

void
LinkIter::GetDataField (const char* field, cMultiParm& value) const
{
	if (!field)
		throw std::invalid_argument ("invalid link data field");
	SService<ILinkToolsSrv> pLTS (g_pScriptManager);
	pLTS->LinkGetData (value, m_links.Link (), field);
}

void
LinkIter::AdvanceToMatch ()
{
	while (m_links.AnyLinksLeft () && !Matches ())
		m_links.NextLink ();
}



/* ScriptParamsIter */

ScriptParamsIter::ScriptParamsIter (object source, const char* data,
		object destination)
	: LinkIter (source, destination, "ScriptParams"),
	  m_data (data), m_only ()
{
	if (!source)
		throw std::invalid_argument ("invalid source object");
	if (data && !strcmp (data, "Self"))
		m_only = source;
	else if (data && !strcmp (data, "Player"))
		m_only = StrToObject ("Player");
	AdvanceToMatch ();
}

ScriptParamsIter::~ScriptParamsIter () noexcept
{}

ScriptParamsIter::operator bool () const
{
	return m_only ? true : LinkIter::operator bool ();
}

ScriptParamsIter&
ScriptParamsIter::operator++ ()
{
	if (m_only)
		m_only = 0;
	else
		LinkIter::operator++ ();
	return *this;
}

ScriptParamsIter::operator object () const
{
	if (m_only)
		return m_only;
	else
		return Destination ();
}

bool
ScriptParamsIter::Matches ()
{
	if (!LinkIter::operator bool ())
		return false;
	else if (m_data)
		return !strnicmp (m_data, (const char*) GetData (),
			m_data.GetLength () + 1);
	else
		return true;
}



/* ChessEngine */

ChessEngine::ChessEngine (const std::string& executable)
	: ein_buf (NULL), ein (NULL), ein_h (NULL),
	  eout_buf (NULL), eout (NULL),
	  difficulty (DIFF_NORMAL)
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
	//FIXME Set difficulty-related options.
}

void
ChessEngine::SetOpeningsBook (const std::string& book_file)
{
	WriteCommand ("setoption name OwnBook value true");
	WriteCommand ("setoption name BookFile value " + book_file);
}

void
ChessEngine::ClearOpeningsBook ()
{
	WriteCommand ("setoption name OwnBook value false");
}

void
ChessEngine::StartGame (const chess::Board* board)
{
	WaitUntilReady ();
	WriteCommand ("ucinewgame");
	if (board)
		WriteCommand ("position fen " + board->GetFEN ());
	else
		WriteCommand ("position startpos");
}

void
ChessEngine::RecordMove (const chess::Move& move)
{
	WaitUntilReady ();
	WriteCommand ("position moves " + move.GetUCICode ());
}

uint
ChessEngine::StartCalculation ()
{
	static const uint comp_time[3] = { 500, 1000, 1500 }; //FIXME Adjust.
	static const uint depth[3] = { 1, 4, 9 }; //FIXME Adjust.

	std::stringstream go_command;
	go_command << "go depth " << depth[difficulty]
		<< " movetime " << comp_time[difficulty];

	WaitUntilReady ();
	WriteCommand (go_command.str ());

	return comp_time[difficulty];
}

void
ChessEngine::StopCalculation ()
{
	WaitUntilReady ();
	WriteCommand ("stop");
}

const std::string&
ChessEngine::PeekBestMove () const
{
	return best_move;
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
		0, NULL, NULL, &start_info, &proc_info));

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
	uint wait_count = 0;

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
		full_reply.erase (0, pos);
		pos = full_reply.find_first_of (" \t");

		if (last_reply.compare ("id") == 0)
		{
			std::string field = full_reply.substr (0, pos);
			full_reply.erase (0, pos);

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

		//FIXME Handle any other reply types?
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



/* ChessGame */

cScr_ChessGame::cScr_ChessGame (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  SCRIPT_VAROBJ (ChessGame, fen, iHostObjId),
	  SCRIPT_VAROBJ (ChessGame, state_data, iHostObjId),
	  SCRIPT_VAROBJ (ChessGame, log_text, iHostObjId),
	  board (NULL), engine (NULL),
	  play_state (STATE_ANIMATING)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	if (fen.Valid () && state_data.Valid ()) // game exists
	{
		board = new chess::Board ((const char*) fen, state_data);
		if (board->GetActiveSide () == chess::SIDE_WHITE)
		{
			play_state = STATE_INTERACTIVE;
			//FIXME Anything else?
		}
		else // SIDE_BLACK
		{
			play_state = STATE_COMPUTING;
			SetTimedMessage ("BeginComputing", 10, kSTM_OneShot);
		}
	}
	else // new game
	{
		board = new chess::Board ();
		fen = board->GetFEN ().data ();
		state_data = board->GetStateData ();
		play_state = STATE_INTERACTIVE;
	}

	log_text.Init (chess::Translate ("log_heading").data ());

	try
	{
		SService<IEngineSrv> pES (g_pScriptManager);
		cScrStr path;

		if (!pES->FindFileInPath ("script_module_path", "fruit.exe", path))
			throw std::runtime_error ("could not find chess engine");
		engine = new ChessEngine ((const char*) path);

		if (pES->FindFileInPath ("script_module_path", "openings.dat", path))
			engine->SetOpeningsBook ((const char*) path); //FIXME Confirm that the book actually gets loaded.
		else
			engine->SetOpeningsBook (NULL);

		SService<IQuestSrv> pQS (g_pScriptManager);
		engine->SetDifficulty
			((ChessEngine::Difficulty) pQS->Get ("difficulty"));

		engine->StartGame (board);

		SetTimedMessage ("EngineCheck", 250, kSTM_Periodic);
	}
	catch (...)
	{
		engine = NULL;
		play_state = STATE_INTERACTIVE; // can't compute
		SetTimedMessage ("EngineFailure", 10, kSTM_OneShot);
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
	if (board)
	{
		delete board;
		board = NULL;
	}
	return 0;
}

long
cScr_ChessGame::OnSim (sSimMsg* pMsg, cMultiParm&)
{
	if (pMsg->fStarting)
	{
		AnalyzeBoard ();
		UpdatePieceSelection ();
		//FIXME Play game-beginning effects.
	}
	return 0;
}

long
cScr_ChessGame::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "SelectPiece"))
		SelectPiece (pMsg->from);

	else if (!strcmp (pMsg->message, "SelectMove"))
		SelectMove (pMsg->from);

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessGame::OnTurnOn (sScrMsg* pMsg, cMultiParm&)
{
	if (!strcmp ((const char*) ObjectToStr (pMsg->from), "TheLogbook"))
		ShowLogbook ();
	return 0;
}

long
cScr_ChessGame::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (engine)
		try
		{
			if (!strcmp (pMsg->name, "BeginComputing"))
				BeginComputerMove ();

			bool stop_calc = !strcmp (pMsg->name, "StopCalculation");
			if (stop_calc)
				engine->StopCalculation ();

			if (stop_calc || !strcmp (pMsg->name, "EngineCheck"))
			{
				engine->WaitUntilReady ();
				if (play_state == STATE_COMPUTING)
					FinishComputerMove ();
			}
		}
		catch (...)
		{
			try { delete engine; } catch (...) {}
			engine = NULL;
			//FIXME Engine's suddenly dead. How to proceed from here? Blackbrook resigns?
		}

	if (!strcmp (pMsg->name, "EngineFailure"))
	{
		// inform the player that both sides will be interactive
		SService<IDarkUISrv> pDUIS (g_pScriptManager);
		pDUIS->ReadBook ("engine-problem", "parch");
	}

	return cBaseScript::OnTimer (pMsg, mpReply);
}

object
cScr_ChessGame::GetSquare (const chess::Square& square)
{
	std::string square_name = "Square" + square.GetCode ();
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
cScr_ChessGame::GetPieceAt (const chess::Square& _square)
{
	object square = GetSquare (_square);
	return square
		? LinkIter (square, 0, "Population").Destination ()
		: object ();
}

void
cScr_ChessGame::AnalyzeBoard ()
{
	// erase old possible-move links
	SInterface<ITraitManager> pTM (g_pScriptManager);
	object archetype = StrToObject ("ChessSquare");
	for (LinkIter old_move (0, 0, "Route"); old_move; ++old_move)
		if (pTM->ObjHasDonor (old_move.Destination (), archetype))
			DestroyLink (old_move);

	if (!board) return;

	chess::GameState state = board->GetGameState ();
	if (state != chess::GAME_ONGOING)
	{
		//FIXME Proceed to endgame.
	}

	if (board->IsInCheck (board->GetActiveSide ()))
	{
		//FIXME Inform of check. (Inform for both sides?)
	}

	// create new possible-move links
	for (auto& move : board->GetPossibleMoves ())
		if (auto pmove = dynamic_cast<chess::PieceMove*> (&*move))
		{
			object piece = GetPieceAt (pmove->GetFrom ()),
				destination = GetSquare (pmove->GetTo ());
			if (piece && destination)
				CreateLink ("Route", piece, destination);
		}
}

void
cScr_ChessGame::UpdatePieceSelection ()
{
	SInterface<ITraitManager> pTM (g_pScriptManager);
	object archetype = StrToObject ("ChessSquare");
	for (LinkIter pieces (0, 0, "Population"); pieces; ++pieces)
		if (pTM->ObjHasDonor (pieces.Source (), archetype))
		{
			object piece = pieces.Destination ();
			bool can_move = (play_state == STATE_INTERACTIVE)
				&& LinkIter (piece, 0, "Route");
			SimpleSend (ObjId (), piece,
				can_move ? "EnableMove" : "DisableMove");
		}
}

void
cScr_ChessGame::SelectPiece (object piece)
{
	if (play_state != STATE_INTERACTIVE) return;
	ClearSelection ();
	CreateLink ("ScriptParams", ObjId (), piece, "SelectedPiece");
	SimpleSend (ObjId (), piece, "Select");
}

void
cScr_ChessGame::ClearSelection ()
{
	SService<IPropertySrv> pPS (g_pScriptManager);

	for (ScriptParamsIter old (ObjId (), "SelectedPiece"); old; ++old)
	{
		SimpleSend (ObjId (), old.Destination (), "Deselect");
		DestroyLink (old);
	}
}

void
cScr_ChessGame::SelectMove (object destination)
{
	if (play_state != STATE_INTERACTIVE) return;

	object piece = GetOneLinkByDataDest
		("ScriptParams", ObjId (), "SelectedPiece", -1);
	object origin = LinkIter (0, piece, "Population").Source ();
	if (!piece || !origin || !destination) return;

	ClearSelection ();

	chess::PieceMove* move = NULL;
	//FIXME Look up the move by the origin and destination.
	if (!move) return; //FIXME More?

	//FIXME Catch and handle exceptions.
	if (engine)
		engine->RecordMove (*move);

	PerformMove (*move);
}

void
cScr_ChessGame::BeginComputerMove ()
{
	//FIXME Don't fail silently. Catch and handle exceptions.
	if (!engine) return;

	play_state = STATE_COMPUTING;
	UpdatePieceSelection ();

	uint comp_time = engine->StartCalculation ();

	// schedule a special extra check of the engine
	SetTimedMessage ("StopCalculation", comp_time, kSTM_OneShot);

	//FIXME Play thinking effects.
}

void
cScr_ChessGame::FinishComputerMove ()
{
	if (play_state != STATE_COMPUTING || !engine ||
			engine->PeekBestMove ().empty ())
		return;

	std::string move_code = engine->TakeBestMove ();
	//FIXME Identify computer resignation and proceed accordingly.

	chess::PieceMove* move = NULL;
	//FIXME Look up the move by its code.
	if (!move) return; //FIXME More?

	PerformMove (*move);
}

void
cScr_ChessGame::PerformMove (const chess::PieceMove& move)
{
	if (!board) return;
	//FIXME Handle non-PieceMoves somewhere.

	play_state = STATE_ANIMATING;
	UpdatePieceSelection ();

	std::stringstream _log_text;
	_log_text << (const char*) log_text;
	_log_text << board->GetHalfmoveName () << ". ";
	_log_text << move.GetDescription () << std::endl;
	log_text = _log_text.str ().data ();

	board->MakeMove (move); //FIXME handle exceptions
	fen = board->GetFEN ().data ();
	state_data = board->GetStateData ();

	AnalyzeBoard ();

	object piece = GetPieceAt (move.GetFrom ()),
		from = GetSquare (move.GetFrom ()),
		to = GetSquare (move.GetTo ());
	if (!piece || !from || !to) return; //FIXME !?!?!?

	if (auto capture = dynamic_cast<const chess::Capture*> (&move))
	{
		object captured =
			GetPieceAt (capture->GetCapturedSquare ());
		if (!captured) return; //FIXME !?!?!?

		for (LinkIter pop (0, captured, "Population"); pop; ++pop)
			DestroyLink (pop);

		//FIXME Have piece attack captured. Delay below until after.

		SService<IActReactSrv> pARS (g_pScriptManager);
		pARS->ARStimulate (captured,
			StrToObject ("FireStim"), 100.0, piece);
	}

	DestroyLink (LinkIter (from, piece, "Population"));
	CreateLink ("Population", to, piece);

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	true_bool result;
	pAIS->MakeGotoObjLoc (result, piece, to, kFastSpeed,
		kHighPriorityAction, cMultiParm::Undef);
	//FIXME Face appropriate direction per piece.GetFacing ().

	if (auto castling = dynamic_cast<const chess::Castling*> (&move))
	{
		object rook = GetPieceAt (castling->GetRookFrom ()),
			rook_from = GetSquare (castling->GetRookFrom ()),
			rook_to = GetSquare (castling->GetRookTo ());
		//FIXME Update and move the rook. Special effect?
	}

	if (false) //FIXME identify promotion
	{
		//FIXME Replace the piece when it arrives, with special effect.
	}

	//FIXME Delay below until after effects play.
	if (engine && board->GetActiveSide () == chess::SIDE_BLACK)
		BeginComputerMove ();
	else
	{
		play_state = STATE_INTERACTIVE;
		UpdatePieceSelection ();
	}
}

void
cScr_ChessGame::ShowLogbook ()
{
	SService<IEngineSrv> pES (g_pScriptManager);
	cScrStr path;

	if (pES->FindFileInPath ("resname_base", "books\\logbook.str", path))
		try
		{
			std::ofstream logbook (path);
			logbook << "page_0: \"" << log_text << "\"" << std::endl;
		}
		catch (...) {} // the default logbook.str is an error message

	SService<IDarkUISrv> pDUIS (g_pScriptManager);
	pDUIS->ReadBook ("logbook", "pbook");
}



/* ChessSquare */

cScr_ChessSquare::cScr_ChessSquare (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId)
{}

long
cScr_ChessSquare::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "EnableMove"))
		AddSingleMetaProperty ("M-PossibleMove", ObjId ());

	else if (!strcmp (pMsg->message, "DisableMove"))
		RemoveSingleMetaProperty ("M-PossibleMove", ObjId ());

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessSquare::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	PlaySchemaAmbient (ObjId (), StrToObject ("bowtwang_player"));
	SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectMove");
	return 0;
}



/* ChessPiece */

cScr_ChessPiece::cScr_ChessPiece (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  cBaseAIScript (pszName, iHostObjId)
{}

long
cScr_ChessPiece::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "EnableMove"))
		AddSingleMetaProperty ("M-MovablePiece", ObjId ());

	else if (!strcmp (pMsg->message, "DisableMove"))
		RemoveSingleMetaProperty ("M-MovablePiece", ObjId ());

	else if (!strcmp (pMsg->message, "Select"))
		Select ();

	else if (!strcmp (pMsg->message, "Deselect"))
		Deselect ();

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessPiece::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "RestInPeace"))
		RestInPeace ();

	return cBaseScript::OnTimer (pMsg, mpReply);
}

long
cScr_ChessPiece::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	PlaySchemaAmbient (ObjId (), StrToObject ("bow_begin"));
	SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectPiece");
	return 0;
}

long
cScr_ChessPiece::OnAIModeChange (sAIModeChangeMsg* pMsg, cMultiParm&)
{
	if (pMsg->mode == kAIM_Dead) // killed or knocked out
		//FIXME Become non-physical to avoid blocking new occupant?
		SetTimedMessage ("RestInPeace", 1000, kSTM_OneShot);
	return 0;
}

void
cScr_ChessPiece::Select ()
{
	AddSingleMetaProperty ("M-SelectedPiece", ObjId ());

	SService<IPropertySrv> pPS (g_pScriptManager);
	pPS->Add (ObjId (), "SelfLit"); // dynamic light

	for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
		SimpleSend (ObjId (), move.Destination (), "EnableMove");
}

void
cScr_ChessPiece::Deselect ()
{
	RemoveSingleMetaProperty ("M-SelectedPiece", ObjId ());

	SService<IPropertySrv> pPS (g_pScriptManager);
	pPS->Remove (ObjId (), "SelfLit"); // dynamic light

	for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
		SimpleSend (ObjId (), move.Destination (), "DisableMove");
}

void
cScr_ChessPiece::RestInPeace ()
{
	//FIXME Instead, teleport to the appropriate graveyard with a puff of smoke.
	SService<IObjectSrv> pOS (g_pScriptManager);
	pOS->Destroy (ObjId ());
}

