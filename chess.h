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

struct Square
{
	Square (File file = FILE_NONE, Rank rank = RANK_NONE);
	Square (const std::string& code);

	bool Valid () const;
	bool operator == (const Square& rhs) const;
	bool operator != (const Square& rhs) const;

	std::string GetCode () const;

	enum Color
	{
		NONE = -1,
		LIGHT,
		DARK,
		N_COLORS
	};
	Color GetColor () const;

	typedef std::pair<int, int> Delta;
	Square Offset (Delta delta) const;
	Square Offset (int delta_file, int delta_rank) const;

	Square& operator ++ ();
	static const Square BEGIN;

	void Clear ();

	File file;
	Rank rank;
};

typedef std::vector<Square> Squares;



/**
 ** Piece (Side, PieceType)
 **/

enum NameCase
{
	NOMINATIVE,
	DATIVE,
	ACCUSATIVE,
	BECOMING
};

enum Side
{
	SIDE_NONE = -1,
	SIDE_WHITE,
	SIDE_BLACK,
	N_SIDES
};

bool SideValid (Side side);
char SideCode (Side side);
Side SideFromCode (char code);
std::string SideName (Side side, NameCase name_case);
int SideFacing (Side side);
Side Opponent (Side side);

struct Piece
{

	enum Type
	{
		NONE = -1,
		KING,
		QUEEN,
		ROOK,
		BISHOP,
		KNIGHT,
		PAWN,
		N_TYPES
	};

	Piece (Side side = SIDE_NONE, Type type = NONE);
	Piece (char code);

	bool Valid () const;
	bool operator == (const Piece& rhs) const;
	bool operator != (const Piece& rhs) const;

	char GetCode () const;
	void SetCode (char code);
	static const char NONE_CODE;

	std::string GetName (NameCase name_case) const;

	Rank GetInitialRank () const;

	Side side;
	Type type;

private:
	static const char CODES[N_SIDES][N_TYPES+1];
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
typedef std::pair<PieceSquares::const_iterator, PieceSquares::const_iterator>
	PieceSquaresRange;



/**
 ** Event (Loss, Draw, Move, Capture, EnPassantCapture,
 **        TwoSquarePawnMove, Castling)
 **/

class Event;
typedef std::shared_ptr<Event> EventPtr;
typedef std::shared_ptr<const Event> EventConstPtr;
typedef std::vector<EventConstPtr> Events;

class Event
{
public:
	virtual ~Event () {}

	bool Valid () const { return valid; }

	virtual Side GetSide () const = 0;

	// modified long algebraic notation, for serialization of history
	virtual std::string GetMLAN () const = 0;
	static EventPtr FromMLAN (const std::string& mlan, Side active_side);

	virtual std::string GetDescription () const = 0;
	virtual std::string GetConcept () const = 0;

protected:
	Event () : valid (true) {}
	Event (const Event&) = delete;

	void Invalidate () { valid = false; }

	friend bool operator == (const Event& lhs, const Event& rhs);
	friend bool operator != (const Event& lhs, const Event& rhs);
	virtual bool Equals (const Event& rhs) const = 0;

private:
	bool valid;
};

bool operator == (const Event& lhs, const Event& rhs);
bool operator != (const Event& lhs, const Event& rhs);

class Loss : public Event
{
public:
	enum Type
	{
		NONE = -1,

		// detected and entered automatically
		CHECKMATE,

		// entered manually; UI must detect and/or confirm conditions
		RESIGNATION,
		TIME_CONTROL
	};

	Loss (Type type, Side side);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	virtual Side GetSide () const { return side; }
	Type GetType () const { return type; }

	virtual std::string GetDescription () const;
	virtual std::string GetConcept () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Type type;
	Side side;
};

class Draw : public Event
{
public:
	enum Type
	{
		NONE = -1,

		// detected and entered automatically
		STALEMATE,
		DEAD_POSITION,

		// entered manually; only valid if conditions are present
		FIFTY_MOVE,

		// entered manually; UI must detect and/or confirm conditions
		THREEFOLD_REPETITION,
		BY_AGREEMENT
	};

	Draw (Type type);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	virtual Side GetSide () const { return SIDE_NONE; }
	Type GetType () const { return type; }

	virtual std::string GetDescription () const;
	virtual std::string GetConcept () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Type type;
};

class Move : public Event
{
public:
	Move (const Piece& piece, const Square& from, const Square& to);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	virtual Side GetSide () const { return piece.side; }
	Piece GetPiece () const { return piece; }
	Square GetFrom () const { return from; }
	Square GetTo () const { return to; }
	Piece GetPromotion () const { return Piece (piece.side, promotion); }

