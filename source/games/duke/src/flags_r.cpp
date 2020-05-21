//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2017-2019 Nuke.YKT

This file is part of Duke Nukem 3D version 1.5 - Atomic Edition

Duke Nukem 3D is free software, you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program, if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Original Source, 1996 - Todd Replogle
Prepared for public release, 03/21/2003 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------

#include "ns.h"
#include "global.h"
#include "actors.h"
#include "names_rr.h"

BEGIN_DUKE_NS

void initactorflags_r()
{
	setflag(SFLAG_INTERNAL_BADGUY|SFLAG_KILLCOUNT, {
		BOULDER,
		BOULDER1,
		EGG,
		RAT,
		TORNADO,
		BILLYCOCK,
		BILLYRAY,
		BILLYRAYSTAYPUT,
		BRAYSNIPER,
		DOGRUN,
		LTH,
		HULKJUMP,
		BUBBASTAND,
		HULK,
		HULKSTAYPUT,
		HEN,
		DRONE,
		PIG,
		RECON,
		MINION,
		MINIONSTAYPUT,
		COOT,
		COOTSTAYPUT,
		SHARK,
		VIXEN});
			
	if (isRRRA()) setflag(SFLAG_INTERNAL_BADGUY|SFLAG_KILLCOUNT, {
		SBSWIPE,
		BIKERB,
		BIKERBV2,
		BIKER,
		MAKEOUT,
		CHEERB,
		CHEER,
		CHEERSTAYPUT,
		COOTPLAY,
		BILLYPLAY,
		MINIONBOAT,
		HULKBOAT,
		CHEERBOAT,
		RABBIT,
		MAMA,
		UFO1_RRRA});
	else setflag(SFLAG_INTERNAL_BADGUY|SFLAG_KILLCOUNT, {
		SBMOVE,
		UFO1_RR,
		UFO2,
		UFO3,
		UFO4,
		UFO5});

	// Why does the pig count as kill? Let's undo that.
	actorinfo[PIG].flags &= ~SFLAG_KILLCOUNT;
	
	actorinfo[DRONE].flags |= SFLAG_NOWATERDIP;
	actorinfo[VIXEN].flags |= SFLAG_NOCANSEECHECK;
	if (isRRRA()) 
	{
		setflag(SFLAG_KILLCOUNT, { ROCK, ROCK2 });	//???
		setflag(SFLAG_NODAMAGEPUSH, { HULK, MAMA, BILLYPLAY, COOTPLAY, MAMACLOUD });
		setflag(SFLAG_NOCANSEECHECK, { COOT, COOTSTAYPUT, BIKERB, BIKERBV2, CHEER, CHEERB,
            CHEERSTAYPUT, MINIONBOAT, HULKBOAT, CHEERBOAT, RABBIT, COOTPLAY, BILLYPLAY, MAKEOUT, MAMA });
	}
	else
	{
		setflag(SFLAG_NODAMAGEPUSH, { HULK, SBMOVE });
	}

	settileflag(TFLAG_WALLSWITCH, {
        HANDPRINTSWITCH,
        HANDPRINTSWITCH+1,
        ALIENSWITCH,
        ALIENSWITCH+1,
        MULTISWITCH,
        MULTISWITCH+1,
        MULTISWITCH+2,
        MULTISWITCH+3,
        ACCESSSWITCH,
        ACCESSSWITCH2,
        PULLSWITCH,
        PULLSWITCH+1,
        HANDSWITCH,
        HANDSWITCH+1,
        SLOTDOOR,
        SLOTDOOR+1,
        LIGHTSWITCH,
        LIGHTSWITCH+1,
        SPACELIGHTSWITCH,
        SPACELIGHTSWITCH+1,
        SPACEDOORSWITCH,
        SPACEDOORSWITCH+1,
        FRANKENSTINESWITCH,
        FRANKENSTINESWITCH+1,
        LIGHTSWITCH2,
        LIGHTSWITCH2+1,
        POWERSWITCH1,
        POWERSWITCH1+1,
        LOCKSWITCH1,
        LOCKSWITCH1+1,
        POWERSWITCH2,
        POWERSWITCH2+1,
        DIPSWITCH,
        DIPSWITCH+1,
        DIPSWITCH2,
        DIPSWITCH2+1,
        TECHSWITCH,
        TECHSWITCH+1,
        DIPSWITCH3,
        DIPSWITCH3+1,
        NUKEBUTTON,
		NUKEBUTTON+1});
	if (isRRRA()) settileflag(TFLAG_WALLSWITCH, {
        MULTISWITCH2,
        MULTISWITCH2+1,
        MULTISWITCH2+2,
        MULTISWITCH2+3,
        RRTILE8464,
		RRTILE8464+1});

	setflag(SFLAG_INVENTORY, {
		FIRSTAID,
		STEROIDS,
		HEATSENSOR,
		BOOTS,
		JETPACK,
		HOLODUKE,
		AIRTANK });

	setflag(SFLAG_ADULT, {
		FEM10,
		NAKED1,
		FEMMAG1,
		FEMMAG2,
		STATUE,
		STATUEFLASH,
		OOZ,
		WALLBLOOD1,
		WALLBLOOD2,
		WALLBLOOD3,
		WALLBLOOD4,
		WALLBLOOD5,
		SUSHIPLATE1,
		SUSHIPLATE2,
		SUSHIPLATE3,
		SUSHIPLATE4,
		DOLPHIN1,
		DOLPHIN2,
		TOUGHGAL });

	actorinfo[RPG2].flags |= SFLAG_FORCEAUTOAIM;

	weaponsandammosprites[0] = RPGSPRITE;
	weaponsandammosprites[1] = CHAINGUNSPRITE;
	weaponsandammosprites[2] = DEVISTATORAMMO;
	weaponsandammosprites[3] = RPGAMMO;
	weaponsandammosprites[4] = RPGAMMO;
	weaponsandammosprites[5] = JETPACK;
	weaponsandammosprites[6] = SHIELD;
	weaponsandammosprites[7] = FIRSTAID;
	weaponsandammosprites[8] = STEROIDS;
	weaponsandammosprites[9] = RPGAMMO;
	weaponsandammosprites[10] = RPGAMMO;
	weaponsandammosprites[11] = RPGSPRITE;
	weaponsandammosprites[12] = RPGAMMO;
	weaponsandammosprites[13] = FREEZESPRITE;
	weaponsandammosprites[14] = FREEZEAMMO;

}

END_DUKE_NS