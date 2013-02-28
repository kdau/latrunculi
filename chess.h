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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

typedef unsigned int uint;

namespace chess {

// to be implemented appropriately by downstream user

std::string Translate (const std::string& msgid);



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
	FILE_H,
	N_FILES
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
	RANK_8,
	N_RANKS
};

enum SquareColor
{
	SQUARE_NONE = -1,
	SQUARE_LIGHT,
	SQUARE_DARK,
	N_SQUARE_COLORS
};

struct Square
{
	Square (File file = FILE_NONE, Rank rank = RANK_NONE);
	Square (const std::string& code);

	bool Valid () const;
	bool operator == (const Square& rhs) const;

	std::string GetCode () const;
	SquareColor GetColor () const;

	Square Offset (int delta_file, int delta_rank) const;

	void Clear ();

	File file;
	Rank rank;
};

typedef std::vector<Square> Squares;



/**
 ** Piece (Side, PieceType)
 **/

enum Side
{
	SIDE_NONE = -1,
	SIDE_WHITE,
	SIDE_BLACK,
	N_SIDES
};

bool SideValid (Side side);
char SideCode (Side side);
std::string SideName (Side side);
int SideFacing (Side side);
Side Opponent (Side side);

enum PieceType
{
	PIECE_NONE = -1,
	PIECE_KING,
	PIECE_QUEEN,
	PIECE_ROOK,
	PIECE_BISHOP,
	PIECE_KNIGHT,
	PIECE_PAWN,
	N_PIECE_TYPES
};

struct Piece
{
	Piece (Side side = SIDE_NONE, PieceType type = PIECE_NONE);
	Piece (char code);

	bool Valid () const;
	bool operator == (const Piece& rhs) const;

	char GetCode () const;
	void SetCode (char code);
	static const char NONE_CODE;

	std::string GetName () const;

	Rank GetInitialRank () const;

	Side side;
	PieceType type;

private:
	static const char CODES[N_SIDES][N_PIECE_TYPES+1];
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
 ** Move (Resignation, Movement, PieceMove, Capture, EnPassantCapture,
 **       TwoSquarePawnMove, Castling)
 **/

//FIXME Write copying and comparison solutions for the Move family.

class Move
{
public:
	virtual ~Move () {}

	bool Valid () const { return valid; }

	virtual Side GetSide () const = 0;

	virtual std::string GetUCICode () const = 0;
	virtual std::string GetDescription () const = 0;

protected:
	Move () : valid (true) {}
	void Invalidate () { valid = false; }

private:
	bool valid;
};
typedef std::unique_ptr<Move> MovePtr;
typedef std::vector<MovePtr> Moves;

class Resignation : public Move
{
public:
	Resignation (Side _side) : side (_side) {}

	virtual Side GetSide () const { return side; }

	virtual std::string GetUCICode () const;
	virtual std::string GetDescription () const;

private:
	Side side;
};

//FIXME What about time control and draws?

struct Movement
{
	Piece piece;
	Square from;
	Square to;
};

class PieceMove : public Move, private Movement
{
public:
	PieceMove (const Piece& piece, const Square& from, const Square& to);

	Piece GetPiece () const { return piece; }
	Square GetFrom () const { return from; }
	Square GetTo () const { return to; }

	virtual Side GetSide () const { return piece.side; }

	virtual std::string GetUCICode () const;
	virtual std::string GetDescription () const;

	//FIXME What about promotions? They can also be Captures (but not EPCs or TSPMs).

private:
	Piece piece;
	Square from;
	Square to;
};

class Capture : public PieceMove
{
public:
	Capture (const Piece& piece, const Square& from,
		const Square& to, const Piece& captured_piece);

	const Piece& GetCapturedPiece () const { return captured_piece; }
	virtual Square GetCapturedSquare () const { return GetTo (); }

	// no special UCI code
	virtual std::string GetDescription () const;

private:
	Piece captured_piece;
};

class EnPassantCapture : public Capture
{
public:
	EnPassantCapture (Side side, File from, File to);

	virtual Square GetCapturedSquare () const { return captured_square; }

	// no special UCI code
	virtual std::string GetDescription () const;

private:
	Square captured_square;
};

class TwoSquarePawnMove : public PieceMove
{
public:
	TwoSquarePawnMove (Side side, File file);

