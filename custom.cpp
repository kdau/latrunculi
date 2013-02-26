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
#include <unistd.h>

#include <ScriptLib.h>

#include "ScriptModule.h"
#include "utils.h"
#include "custom.h"



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

#define ENGINE_BUFSIZE 4096

// prototype for main loop of GNU Chess engine
namespace engine { void main_engine (int pipefd_a2e_0, int pipefd_e2a_1); }

ChessEngine::ChessEngine ()
	: engine_thread (0), output_pipe (NULL), input_pipe (NULL),
	  difficulty (DIFF_NORMAL)
{
	memset (best_move, 0, sizeof (best_move));

	HANDLE read, write;
	int pipefd[2] = { 0 };

	if (!CreatePipe (&read, &write, NULL, 0))
		throw std::runtime_error ("could not create engine output pipe");
	pipefd[0] = _open_osfhandle ((intptr_t) read, _O_RDONLY);
	output_pipe = _fdopen (_open_osfhandle ((intptr_t) write, _O_APPEND), "a");
	if (pipefd[0] == -1 || output_pipe == NULL)
		throw std::runtime_error ("could not connect engine output pipe");
	setvbuf (output_pipe, NULL, _IONBF, 0);

	if (!CreatePipe (&read, &write, NULL, 0))
		throw std::runtime_error ("could not create engine input pipe");
	pipefd[1] = _open_osfhandle ((intptr_t) write, _O_APPEND);
	input_pipe = _fdopen (_open_osfhandle ((intptr_t) read, _O_RDONLY), "r");
	if (pipefd[1] == -1 || input_pipe == NULL)
		throw std::runtime_error ("could not connect engine input pipe");
	setvbuf (input_pipe, NULL, _IONBF, 0);

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
	try
	{
		if (engine_thread > 0 && output_pipe)
			WriteCommand ("quit");
	}
	catch (...) {}

	if (input_pipe) fclose (input_pipe);
	if (output_pipe) fclose (output_pipe);
}

void
ChessEngine::SetDifficulty (Difficulty _difficulty)
{
	difficulty = _difficulty;
	//FIXME Set difficulty-related options.
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
ChessEngine::RecordMove (const char* move)
{
	char move_command[ENGINE_BUFSIZE];
	snprintf (move_command, ENGINE_BUFSIZE,
		"position moves %s", move);
	WriteCommand (move_command);
}

uint
ChessEngine::BeginCalculation ()
{
	static const uint comp_time[3] = { 500, 1000, 1500 }; //FIXME Adjust.
	static const uint depth[3] = { 1, 4, 9 }; //FIXME Adjust.

	char go_command[ENGINE_BUFSIZE];
	snprintf (go_command, ENGINE_BUFSIZE, "go depth %u movetime %u",
		depth[difficulty], comp_time[difficulty]);
	WriteCommand (go_command);
	return comp_time[difficulty];
}

const char*
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

bool
ChessEngine::ReadReply (const char* desired_reply)
{
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
			g_pfnMPrintf ("INFO: The chess engine is %s.\n", value);
		else if (!strcmp (field, "author"))
			g_pfnMPrintf ("INFO: The chess engine was written by %s.\n", value);
	}
	else if (!strcmp (reply, "bestmove"))
	{
		char* move = strsep (&remainder, " \t\n");
		if (move)
			strncpy (best_move, move, sizeof (best_move));
		else
			memset (best_move, 0, sizeof (best_move));
	}
	//FIXME Handle any other replies?

	return !desired_reply || !strcmp (reply, desired_reply);
}

