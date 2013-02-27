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

#include <sstream>
#include <ext/stdio_filebuf.h>
#include <fcntl.h>
#include <process.h>

#include <ScriptLib.h>

#include "ScriptModule.h"
#include "utils.h"

#include "custom.h"

// avoid including windows.h
extern "C" __declspec(dllimport)
	int __stdcall CreatePipe (HANDLE*, HANDLE*, void*, unsigned long);



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

// prototype for main loop of GNU Chess engine
namespace engine { void main (int input_fd, int output_fd); }

ChessEngine::ChessEngine ()
	: ein_buf (NULL), ein (NULL),
	  eout_buf (NULL), eout (NULL),
	  engine_thread (0),
	  difficulty (DIFF_NORMAL)
{
	int pipefd[2] = { 0 }, ein_fd, eout_fd;

	SetupPipe (ein_fd, pipefd[1]);
	SetupPipe (pipefd[0], eout_fd);

	ein_buf = new __gnu_cxx::stdio_filebuf<char> (ein_fd, std::ios::in);
	ein = new std::istream (ein_buf);

	eout_buf = new __gnu_cxx::stdio_filebuf<char> (ein_fd, std::ios::out);
	eout = new std::ostream (eout_buf);

	engine_thread = _beginthread (EngineThread, 0, pipefd);
	if (engine_thread <= 0)
		throw std::runtime_error ("could not spawn engine thread");

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
	WriteCommand ("ucinewgame");
	if (board)
		WriteCommand ("position fen " + board->GetFEN ());
	else
		WriteCommand ("position startpos");
}

void
ChessEngine::RecordMove (const chess::MovePtr& move)
{
	if (move)
		WriteCommand ("position moves " + move->GetUCICode ());
}

uint
ChessEngine::BeginCalculation ()
{
	static const uint comp_time[3] = { 500, 1000, 1500 }; //FIXME Adjust.
	static const uint depth[3] = { 1, 4, 9 }; //FIXME Adjust.

	std::stringstream go_command;
	go_command << "go depth" << depth[difficulty] << " movetime "
		<< comp_time[difficulty];
	WriteCommand (go_command.str ());

	return comp_time[difficulty];
}

const std::string&
ChessEngine::EndCalculation ()
{
	WriteCommand ("stop");
	ReadReplies ("bestmove");
	return best_move;
}

void
ChessEngine::WaitUntilReady ()
{
	WriteCommand ("isready");
	ReadReplies ("readyok");
}

void
ChessEngine::ReadReplies (const std::string& desired_reply)
{
	while (!ReadReply (desired_reply));
}

bool
ChessEngine::ReadReply (const std::string& desired_reply)
{
	if (!ein) throw std::runtime_error ("no pipe from engine");
	std::string full_reply;
	std::getline (*ein, full_reply);

#ifdef DEBUG
	g_pfnMPrintf ("ChessEngine -> %s\n", full_reply.data ());
#endif

	std::size_t pos = full_reply.find_first_of (" \t");
	if (pos == std::string::npos) return false;
	std::string reply = full_reply.substr (0, pos);
	full_reply.erase (0, pos);
	pos = full_reply.find_first_of (" \t");

	if (reply.compare ("id"))
	{
		std::string field = full_reply.substr (0, pos);
		full_reply.erase (0, pos);

		if (field.compare ("name"))
			g_pfnMPrintf ("INFO: The chess engine is %s.\n",
				full_reply.data ());
		else if (field.compare ("author"))
			g_pfnMPrintf ("INFO: The chess engine was written by %s.\n",
				full_reply.data ());
	}

	else if (reply.compare ("bestmove"))
		best_move = full_reply.substr (0, pos);

	//FIXME Handle any other replies?

	return desired_reply.compare (reply);
}

void
ChessEngine::WriteCommand (const std::string& command)
{
#ifdef DEBUG
	g_pfnMPrintf ("ChessEngine <- %s\n", command.data ());
#endif
	if (!eout) throw std::runtime_error ("no pipe to engine");
	*eout << command << std::endl;
}

void
ChessEngine::SetupPipe (int& read_fd, int& write_fd)
{
	HANDLE read, write;
	if (!CreatePipe (&read, &write, NULL, 0))
		throw std::runtime_error ("could not create pipe");

	read_fd = _open_osfhandle ((intptr_t) read, _O_RDONLY);
	write_fd = _open_osfhandle ((intptr_t) write, _O_APPEND);
	if (read_fd == -1 || write_fd == -1)
		throw std::runtime_error ("could not open pipe");
}

void
ChessEngine::EngineThread (void* _pipefd)
{
	int* pipefd = reinterpret_cast<int*> (_pipefd);
	engine::main (pipefd[0], pipefd[1]);
}



