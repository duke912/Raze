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

#include "build.h"
#include "pragmas.h"
#include "mmulti.h"
#include "common_game.h"

#include "actor.h"
#include "ai.h"
#include "blood.h"
#include "db.h"
#include "dude.h"
#include "levels.h"
#include "player.h"
#include "seq.h"
#include "sound.h"

BEGIN_BLD_NS

static void eelBiteSeqCallback(int, int);
static void eelThinkTarget(spritetype *, XSPRITE *);
static void eelThinkSearch(spritetype *, XSPRITE *);
static void eelThinkGoto(spritetype *, XSPRITE *);
static void eelThinkPonder(spritetype *, XSPRITE *);
static void eelMoveDodgeUp(spritetype *, XSPRITE *);
static void eelMoveDodgeDown(spritetype *, XSPRITE *);
static void eelThinkChase(spritetype *, XSPRITE *);
static void eelMoveForward(spritetype *, XSPRITE *);
static void eelMoveSwoop(spritetype *, XSPRITE *);
static void eelMoveAscend(spritetype *pSprite, XSPRITE *pXSprite);
static void eelMoveToCeil(spritetype *, XSPRITE *);

static int nEelBiteClient = seqRegisterClient(eelBiteSeqCallback);

AISTATE eelIdle = { kAiStateIdle, 0, -1, 0, NULL, NULL, eelThinkTarget, NULL };
AISTATE eelFlyIdle = { kAiStateIdle, 0, -1, 0, NULL, NULL, eelThinkTarget, NULL };
AISTATE eelChase = { kAiStateChase, 0, -1, 0, NULL, eelMoveForward, eelThinkChase, &eelIdle };
AISTATE eelPonder = { kAiStateOther, 0, -1, 0, NULL, NULL, eelThinkPonder, NULL };
AISTATE eelGoto = { kAiStateMove, 0, -1, 600, NULL, NULL, eelThinkGoto, &eelIdle };
AISTATE eelBite = { kAiStateChase, 7, nEelBiteClient, 60, NULL, NULL, NULL, &eelChase };
AISTATE eelRecoil = { kAiStateRecoil, 5, -1, 0, NULL, NULL, NULL, &eelChase };
AISTATE eelSearch = { kAiStateSearch, 0, -1, 120, NULL, eelMoveForward, eelThinkSearch, &eelIdle };
AISTATE eelSwoop = { kAiStateOther, 0, -1, 60, NULL, eelMoveSwoop, eelThinkChase, &eelChase };
AISTATE eelFly = { kAiStateMove, 0, -1, 0, NULL, eelMoveAscend, eelThinkChase, &eelChase };
AISTATE eelTurn = { kAiStateMove, 0, -1, 60, NULL, aiMoveTurn, NULL, &eelChase };
AISTATE eelHide = { kAiStateOther, 0, -1, 0, NULL, eelMoveToCeil, eelMoveForward, NULL };
AISTATE eelDodgeUp = { kAiStateMove, 0, -1, 120, NULL, eelMoveDodgeUp, NULL, &eelChase };
AISTATE eelDodgeUpRight = { kAiStateMove, 0, -1, 90, NULL, eelMoveDodgeUp, NULL, &eelChase };
AISTATE eelDodgeUpLeft = { kAiStateMove, 0, -1, 90, NULL, eelMoveDodgeUp, NULL, &eelChase };
AISTATE eelDodgeDown = { kAiStateMove, 0, -1, 120, NULL, eelMoveDodgeDown, NULL, &eelChase };
AISTATE eelDodgeDownRight = { kAiStateMove, 0, -1, 90, NULL, eelMoveDodgeDown, NULL, &eelChase };
AISTATE eelDodgeDownLeft = { kAiStateMove, 0, -1, 90, NULL, eelMoveDodgeDown, NULL, &eelChase };

