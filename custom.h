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
#include <vector>

#include "BaseScript.h"
#include "scriptvars.h"

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



#if !SCR_GENSCRIPTS
/**
 * Utility Class: ChessBoard
 *
 * Stores and analyzes the instantaneous state of a chess game.
 */
class ChessBoard
{
public:

// squares

	enum File
	{
		FILE_NONE = 0,
		FILE_a = 'a',
		FILE_b = 'b',
		FILE_c = 'c',
		FILE_d = 'd',
		FILE_e = 'e',
		FILE_f = 'f',
		FILE_g = 'g',
		FILE_h = 'h'
	};

	enum Rank
	{
		RANK_NONE = 0,
		RANK_1 = '1',
		RANK_2 = '2',
		RANK_3 = '3',
		RANK_4 = '4',
		RANK_5 = '5',
		RANK_6 = '6',
		RANK_7 = '7',
		RANK_8 = '8'
	};

	struct Square
	{
		Square (File _file = FILE_NONE, Rank _rank = RANK_NONE);
		operator bool () const;
		Square Offset (Square by) const;

		File file;
		Rank rank;
	};

// pieces

	enum Camp
	{
		CAMP_NONE = 0,
		CAMP_WHITE,
		CAMP_BLACK
	};

	enum PieceType
	{
		PIECE_NONE = ' ',
		PIECE_KING = 'k',
		PIECE_QUEEN = 'q',
		PIECE_ROOK = 'r',
		PIECE_BISHOP = 'b',
		PIECE_KNIGHT = 'n',
		PIECE_PAWN = 'p'
	};

	struct Piece
	{
		Piece (Camp _camp = CAMP_NONE, PieceType _type = PIECE_NONE);
		Piece (char code);
		operator char () const;
		Camp camp;
		PieceType type;
	};

// state information

	enum Castling
	{
		CASTLE_NONE = 0,
		CASTLE_KINGSIDE = 1,
		CASTLE_QUEENSIDE = 2
	};

	char piece_placement[65];
	static const char INITIAL_PLACEMENT[65];

	Camp active_camp;
	int castling_white;
	int castling_black;
	Square en_passant_square;
	uint halfmove_clock;
	uint fullmove_number;

// constructor and data access

	ChessBoard ();

	Piece operator[] (const Square& square) const;

// representations and persistence

#define FEN_BUFSIZE 128
	void EncodeFEN (char* buffer);

	void Load (object store);
	void Save (object store);

// possible moves analysis

	struct Move
	{
		Square from;
		Square to;
	};
	typedef std::vector<Move> MoveSet;

	void EnumerateMoves (MoveSet& set);

private:
	void TryMovePath (MoveSet& set, Piece piece, Square origin,
		char file_dirn, char rank_dirn);
	bool TryMovePoint (MoveSet& set, Piece piece, Square origin,
		char file_vect, char rank_vect,
		bool if_hostile = true, bool if_empty = true);
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

private:
	object GetSquare (ChessBoard::Square square);
	object GetPieceAt (ChessBoard::Square square);

	void AnalyzeBoard ();

	void SelectPiece (object piece);
	void DeselectPieces ();

	void MakeMove (object square);

	ChessEngine* engine;
	ChessBoard board;
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
	virtual long OnFrobWorldEnd (sFrobMsg* pMsg, cMultiParm& mpReply);

private:
	void Select ();
	void Deselect ();
};
#else // SCR_GENSCRIPTS
GEN_FACTORY("ChessPiece","BaseAIScript",cScr_ChessPiece)
#endif // SCR_GENSCRIPTS



#endif // CUSTOM_H