	const Square& GetPassedSquare () const { return passed_square; }

	// no special UCI code or description

private:
	Square passed_square;
};

class Castling : public PieceMove
{
public:
	enum Type
	{
		NONE = 0,
		KINGSIDE = 1,
		QUEENSIDE = 2,
		BOTH = KINGSIDE | QUEENSIDE
	};

	Castling (Side side, Type type);

	Type GetCastlingType () const { return type; }

	Piece GetRookPiece () const { return rook_piece; }
	Square GetRookFrom () const { return rook_from; }
	Square GetRookTo () const { return rook_to; }

	// no special UCI code
	virtual std::string GetDescription () const;

private:
	Type type;
	Piece rook_piece;
	Square rook_from;
	Square rook_to;
};



/**
 ** Board
 **/

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

class Board
{
public:

	// constructors and persistence

	Board ();
	Board (const Board& board);
	Board (const std::string& fen, int state_data);

	const std::string& GetFEN () const;
	int GetStateData () const;

	// basic data access

	bool IsEmpty (const Square& square) const;
	Piece GetPieceAt (const Square& square) const;
	void GetSquaresWith (Squares& squares, const Piece& piece) const;

	Side GetActiveSide () const;
	uint GetCastlingOptions (Side side) const;
	Square GetEnPassantSquare () const;

	uint GetFiftyMoveClock () const;
	uint GetFullmoveNumber () const;
	std::string GetHalfmoveName () const;

	// game status and analysis

	GameState GetGameState () const;
	Side GetVictor () const;

	const Moves& GetPossibleMoves () const; //FIXME This doesn't keep the Moves themselves const!
	bool IsUnderAttack (const Square& square, Side attacker) const;
	bool IsInCheck (Side side) const;

	// movement and player actions

	void MakeMove (const PieceMove& move);
	void Resign ();
	void ExpireTime (Side against);
	void Draw (GameState draw_type);

private:

	void EndGame (GameState result, Side victor);

	// persistent data and initial values

	char board[N_RANKS][N_FILES];
	char& operator [] (const Square& square);
	const char& operator [] (const Square& square) const;

	Side active_side;
	uint castling_options[N_SIDES];
	Square en_passant_square;
	uint fifty_move_clock;
	uint fullmove_number;

	GameState game_state;
	Side victor;

	static const char INITIAL_BOARD[N_RANKS * N_FILES + 1];
	static const Side INITIAL_SIDE;

	// calculated data

	void CalculateData ();

	std::string fen;
	int state_data;
	void UpdatePersistence ();

	PieceSquares piece_squares;
	bool IsDeadPosition () const;

	Moves possible_moves;
	void EnumerateMoves ();
	void EnumerateKingMoves (const Piece& piece, const Square& from);
	void EnumerateRangedMoves (const Piece& piece, const Square& from);
	void EnumerateKnightMoves (const Piece& piece, const Square& from);
	void EnumeratePawnMoves (const Piece& piece, const Square& from);
	bool ConfirmPossibleMove (PieceMove& move);

	bool in_check[N_SIDES];
};

inline const std::string& Board::GetFEN () const { return fen; }
inline int Board::GetStateData () const { return state_data; }

inline bool Board::IsEmpty (const Square& square) const
	{ return !GetPieceAt (square).Valid (); }
inline Piece Board::GetPieceAt (const Square& square) const
	{ return square.Valid () ? (*this)[square] : Piece::NONE_CODE; }

inline Side Board::GetActiveSide () const { return active_side; }
inline uint Board::GetCastlingOptions (Side side) const
	{ return SideValid (side) ? castling_options[side] : uint (Castling::NONE); }
inline Square Board::GetEnPassantSquare () const { return en_passant_square; }
inline uint Board::GetFiftyMoveClock () const { return fifty_move_clock; }
inline uint Board::GetFullmoveNumber () const { return fullmove_number; }

inline GameState Board::GetGameState () const { return game_state; }
inline Side Board::GetVictor () const { return victor; }

inline const Moves& Board::GetPossibleMoves () const { return possible_moves; }

inline bool Board::IsInCheck (Side side) const
	{ return SideValid (side) ? in_check[side] : false; }



} // namespace chess

#endif // !SCR_GENSCRIPTS
#endif // CHESS_H

