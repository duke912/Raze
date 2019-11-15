/*
** c_dispatch.cpp
** Functions for executing console commands and aliases
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
** Copyright 2019 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "c_buttons.h"
#include "printf.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "gamecontrol.h"

ButtonMap buttonMap;

struct ButtonDesc
{
	int index;
	const char *name;
};

static const ButtonDesc gamefuncs[] = {
	{ gamefunc_Move_Forward, "Move_Forward"},
	{ gamefunc_Move_Backward, "Move_Backward"},
	{ gamefunc_Turn_Left, "Turn_Left"},
	{ gamefunc_Turn_Right, "Turn_Right"},
	{ gamefunc_Strafe, "Strafe"},
	{ gamefunc_Fire, "Fire"},
	{ gamefunc_Open, "Open"},
	{ gamefunc_Run, "Run"},
	{ gamefunc_Alt_Fire, "Alt_Fire"},
	{ gamefunc_Jump, "Jump"},
	{ gamefunc_Crouch, "Crouch"},
	{ gamefunc_Look_Up, "Look_Up"},
	{ gamefunc_Look_Down, "Look_Down"},
	{ gamefunc_Look_Left, "Look_Left"},
	{ gamefunc_Look_Right, "Look_Right"},
	{ gamefunc_Strafe_Left, "Strafe_Left"},
	{ gamefunc_Strafe_Right, "Strafe_Right"},
	{ gamefunc_Aim_Up, "Aim_Up"},
	{ gamefunc_Aim_Down, "Aim_Down"},
	{ gamefunc_Weapon_1, "Weapon_1"},
	{ gamefunc_Weapon_2, "Weapon_2"},
	{ gamefunc_Weapon_3, "Weapon_3"},
	{ gamefunc_Weapon_4, "Weapon_4"},
	{ gamefunc_Weapon_5, "Weapon_5"},
	{ gamefunc_Weapon_6, "Weapon_6"},
	{ gamefunc_Weapon_7, "Weapon_7"},
	{ gamefunc_Weapon_8, "Weapon_8"},
	{ gamefunc_Weapon_9, "Weapon_9"},
	{ gamefunc_Weapon_10, "Weapon_10"},
	{ gamefunc_Inventory, "Inventory"},
	{ gamefunc_Inventory_Left, "Inventory_Left"},
	{ gamefunc_Inventory_Right, "Inventory_Right"},
	{ gamefunc_Holo_Duke, "Holo_Duke"},
	{ gamefunc_Jetpack, "Jetpack"},
	{ gamefunc_NightVision, "NightVision"},
	{ gamefunc_MedKit, "MedKit"},
	{ gamefunc_TurnAround, "Turn_Around"},
	{ gamefunc_SendMessage, "Send_Message"},
	{ gamefunc_Map, "Map"},
	{ gamefunc_Shrink_Screen, "Shrink_Screen"},
	{ gamefunc_Enlarge_Screen, "Enlarge_Screen"},
	{ gamefunc_Center_View, "Center_View"},
	{ gamefunc_Holster_Weapon, "Holster_Weapon"},
	{ gamefunc_Show_Opponents_Weapon, "Show_Opponents_Weapon"},
	{ gamefunc_Map_Follow_Mode, "Map_Follow_Mode"},
	{ gamefunc_See_Coop_View, "See_Coop_View"},
	{ gamefunc_Mouse_Aiming, "Mouse_Aiming"},
	{ gamefunc_Toggle_Crosshair, "Toggle_Crosshair"},
	{ gamefunc_Steroids, "Steroids"},
	{ gamefunc_Quick_Kick, "Quick_Kick"},
	{ gamefunc_Next_Weapon, "Next_Weapon"},
	{ gamefunc_Previous_Weapon, "Previous_Weapon"},
	{ gamefunc_Show_Console, "Show_Console"},
	{ gamefunc_Show_DukeMatch_Scores, "Show_DukeMatch_Scores"},
	{ gamefunc_Dpad_Select, "Dpad_Select"},
	{ gamefunc_Dpad_Aiming, "Dpad_Aiming"},
	{ gamefunc_AutoRun, "AutoRun"},
	{ gamefunc_Last_Weapon, "Last_Used_Weapon"},
	{ gamefunc_Quick_Save, "Quick_Save"},
	{ gamefunc_Quick_Load, "Quick_Load"},
	{ gamefunc_Alt_Weapon, "Alt_Weapon"},
	{ gamefunc_Third_Person_View, "Third_Person_View"},
	{ gamefunc_Toggle_Crouch, "Toggle_Crouch"},
	{ gamefunc_See_Chase_View, "See_Chase_View"},	// the following were added by Blood
	{ gamefunc_BeastVision, "BeastVision"},
	{ gamefunc_CrystalBall, "CrystalBall"},
	{ gamefunc_JumpBoots, "JumpBoots"},
	{ gamefunc_ProximityBombs, "ProximityBombs"},
	{ gamefunc_RemoteBombs, "RemoteBombs"},
	{ gamefunc_Smoke_Bomb, "Smoke_Bomb" },
	{ gamefunc_Gas_Bomb, "Gas_Bomb" },
	{ gamefunc_Flash_Bomb, "Flash_Bomb" },
	{ gamefunc_Caltrops, "Calitrops" },

};

static const ButtonDesc gamealiases_Duke3D[] = {
	{ gamefunc_BeastVision, ""},
	{ gamefunc_CrystalBall, ""},
	{ gamefunc_ProximityBombs, ""},
	{ gamefunc_RemoteBombs, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },

};

static const ButtonDesc gamealiases_Nam[] = {
	{ gamefunc_Holo_Duke, "Holo_Soldier"},
	{ gamefunc_Jetpack, "Huey"},
	{ gamefunc_Steroids, "Tank_Mode"},
	{ gamefunc_Show_DukeMatch_Scores, "Show_GruntMatch_Scores"},
	{ gamefunc_BeastVision, ""},
	{ gamefunc_CrystalBall, ""},
	{ gamefunc_ProximityBombs, ""},
	{ gamefunc_RemoteBombs, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },

};

static const ButtonDesc gamealiases_WW2GI[] = {
	{ gamefunc_Holo_Duke, "Fire Mission"},
	{ gamefunc_Jetpack, ""},
	{ gamefunc_Steroids, "Smokes"},
	{ gamefunc_Show_DukeMatch_Scores, "Show_GIMatch_Scores"},
	{ gamefunc_BeastVision, ""},
	{ gamefunc_CrystalBall, ""},
	{ gamefunc_ProximityBombs, ""},
	{ gamefunc_RemoteBombs, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },
};

static const ButtonDesc gamealiases_RR[] = {
	{ gamefunc_Holo_Duke, "Beer"},
	{ gamefunc_Jetpack, "Cow Pie"},
	{ gamefunc_NightVision, "Yeehaa"},
	{ gamefunc_MedKit, "Whiskey"},
	{ gamefunc_Steroids, "Moonshine"},
	{ gamefunc_Quick_Kick, "Pee"},
	{ gamefunc_Show_DukeMatch_Scores, "Show_Scores"},
	{ gamefunc_Alt_Fire, ""},
	{ gamefunc_BeastVision, ""},
	{ gamefunc_CrystalBall, ""},
	{ gamefunc_ProximityBombs, ""},
	{ gamefunc_RemoteBombs, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },
};

static const ButtonDesc gamealiases_Blood[] = {
	{ gamefunc_Holo_Duke, ""},
	{ gamefunc_JumpBoots, "JumpBoots"},
	{ gamefunc_Steroids, ""},
	{ gamefunc_Quick_Kick, ""},
	{ gamefunc_Show_DukeMatch_Scores, ""},
	{ gamefunc_Alt_Weapon, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },

};

static const ButtonDesc gamealiases_SW[] = {
	{ gamefunc_Holo_Duke, ""},
	{ gamefunc_Jetpack, ""},
	{ gamefunc_NightVision, ""},
	{ gamefunc_MedKit, ""},
	{ gamefunc_Steroids, ""},
	{ gamefunc_Quick_Kick, ""},
	{ gamefunc_Show_DukeMatch_Scores, ""},
	{ gamefunc_Smoke_Bomb, "" },
	{ gamefunc_Gas_Bomb, "" },
	{ gamefunc_Flash_Bomb, "" },
	{ gamefunc_Caltrops, "" },

};

// This is for use by the tab command builder which can run before the optimized tables are initialized.
const char* StaticGetButtonName(int32_t func)
{
	for (auto& entry : gamefuncs)
		if (entry.index == func) return entry.name;
	return "";
}



//=============================================================================
//
//
//
//=============================================================================

ButtonMap::ButtonMap()
{
	for(auto &gf : gamefuncs)
	{
		NameToNum.Insert(gf.name, gf.index);
		NumToAlias[gf.index] = NumToName[gf.index] = gf.name;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void ButtonMap::SetGameAliases()
{
	// Ion Fury hacks this together from the CON script and uses the same table as Duke Nukem
	if (g_gameType & (GAMEFLAG_DUKE|GAMEFLAG_FURY))
	{
		for (auto& gf : gamealiases_Duke3D)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
	if (g_gameType & GAMEFLAG_NAM)
	{
		for (auto& gf : gamealiases_Nam)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
	if (g_gameType & GAMEFLAG_WW2GI)
	{
		for (auto& gf : gamealiases_WW2GI)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
	if (g_gameType & (GAMEFLAG_RR|GAMEFLAG_RRRA))
	{
		for (auto& gf : gamealiases_RR)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
	if (g_gameType & GAMEFLAG_BLOOD)
	{
		for (auto& gf : gamealiases_Blood)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
	if (g_gameType & GAMEFLAG_SW)
	{
		for (auto& gf : gamealiases_SW)
		{
			NumToAlias[gf.index] = gf.name;
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

int ButtonMap::ListActionCommands (const char *pattern)
{
	char matcher[32];
	int count = 0;

	for (int i = 0; i < NumButtons(); i++)
	{
		if (NumToAlias[i].IsEmpty()) continue;	// do not list buttons that were removed from the alias list
		
		if (pattern == NULL || CheckWildcards (pattern,
			(snprintf (matcher, countof(matcher), "+%s", NumToName[i].GetChars()), matcher)))
		{
			Printf ("+%s\n", NumToName[i]);
			count++;
		}
		if (pattern == NULL || CheckWildcards (pattern,
			(snprintf (matcher, countof(matcher), "-%s", NumToName[i].GetChars()), matcher)))
		{
			Printf ("-%s\n", NumToName[i]);
			count++;
		}
	}
	return count;
}


//=============================================================================
//
//
//
//=============================================================================

int ButtonMap::FindButtonIndex (const char *key) const
{
    if (!key) return -1;
	
	FName name(key, true);
	if (name == NAME_None) return -1;
	
	auto res = NameToNum.CheckKey(name);
	if (!res) return -1;
	
	return *res;
}


//=============================================================================
//
//
//
//=============================================================================

void ButtonMap::ResetButtonTriggers ()
{
	for (auto &button : Buttons)
	{
		button.ResetTriggers ();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void ButtonMap::ResetButtonStates ()
{
	for (auto &button : Buttons)
	{
		button.ReleaseKey (0);
		button.ResetTriggers ();
	}
}

//=============================================================================
//
//
//
//=============================================================================

void ButtonMap::SetButtonAlias(int num, const char *text)
{
    if ((unsigned)num >= (unsigned)NUMGAMEFUNCTIONS)
        return;
	NumToAlias[num] = text;
	NameToNum.Insert(text, num);
}

//=============================================================================
//
//
//
//=============================================================================

void ButtonMap::ClearButtonAlias(int num)
{
    if ((unsigned)num >= (unsigned)NUMGAMEFUNCTIONS)
        return;
	NumToAlias[num] = "";
}


//=============================================================================
//
//
//
//=============================================================================

bool FButtonStatus::PressKey (int keynum)
{
	int i, open;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force on
		Keys[0] = 0xffff;
		for (i = MAX_KEYS-1; i > 0; --i)
		{
			Keys[i] = 0;
		}
	}
	else
	{
		for (i = MAX_KEYS-1, open = -1; i >= 0; --i)
		{
			if (Keys[i] == 0)
			{
				open = i;
			}
			else if (Keys[i] == keynum)
			{ // Key is already down; do nothing
				return false;
			}
		}
		if (open < 0)
		{ // No free key slots, so do nothing
			Printf ("More than %u keys pressed for a single action!\n", MAX_KEYS);
			return false;
		}
		Keys[open] = keynum;
	}
	uint8_t wasdown = bDown;
	bDown = bWentDown = true;
	// Returns true if this key caused the button to go down.
	return !wasdown;
}

//=============================================================================
//
//
//
//=============================================================================

bool FButtonStatus::ReleaseKey (int keynum)
{
	int i, numdown, match;
	uint8_t wasdown = bDown;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force off
		for (i = MAX_KEYS-1; i >= 0; --i)
		{
			Keys[i] = 0;
		}
		bWentUp = true;
		bDown = false;
	}
	else
	{
		for (i = MAX_KEYS-1, numdown = 0, match = -1; i >= 0; --i)
		{
			if (Keys[i] != 0)
			{
				++numdown;
				if (Keys[i] == keynum)
				{
					match = i;
				}
			}
		}
		if (match < 0)
		{ // Key was not down; do nothing
			return false;
		}
		Keys[match] = 0;
		bWentUp = true;
		if (--numdown == 0)
		{
			bDown = false;
		}
	}
	// Returns true if releasing this key caused the button to go up.
	return wasdown && !bDown;
}