static void eelBiteSeqCallback(int, int nXSprite)
{
    XSPRITE *pXSprite = &xsprite[nXSprite];
    spritetype *pSprite = &sprite[pXSprite->reference];
    spritetype *pTarget = &sprite[pXSprite->target];
    int dx = CosScale16(pSprite->ang);
    int dy = SinScale16(pSprite->ang);
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    DUDEINFO *pDudeInfoT = getDudeInfo(pTarget->type);
    int height = (pSprite->yrepeat*pDudeInfo->eyeHeight)<<2;
    int height2 = (pTarget->yrepeat*pDudeInfoT->eyeHeight)<<2;
    /*
     * workaround for 
     * pXSprite->target >= 0 && pXSprite->target < kMaxSprites in file NBlood/source/blood/src/aiboneel.cpp at line 86
     * The value of pXSprite->target is -1. 
     * copied from lines 177:181
     * resolves this case, but may cause other issues? 
     */
    if (pXSprite->target == -1)
    {
        aiNewState(pSprite, pXSprite, &eelSearch);
        return;
    }
    assert(pXSprite->target >= 0 && pXSprite->target < kMaxSprites);
    actFireVector(pSprite, 0, 0, dx, dy, height2-height, VECTOR_TYPE_7);
}

static void eelThinkTarget(spritetype *pSprite, XSPRITE *pXSprite)
{
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    DUDEEXTRA_at6_u1 *pDudeExtraE = &gDudeExtra[pSprite->extra].at6.u1;
    if (pDudeExtraE->at8 && pDudeExtraE->Kills < 10)
        pDudeExtraE->Kills++;
    else if (pDudeExtraE->Kills >= 10 && pDudeExtraE->at8)
    {
        pDudeExtraE->Kills = 0;
        pXSprite->goalAng += 256;
        POINT3D *pTarget = &baseSprite[pSprite->index];
        aiSetTarget(pXSprite, pTarget->x, pTarget->y, pTarget->z);
        aiNewState(pSprite, pXSprite, &eelTurn);
        return;
    }
    if (Chance(pDudeInfo->alertChance))
    {
        for (int p = connecthead; p >= 0; p = connectpoint2[p])
        {
            PLAYER *pPlayer = &gPlayer[p];
            if (pPlayer->pXSprite->health == 0 || powerupCheck(pPlayer, kPwUpShadowCloak) > 0)
                continue;
            int x = pPlayer->pSprite->x;
            int y = pPlayer->pSprite->y;
            int z = pPlayer->pSprite->z;
            int nSector = pPlayer->pSprite->sectnum;
            int dx = x-pSprite->x;
            int dy = y-pSprite->y;
            int nDist = approxDist(dx, dy);
            if (nDist > pDudeInfo->seeDist && nDist > pDudeInfo->hearDist)
                continue;
            if (!cansee(x, y, z, nSector, pSprite->x, pSprite->y, pSprite->z-((pDudeInfo->eyeHeight*pSprite->yrepeat)<<2), pSprite->sectnum))
                continue;
            int nDeltaAngle = ((getangle(dx,dy)+1024-pSprite->ang)&2047)-1024;
            if (nDist < pDudeInfo->seeDist && klabs(nDeltaAngle) <= pDudeInfo->periphery)
            {
                pDudeExtraE->Kills = 0;
                aiSetTarget(pXSprite, pPlayer->nSprite);
                aiActivateDude(pSprite, pXSprite);
            }
            else if (nDist < pDudeInfo->hearDist)
            {
                pDudeExtraE->Kills = 0;
                aiSetTarget(pXSprite, x, y, z);
                aiActivateDude(pSprite, pXSprite);
            }
            else
                continue;
            break;
        }
    }
}

static void eelThinkSearch(spritetype *pSprite, XSPRITE *pXSprite)
{
    aiChooseDirection(pSprite, pXSprite, pXSprite->goalAng);
    eelThinkTarget(pSprite, pXSprite);
}

static void eelThinkGoto(spritetype *pSprite, XSPRITE *pXSprite)
{
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int dx = pXSprite->targetX-pSprite->x;
    int dy = pXSprite->targetY-pSprite->y;
    int nAngle = getangle(dx, dy);
    int nDist = approxDist(dx, dy);
    aiChooseDirection(pSprite, pXSprite, nAngle);
    if (nDist < 512 && klabs(pSprite->ang - nAngle) < pDudeInfo->periphery)
        aiNewState(pSprite, pXSprite, &eelSearch);
    eelThinkTarget(pSprite, pXSprite);
}

