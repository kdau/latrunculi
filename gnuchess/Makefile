# Makefile for the GNU Chess engine, adapted for use in Thief fan missions.
#
# Copyright (C) 2001-2011 Free Software Foundation, Inc.
# Copyright (C) 2013 Kevin Daughtridge <kevin@kdau.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

.PHONY: default all clean
.PRECIOUS: %.o
default: all

srcdir = .
bindir = .

PREFIX = i686-w64-mingw32-
CXX = $(PREFIX)g++
AR = $(PREFIX)ar

INCLUDES = -I. -I$(srcdir)
LIBDIRS = -L.

DEFINES = -D_WIN32 -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 -DWIN32_LEAN_AND_MEAN \
	-DVERSION=\"6.0.2\"
DEFINES_D = -DDEBUG

CXXFLAGS = -W -Wall -masm=intel -std=gnu++0x -O2
CXXFLAGS_D = -g -O0

ARFLAGS = rc

LIBRARY = libgnuchess.a
LIBRARY_D = libgnuchess-d.a

SOURCES = \
	attack.cpp \
	board.cpp \
	book.cpp \
	eval.cpp \
	fen.cpp \
	hash.cpp \
	list.cpp \
	main.cpp \
	material.cpp \
	move.cpp \
	move_check.cpp \
	move_do.cpp \
	move_evasion.cpp \
	move_gen.cpp \
	move_legal.cpp \
	option.cpp \
	pawn.cpp \
	piece.cpp \
	posix.cpp \
	protocol.cpp \
	pst.cpp \
	pv.cpp \
	random.cpp \
	recog.cpp \
	search.cpp \
	search_full.cpp \
	see.cpp \
	sort.cpp \
	square.cpp \
	trans.cpp \
	util.cpp \
	value.cpp \
	vector.cpp

OBJECTS = $(SOURCES:%.cpp=$(bindir)/%.o)
OBJECTS_D = $(SOURCES:%.cpp=$(bindir)/%-d.o)

$(bindir):
	mkdir -p $@

$(bindir)/%.o: $(srcdir)/%.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -o $@ -c $<

$(bindir)/%-d.o: $(srcdir)/%.cpp
	$(CXX) $(CXXFLAGS) $(CXXFLAGS_D) $(DEFINES) $(DEFINES_D) $(INCLUDES) -o $@ -c $<

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $?

$(LIBRARY_D): $(OBJECTS_D)
	$(AR) $(ARFLAGS) $@ $?

all: $(bindir) $(LIBRARY) $(LIBRARY_D)

clean:
	$(RM) $(bindir)/*.o $(LIBRARY) $(LIBRARY_D)

