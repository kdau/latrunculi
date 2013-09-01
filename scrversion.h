/******************************************************************************
 *  scrversion.h
 *
 *  This file is part of Public Scripts
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
#ifndef SCRVERSION_H
#define SCRVERSION_H

#include "Script.h"

class cScr_VersionCheck : public cScript
{
public:
	static IScript* MakeVersionCheck(const char* pszName, int iHostObjId);

	cScr_VersionCheck(const char* pszName, int iHostObjId)
		: cScript(pszName,iHostObjId),
		m_iTextType(0)
	{ }

	STDMETHOD(ReceiveMessage)(sScrMsg* pMsg, sMultiParm* pReply, eScrTraceAction eTrace);

private:
	int m_iTextType;

};

#endif // SCRVERSION_H