static void eelThinkPonder(spritetype *pSprite, XSPRITE *pXSprite)
{
    if (pXSprite->target == -1)
    {
        aiNewState(pSprite, pXSprite, &eelSearch);
        return;
    }
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    assert(pXSprite->target >= 0 && pXSprite->target < kMaxSprites);
    spritetype *pTarget = &sprite[pXSprite->target];
    XSPRITE *pXTarget = &xsprite[pTarget->extra];
    int dx = pTarget->x-pSprite->x;
    int dy = pTarget->y-pSprite->y;
    aiChooseDirection(pSprite, pXSprite, getangle(dx, dy));
    if (pXTarget->health == 0)
    {
        aiNewState(pSprite, pXSprite, &eelSearch);
        return;
    }
    int nDist = approxDist(dx, dy);
    if (nDist <= pDudeInfo->seeDist)
    {
        int nDeltaAngle = ((getangle(dx,dy)+1024-pSprite->ang)&2047)-1024;
        int height = (pDudeInfo->eyeHeight*pSprite->yrepeat)<<2;
        int height2 = (getDudeInfo(pTarget->type)->eyeHeight*pTarget->yrepeat)<<2;
        int top, bottom;
        GetSpriteExtents(pSprite, &top, &bottom);
        if (cansee(pTarget->x, pTarget->y, pTarget->z, pTarget->sectnum, pSprite->x, pSprite->y, pSprite->z - height, pSprite->sectnum))
        {
            aiSetTarget(pXSprite, pXSprite->target);
            if (height2-height < -0x2000 && nDist < 0x1800 && nDist > 0xc00 && klabs(nDeltaAngle) < 85)
                aiNewState(pSprite, pXSprite, &eelDodgeUp);
            else if (height2-height > 0xccc && nDist < 0x1800 && nDist > 0xc00 && klabs(nDeltaAngle) < 85)
                aiNewState(pSprite, pXSprite, &eelDodgeDown);
            else if (height2-height < 0xccc && nDist < 0x399 && klabs(nDeltaAngle) < 85)
                aiNewState(pSprite, pXSprite, &eelDodgeUp);
            else if (height2-height > 0xccc && nDist < 0x1400 && nDist > 0x800 && klabs(nDeltaAngle) < 85)
                aiNewState(pSprite, pXSprite, &eelDodgeDown);
            else if (height2-height < -0x2000 && nDist < 0x1400 && nDist > 0x800 && klabs(nDeltaAngle) < 85)
                aiNewState(pSprite, pXSprite, &eelDodgeUp);
            else if (height2-height < -0x2000 && klabs(nDeltaAngle) < 85 && nDist > 0x1400)
                aiNewState(pSprite, pXSprite, &eelDodgeUp);
            else if (height2-height > 0xccc)
                aiNewState(pSprite, pXSprite, &eelDodgeDown);
            else
                aiNewState(pSprite, pXSprite, &eelDodgeUp);
            return;
        }
    }
    aiNewState(pSprite, pXSprite, &eelGoto);
    pXSprite->target = -1;
}

static void eelMoveDodgeUp(spritetype *pSprite, XSPRITE *pXSprite)
{
    int nSprite = pSprite->index;
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nAng = ((pXSprite->goalAng+1024-pSprite->ang)&2047)-1024;
    int nTurnRange = (pDudeInfo->angSpeed<<2)>>4;
    pSprite->ang = (pSprite->ang+ClipRange(nAng, -nTurnRange, nTurnRange))&2047;
    int nCos = Cos(pSprite->ang);
    int nSin = Sin(pSprite->ang);
    int dx = xvel[nSprite];
    int dy = yvel[nSprite];
    int t1 = dmulscale30(dx, nCos, dy, nSin);
    int t2 = dmulscale30(dx, nSin, -dy, nCos);
    if (pXSprite->dodgeDir > 0)
        t2 += pDudeInfo->sideSpeed;
    else
        t2 -= pDudeInfo->sideSpeed;

    xvel[nSprite] = dmulscale30(t1, nCos, t2, nSin);
    yvel[nSprite] = dmulscale30(t1, nSin, -t2, nCos);
    zvel[nSprite] = -0x8000;
}

