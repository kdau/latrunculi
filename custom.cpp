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

void
DebugMessage (const std::string& message)
{
	g_pfnMPrintf (message.data ());
}

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
	if (!source)
		throw std::invalid_argument ("invalid source object");
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
ChessEngine::StartGame (const chess::Position* position)
{
	WaitUntilReady ();
	WriteCommand ("ucinewgame");
	if (position)
	{
		std::ostringstream fen;
		fen << "position fen ";
		position->Serialize (fen);
		WriteCommand (fen.str ());
	}
	else
		WriteCommand ("position startpos");
}

void
ChessEngine::RecordMove (const chess::Move& move)
{
	WaitUntilReady ();
	WriteCommand ("position moves " + move.GetUCICode ());
}

unsigned
ChessEngine::StartCalculation ()
{
	static const unsigned comp_time[3] = { 500, 1000, 1500 }; //FIXME Adjust.
	static const unsigned depth[3] = { 1, 4, 9 }; //FIXME Adjust.

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
	  SCRIPT_VAROBJ (ChessGame, record, iHostObjId),
	  game (NULL), engine (NULL),
	  interactive (false), computing (false)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	if (record.Valid ()) // game exists
	{
		//FIXME Handle exceptions from record parse.
		std::istringstream _record ((const char*) record);
		game = new chess::Game (_record);

		if (game->GetActiveSide () == chess::SIDE_WHITE)
			interactive = true;

		else // SIDE_BLACK
			SetTimedMessage ("BeginComputing", 10, kSTM_OneShot);
	}
	else // new game
	{
		game = new chess::Game ();
		UpdateRecord ();
		interactive = true;
	}

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

		engine->StartGame (game);

		SetTimedMessage ("EngineCheck", 250, kSTM_Periodic);
	}
	catch (...)
	{
		engine = NULL;
		SetTimedMessage ("EngineFailure", 10, kSTM_OneShot);
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
	if (pMsg->fStarting)
	{
		UpdateBoardObjects ();
		UpdateSquareSelection ();
		//FIXME Play game-beginning effects.
	}
	return 0;
}

long
cScr_ChessGame::OnMessage (sScrMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->message, "SelectFrom"))
		SelectFrom (pMsg->from);

	else if (!strcmp (pMsg->message, "SelectTo"))
		SelectTo (pMsg->from);

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
	try
	{
		if (engine && !strcmp (pMsg->name, "BeginComputing"))
			BeginComputerMove ();

		else if (engine && !strcmp (pMsg->name, "StopCalculation"))
			engine->StopCalculation ();
			// the next EngineCheck will pick up the move

		else if (engine && !strcmp (pMsg->name, "EngineCheck"))
		{
			engine->WaitUntilReady ();
			if (computing)
				FinishComputerMove ();
		}
	}
	catch (...) { EngineFailure (); }

	if (!strcmp (pMsg->name, "EngineFailure"))
		EngineFailure ();

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
cScr_ChessGame::GetPieceAt (const chess::Square& square)
{
	return GetPieceAt (GetSquare (square));
}

object
cScr_ChessGame::GetPieceAt (object square)
{
	return square
		? LinkIter (square, 0, "Population").Destination ()
		: object ();
}

void
cScr_ChessGame::UpdateRecord ()
{
	if (game)
	{
		std::ostringstream _record;
		game->Serialize (_record);
		record = _record.str ().data ();
	}
}

void
cScr_ChessGame::UpdateBoardObjects ()
{
	// erase old possible-move links (all Route links in mission are ours)
	for (LinkIter old_move (0, 0, "Route"); old_move; ++old_move)
		DestroyLink (old_move);

	if (!game) return;

	if (game->GetResult () != chess::Game::ONGOING)
	{
		//FIXME Proceed to endgame.
	}

	if (game->IsInCheck ())
	{
		//FIXME Inform of check.
	}

	// create new possible-move links
	for (auto& move : game->GetPossibleMoves ())
	{
		if (!move) continue;
		object from = GetSquare (move->GetFrom ()),
			to = GetSquare (move->GetTo ());
		if (!from || !to) continue; //FIXME ?!?!?!
		CreateLink ("Route", from, to);
	}
}

void
cScr_ChessGame::UpdateSquareSelection ()
{
	ClearSelection ();

	for (auto _square = chess::Square::BEGIN; _square != chess::Square::END;
		++_square)
	{
		object square = GetSquare (_square);
		if (!square) continue; //FIXME ?!?!?!
		bool can_move = interactive && LinkIter (square, 0, "Route");
		SimpleSend (ObjId (), square,
			can_move ? "EnableFrom" : "Disable");
	}
}

