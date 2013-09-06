/******************************************************************************
 *  module.cc
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

#include "NGC.hh"
#include "NGCGame.hh"
#include "NGCPiece.hh"

#include "version.rc"
#include <Thief/module.hh>

THIEF_MODULE_BEGIN (MODULE_NAME)
	THIEF_SCRIPT ("NGCClock", "NGCTitled", NGCClock)
	THIEF_SCRIPT ("NGCFlag", "NGCTitled", NGCFlag)
	THIEF_SCRIPT ("NGCGame", "Script", NGCGame)
	THIEF_SCRIPT ("NGCIntro", "Script", NGCIntro)
	THIEF_SCRIPT ("NGCPiece", "Script", NGCPiece)
	THIEF_SCRIPT ("NGCScenario", "NGCTitled", NGCScenario)
	THIEF_SCRIPT ("NGCSquare", "Script", NGCSquare)
	THIEF_SCRIPT ("NGCTitled", "Script", NGCTitled)
THIEF_MODULE_END