/* ChessGame */

cScr_ChessGame::cScr_ChessGame (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  engine (NULL), board (NULL),
	  SCRIPT_VAROBJ (ChessGame, fen, iHostObjId),
	  SCRIPT_VAROBJ (ChessGame, state_data, iHostObjId),
	  play_state (STATE_ANIMATING)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	try
	{
		engine = new ChessEngine ();

		SService<IEngineSrv> pES (g_pScriptManager);
		cScrStr book;
		//FIXME This is relative to $PWD. Is that okay?
		if (pES->FindFileInPath ("script_module_path", "openings.dat",
				book))
			engine->SetOpeningsBook ((const char*) book);
		else
			engine->SetOpeningsBook (NULL);

		SService<IQuestSrv> pQS (g_pScriptManager);
		int difficulty = pQS->Get ("difficulty");
		engine->SetDifficulty ((ChessEngine::Difficulty) difficulty);

		engine->WaitUntilReady ();

		if (fen.Valid () && state_data.Valid ()) // game exists
		{
			board = new chess::Board
				((const char*) fen, state_data);
			if (board->GetActiveSide () == chess::SIDE_WHITE)
			{
				play_state = STATE_INTERACTIVE;
				//FIXME Anything else?
			}
			else // SIDE_BLACK
			{
				play_state = STATE_COMPUTING;
				//FIXME How to proceed?
			}
		}
		else // new game
		{
			board = new chess::Board ();
			fen = board->GetFEN ().data ();
			state_data = board->GetStateData ();
			play_state = STATE_INTERACTIVE;
		}
		engine->StartGame (board);
	}
	catch (...)
	{
		throw; //FIXME Handle any exceptions nicely.
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
	if (pMsg->fStarting && engine)
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
cScr_ChessGame::OnTimer (sScrTimerMsg* pMsg, cMultiParm& mpReply)
{
	if (!strcmp (pMsg->name, "ComputerMove"))
		FinishComputerMove ();

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
			square_name.substr (0, 6).compare ("Square"))
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
	{
		object piece = GetPieceAt (move->from),
			destination = GetSquare (move->to);
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

	chess::MovePtr move;
	//FIXME Look up the chess::Move by the origin and destination.

	if (engine)
		engine->RecordMove (move);

	PerformMove (move);
}

void
cScr_ChessGame::BeginComputerMove ()
{
	if (!engine) return;

	play_state = STATE_COMPUTING;
	UpdatePieceSelection ();

	uint comp_time = engine->BeginCalculation ();
	SetTimedMessage ("ComputerMove", comp_time, kSTM_OneShot);

	//FIXME Play thinking effects.
}

void
cScr_ChessGame::FinishComputerMove ()
{
	if (!engine) return;

	std::string move_code = engine->EndCalculation ();

	chess::MovePtr move;
	//FIXME Look up the chess::Move by its code.

	//FIXME Identify computer resignation and proceed accordingly.

	PerformMove (move);
}

void
cScr_ChessGame::PerformMove (const chess::MovePtr& move)
{
	if (!board || !move) return;

	play_state = STATE_ANIMATING;
	UpdatePieceSelection ();

	board->MakeMove (move);
	fen = board->GetFEN ().data ();
	state_data = board->GetStateData ();
	AnalyzeBoard ();

	object piece = GetPieceAt (move->from),
		from = GetSquare (move->from),
		to = GetSquare (move->to);

	if (!piece || !from || !to) return; // !?!?!?

	if (move->type & chess::MOVE_CAPTURE)
	{
		object captured = GetPieceAt (move->GetCapturedSquare ());
		if (!captured) return; // !?!?!?
		for (LinkIter pop (0, captured, "Population"); pop; ++pop)
			DestroyLink (pop);
		//FIXME Have piece attack captured. Delay below until after.
		SService<IActReactSrv> pARS (g_pScriptManager);
		pARS->ARStimulate (captured, StrToObject ("FireStim"), 100.0, piece);
	}

	DestroyLink (LinkIter (from, piece, "Population"));
	CreateLink ("Population", to, piece);

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	true_bool result;
	pAIS->MakeGotoObjLoc (result, piece, to, kFastSpeed,
		kHighPriorityAction, cMultiParm::Undef);
	//FIXME Face appropriate direction.

	//FIXME Handle castling and promotion.

	//FIXME Delay below until after effects play.
	if (engine && board->GetActiveSide () == chess::SIDE_BLACK)
		BeginComputerMove ();
	else
	{
		play_state = STATE_INTERACTIVE;
		UpdatePieceSelection ();
	}
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

