/******************************************************************************
 *  chess.h
 *
 *  Custom scripts for A Nice Game of Chess
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
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

#ifndef CHESS_H
#define CHESS_H
#if !SCR_GENSCRIPTS

#include <unordered_map>
#include <vector>

typedef unsigned int uint;

namespace chess {



/**
 ** Square (File, Rank)
 **/

enum File
{
	FILE_NONE = -1,
	FILE_A,
	FILE_B,
	FILE_C,
	FILE_D,
	FILE_E,
	FILE_F,
	FILE_G,
	FILE_H
};

enum Rank
{
	RANK_NONE = -1,
	RANK_1,
	RANK_2,
	RANK_3,
	RANK_4,
	RANK_5,
	RANK_6,
	RANK_7,
	RANK_8
};

struct Square
{
	Square (File file = FILE_NONE, Rank rank = RANK_NONE);
	Square (const char* position);

	operator bool () const;
	void GetCode (char* code) const;

	Square Offset (int delta_file, int delta_rank) const;

	File file;
	Rank rank;
};

typedef std::vector<Square> Squares;



/**
 ** Piece (Side, PieceType, PieceSquares)
 **/

#define N_SIDES 2

enum Side
{
	SIDE_NONE = -1,
	SIDE_WHITE,
	SIDE_BLACK
};

Side Opponent (Side side);

enum PieceType
{
	PIECE_NONE = -1,
	PIECE_KING,
	PIECE_QUEEN,
	PIECE_ROOK,
	PIECE_BISHOP,
	PIECE_KNIGHT,
	PIECE_PAWN
};

struct Piece
{
	Piece (Side side = SIDE_NONE, PieceType type = PIECE_NONE);
	Piece (char code);

	operator bool () const;

	char GetCode () const;
	void SetCode (char code);

	Rank GetInitialRank () const;

	Side side;
	PieceType type;
};

} // namespace chess
namespace std {

template<>
struct hash<chess::Piece>
{
	size_t
	operator () (const chess::Piece& piece) const
	{
		return std::hash<char> () (piece.GetCode ());
	}
};

} // namespace std
namespace chess {

typedef std::unordered_multimap<Piece, Square> PieceSquares;



/**
 ** Move (MoveType, Moves)
 **/

enum MoveType
{
	MOVE_NONE = 0,
	MOVE_TO_EMPTY = 1,
	MOVE_CAPTURE = 2,
	MOVE_CASTLING = 4,
	MOVE_EN_PASSANT = 8,
	MOVE_PROMOTION = 16
};

struct Move
{
	MoveType type;
	Piece piece;
	Square from;
	Square to;

	Piece captured_piece; // valid if type & MOVE_CAPTURE
	Piece comoving_rook; // valid if type & MOVE_CASTLING
	Square captured_at; // valid if type & MOVE_EN_PASSANT
	PieceType promotion; // valid if type & MOVE_PROMOTION

private:
	friend class Board;
	Move ();
};

typedef std::vector<Move> Moves;



/**
 ** Board (GameState, CastlingOptions)
 **/

#define N_RANKS 8
#define N_FILES 8
#define FEN_BUFSIZE 128

enum GameState
{
	// set automatically
	GAME_ONGOING,
	GAME_WON_CHECKMATE,
	GAME_DRAWN_STALEMATE,
	GAME_DRAWN_DEAD_POSITION, // not all positions detected

	// set manually by Resign()
	GAME_WON_RESIGNATION,

	// set manually by ExpireTime() - UI must persist and check timer(s)
	GAME_WON_TIME_CONTROL,

	// set manually by Draw() - UI must detect and/or confirm conditions
	GAME_DRAWN_AGREEMENT,
	GAME_DRAWN_THREEFOLD,

	// set manually by Draw() only if conditions are present
	GAME_DRAWN_FIFTY_MOVE
};

enum CastlingOptions
{
	CASTLING_NONE = 0,
	CASTLING_KINGSIDE = 1,
	CASTLING_QUEENSIDE = 2,
	CASTLING_ALL = CASTLING_KINGSIDE | CASTLING_QUEENSIDE
};

class Board
{
public:

	// constructors and persistence

	Board ();
	Board (const char* fen, int state_data);

	const char* GetFEN () const;
	int GetStateData () const;

	// basic data access

	Piece GetPieceAt (const Square& square) const;
	void GetSquaresWith (Squares& squares, const Piece& piece) const;

	Side GetActiveSide () const;
	CastlingOptions GetCastlingOptions (Side side) const;
	Square GetEnPassantSquare () const;
	uint GetHalfmoveClock () const;
	uint GetFullmoveNumber () const;

	// game status and analysis

	GameState GetGameState () const;
	Side GetVictor () const;

	const Moves& GetPossibleMoves () const;
	bool IsUnderAttack (const Square& square, Side attacker) const;
	bool IsInCheck (Side side) const;

	// movement and player actions

	bool MakeMove (const Move& move);
	bool Resign ();
	bool ExpireTime (Side against);
	bool Draw (GameState draw_type);

private:

	// persistent data and initial values

	char board[N_RANKS][N_FILES];
	Side active_side;
	CastlingOptions castling_options[N_SIDES];
	Square en_passant_square;
	uint halfmove_clock;
	uint fullmove_number;

	GameState game_state;
	Side victor;

	void Restore (const char* fen, int state_data);

	void Reset ();
	static const char INITIAL_BOARD[N_RANKS * N_FILES + 1];
	static const Side INITIAL_SIDE;

	// calculated data

	void CalculateData ();

	void UpdatePersistence ();
	char fen[FEN_BUFSIZE];
	int state_data;

	PieceSquares piece_squares;

	void EnumerateMoves ();
	Moves possible_moves;

	bool in_check[N_SIDES];
};

inline const char* Board::GetFEN () const { return fen; }
inline int Board::GetStateData () const { return state_data; }

inline Side Board::GetActiveSide () const { return active_side; }
inline Square Board::GetEnPassantSquare () const { return en_passant_square; }
inline uint Board::GetHalfmoveClock () const { return halfmove_clock; }
inline uint Board::GetFullmoveNumber () const { return fullmove_number; }

inline GameState Board::GetGameState () const { return game_state; }
inline Side Board::GetVictor () const { return victor; }
inline const Moves& Board::GetPossibleMoves () const { return possible_moves; }



} // namespace chess

#endif // !SCR_GENSCRIPTS
#endif // CHESS_H