static void eelMoveDodgeDown(spritetype *pSprite, XSPRITE *pXSprite)
{
    int nSprite = pSprite->index;
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nAng = ((pXSprite->goalAng+1024-pSprite->ang)&2047)-1024;
    int nTurnRange = (pDudeInfo->angSpeed<<2)>>4;
    pSprite->ang = (pSprite->ang+ClipRange(nAng, -nTurnRange, nTurnRange))&2047;
    if (pXSprite->dodgeDir == 0)
        return;
    int nCos = Cos(pSprite->ang);
    int nSin = Sin(pSprite->ang);
    int dx = xvel[nSprite];
    int dy = yvel[nSprite];
    int t1 = dmulscale30(dx, nCos, dy, nSin);
    int t2 = dmulscale30(dx, nSin, -dy, nCos);
    if (pXSprite->dodgeDir > 0)
        t2 += pDudeInfo->sideSpeed;
    else
        t2 -= pDudeInfo->sideSpeed;

    xvel[nSprite] = dmulscale30(t1, nCos, t2, nSin);
    yvel[nSprite] = dmulscale30(t1, nSin, -t2, nCos);
    zvel[nSprite] = 0x44444;
}

static void eelThinkChase(spritetype *pSprite, XSPRITE *pXSprite)
{
    if (pXSprite->target == -1)
    {
        aiNewState(pSprite, pXSprite, &eelGoto);
        return;
    }
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    assert(pXSprite->target >= 0 && pXSprite->target < kMaxSprites);
    spritetype *pTarget = &sprite[pXSprite->target];
    XSPRITE *pXTarget = &xsprite[pTarget->extra];
    int dx = pTarget->x-pSprite->x;
    int dy = pTarget->y-pSprite->y;
    aiChooseDirection(pSprite, pXSprite, getangle(dx, dy));
    if (pXTarget->health == 0)
    {
        aiNewState(pSprite, pXSprite, &eelSearch);
        return;
    }
    if (IsPlayerSprite(pTarget) && powerupCheck(&gPlayer[pTarget->type-kDudePlayer1], kPwUpShadowCloak) > 0)
    {
        aiNewState(pSprite, pXSprite, &eelSearch);
        return;
    }
    int nDist = approxDist(dx, dy);
    if (nDist <= pDudeInfo->seeDist)
    {
        int nDeltaAngle = ((getangle(dx,dy)+1024-pSprite->ang)&2047)-1024;
        int height = (pDudeInfo->eyeHeight*pSprite->yrepeat)<<2;
        int top, bottom;
        GetSpriteExtents(pSprite, &top, &bottom);
        int top2, bottom2;
        GetSpriteExtents(pTarget, &top2, &bottom2);
        if (cansee(pTarget->x, pTarget->y, pTarget->z, pTarget->sectnum, pSprite->x, pSprite->y, pSprite->z - height, pSprite->sectnum))
        {
            if (nDist < pDudeInfo->seeDist && klabs(nDeltaAngle) <= pDudeInfo->periphery)
            {
                aiSetTarget(pXSprite, pXSprite->target);
                if (nDist < 0x399 && top2 > top && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &eelSwoop);
                else if (nDist <= 0x399 && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &eelBite);
                else if (bottom2 > top && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &eelSwoop);
                else if (top2 < top && klabs(nDeltaAngle) < 85)
                    aiNewState(pSprite, pXSprite, &eelFly);
            }
        }
        return;
    }

    pXSprite->target = -1;
    aiNewState(pSprite, pXSprite, &eelSearch);
}

static void eelMoveForward(spritetype *pSprite, XSPRITE *pXSprite)
{
    int nSprite = pSprite->index;
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nAng = ((pXSprite->goalAng+1024-pSprite->ang)&2047)-1024;
    int nTurnRange = (pDudeInfo->angSpeed<<2)>>4;
    pSprite->ang = (pSprite->ang+ClipRange(nAng, -nTurnRange, nTurnRange))&2047;
    int nAccel = (pDudeInfo->frontSpeed-(((4-gGameOptions.nDifficulty)<<26)/120)/120)<<2;
    if (klabs(nAng) > 341)
        return;
    if (pXSprite->target == -1)
        pSprite->ang = (pSprite->ang+256)&2047;
    int dx = pXSprite->targetX-pSprite->x;
    int dy = pXSprite->targetY-pSprite->y;
    int nDist = approxDist(dx, dy);
    if (nDist <= 0x399)
        return;
    int nCos = Cos(pSprite->ang);
    int nSin = Sin(pSprite->ang);
    int vx = xvel[nSprite];
    int vy = yvel[nSprite];
    int t1 = dmulscale30(vx, nCos, vy, nSin);
    int t2 = dmulscale30(vx, nSin, -vy, nCos);
    if (pXSprite->target == -1)
        t1 += nAccel;
    else
        t1 += nAccel>>1;
    xvel[nSprite] = dmulscale30(t1, nCos, t2, nSin);
    yvel[nSprite] = dmulscale30(t1, nSin, -t2, nCos);
}

