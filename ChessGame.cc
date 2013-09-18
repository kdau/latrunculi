/******************************************************************************
 *  ChessGame.cc
 *
 *  Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include "ChessGame.hh"

namespace Chess {



static const Square::Delta
KING_MOVES[] =
	{ {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}, {0,1} };

static const Square::Delta
ROOK_MOVES[] = { {0,1}, {0,-1}, {1,0}, {-1,0} };

static const Square::Delta
BISHOP_MOVES[] = { {1,1}, {1,-1}, {-1,-1}, {-1,1} };

static const Square::Delta
KNIGHT_MOVES[] =
	{ {1,2}, {2,1}, {-1,2}, {-2,1}, {-1,-2}, {-2,-1}, {1,-2}, {2,-1} };



// Position

const char*
Position::INITIAL_BOARD =
	"RNBQKBNRPPPPPPPP\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0pppppppprnbqkbnr";

Position::Position ()
	: active_side (Side::WHITE),
	  castling_white (unsigned (Castling::Type::BOTH)),
	  castling_black (unsigned (Castling::Type::BOTH)),
	  en_passant_square (),
	  fifty_move_clock (0),
	  fullmove_number (1)
{
	std::memcpy (board, INITIAL_BOARD, sizeof board);
}

Position::Position (const Position& copy)
	: active_side (copy.active_side),
	  castling_white (copy.castling_white),
	  castling_black (copy.castling_black),
	  en_passant_square (copy.en_passant_square),
	  fifty_move_clock (copy.fifty_move_clock),
	  fullmove_number (copy.fullmove_number)
{
	std::memcpy (board, copy.board, sizeof board);
}

#define FEN_THROW_INVALID(detail) \
	throw std::invalid_argument ("invalid FEN: " detail)

#define FEN_EXPECT_CHAR(expected, message) \
	if (c != expected) \
		FEN_THROW_INVALID (message);

#define FEN_READ_CHAR(expected, message) \
	if (!(fen >> c) || c != expected) \
		FEN_THROW_INVALID (message);

Position::Position (std::istream& fen)
{
	char c = '\0';

	std::noskipws (fen); // extract by character

	size_t rank = N_RANKS, file = 0u; // FEN is backwards rank-wise
	while (fen >> c)
	{
		if (file == N_FILES)
		{
			file = 0u;
			if (--rank == 0u)
			{
				FEN_EXPECT_CHAR (' ',
					"expected end of piece placement")
				break;
			}
			else
				FEN_EXPECT_CHAR ('/',
					"expected end of board rank")
		}
		else if (Piece (c).is_valid ())
		{
			board [rank-1] [file++] = c;
		}
		else if (c >= '1' && c <= '8' - char (file))
		{
			size_t blank_count = c - '0';
			while (blank_count--)
				board [rank-1] [file++] = Piece::NONE_CODE;
		}
		else
			FEN_THROW_INVALID ("malformed piece placement");
	}
	if (rank != 0u) FEN_THROW_INVALID ("incomplete piece placement");

	bool got_side = false;
	while (fen >> c)
	{
		if (c == 'w')
			active_side = Side::WHITE;
		else if (c == 'b')
			active_side = Side::BLACK;
		else if (c == '-')
			active_side = Side::NONE; // for completed games
		else if (c == ' ')
			break;
		else
			FEN_THROW_INVALID ("invalid active side");
		got_side = true;
	}
	if (!got_side) FEN_THROW_INVALID ("missing active side");

	castling_white = unsigned (Castling::Type::NONE);
	castling_black = unsigned (Castling::Type::NONE);
	bool got_castling = false;
	while (fen >> c)
	{
		if (c == 'K')
			castling_white |= unsigned (Castling::Type::KINGSIDE);
		else if (c == 'Q')
			castling_white |= unsigned (Castling::Type::QUEENSIDE);
		else if (c == 'k')
			castling_black |= unsigned (Castling::Type::KINGSIDE);
		else if (c == 'q')
			castling_black |= unsigned (Castling::Type::QUEENSIDE);
		else if (c == '-')
			{}
		else if (c == ' ')
			break;
		else
			FEN_THROW_INVALID ("invalid castling options");
		got_castling = true;
	}
	if (!got_castling) FEN_THROW_INVALID ("missing castling options");

	std::skipws (fen); // extract by token

	if (!fen) FEN_THROW_INVALID ("missing en passant square");
	String _eps; fen >> _eps;
	if (_eps == "-")
		en_passant_square.clear ();
	else
	{
		Square eps (_eps);
		if (eps.is_valid ())
			en_passant_square = eps;
		else
			FEN_THROW_INVALID ("invalid en passant square");
	}

	if (!fen) FEN_THROW_INVALID ("missing fifty move clock");
	fen >> fifty_move_clock;

	if (!fen) FEN_THROW_INVALID ("missing fullmove number");
	fen >> fullmove_number;
}

void
Position::serialize (std::ostream& fen) const
{
	// FEN is backwards, rank-wise.
	size_t rank = N_RANKS - 1u;
	do
	{
		size_t file = 0u;
		while (file < N_FILES)
		{
			size_t blank_count = 0u;
			while (board [rank] [file] == Piece::NONE_CODE &&
				++blank_count && ++file < N_FILES);
			if (blank_count != 0u)
				fen << blank_count;
			if (file < N_FILES)
				fen << board [rank] [file++];
		}
		if (rank != 0u) fen << '/'; // don't delimit the last one
	}
	while (rank-- != 0u);

	fen << ' ' << active_side.get_code ();

	fen << ' ';
	if (castling_white & unsigned (Castling::Type::KINGSIDE))  fen << 'K';
	if (castling_white & unsigned (Castling::Type::QUEENSIDE)) fen << 'Q';
	if (castling_black & unsigned (Castling::Type::KINGSIDE))  fen << 'k';
	if (castling_black & unsigned (Castling::Type::QUEENSIDE)) fen << 'q';
	if (castling_white == unsigned (Castling::Type::NONE) &&
	    castling_black == unsigned (Castling::Type::NONE))     fen << '-';

	fen << ' ' << en_passant_square.get_code ();
	fen << ' ' << fifty_move_clock;
	fen << ' ' << fullmove_number;
}

bool
Position::is_empty (const Square& square) const
{
	return !get_piece_at (square).is_valid ();
}

Piece
Position::get_piece_at (const Square& square) const
{
	return Piece (square.is_valid () ? (*this) [square] : Piece::NONE_CODE);
}

Castling::Type
Position::get_castling_options (Side side) const
{
	switch (side.value)
	{
	case Side::WHITE: return Castling::Type (castling_white);
	case Side::BLACK: return Castling::Type (castling_black);
	default: return Castling::Type::NONE;
	}
}

bool
Position::is_under_attack (const Square& square, Side attacker) const
{
	if (!square.is_valid () || !attacker.is_valid ()) return false;

	// Check for attacking kings.
	for (auto& delta : KING_MOVES)
		if (get_piece_at (square.offset (delta)) ==
		    Piece (attacker, Piece::Type::KING))
			return true;

	// Check for attacking queens/rooks.
	for (auto& delta : ROOK_MOVES)
		for (Square to = square.offset (delta); to.is_valid ();
		    to = to.offset (delta))
			if (get_piece_at (to) ==
					Piece (attacker, Piece::Type::QUEEN) ||
			    get_piece_at (to) ==
					Piece (attacker, Piece::Type::ROOK))
				return true;
			else if (!is_empty (to))
				break; // Can't pass an occupied square.

	// Check for attacking queens/bishops.
	for (auto& delta : BISHOP_MOVES)
		for (Square to = square.offset (delta); to.is_valid ();
		    to = to.offset (delta))
			if (get_piece_at (to) ==
					Piece (attacker, Piece::Type::QUEEN) ||
			    get_piece_at (to) ==
					Piece (attacker, Piece::Type::BISHOP))
				return true;
			else if (!is_empty (to))
				break; // Can't pass an occupied square.

	// Check for attacking knights.
	for (auto& delta : KNIGHT_MOVES)
		if (get_piece_at (square.offset (delta)) ==
		    Piece (attacker, Piece::Type::KNIGHT))
			return true;

	// Check for attacking pawns.
	int facing = attacker.get_facing_direction ();
	for (int delta_file : { -1, 1 })
	{
		if (get_piece_at (square.offset ({ delta_file, -facing })) ==
		    Piece (attacker, Piece::Type::PAWN))
			return true;

		// Check for en passant capture (behind EPS implies a pawn).
		if (square.offset ({ 0, facing }) == en_passant_square &&
		    get_piece_at (square.offset ({ delta_file, 0 })) ==
				Piece (attacker, Piece::Type::PAWN))
		        return true;
	}

	return false;
}

bool
Position::is_in_check (Side side) const
{
	if (side == Side::NONE) side = active_side;
	Side opponent = side.get_opponent ();
	char king = Piece (side, Piece::Type::KING).get_code ();

	for (auto square = Square::BEGIN; square.is_valid (); ++square)
		if ((*this) [square] == king &&
		    is_under_attack (square, opponent))
			return true;
	return false;
}

bool
Position::is_dead () const
{
	// Dead positions with these remaining non-king materials are detected:
	//   none; one knight; any number of bishops of same square color.

	size_t n_knights = 0u, n_bishops_light = 0u, n_bishops_dark = 0u;
	for (auto square = Square::BEGIN; square.is_valid (); ++square)
		switch (get_piece_at (square).type)
		{
		case Piece::Type::KING: // Kings don't count here.
		case Piece::Type::NONE:
			break;
		case Piece::Type::KNIGHT:
			++n_knights;
			break;
		case Piece::Type::BISHOP:
			switch (square.get_color ())
			{
			case Square::Color::LIGHT: ++n_bishops_light; break;
			case Square::Color::DARK: ++n_bishops_dark; break;
			default: break;
			}
			break;
		default:
			return false; // Pawn, rook, or queen = not dead.
		}

	if (n_knights <= 1u && n_bishops_light == 0u && n_bishops_dark == 0u)
		return true; // none, or one knight

	if (n_knights > 1u)
		return false; // knights and bishops

	// any number of bishops of same square color
	return (n_bishops_light == 0u) || (n_bishops_dark == 0u);
}

bool
Position::operator == (const Position& rhs) const
{
	return std::memcmp (board, rhs.board, sizeof board) == 0 &&
		active_side == rhs.active_side &&
		castling_white == rhs.castling_white &&
		castling_black == rhs.castling_black &&
		(en_passant_square == rhs.en_passant_square ||
			(!en_passant_square.is_valid () &&
			 !rhs.en_passant_square.is_valid ()));
}

void
Position::make_move (const Move::Ptr& move)
{
	if (!move || !move->is_valid ())
		throw std::runtime_error ("invalid move specified");

	// Promote the piece, if applicable.
	Piece piece = move->get_piece ();
	Piece::Type orig_type = piece.type;
	if (move->get_promoted_piece ().is_valid ())
		piece = move->get_promoted_piece ();

	// Clear any captured square.
	auto capture = std::dynamic_pointer_cast<const Capture> (move);
	if (capture)
		(*this) [capture->get_captured_square ()] = Piece::NONE_CODE;

	// Move the piece.
	(*this) [move->get_from ()] = Piece::NONE_CODE;
	(*this) [move->get_to ()] = piece.get_code ();

	// Move any castling rook.
	if (auto castling = std::dynamic_pointer_cast<const Castling> (move))
	{
		(*this) [castling->get_rook_from ()] = Piece::NONE_CODE;
		(*this) [castling->get_rook_to ()] =
			castling->get_rook_piece ().get_code ();
	}

	// Update the castling options.
	unsigned& options = (piece.side == Side::WHITE)
		? castling_white : castling_black;
	if (piece.type == Piece::Type::KING)
		options = unsigned (Castling::Type::NONE);
	else if (orig_type == Piece::Type::ROOK && // not a new-promoted one
		 move->get_from ().rank == piece.get_initial_rank ())
	{
		if (move->get_from ().file == File::A)
			options &= ~unsigned (Castling::Type::QUEENSIDE);
		else if (move->get_from ().file == File::H)
			options &= ~unsigned (Castling::Type::KINGSIDE);
	}

	// Move the turn to the opponent.
	active_side = move->get_side ().get_opponent ();

	// Update the en passant square.
	if (auto two_square = std::dynamic_pointer_cast
	                        <const TwoSquarePawnMove> (move))
		en_passant_square = two_square->get_passed_square ();
	else
		en_passant_square.clear ();

	// Update the fifty-move clock.
	if (orig_type == Piece::Type::PAWN || capture)
		fifty_move_clock = 0;
	else
		++fifty_move_clock;

	// Update the fullmove number.
	if (move->get_side () == Side::BLACK)
		++fullmove_number;
}

char&
Position::operator [] (const Square& square)
{
	if (!square.is_valid ())
		throw std::invalid_argument ("invalid square specified");
	return board [size_t (square.rank)] [size_t (square.file)];
}

const char&
Position::operator [] (const Square& square) const
{
	if (!square.is_valid ())
		throw std::invalid_argument ("invalid square specified");
	return board [size_t (square.rank)] [size_t (square.file)];
}

void
Position::end_game ()
{
	active_side = Side::NONE;
	castling_white = unsigned (Castling::Type::NONE);
	castling_black = unsigned (Castling::Type::NONE);
	en_passant_square.clear ();
	fifty_move_clock = 0;
	// fullmove_number remains valid
}



// Game

Game::Game ()
	: result (Result::ONGOING),
	  victor (Side::NONE)
{
	update_possible_moves ();
}

Game::Game (std::istream& record)
	: Position (record),
	  result (Result::ONGOING),
	  victor (Side::NONE)
{
	Side event_side = Side::WHITE;
	unsigned event_fullmove = 1u;
	while (!record.eof ())
	{
		std::string token;
		record >> token;
		
		auto event = Event::deserialize (token, event_side);
		if (!event)
			throw std::invalid_argument ("invalid event");
		record_event (event);

		if (auto loss = std::dynamic_pointer_cast<Loss> (event))
		{
			result = Result::WON;
			victor = loss->get_side ().get_opponent ();
			event_side = Side::NONE;
		}
		else if (auto draw = std::dynamic_pointer_cast<Draw> (event))
		{
			result = Result::DRAWN;
			event_side = Side::NONE;
		}
		else if (auto move = std::dynamic_pointer_cast<Move> (event))
		{
			if (event_side == Side::BLACK) ++event_fullmove;
			event_side = event_side.get_opponent ();
		}
	}
	if (get_fullmove_number () != event_fullmove)
		Thief::mono.log ("WARNING: Chess::Game: The history is not"
			"consistent with the recorded position.");

	update_possible_moves ();
	detect_endgames (); // just in case
}

void
Game::serialize (std::ostream& record)
{
	Position::serialize (record);
	for (auto& entry : history)
		if (entry.second)
			record << ' ' << entry.second->serialize ();
}

String
Game::get_logbook_heading (unsigned page)
{
	return translate_format ("logbook_heading",
		Side (Side::WHITE).get_name (Case::DATIVE),
		Side (Side::BLACK).get_name (Case::DATIVE),
		page);
}

String
Game::get_halfmove_prefix (unsigned halfmove)
{
	return translate_format ((halfmove % 2 == 0)
			? "event_prefix_a" : "event_prefix_b",
		(halfmove / 2 + 1));
}



// Game: status and analysis

Event::ConstPtr
Game::get_last_event () const
{
	return history.empty () ? nullptr : history.back ().second;
}

bool
Game::is_third_repetition () const
{
	unsigned repetitions = 1u;
	for (auto& entry : history)
		if (*this == entry.first && ++repetitions == 3u)
			return true;
	return false;
}

Move::Ptr
Game::find_possible_move (const Square& from, const Square& to) const
{
	for (auto& move : possible_moves)
		if (move && move->get_from () == from && move->get_to () == to)
			return move;
	return nullptr;
}

Move::Ptr
Game::find_possible_move (const String& uci_code) const
{
	Piece promotion;
	switch (uci_code.length ())
	{
	case 5u:
		promotion.set_code (uci_code.back ());
		if (!promotion.is_valid ()) return nullptr;
		// Discard the promotion type; always promote to queen.
	case 4u:
		return find_possible_move (Square (uci_code.substr (0u, 2u)),
			Square (uci_code.substr (2u, 2u)));
	default:
		return nullptr;
	}
}



// Game: movement and player actions

void
Game::make_move (const Move::Ptr& move)
{
	if (result != Result::ONGOING)
		throw std::runtime_error ("cannot move in a completed game");
	if (!move)
		throw std::runtime_error ("no move specified");

	bool possible = false;
	for (auto& possible_move : possible_moves)
		if (*move == *possible_move)
		{
			possible = true;
			break;
		}

	if (!possible)
		throw std::runtime_error ("move not currently possible");

	record_event (move);
	Position::make_move (move);
	update_possible_moves ();
	detect_endgames ();
}

void
Game::record_loss (Loss::Type type, Side side)
{
	if (result != Result::ONGOING)
		throw std::runtime_error ("cannot lose a completed game");

	switch (type)
	{
	case Loss::Type::CHECKMATE:
		throw std::runtime_error
			("loss type must be automatically detected");
	case Loss::Type::RESIGNATION:
		if (side != get_active_side ())
			throw std::runtime_error ("only active side may resign");
		break;
	case Loss::Type::TIME_CONTROL:
		break;
	default:
		throw std::runtime_error ("invalid loss type");
	}

	record_event (std::make_shared<Loss> (type, side));
	end_game (Result::WON, side.get_opponent ());
}

void
Game::record_draw (Draw::Type type)
{
	if (result != Result::ONGOING)
		throw std::runtime_error ("cannot draw a completed game");

	switch (type)
	{
	case Draw::Type::STALEMATE:
	case Draw::Type::DEAD_POSITION:
		throw std::invalid_argument
			("draw type must be automatically detected");
	case Draw::Type::FIFTY_MOVE:
		if (get_fifty_move_clock () < 50u)
			throw std::runtime_error
				("fifty move rule not in effect");
		break;
	case Draw::Type::THREEFOLD_REPETITION:
		if (!is_third_repetition ())
			throw std::runtime_error
				("threefold repetition rule not in effect");
		break;
	case Draw::Type::BY_AGREEMENT:
		// Accept unconditionally; UI must broker.
		break;
	default:
		throw std::runtime_error ("invalid draw type");
	}

	record_event (std::make_shared<Draw> (type));
	end_game (Result::DRAWN, Side::NONE);
}

void
Game::record_war_result (Side _victor)
{
	if (result != Result::ONGOING)
		throw std::runtime_error ("cannot complete a completed war");

	if (_victor == Side::NONE)
	{
		record_event (std::make_shared<Draw> (Draw::Type::DEAD_POSITION));
		end_game (Result::DRAWN, Side::NONE);
	}
	else
	{
		record_event (std::make_shared<Loss> (Loss::Type::CHECKMATE,
			_victor.get_opponent ()));
		end_game (Result::WON, _victor);
	}
}

void
Game::record_event (const Event::ConstPtr& event)
{
	history.push_back (HistoryEntry (*this, event));
}

void
Game::end_game (Result _result, Side _victor)
{
	Position::end_game ();
	result = _result;
	victor = _victor;
	possible_moves.clear ();
}

void
Game::detect_endgames ()
{
	// Bail out if game is already over.
	if (result != Result::ONGOING) return;

	// Detect dead positions.
	if (is_dead ())
	{
		record_event (std::make_shared<Draw>
			(Draw::Type::DEAD_POSITION));
		end_game (Result::DRAWN, Side::NONE);
	}

	// Detect checkmate.
	else if (possible_moves.empty () && is_in_check ())
	{
		record_event (std::make_shared<Loss>
			(Loss::Type::CHECKMATE, get_active_side ()));
		end_game (Result::WON, get_active_side ().get_opponent ());
	}

	// Detect stalemate.
	else if (possible_moves.empty ())
	{
		record_event (std::make_shared<Draw> (Draw::Type::STALEMATE));
		end_game (Result::DRAWN, Side::NONE);
	}
}

void
Game::update_possible_moves ()
{
	possible_moves.clear ();

	for (auto from = Square::BEGIN; from.is_valid (); ++from)
	{
		Piece piece = Piece ((*this) [from]);
		if (piece.side != get_active_side ()) continue;
		switch (piece.type)
		{
		case Piece::Type::KING:
			enumerate_king_moves (piece, from);
			break;
		case Piece::Type::QUEEN:
			enumerate_rook_moves (piece, from);
			enumerate_bishop_moves (piece, from);
			break;
		case Piece::Type::ROOK:
			enumerate_rook_moves (piece, from);
			break;
		case Piece::Type::BISHOP:
			enumerate_bishop_moves (piece, from);
			break;
		case Piece::Type::KNIGHT:
			enumerate_knight_moves (piece, from);
			break;
		case Piece::Type::PAWN:
			enumerate_pawn_moves (piece, from);
			break;
		default:
			break;
		}
	}
}

void
Game::enumerate_king_moves (const Piece& piece, const Square& from)
{
	// Enumerate basic moves.

	for (auto& delta : KING_MOVES)
		confirm_possible_capture (piece, from, from.offset (delta));

	// Enumerate castling moves.

	if (is_in_check ()) return;
	Side opponent = piece.side.get_opponent ();

	unsigned options = unsigned (get_castling_options (piece.side));

	if (options & unsigned (Castling::Type::KINGSIDE))
	{
		Square rook_to = from.offset ({ 1, 0 }),
			king_to = from.offset ({ 2, 0 });
		if (is_empty (rook_to) && !is_under_attack (rook_to, opponent) &&
		    is_empty (king_to) && !is_under_attack (king_to, opponent))
			confirm_possible_move (std::make_shared<Castling>
				(piece.side, Castling::Type::KINGSIDE));
	}

	if (options & unsigned (Castling::Type::QUEENSIDE))
	{
		Square rook_to = from.offset ({ -1, 0 }),
			king_to = from.offset ({ -2, 0 }),
			rook_pass = from.offset ({ -3, 0 });
		if (is_empty (rook_to) && !is_under_attack (rook_to, opponent) &&
		    is_empty (king_to) && !is_under_attack (king_to, opponent)
		    && is_empty (rook_pass))
			confirm_possible_move (std::make_shared<Castling>
				(piece.side, Castling::Type::QUEENSIDE));
	}
}

void
Game::enumerate_rook_moves (const Piece& piece, const Square& from)
{
	for (auto& delta : ROOK_MOVES)
		for (Square to = from; to.is_valid (); to = to.offset (delta))
		{
			if (to == from) continue;
			confirm_possible_capture (piece, from, to);
			if (!is_empty (to))
				break; // Can't pass an occupied square.
		}
}

void
Game::enumerate_bishop_moves (const Piece& piece, const Square& from)
{
	for (auto& delta : BISHOP_MOVES)
		for (Square to = from; to.is_valid (); to = to.offset (delta))
		{
			if (to == from) continue;
			confirm_possible_capture (piece, from, to);
			if (!is_empty (to))
				break; // Can't pass an occupied square.
		}
}

void
Game::enumerate_knight_moves (const Piece& piece, const Square& from)
{
	for (auto& delta : KNIGHT_MOVES)
		confirm_possible_capture (piece, from, from.offset (delta));
}

void
Game::enumerate_pawn_moves (const Piece& piece, const Square& from)
{
	int facing = piece.side.get_facing_direction ();

	// Enumerate forward moves.

	Square one_square = from.offset ({ 0, facing });
	if (is_empty (one_square))
	{
		confirm_possible_move (std::make_shared<Move>
			(piece, from, one_square));

		Square two_square = one_square.offset ({ 0, facing });
		if (is_empty (two_square) &&
		    from.rank == piece.get_initial_rank ())
			confirm_possible_move
				(std::make_shared<TwoSquarePawnMove>
					(piece.side, from.file));
	}

	// Enumerate captures. Go directly to confirm_possible_move since the
	//	capture checks have already been performed.

	for (int delta_file : { -1, 1 })
	{
		Square to = from.offset ({ delta_file, facing });
		if (get_piece_at (to).side == piece.side.get_opponent ())
			confirm_possible_move (std::make_shared<Capture>
				(piece, from, to, Piece ((*this) [to])));
		else if (to == get_en_passant_square ())
			confirm_possible_move (std::make_shared<EnPassantCapture>
				(piece.side, from.file, to.file));
	}
}

bool
Game::confirm_possible_capture (const Piece& piece, const Square& from,
	const Square& to)
{
	Piece to_occupant = get_piece_at (to);
		
	if (piece.side == to_occupant.side)
		return false; // Move cannot be to a friendly-occupied square.

	else if (to_occupant.is_valid ())
		return confirm_possible_move (std::make_shared<Capture>
			(piece, from, to, to_occupant));

	else // The to square is empty.
		return confirm_possible_move (std::make_shared<Move>
			(piece, from, to));
}

bool
Game::confirm_possible_move (const Move::Ptr& move)
{
	// The move must exist and be basically valid.
	if (!move || !move->is_valid ())
		return false;

	// The move cannot place the moving piece's side in check.
	Position check_test (*this);
	check_test.make_move (move);
	if (check_test.is_in_check (move->get_side ()))
		return false;

	possible_moves.push_back (move);
	return true;
}



} // namespace Chess