void
ChessEngine::ReadReplies (const char* desired_reply)
{
	while (!ReadReply (desired_reply));
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



/* ChessBoard::Square */

ChessBoard::Square::Square (File _file, Rank _rank)
	: file (_file), rank (_rank)
{}

ChessBoard::Square::Square (const char* position)
	: file (FILE_NONE), rank (RANK_NONE)
{
	if (strlen (position) >= 2)
	{
		file = (File) tolower (position[0]);
		rank = (Rank) position[1];
	}
}

ChessBoard::Square::Square (object square_marker)
	: file (FILE_NONE), rank (RANK_NONE)
{
	cAnsiStr square_name = ObjectToStr (square_marker);
	if (square_name.GetLength () == 8 &&
		!strncmp (square_name, "Square", 6))
	{
		file = (File) tolower (square_name.GetAt (6));
		rank = (Rank) square_name.GetAt (7);
	}
}

ChessBoard::Square::operator bool () const
{
	return file >= FILE_a && file <= FILE_h &&
		rank >= RANK_1 && rank <= RANK_8;
}

ChessBoard::Square
ChessBoard::Square::Offset (Square by) const
{
	Square result;
	result.file = File (file + by.file);
	result.rank = Rank (rank + by.rank);
	return result ? result : Square ();
}



/* ChessBoard::Piece */

#define BLACK_DELTA ('a' - 'A')

ChessBoard::Camp
ChessBoard::Enemy (Camp of)
{
	switch (of)
	{
		case CAMP_WHITE: return CAMP_BLACK;
		case CAMP_BLACK: return CAMP_WHITE;
		default: return CAMP_NONE;
	}
}

ChessBoard::Piece::Piece (Camp _camp, PieceType _type)
	: camp (_camp), type (_type)
{}

ChessBoard::Piece::Piece (char code)
	: camp (CAMP_NONE), type (PIECE_NONE)
{
	char code2 = code - BLACK_DELTA;
	if (code == PIECE_KING || code == PIECE_QUEEN ||
		code == PIECE_ROOK || code == PIECE_BISHOP ||
		code == PIECE_KNIGHT || code == PIECE_PAWN)
	{
		camp = CAMP_WHITE;
		type = (PieceType) code;
	}
	else if (code2 == PIECE_KING || code2 == PIECE_QUEEN ||
		code2 == PIECE_ROOK || code2 == PIECE_BISHOP ||
		code2 == PIECE_KNIGHT || code2 == PIECE_PAWN)
	{
		camp = CAMP_BLACK;
		type = (PieceType) code2;
	}
}

ChessBoard::Piece::operator bool () const
{
	return camp != CAMP_NONE && type != PIECE_NONE;
}

char
ChessBoard::Piece::Code () const
{
	if (camp == CAMP_WHITE)
		return type;
	else if (camp == CAMP_BLACK)
		return type + BLACK_DELTA;
	else
		return PIECE_NONE;
}



/* ChessBoard */

const char ChessBoard::INITIAL_PLACEMENT[BOARD_SIZE] =
	"RNBQKBNRPPPPPPPP                                pppppppprnbqkbnr";

ChessBoard::ChessBoard ()
	: active_camp (CAMP_WHITE),
	  castling_white (CASTLE_KINGSIDE | CASTLE_QUEENSIDE),
	  castling_black (CASTLE_KINGSIDE | CASTLE_QUEENSIDE),
	  en_passant_square (FILE_NONE, RANK_NONE),
	  halfmove_clock (0),
	  fullmove_number (1)
{
	strncpy (piece_placement, INITIAL_PLACEMENT, sizeof (piece_placement));
}

ChessBoard::Piece
ChessBoard::GetAt (const Square& square) const
{
	if (!square) return '\0';
	return Piece (piece_placement
		[(square.rank - RANK_1) * 8 + (square.file - FILE_a)]);
}

void
ChessBoard::SetAt (const Square& square, const Piece& piece)
{
	if (!square) return;
	piece_placement [(square.rank - RANK_1) * 8 + (square.file - FILE_a)]
		= piece.Code ();
}

ChessBoard::Square
ChessBoard::FindPiece (const Piece& piece, const Square& last) const
{
	return Square (); //FIXME Find the piece.
}

void
ChessBoard::EncodeFEN (char* buffer)
{
	if (!buffer) return;
	char* ptr;

	char board_fen[FEN_BUFSIZE]; ptr = board_fen;
	//FIXME Read off board into board_fen in FEN format.

	char castling[5]; ptr = castling;
	if (castling_white & CASTLE_KINGSIDE) *ptr++ = 'K';
	if (castling_white & CASTLE_QUEENSIDE) *ptr++ = 'Q';
	if (castling_black & CASTLE_KINGSIDE) *ptr++ = 'k';
	if (castling_black & CASTLE_QUEENSIDE) *ptr++ = 'q';
	if (ptr == castling) *ptr++ = '-';
	*ptr = '\0';

	char en_passant[3] = "-\0";
	if (en_passant_square)
	{
		en_passant[0] = en_passant_square.file;
		en_passant[1] = en_passant_square.rank;
	}

	snprintf (buffer, FEN_BUFSIZE, "%s %c %s %s %u %u", board_fen,
		(char) active_camp, castling, en_passant, halfmove_clock,
		fullmove_number);
}

#define load_script_int(type, name) \
	{ script_int _##name ("ChessGame", #name, store); \
	if (_##name.Valid ()) name = (type) (int) _##name; }

void
ChessBoard::Load (object store)
{
	script_str _piece_placement ("ChessGame", "piece_placement", store);
	if (_piece_placement.Valid ())
		strncpy (piece_placement, _piece_placement, BOARD_SIZE);

	load_script_int (Camp, active_camp);
	load_script_int (int, castling_white);
	load_script_int (int, castling_black);

	script_str eps ("ChessGame", "en_passant_square", store);
	if (eps.Valid ()) en_passant_square = Square (eps);

	load_script_int (uint, halfmove_clock);
	load_script_int (uint, fullmove_number);
}

#undef load_script_var
#define save_script_var(type, name) \
	{ script_##type ("ChessGame", #name, store) = name; }

void
ChessBoard::Save (object store)
{
	save_script_var (str, piece_placement);
	save_script_var (int, active_camp);
	save_script_var (int, castling_white);
	save_script_var (int, castling_black);

	char eps[3] = "\0\0";
	eps[0] = en_passant_square.file;
	eps[1] = en_passant_square.rank;
	script_str ("ChessGame", "en_passant_square", store) = eps;

	save_script_var (int, halfmove_clock);
	save_script_var (int, fullmove_number);
}

#undef save_script_var

bool
ChessBoard::IsUnderAttack (const Square& square, Camp camp) const
{
	ChessBoard test (*this);
	test.active_camp = Enemy (camp);

	MoveSet moves;
	test.EnumerateMoves (moves);

	//FIXME Iterate over moves and return true if any targets the square.
	return false;
}

bool
ChessBoard::IsInCheck (Camp camp) const
{
	Square square = FindPiece (Piece (camp, PIECE_KING));
	return square ? IsUnderAttack (square, camp) : true; //FIXME How is an infinite loop avoided?
}

void
ChessBoard::PerformMove (const Square& from, const Square& to)
{
	Piece mover = GetAt (from);
	if (!mover) return;

	SetAt (from); // remove mover from origin

	//FIXME Handle promotion.

	SetAt (to, mover); // place mover in destination

	//FIXME Handle rook portion of castling.

	//FIXME Handle en passant capture.

	active_camp = Enemy (active_camp);

	//FIXME Update en passant square.

	int& castling = (mover.camp == CAMP_WHITE)
		? castling_white : castling_black;
	if (mover.type == PIECE_KING)
		castling = CASTLE_NONE;
	else if (mover.type == PIECE_ROOK && from.rank ==
		(mover.camp == CAMP_WHITE) ? RANK_1 : RANK_8)
	{
		if (from.file == FILE_a)
			castling &= ~CASTLE_QUEENSIDE;
		else if (from.file == FILE_h)
			castling &= ~CASTLE_KINGSIDE;
	}

	//FIXME Update halfmove clock.

	if (mover.camp == CAMP_BLACK)
		++fullmove_number;
}

void
ChessBoard::EnumerateMoves (MoveSet& set) const
{
	set.clear ();
	for (char rank = RANK_1; rank <= RANK_8; ++rank)
	for (char file = FILE_a; file <= FILE_h; ++file)
	{
		Square origin ((File) file, (Rank) rank);
		EnumeratePieceMoves (set, GetAt (origin), origin);
	}
}

void
ChessBoard::EnumeratePieceMoves (MoveSet& set, const Piece& piece,
	const Square& origin) const
{
	if (piece.camp != active_camp) return;

	switch (piece.type)
	{
	case PIECE_ROOK:
	case PIECE_BISHOP:
	case PIECE_QUEEN:
		if (piece.type != PIECE_ROOK)
		{
			TryMovePath (set, piece, origin,  1,  1);
			TryMovePath (set, piece, origin, -1,  1);
			TryMovePath (set, piece, origin,  1, -1);
			TryMovePath (set, piece, origin, -1, -1);
		}
		if (piece.type != PIECE_BISHOP)
		{
			TryMovePath (set, piece, origin,  0,  1);
			TryMovePath (set, piece, origin,  0, -1);
			TryMovePath (set, piece, origin,  1,  0);
			TryMovePath (set, piece, origin, -1,  0);
		}
		break;
	case PIECE_KNIGHT:
		TryMovePoint (set, piece, origin,  1,  2);
		TryMovePoint (set, piece, origin,  2,  1);
		TryMovePoint (set, piece, origin, -1,  2);
		TryMovePoint (set, piece, origin, -2,  1);
		TryMovePoint (set, piece, origin,  1, -2);
		TryMovePoint (set, piece, origin,  2, -1);
		TryMovePoint (set, piece, origin, -1, -2);
		TryMovePoint (set, piece, origin, -2, -1);
		break;
	case PIECE_KING:
	    {
		TryMovePoint (set, piece, origin,  1,  1);
		TryMovePoint (set, piece, origin,  1,  0);
		TryMovePoint (set, piece, origin,  1, -1);
		TryMovePoint (set, piece, origin,  0, -1);
		TryMovePoint (set, piece, origin, -1, -1);
		TryMovePoint (set, piece, origin, -1,  0);
		TryMovePoint (set, piece, origin, -1,  1);
		TryMovePoint (set, piece, origin,  0,  1);

		int castling = (piece.camp == CAMP_WHITE)
			? castling_white : castling_black;
		if (IsInCheck (piece.camp)) castling = CASTLE_NONE;
		if ((castling & CASTLE_KINGSIDE))
			{} //FIXME Consider kingside castling.
		if ((castling & CASTLE_QUEENSIDE))
			{} //FIXME Consider queenside castling.

		break;
	    }
	case PIECE_PAWN:
	    {
		char dirn = (piece.camp == CAMP_WHITE) ? 1 : -1;
		char init_rank = (piece.camp == CAMP_WHITE) ? RANK_2 : RANK_7;
		bool one_okay = TryMovePoint (set, piece, origin, 0, dirn, MOVE_EMPTY);
		if (one_okay && (origin.rank == init_rank))
			TryMovePoint (set, piece, origin, 0, 2 * dirn, MOVE_EMPTY);
		TryMovePoint (set, piece, origin, -1, dirn, MOVE_CAPTURE);
		TryMovePoint (set, piece, origin,  1, dirn, MOVE_CAPTURE);
		//FIXME Consider en passant capture.
		break;
	    }
	default:
		break;
	}
}

void
ChessBoard::TryMovePath (MoveSet& set, const Piece& piece, const Square& origin,
	char file_dirn, char rank_dirn) const
{
	char file_vect = 0, rank_vect = 0;
	while (true)
	{
		file_vect += file_dirn;
		rank_vect += rank_dirn;

		if (TryMovePoint (set, piece, origin,
				file_vect, rank_vect, MOVE_CAPTURE))
			return; // piece capture required, so can't go farther

		if (!TryMovePoint (set, piece, origin,
				file_vect, rank_vect, MOVE_EMPTY))
			return; // something else in the way
	}
}

bool
ChessBoard::TryMovePoint (MoveSet& set, const Piece& piece, const Square& origin,
	char file_vect, char rank_vect, int allowed_types) const
{
	Square offset ((File) file_vect, (Rank) rank_vect);
	Square destination = origin.Offset (offset);
	if (!destination) return false; // invalid position

	MoveType type = MOVE_NONE;
	Piece occupant = GetAt (destination);
	if (occupant)
	{
		if (occupant.camp == piece.camp)
			return false; // can't capture friendly occupant
		else if (!(allowed_types & MOVE_CAPTURE))
			return false; // this can't capture hostile occupant
		else
			type = MOVE_CAPTURE;
	}
	else if (!(allowed_types & MOVE_EMPTY))
		return false; // this can't move to empty square
	else
		type = MOVE_EMPTY;

	//FIXME Filter otherwise illegal moves. Mark castlings and en passants (and promotions?).

	Move move = { origin, destination, type };
	set.push_back (move);
	return true;
}



/* ChessGame */

cScr_ChessGame::cScr_ChessGame (const char* pszName, int iHostObjId)
	: cBaseScript (pszName, iHostObjId),
	  engine (NULL), play_state (STATE_ANIMATING)
{}

long
cScr_ChessGame::OnBeginScript (sScrMsg*, cMultiParm&)
{
	//FIXME Handle any exceptions nicely.

	engine = new ChessEngine ();

	SService<IEngineSrv> pES (g_pScriptManager);
	cScrStr book;
	if (pES->FindFileInPath ("script_module_path", "openings.dat", book)) //FIXME This still isn't an absolute file name. Is that okay?
		engine->SetOpeningsBook (book); //FIXME Should the book string be freed?
	else
		engine->SetOpeningsBook (NULL);

	SService<IQuestSrv> pQS (g_pScriptManager);
	int difficulty = pQS->Get ("difficulty");
	engine->SetDifficulty ((ChessEngine::Difficulty) difficulty);

	engine->WaitUntilReady ();

	if (script_var ("ChessGame", "piece_placement", ObjId ()).Valid ())
	{ // game in progress
		board.Load (ObjId ());
		char position[FEN_BUFSIZE];
		board.EncodeFEN (position);
		engine->StartGame (position);
		if (board.active_camp == ChessBoard::CAMP_WHITE)
			play_state = STATE_INTERACTIVE; //FIXME Anything else?
		else // CAMP_BLACK
		{
			play_state = STATE_COMPUTING;
			//FIXME How to proceed?
		}
	}
	else // new game
	{
		board.Save (ObjId ());
		engine->StartGame (NULL);
		play_state = STATE_INTERACTIVE;
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
cScr_ChessGame::GetSquare (ChessBoard::Square _square)
{
	char square_name[9] = "Square\0\0";
	square_name[6] = _square.file;
	square_name[7] = _square.rank;
	return StrToObject (square_name);
}

object
cScr_ChessGame::GetPieceAt (ChessBoard::Square _square)
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

	// create new possible-move links
	ChessBoard::MoveSet moves;
	board.EnumerateMoves (moves);
	for (ChessBoard::MoveSet::iterator move = moves.begin ();
		move != moves.end (); ++move)
	{
		object piece = GetPieceAt (move->from),
			destination = GetSquare (move->to);
		if (piece && destination)
			CreateLink ("Route", piece, destination);
	}

	//FIXME Check for checks/checkmates/stalemates/etc. and react accordingly.
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

	if (engine)
	{
		ChessBoard::Square osquare (origin), dsquare (destination);
		char move[5] = "\0\0\0\0";
		move[0] = osquare.file; move[1] = osquare.rank;
		move[2] = dsquare.file; move[3] = dsquare.rank;
		engine->RecordMove (move);
	}

	PerformMove (piece, origin, destination);
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

	const char* move = engine->EndCalculation ();
	object piece = GetPieceAt (move),
		origin = GetSquare (move),
		destination = GetSquare (move + 2);

	if (!piece || !origin || !destination)
		return; //FIXME Computer has resigned. Proceed accordingly.

	PerformMove (piece, origin, destination);
}

void
cScr_ChessGame::PerformMove (object piece, object origin, object destination)
{
	play_state = STATE_ANIMATING;
	UpdatePieceSelection ();

	board.PerformMove (origin, destination);
	board.Save (ObjId ());
	AnalyzeBoard ();

	//FIXME Handle en passant attack targets.
	object occupant = GetPieceAt (destination);
	if (occupant)
	{
		DestroyLink (LinkIter (destination, occupant, "Population"));
		//FIXME Have piece attack occupant. Delay below until after.
		SService<IActReactSrv> pARS (g_pScriptManager);
		pARS->ARStimulate (occupant, StrToObject ("FireStim"), 100.0, piece);
	}

	DestroyLink (LinkIter (origin, piece, "Population"));
	CreateLink ("Population", destination, piece);

	SService<IAIScrSrv> pAIS (g_pScriptManager);
	true_bool result;
	pAIS->MakeGotoObjLoc (result, piece, destination, kFastSpeed,
		kHighPriorityAction, cMultiParm::Undef);
	//FIXME Face appropriate direction.

	//FIXME Handle castling and promotion.

	//FIXME Delay below until after effects play.
	if (engine && board.active_camp == ChessBoard::CAMP_BLACK)
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