void
cScr_ChessGame::SelectFrom (object from)
{
	if (!interactive) return;
	ClearSelection ();
	CreateLink ("ScriptParams", ObjId (), from, "SelectedSquare");
	SimpleSend (ObjId (), from, "Select");
}

void
cScr_ChessGame::SelectTo (object _to)
{
	if (!interactive) return;

	object _from = ScriptParamsIter (ObjId (), "SelectedSquare");
	ClearSelection ();

	chess::Square from = GetSquare (_from), to = GetSquare (_to);
	if (!from.Valid () || !to.Valid () || !game) return; //FIXME ?!?!?!

	auto move = game->FindPossibleMove (from, to);
	if (!move) return; //FIXME ?!?!?!

	try { if (engine) engine->RecordMove (*move); }
	catch (...) { EngineFailure (); }

	PerformMove (move);
}

void
cScr_ChessGame::ClearSelection ()
{
	SService<IPropertySrv> pPS (g_pScriptManager);

	for (ScriptParamsIter old (ObjId (), "SelectedSquare"); old; ++old)
	{
		SimpleSend (ObjId (), old.Destination (), "Deselect");
		DestroyLink (old);
	}
}

void
cScr_ChessGame::BeginComputerMove ()
{
	interactive = false;
	computing = true;
	UpdateSquareSelection ();

	try
	{
		if (!engine) throw std::runtime_error ("no engine");

		unsigned comp_time = engine->StartCalculation ();

		// schedule a special extra check of the engine
		SetTimedMessage ("StopCalculation", comp_time, kSTM_OneShot);
	}
	catch (...) { EngineFailure (); return; }

	//FIXME Play thinking effects.
	if (game->IsInCheck ())
	{
		//FIXME Note the opponent's check.
	}
}

void
cScr_ChessGame::FinishComputerMove ()
{
	//FIXME Call EngineFailure from here when appropriate.
	if (!computing || !engine || !game || engine->PeekBestMove ().empty ())
		return;

	std::string move_code = engine->TakeBestMove ();
	//FIXME Identify computer resignation and proceed accordingly. (Is this even possible?)

	auto move = game->FindPossibleMove (move_code);
	if (!move) return; //FIXME ?!?!?!

	PerformMove (move);
}

void
cScr_ChessGame::EngineFailure ()
{
	if (engine)
	{
		try { delete engine; } catch (...) {}
		engine = NULL;
	}

	interactive = true;
	computing = false;

	// inform the player that both sides will be interactive
	SService<IDarkUISrv> pDUIS (g_pScriptManager);
	pDUIS->ReadBook ("engine-problem", "parch");

	UpdateSquareSelection ();
}

void
cScr_ChessGame::PerformMove (const chess::MovePtr& move)
{
	if (!game || !move) return;
	interactive = false;
	UpdateSquareSelection ();

	//FIXME Handle these in cScr_ChessPiece.
	SService<IAIScrSrv> pAIS (g_pScriptManager);
	true_bool result;

	game->MakeMove (move); //FIXME handle exceptions
	UpdateRecord ();

	object piece = GetPieceAt (move->GetFrom ()),
		from = GetSquare (move->GetFrom ()),
		to = GetSquare (move->GetTo ());
	if (!piece || !from || !to) return; //FIXME !?!?!?

	if (auto capture =
		std::dynamic_pointer_cast<const chess::Capture> (move))
	{
		object captured =
			GetPieceAt (capture->GetCapturedSquare ());
		if (!captured) return; //FIXME !?!?!?

		for (LinkIter pop (0, captured, "Population"); pop; ++pop)
			DestroyLink (pop);

		//FIXME Have @piece attack @captured. Delay below until after.

		SService<IActReactSrv> pARS (g_pScriptManager);
		pARS->ARStimulate (captured,
			StrToObject ("FireStim"), 100.0, piece);
	}

	DestroyLink (LinkIter (from, piece, "Population"));
	CreateLink ("Population", to, piece);

	eAIScriptSpeed speed = (move->GetPiece ().type == chess::Piece::KING)
		? kNormalSpeed : kFastSpeed; //FIXME Other variations based on piece and/or movement.
	pAIS->MakeGotoObjLoc (result, piece, to, speed,
		kHighPriorityAction, cMultiParm::Undef);
	//FIXME Face appropriate direction per GetFacing ().

	if (auto castling =
		std::dynamic_pointer_cast<const chess::Castling> (move))
	{
		object rook = GetPieceAt (castling->GetRookFrom ()),
			rook_from = GetSquare (castling->GetRookFrom ()),
			rook_to = GetSquare (castling->GetRookTo ());

		DestroyLink (LinkIter (rook_from, rook, "Population"));
		CreateLink ("Population", rook_to, rook);

		pAIS->MakeGotoObjLoc (result, rook, rook_to, speed,
			kHighPriorityAction, cMultiParm::Undef);
		//FIXME Can the rook bow as it passes the king?
		//FIXME Face appropriate direction per GetFacing ().
	}

	if (move->GetPromotion ().Valid ())
	{
		//FIXME DestroyLink (LinkIter (to, piece, "Population"));
		//FIXME Create the promoted piece and populate @to with it.
		//FIXME Destroy the old piece and reveal the new on arrival, with a special effect.
	}

	UpdateBoardObjects ();

	//FIXME Delay below until after effects play.
	if (game->GetResult () != chess::Game::ONGOING)
		{} // UpdateBoardObjects will proceed from here
	else if (engine && game->GetActiveSide () == chess::SIDE_BLACK)
		BeginComputerMove ();
	else
	{
		interactive = true;
		UpdateSquareSelection ();
	}
}