	std::string GetUCICode () const;
	virtual std::string GetDescription () const;
	virtual std::string GetConcept () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Piece piece;
	Square from;
	Square to;
	Piece::Type promotion;
};

typedef std::shared_ptr<const Move> MovePtr;
typedef std::vector<MovePtr> Moves;

class Capture : public Move
{
public:
	Capture (const Piece& piece, const Square& from,
		const Square& to, const Piece& captured_piece);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	const Piece& GetCapturedPiece () const { return captured_piece; }
	virtual Square GetCapturedSquare () const { return GetTo (); }

	virtual std::string GetDescription () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Piece captured_piece;
};

class EnPassantCapture : public Capture
{
public:
	EnPassantCapture (Side side, File from, File to);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	virtual Square GetCapturedSquare () const { return captured_square; }

	virtual std::string GetDescription () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Square captured_square;
};

class TwoSquarePawnMove : public Move
{
public:
	TwoSquarePawnMove (Side side, File file);

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	const Square& GetPassedSquare () const { return passed_square; }


protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Square passed_square;
};

class Castling : public Move
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

	static EventPtr FromMLAN (const std::string& mlan, Side active_side);
	virtual std::string GetMLAN () const;

	Type GetCastlingType () const { return type; }
	Piece GetRookPiece () const { return rook_piece; }
	Square GetRookFrom () const { return rook_from; }
	Square GetRookTo () const { return rook_to; }

	virtual std::string GetDescription () const;

protected:
	virtual bool Equals (const Event& rhs) const;

private:
	Type type;
	Piece rook_piece;
	Square rook_from;
	Square rook_to;
};



/**
 ** Position
 **/

class Position
{
public:
	Position ();
	Position (const Position& position);

	// Serialize is intentionally non-virtual to allow FEN access for Game.
	Position (std::istream& fen);
	void Serialize (std::ostream& fen) const;

	bool IsEmpty (const Square& square) const;
	Piece GetPieceAt (const Square& square) const;

	Side GetActiveSide () const { return active_side; }
	unsigned GetCastlingOptions (Side side) const;
	Square GetEnPassantSquare () const { return en_passant_square; }

	unsigned GetFiftyMoveClock () const { return fifty_move_clock; }
	unsigned GetFullmoveNumber () const { return fullmove_number; }

	bool IsUnderAttack (const Square& square, Side attacker) const;
	bool IsInCheck (Side side = SIDE_NONE) const; // default: active side
	bool IsDead () const;

	virtual void MakeMove (const MovePtr& move);

protected:
	char& operator [] (const Square& square);
	const char& operator [] (const Square& square) const;
	PieceSquaresRange GetSquaresWith (const Piece& piece) const;

	void EndGame ();

private:
	char board[N_RANKS][N_FILES];
	Side active_side;
	unsigned castling_options[N_SIDES];
	Square en_passant_square;
	unsigned fifty_move_clock;
	unsigned fullmove_number;

	static const char INITIAL_BOARD[N_RANKS * N_FILES + 1];
	static const Side INITIAL_SIDE;

	void UpdatePieceSquares ();
	PieceSquares piece_squares;
};



/**
 ** Game
 **/

class Game : public Position
{
public:
	Game ();
	Game (const Game&) = delete;

	Game (std::istream& record);
	void Serialize (std::ostream& record);

	static std::string GetLogbookHeading (unsigned page);
	static std::string GetHalfmovePrefix (unsigned halfmove);

	// status and analysis

	enum Result
	{
		ONGOING,
		WON,
		DRAWN
	};

	Result GetResult () const { return result; }
	Side GetVictor () const { return victor; }
	const Events& GetHistory () const { return history; }

	const Moves& GetPossibleMoves () const { return possible_moves; }
	MovePtr FindPossibleMove (const Square& from, const Square& to) const;
	MovePtr FindPossibleMove (const std::string& uci_code) const;

	// movement and player actions

	virtual void MakeMove (const MovePtr& move);
	void RecordLoss (Loss::Type type, Side side);
	void RecordDraw (Draw::Type type);

private:
	void EndGame (Result result, Side victor);
	void DetectEndgames ();

	Result result;
	Side victor;
	Events history;

	// possible moves

	void UpdatePossibleMoves ();
	void EnumerateKingMoves (const Piece& piece, const Square& from);
	void EnumerateRookMoves (const Piece& piece, const Square& from);
	void EnumerateBishopMoves (const Piece& piece, const Square& from);
	void EnumerateKnightMoves (const Piece& piece, const Square& from);
	void EnumeratePawnMoves (const Piece& piece, const Square& from);
	bool ConfirmPossibleCapture (const Piece& piece, const Square& from,
		const Square& to);
	bool ConfirmPossibleMove (const MovePtr& move);

	Moves possible_moves;
};



/**
 ** Utility functions
 **/

// to be implemented appropriately by downstream user

void DebugMessage (const std::string& message);
std::string Translate (const std::string& msgid, Side side = SIDE_NONE);

// implemented here

std::string TranslateFormat (const std::string& format_msgid, ...);



} // namespace chess

#endif // !SCR_GENSCRIPTS
#endif // CHESS_H

