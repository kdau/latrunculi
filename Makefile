#******************************************************************************
#   Makefile
#
#   Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program. If not, see <http://www.gnu.org/licenses/>.
#
#******************************************************************************

MODULE_NAME = latrunculi
HOST = x86_64-linux-gnu
TARGET = i686-w64-mingw32
THIEFLIBDIR = ThiefLib
default: thief2

SCRIPT_HEADERS = \
	Chess.hh \
	ChessGame.hh \
	ChessEngine.hh \
	NGC.hh \
	NGCGame.hh \
	NGCPiece.hh \

include $(THIEFLIBDIR)/module.mk

$(bindir2)/Chess.o: Chess.inl
$(bindir2)/ChessGame.o: Chess.hh Chess.inl
$(bindir2)/ChessEngine.o: Chess.hh Chess.inl ChessGame.hh
$(bindir2)/NGC.o: Chess.hh Chess.inl
$(bindir2)/NGCGame.o: Chess.hh Chess.inl NGC.hh ChessGame.hh ChessEngine.hh
$(bindir2)/NGCPiece.o: Chess.hh Chess.inl NGC.hh