void
cScr_ChessGame::ShowLogbook ()
{
	SService<IEngineSrv> pES (g_pScriptManager);
	cScrStr path;

	if (game && pES->FindFileInPath
	    ("resname_base", "books\\logbook.str", path))
		try
		{
			std::ofstream logbook (path);
			logbook << "page_0: \"";
			game->WriteLogbook (logbook);
			logbook << "\"" << std::endl;
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
	SService<IPropertySrv> pPS (g_pScriptManager);

	if (!strcmp (pMsg->message, "EnableFrom"))
	{
		//FIXME Rotate appropriately.
		AddSingleMetaProperty ("M-PossibleFrom", ObjId ());
	}

	else if (!strcmp (pMsg->message, "EnableTo"))
	{
		//FIXME Rotate appropriately.
		pPS->Add (ObjId (), "Corona"); //FIXME Not appearing.
		AddSingleMetaProperty ("M-PossibleTo", ObjId ());
	}

	else if (!strcmp (pMsg->message, "Disable"))
	{
		if (HasMetaProperty ("M-PossibleTo", ObjId ()))
		{
			pPS->Remove (ObjId (), "Corona");
			RemoveSingleMetaProperty ("M-PossibleTo", ObjId ());
		}
		else
			RemoveSingleMetaProperty ("M-PossibleFrom", ObjId ());
	}

	else if (!strcmp (pMsg->message, "Select"))
	{
		pPS->Add (ObjId (), "Corona"); //FIXME Not appearing.
		for (LinkIter piece (ObjId (), 0, "Population"); piece; ++piece)
			SimpleSend (ObjId (), piece.Destination (), "Select");
		for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
			SimpleSend (ObjId (), move.Destination (), "EnableTo");
	}

	else if (!strcmp (pMsg->message, "Deselect"))
	{
		pPS->Remove (ObjId (), "Corona");
		for (LinkIter piece (ObjId (), 0, "Population"); piece; ++piece)
			SimpleSend (ObjId (), piece.Destination (), "Deselect");
		for (LinkIter move (ObjId (), 0, "Route"); move; ++move)
			SimpleSend (ObjId (), move.Destination (), "Disable");
	}

	return cBaseScript::OnMessage (pMsg, mpReply);
}

long
cScr_ChessSquare::OnFrobWorldEnd (sFrobMsg*, cMultiParm&)
{
	if (HasMetaProperty ("M-PossibleFrom", ObjId ()))
	{
		PlaySchemaAmbient (ObjId (), StrToObject ("bow_begin"));
		SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectFrom");
	}

	else if (HasMetaProperty ("M-PossibleTo", ObjId ()))
	{
		PlaySchemaAmbient (ObjId (), StrToObject ("bowtwang_player"));
		SimpleSend (ObjId (), StrToObject ("TheGame"), "SelectTo");
	}

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
	SService<IPropertySrv> pPS (g_pScriptManager);

	if (!strcmp (pMsg->message, "Select"))
	{
		AddSingleMetaProperty ("M-SelectedPiece", ObjId ());
		pPS->Add (ObjId (), "SelfLit"); // dynamic light
	}

	else if (!strcmp (pMsg->message, "Deselect"))
	{
		RemoveSingleMetaProperty ("M-SelectedPiece", ObjId ());
		pPS->Remove (ObjId (), "SelfLit"); // dynamic light
	}

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
cScr_ChessPiece::OnAIModeChange (sAIModeChangeMsg* pMsg, cMultiParm&)
{
	if (pMsg->mode == kAIM_Dead) // killed or knocked out
		//FIXME Become non-physical to avoid blocking new occupant?
		SetTimedMessage ("RestInPeace", 1000, kSTM_OneShot);
	return 0;
}

void
cScr_ChessPiece::RestInPeace ()
{
	//FIXME Instead, teleport to the appropriate graveyard with a puff of smoke.
	SService<IObjectSrv> pOS (g_pScriptManager);
	pOS->Destroy (ObjId ());
}

