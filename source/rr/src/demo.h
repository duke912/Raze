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

#ifndef demo_h_
#define demo_h_

#include "compat.h"
#include "files.h"

BEGIN_RR_NS


#define DEMOFN_FMT "edemo%03d.edm"
#define LDEMOFN_FMT "demo%d.dmo"
#define MAXDEMOS 1000

extern FILE *g_demo_filePtr;
extern char g_firstDemoFile[BMAX_PATH];

extern int32_t g_demo_cnt;
extern int32_t g_demo_goalCnt;
extern int32_t g_demo_paused;
extern FileReader g_demo_recFilePtr;
extern int32_t g_demo_rewind;
extern int32_t g_demo_showStats;
extern int32_t g_demo_totalCnt;

int32_t G_PlaybackDemo(void);
void Demo_PrepareWarp(void);
void G_CloseDemoWrite(void);
void G_DemoRecord(void);
void G_OpenDemoWrite(void);

void Demo_PlayFirst(int32_t prof, int32_t exitafter);
void Demo_SetFirst(const char *demostr);

int32_t Demo_IsProfiling(void);

#if KRANDDEBUG
int32_t krd_print(const char *filename);
void krd_enable(int32_t which);
#endif

END_RR_NS

#endif