static void eelMoveSwoop(spritetype *pSprite, XSPRITE *pXSprite)
{
    int nSprite = pSprite->index;
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nAng = ((pXSprite->goalAng+1024-pSprite->ang)&2047)-1024;
    int nTurnRange = (pDudeInfo->angSpeed<<2)>>4;
    pSprite->ang = (pSprite->ang+ClipRange(nAng, -nTurnRange, nTurnRange))&2047;
    int nAccel = (pDudeInfo->frontSpeed-(((4-gGameOptions.nDifficulty)<<26)/120)/120)<<2;
    if (klabs(nAng) > 341)
        return;
    int dx = pXSprite->targetX-pSprite->x;
    int dy = pXSprite->targetY-pSprite->y;
    int nDist = approxDist(dx, dy);
    if (Chance(0x8000) && nDist <= 0x399)
        return;
    int nCos = Cos(pSprite->ang);
    int nSin = Sin(pSprite->ang);
    int vx = xvel[nSprite];
    int vy = yvel[nSprite];
    int t1 = dmulscale30(vx, nCos, vy, nSin);
    int t2 = dmulscale30(vx, nSin, -vy, nCos);
    t1 += nAccel>>1;
    xvel[nSprite] = dmulscale30(t1, nCos, t2, nSin);
    yvel[nSprite] = dmulscale30(t1, nSin, -t2, nCos);
    zvel[nSprite] = 0x22222;
}

static void eelMoveAscend(spritetype *pSprite, XSPRITE *pXSprite)
{
    int nSprite = pSprite->index;
    assert(pSprite->type >= kDudeBase && pSprite->type < kDudeMax);
    DUDEINFO *pDudeInfo = getDudeInfo(pSprite->type);
    int nAng = ((pXSprite->goalAng+1024-pSprite->ang)&2047)-1024;
    int nTurnRange = (pDudeInfo->angSpeed<<2)>>4;
    pSprite->ang = (pSprite->ang+ClipRange(nAng, -nTurnRange, nTurnRange))&2047;
    int nAccel = (pDudeInfo->frontSpeed-(((4-gGameOptions.nDifficulty)<<26)/120)/120)<<2;
    if (klabs(nAng) > 341)
        return;
    int dx = pXSprite->targetX-pSprite->x;
    int dy = pXSprite->targetY-pSprite->y;
    int nDist = approxDist(dx, dy);
    if (Chance(0x4000) && nDist <= 0x399)
        return;
    int nCos = Cos(pSprite->ang);
    int nSin = Sin(pSprite->ang);
    int vx = xvel[nSprite];
    int vy = yvel[nSprite];
    int t1 = dmulscale30(vx, nCos, vy, nSin);
    int t2 = dmulscale30(vx, nSin, -vy, nCos);
    t1 += nAccel>>1;
    xvel[nSprite] = dmulscale30(t1, nCos, t2, nSin);
    yvel[nSprite] = dmulscale30(t1, nSin, -t2, nCos);
    zvel[nSprite] = -0x8000;
}

void eelMoveToCeil(spritetype *pSprite, XSPRITE *pXSprite)
{
    int x = pSprite->x;
    int y = pSprite->y;
    int z = pSprite->z;
    int nSector = pSprite->sectnum;
    if (z - pXSprite->targetZ < 0x1000)
    {
        DUDEEXTRA_at6_u1 *pDudeExtraE = &gDudeExtra[pSprite->extra].at6.u1;
        pDudeExtraE->at8 = 0;
        pSprite->flags = 0;
        aiNewState(pSprite, pXSprite, &eelIdle);
    }
    else
        aiSetTarget(pXSprite, x, y, sector[nSector].ceilingz);
}

END_BLD_NS
