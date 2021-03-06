//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include "ns.h"
#include "compat.h"
#include "build.h"
#include "exhumed.h"
#include "player.h"
#include "view.h"
#include "mapinfo.h"
#include "aistuff.h"
#include "ps_input.h"
#include "cheathandler.h"
#include "gamestate.h"
#include "mmulti.h"

BEGIN_PS_NS

static bool gamesetinput = false;

static int osdcmd_warptocoords(CCmdFuncPtr parm)
{
    if (parm->numparms < 3 || parm->numparms > 5)
        return CCMD_SHOWHELP;

    Player     *nPlayer = &PlayerList[nLocalPlayer];
    spritetype *pSprite = &sprite[nPlayer->nSprite]; 

    nPlayer->opos.x = pSprite->x = atoi(parm->parms[0]);
    nPlayer->opos.y = pSprite->y = atoi(parm->parms[1]);
    nPlayer->opos.z = pSprite->z = atoi(parm->parms[2]);

    if (parm->numparms >= 4)
    {
        nPlayer->angle.oang = nPlayer->angle.ang = buildang(atoi(parm->parms[3]));
    }

    if (parm->numparms == 5)
    {
        nPlayer->horizon.ohoriz = nPlayer->horizon.horiz = buildhoriz(atoi(parm->parms[4]));
    }

    return CCMD_OK;
}

static int osdcmd_doors(CCmdFuncPtr parm)
{
    for (int i = 0; i < kMaxChannels; i++)
    {
        // CHECKME - does this toggle?
        if (sRunChannels[i].c == 0) {
            runlist_ChangeChannel(i, 1);
        }
        else {
            runlist_ChangeChannel(i, 0);
        }
    }
    return CCMD_OK;
}

extern int initx;
extern int inity;
extern int initz;
extern short inita;
extern short initsect;

static int osdcmd_spawn(CCmdFuncPtr parm)
{
    if (parm->numparms != 1) return CCMD_SHOWHELP;
    auto c = parm->parms[0];

    if (!stricmp(c, "anubis")) BuildAnubis(-1, initx, inity, sector[initsect].floorz, initsect, inita, false);
    else if (!stricmp(c, "spider")) BuildSpider(-1, initx, inity, sector[initsect].floorz, initsect, inita);
    else if (!stricmp(c, "mummy")) BuildMummy(-1, initx, inity, sector[initsect].floorz, initsect, inita);
    else if (!stricmp(c, "fish")) BuildFish(-1, initx, inity, initz + eyelevel[nLocalPlayer], initsect, inita);
    else if (!stricmp(c, "lion")) BuildLion(-1, initx, inity, sector[initsect].floorz, initsect, inita);
    else if (!stricmp(c, "lava")) BuildLava(-1, initx, inity, sector[initsect].floorz, initsect, inita, nNetPlayerCount);
    else if (!stricmp(c, "rex")) BuildRex(-1, initx, inity, sector[initsect].floorz, initsect, inita, nNetPlayerCount);
    else if (!stricmp(c, "set")) BuildSet(-1, initx, inity, sector[initsect].floorz, initsect, inita, nNetPlayerCount);
    else if (!stricmp(c, "queen")) BuildQueen(-1, initx, inity, sector[initsect].floorz, initsect, inita, nNetPlayerCount);
    else if (!stricmp(c, "roach")) BuildRoach(0, -1, initx, inity, sector[initsect].floorz, initsect, inita);
    else if (!stricmp(c, "roach2")) BuildRoach(1, -1, initx, inity, sector[initsect].floorz, initsect, inita);
    else if (!stricmp(c, "wasp")) BuildWasp(-1, initx, inity, sector[initsect].floorz - 25600, initsect, inita);
    else if (!stricmp(c, "scorp")) BuildScorp(-1, initx, inity, sector[initsect].floorz, initsect, inita, nNetPlayerCount);
    else if (!stricmp(c, "rat")) BuildRat(-1, initx, inity, sector[initsect].floorz, initsect, inita);
    else Printf("Unknown creature type %s\n", c);
    return CCMD_OK;
}

static int osdcmd_third_person_view(CCmdFuncPtr parm)
{
    if (gamestate != GS_LEVEL) return CCMD_OK;
    if (!nFreeze)
    {
        bCamera = !bCamera;

        if (bCamera && !cl_syncinput)
        {
            gamesetinput = cl_syncinput = true;
            GrabPalette();
        }
    }
    if (gamesetinput && !bCamera)
    {
        gamesetinput = cl_syncinput = false;
    }
    return CCMD_OK;
}


static int osdcmd_noop(CCmdFuncPtr parm)
{
	// this is for silencing key bindings only.
	return CCMD_OK;
}

int32_t registerosdcommands(void)
{
    //if (VOLUMEONE)
    C_RegisterFunction("doors", "opens/closes doors", osdcmd_doors);
    C_RegisterFunction("spawn","spawn <creaturetype>: spawns a creature",osdcmd_spawn);
    C_RegisterFunction("warptocoords","warptocoords [x] [y] [z] [ang] (optional) [horiz] (optional): warps the player to the specified coordinates",osdcmd_warptocoords);
    C_RegisterFunction("third_person_view", "Switch to third person view", osdcmd_third_person_view);
	C_RegisterFunction("coop_view", "Switch player to view from in coop", osdcmd_noop);
	C_RegisterFunction("show_weapon", "Show opponents' weapons", osdcmd_noop);

    return 0;
}


END_PS_NS
