//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
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

#include "ns.h"	// Must come before everything else!

#include "compat.h"
#include "build.h"
#include "pragmas.h"
#include "mmulti.h"
#include "common_game.h"


#include "actor.h"
#include "ai.h"
#include "blood.h"
#include "db.h"
#include "dude.h"
#include "eventq.h"
#include "levels.h"
#include "player.h"
#include "seq.h"
#include "sound.h"
#include "nnexts.h"

BEGIN_BLD_NS

static void houndBiteSeqCallback(int, int);
static void houndBurnSeqCallback(int, int);
static void houndThinkSearch(spritetype *, XSPRITE *);
static void houndThinkGoto(spritetype *, XSPRITE *);
static void houndThinkChase(spritetype *, XSPRITE *);

static int nHoundBiteClient = seqRegisterClient(houndBiteSeqCallback);
static int nHoundBurnClient = seqRegisterClient(houndBurnSeqCallback);

AISTATE houndIdle = { kAiStateIdle, 0, -1, 0, NULL, NULL, aiThinkTarget, NULL };
AISTATE houndSearch = { kAiStateMove, 8, -1, 1800, NULL, aiMoveForward, houndThinkSearch, &houndIdle };
AISTATE houndChase = { kAiStateChase, 8, -1, 0, NULL, aiMoveForward, houndThinkChase, NULL };
AISTATE houndRecoil = { kAiStateRecoil, 5, -1, 0, NULL, NULL, NULL, &houndSearch };
AISTATE houndTeslaRecoil = { kAiStateRecoil, 4, -1, 0, NULL, NULL, NULL, &houndSearch };
AISTATE houndGoto = { kAiStateMove, 8, -1, 600, NULL, aiMoveForward, houndThinkGoto, &houndIdle };
AISTATE houndBite = { kAiStateChase, 6, nHoundBiteClient, 60, NULL, NULL, NULL, &houndChase };
AISTATE houndBurn = { kAiStateChase, 7, nHoundBurnClient, 60, NULL, NULL, NULL, &houndChase };

static void houndBiteSeqCallback(int, int nXSprite)
{
    XSPRITE *pXSprite = &xsprite[nXSprite];
    int nSprite = pXSprite->reference;
    spritetype *pSprite = &sprite[nSprite];
    int dx = CosScale16(pSprite->ang);
    int dy = SinScale16(pSprite->ang);
    ///assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    if (!(pSprite->type >= kDudeBase && pSprite->type < kDudeMax)) {
        Printf(PRINT_HIGH, "pSprite->type >= kDudeBase && pSprite->type < kDudeMax");
        return;
    }

    ///assert(pXSprite->target >= 0 && pXSprite->target < kMaxSprites);
    if (!(pXSprite->target >= 0 && pXSprite->target < kMaxSprites)) {
        Printf(PRINT_HIGH, "pXSprite->target >= 0 && pXSprite->target < kMaxSprites");
        return;
    }
    spritetype *pTarget = &sprite[pXSprite->target];
    #ifdef NOONE_EXTENSIONS
        if (IsPlayerSprite(pTarget) || gModernMap) // allow to hit non-player targets
            actFireVector(pSprite, 0, 0, dx, dy, pTarget->z - pSprite->z, VECTOR_TYPE_15);
    #else
        if (IsPlayerSprite(pTarget))
            actFireVector(pSprite, 0, 0, dx, dy, pTarget->z - pSprite->z, VECTOR_TYPE_15);
    #endif
}

static void houndBurnSeqCallback(int, int nXSprite)
{
    XSPRITE *pXSprite = &xsprite[nXSprite];
    int nSprite = pXSprite->reference;
    spritetype *pSprite = &sprite[nSprite];
    actFireMissile(pSprite, 0, 0, CosScale16(pSprite->ang), SinScale16(pSprite->ang), 0, kMissileFlameHound);
}

static void houndThinkSearch(spritetype *pSprite, XSPRITE *pXSprite)
{
    aiChooseDirection(pSprite, pXSprite, pXSprite->goalAng);
    aiThinkTarget(pSprite, pXSprite);
}

static void houndThinkGoto(spritetype *pSprite, XSPRITE *pXSprite)
{
    ///assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    if (!(pSprite->type >= kDudeBase && pSprite->type < kDudeMax)) {
        Printf(PRINT_HIGH, "pSprite->type >= kDudeBase && pSprite->type < kDudeMax");
        return;
    }
    
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int dx = pXSprite->targetX-pSprite->x;
    int dy = pXSprite->targetY-pSprite->y;
    int nAngle = getangle(dx, dy);
    int nDist = approxDist(dx, dy);
    aiChooseDirection(pSprite, pXSprite, nAngle);
    if (nDist < 512 && klabs(pSprite->ang - nAngle) < pDudeInfo->periphery)
        aiNewState(pSprite, pXSprite, &houndSearch);
    aiThinkTarget(pSprite, pXSprite);
}

static void houndThinkChase(spritetype *pSprite, XSPRITE *pXSprite)
{
    if (pXSprite->target == -1)
    {
        aiNewState(pSprite, pXSprite, &houndGoto);
        return;
    }
    ///assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    if (!(pSprite->type >= kDudeBase && pSprite->type < kDudeMax)) {
        Printf(PRINT_HIGH, "pSprite->type >= kDudeBase && pSprite->type < kDudeMax");
        return;
    }
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    ///assert(pXSprite->target >= 0 && pXSprite->target < kMaxSprites);
    if (!(pXSprite->target >= 0 && pXSprite->target < kMaxSprites)) {
        Printf(PRINT_HIGH, "pXSprite->target >= 0 && pXSprite->target < kMaxSprites");
        return;
    }
    spritetype *pTarget = &sprite[pXSprite->target];
    XSPRITE *pXTarget = &xsprite[pTarget->extra];
    int dx = pTarget->x-pSprite->x;
    int dy = pTarget->y-pSprite->y;
    aiChooseDirection(pSprite, pXSprite, getangle(dx, dy));
    if (pXTarget->health == 0)
    {
        aiNewState(pSprite, pXSprite, &houndSearch);
        return;
    }
    if (IsPlayerSprite(pTarget) && powerupCheck(&gPlayer[pTarget->type-kDudePlayer1], kPwUpShadowCloak) > 0)
    {
        aiNewState(pSprite, pXSprite, &houndSearch);
        return;
    }
    int nDist = approxDist(dx, dy);
    if (nDist <= pDudeInfo->seeDist)
    {
        int nDeltaAngle = ((getangle(dx,dy)+1024-pSprite->ang)&2047)-1024;
        int height = (pDudeInfo->eyeHeight*pSprite->yrepeat)<<2;
        if (cansee(pTarget->x, pTarget->y, pTarget->z, pTarget->sectnum, pSprite->x, pSprite->y, pSprite->z - height, pSprite->sectnum))
        {
            if (nDist < pDudeInfo->seeDist && klabs(nDeltaAngle) <= pDudeInfo->periphery)
            {
                aiSetTarget(pXSprite, pXSprite->target);
                if (nDist < 0xb00 && nDist > 0x500 && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &houndBurn);
                else if(nDist < 0x266 && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &houndBite);
                return;
            }
        }
    }

    aiNewState(pSprite, pXSprite, &houndGoto);
    pXSprite->target = -1;
}

END_BLD_NS
