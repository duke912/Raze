// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
// This file has been modified from Ken Silverman's original release
// by Jonathon Fowler (jf@jonof.id.au)

#include "build.h"
#include "compat.h"
#include "pragmas.h"
#include "osd.h"
#include "cache1d.h"
#include "editor.h"
#include "common.h"

#include "baselayer.h"
#ifdef RENDERTYPEWIN
#include "winlayer.h"
#endif

#include "m32script.h"

#define TIMERINTSPERSECOND 120

#define updatecrc16(crc,dat) (crc = (((crc<<8)&65535)^crctable[((((uint16_t)crc)>>8)&65535)^dat]))
static int32_t crctable[256];
static char kensig[64];

extern const char *ExtGetVer(void);

int8_t m32_clipping=2;

// 0   1     2     3      4       5      6      7
// up, down, left, right, lshift, rctrl, lctrl, space
// 8  9  10    11    12   13
// a, z, pgdn, pgup, [,], [.]
// 14       15     16 17 18   19
// kpenter, enter, =, -, tab, `
uint8_t buildkeys[NUMBUILDKEYS] =
{
    0xc8,0xd0,0xcb,0xcd,0x2a,0x9d,0x1d,0x39,
    0x1e,0x2c,0xd1,0xc9,0x33,0x34,
    0x9c,0x1c,0xd,0xc,0xf,0x29
};

vec3_t pos;
int32_t horiz = 100;
int16_t ang, cursectnum;
int32_t hvel, vel, svel, angvel;

static int32_t mousexsurp = 0, mouseysurp = 0;

int32_t grponlymode = 0;
int32_t graphicsmode = 0;
extern int32_t totalclocklock;

int32_t synctics = 0, lockclock = 0;

// those ones save the respective 3d video vars while in 2d mode
// so that exiting from mapster32 in 2d mode saves the correct ones
double vid_gamma_3d=-1, vid_contrast_3d=-1, vid_brightness_3d=-1;

int32_t xdim2d = 640, ydim2d = 480, xdimgame = 640, ydimgame = 480, bppgame = 8;
int32_t forcesetup = 1;

int32_t g_maxCacheSize = 24<<20;

static int16_t oldmousebstatus = 0;
char game_executable[BMAX_PATH] = DEFAULT_GAME_LOCAL_EXEC;
int32_t zlock = 0x7fffffff, zmode = 0, kensplayerheight = 32;
//int16_t defaultspritecstat = 0;

int16_t localartfreq[MAXTILES];
int16_t localartlookup[MAXTILES], localartlookupnum;

char tempbuf[4096];

char names[MAXTILES][25];
const char *g_namesFileName = "NAMES.H";

int16_t asksave = 0;
int32_t osearchx, osearchy;                               //old search input

int32_t grid = 3, autogrid = 0, gridlock = 1, showtags = 2;
int32_t zoom = 768, gettilezoom = 1;
int32_t lastpm16time = 0;

extern int32_t mapversion;

int16_t highlight[MAXWALLS+MAXSPRITES];
int16_t highlightsector[MAXSECTORS], highlightsectorcnt = -1;
extern char textfont[128][8];

// only valid when highlightsectorcnt>0 and no structural
// modifications (deleting/inserting sectors or points, setting new firstwall)
// have been made
static int16_t onextwall[MAXWALLS];  // onextwall[i]>=0 implies wall[i].nextwall < 0
static void mkonwvalid(void) { chsecptr_onextwall = onextwall; }
static void mkonwinvalid(void) { chsecptr_onextwall = NULL; }
static int32_t onwisvalid(void) { return chsecptr_onextwall != NULL; }

int32_t temppicnum, tempcstat, templotag, temphitag, tempextra;
uint32_t temppal, tempvis, tempxrepeat, tempyrepeat, tempxpanning=0, tempypanning=0;
int32_t tempshade, tempxvel, tempyvel, tempzvel;
char somethingintab = 255;

int32_t mlook = 0, mskip=0;
int32_t revertCTRL=0,scrollamount=3;
int32_t unrealedlook=1, quickmapcycling=1; //PK

char program_origcwd[BMAX_PATH];
const char *mapster32_fullpath;
char *testplay_addparam = 0;

static char boardfilename[BMAX_PATH], selectedboardfilename[BMAX_PATH];
//extern char levelname[BMAX_PATH];  // in astub.c   XXX: clean up this mess!!!

static fnlist_t fnlist;
static CACHE1D_FIND_REC *finddirshigh=NULL, *findfileshigh=NULL;
static int32_t currentlist=0;

//static int32_t repeatcountx, repeatcounty;

static int32_t fillist[640];
// used for fillsector, batch point insertion, backup_highlighted_map
static int32_t tempxyar[MAXWALLS][2];

static int32_t mousx, mousy;
int16_t prefixtiles[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
uint8_t hlsectorbitmap[MAXSECTORS>>3];  // show2dsector is already taken...
static int32_t minhlsectorfloorz, numhlsecwalls;

static uint8_t visited[MAXWALLS>>3];  // used for AlignWalls and trace_loop

typedef struct
{
    int16_t numsectors, numwalls, numsprites;
#ifdef YAX_ENABLE
    int16_t numyaxbunches;
    int16_t *bunchnum;  // [numsectors][2]
    int16_t *ynextwall;  // [numwalls][2]
#endif
    sectortype *sector;
    walltype *wall;
    spritetype *sprite;
} mapinfofull_t;

static int32_t backup_highlighted_map(mapinfofull_t *mapinfo);
static int32_t restore_highlighted_map(mapinfofull_t *mapinfo, int32_t forreal);
static void SaveBoardAndPrintMessage(const char *fn);
static const char *GetSaveBoardFilename(void);

/*
static char scantoasc[128] =
{
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
    'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s',
    'd','f','g','h','j','k','l',';',39,'`',0,92,'z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,32,0,0,0,0,0,0,
    0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1',
    '2','3','0','.',0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
static char scantoascwithshift[128] =
{
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S',
    'D','F','G','H','J','K','L',':',34,'~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,32,0,0,0,0,0,0,
    0,0,0,0,0,0,0,'7','8','9','-','4','5','6','+','1',
    '2','3','0','.',0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
*/

#define eitherALT   (keystatus[0x38]|keystatus[0xb8])
#define eitherCTRL  (keystatus[0x1d]|keystatus[0x9d])
#define eitherSHIFT (keystatus[0x2a]|keystatus[0x36])

#define DOWN_BK(BuildKey) (keystatus[buildkeys[BK_##BuildKey]])

#define CLOCKDIR_CW 0
#define CLOCKDIR_CCW 1

int32_t pk_turnaccel=16;
int32_t pk_turndecel=12;
int32_t pk_uedaccel=3;

int8_t sideview_reversehrot = 0;

char lastpm16buf[156];

//static int32_t checksectorpointer_warn = 0;
static int32_t saveboard_savedtags, saveboard_fixedsprites;

char changechar(char dachar, int32_t dadir, char smooshyalign, char boundcheck);
static int32_t adjustmark(int32_t *xplc, int32_t *yplc, int16_t danumwalls);
static void locktogrid(int32_t *dax, int32_t *day);
static int32_t checkautoinsert(int32_t dax, int32_t day, int16_t danumwalls);
void keytimerstuff(void);
static int32_t clockdir(int16_t wallstart);
static void flipwalls(int16_t numwalls, int16_t newnumwalls);
static int32_t insertpoint(int16_t linehighlight, int32_t dax, int32_t day, int32_t *mapwallnum);
static void deletepoint(int16_t point, int32_t runi);
static int32_t deletesector(int16_t sucksect);
void fixrepeats(int16_t i);
static int16_t loopinside(int32_t x, int32_t y, int16_t startwall);
int32_t fillsector(int16_t sectnum, int32_t fillcolor);  // fillcolor == -1: default (pulsating)
static int16_t whitelinescan(int16_t sucksect, int16_t dalinehighlight);
void printcoords16(int32_t posxe, int32_t posye, int16_t ange);
int32_t drawtilescreen(int32_t pictopleft, int32_t picbox);
void overheadeditor(void);
static int32_t getlinehighlight(int32_t xplc, int32_t yplc, int32_t line);
int32_t fixspritesectors(void);
static int32_t movewalls(int32_t start, int32_t offs);
int32_t loadnames(const char *namesfile, int8_t root);
static void getclosestpointonwall(int32_t x, int32_t y, int32_t dawall, int32_t *nx, int32_t *ny,
                                  int32_t maybe_screen_coord_p);
static void initcrc(void);
int32_t gettile(int32_t tilenum);

static int32_t menuselect(void);
static int32_t menuselect_auto(int32_t); //PK

static int32_t insert_sprite_common(int32_t sucksect, int32_t dax, int32_t day);
static void correct_ornamented_sprite(int32_t i, int32_t hitw);

static int32_t getfilenames(const char *path, const char *kind);

void clearkeys(void) { Bmemset(keystatus,0,sizeof(keystatus)); }

#ifdef USE_OPENGL
static int32_t osdcmd_restartvid(const osdfuncparm_t *parm)
{
    UNREFERENCED_PARAMETER(parm);

    if (qsetmode != 200) return OSDCMD_OK;

    resetvideomode();
    if (setgamemode(fullscreen,xdim,ydim,bpp))
        OSD_Printf("restartvid: Reset failed...\n");

    return OSDCMD_OK;
}
#endif

static int32_t osdcmd_vidmode(const osdfuncparm_t *parm)
{
    int32_t newx = xdim, newy = ydim, newbpp = bpp, newfullscreen = fullscreen;
#ifdef USE_OPENGL
    int32_t tmp;
#endif

    switch (parm->numparms)
    {
#ifdef USE_OPENGL
    case 1:	// bpp switch
        tmp = Batol(parm->parms[0]);
        if (!(tmp==8 || tmp==16 || tmp==32))
            return OSDCMD_SHOWHELP;
        newbpp = tmp;
        break;
    case 4:	// fs, res, bpp switch
        newfullscreen = (Batol(parm->parms[3]) != 0);
    case 3:	// res & bpp switch
        tmp = Batol(parm->parms[2]);
        if (!(tmp==8 || tmp==16 || tmp==32))
            return OSDCMD_SHOWHELP;
        newbpp = tmp;
#endif
    case 2: // res switch
        newx = Batol(parm->parms[0]);
        newy = Batol(parm->parms[1]);
        break;
    default:
        return OSDCMD_SHOWHELP;
    }

    if (qsetmode != 200)
    {
        qsetmodeany(newx,newy);
        xdim2d = xdim;
        ydim2d = ydim;

        begindrawing();	//{{{
        CLEARLINES2D(0, ydim16, 0);
        enddrawing();	//}}}

        ydim16 = ydim-STATUS2DSIZ2;

        return OSDCMD_OK;
    }

    if (setgamemode(newfullscreen,newx,newy,newbpp))
        OSD_Printf("vidmode: Mode change failed!\n");

    xdimgame = newx;
    ydimgame = newy;
    bppgame = newbpp;
    fullscreen = newfullscreen;

    return OSDCMD_OK;
}


#ifdef M32_SHOWDEBUG
char m32_debugstr[64][128];
int32_t m32_numdebuglines=0;

static void M32_drawdebug(void)
{
    int32_t i;
    int32_t x=4, y=8;

#if 0
    {
        static char tstr[128];
        Bsprintf(tstr, "search... stat=%d, sector=%d, wall=%d (%d), isbottom=%d, asksave=%d",
                 searchstat, searchsector, searchwall, searchbottomwall, searchisbottom, asksave);
        printext256(x,y,whitecol,0,tstr,xdimgame>640?0:1);
    }
#endif
    if (m32_numdebuglines>0)
    {
        begindrawing();
        for (i=0; i<m32_numdebuglines && y<ydim-8; i++, y+=8)
            printext256(x,y,whitecol,0,m32_debugstr[i],xdimgame>640?0:1);
        enddrawing();
    }
    m32_numdebuglines=0;
}
#endif

#ifdef YAX_ENABLE
// Check whether bunchnum has exactly one corresponding floor and ceiling
// and return it in this case. If not 1-to-1, return -1.
int32_t yax_is121(int16_t bunchnum, int16_t getfloor)
{
    int32_t i;
    i = headsectbunch[0][bunchnum];
    if (i<0 || nextsectbunch[0][i]>=0)
        return -1;
    i = headsectbunch[1][bunchnum];
    if (i<0 || nextsectbunch[1][i]>=0)
        return -1;

    return headsectbunch[getfloor][bunchnum];
}

static int32_t yax_numsectsinbunch(int16_t bunchnum, int16_t cf)
{
    int32_t i, n=0;

    if (bunchnum<0 || bunchnum>=numyaxbunches)
        return -1;

    for (SECTORS_OF_BUNCH(bunchnum, cf, i))
        n++;

    return n;
}

static void yax_fixreverselinks(int16_t oldwall, int16_t newwall)
{
    int32_t cf, ynw;
    for (cf=0; cf<2; cf++)
    {
        ynw = yax_getnextwall(oldwall, cf);
        if (ynw >= 0)
            yax_setnextwall(ynw, !cf, newwall);
    }
}

static void yax_tweakwalls(int16_t start, int16_t offs)
{
    int32_t i, nw, cf;
    for (i=0; i<numwalls; i++)
        for (cf=0; cf<2; cf++)
        {
            nw = yax_getnextwall(i, cf);
            if (nw >= start)
                yax_setnextwall(i, cf, nw+offs);
        }
}

static void yax_resetbunchnums(void)
{
    int32_t i;

    for (i=0; i<MAXSECTORS; i++)
        yax_setbunches(i, -1, -1);
    yax_update(1);
    yax_updategrays(pos.z);
}

// Whether a wall is constrained by sector extensions.
// If false, it's a wall that you can freely move around,
// attach points to, etc...
static int32_t yax_islockedwall(int16_t line)
{
    return !!(wall[line].cstat&YAX_NEXTWALLBITS);
}

# define DEFAULT_YAX_HEIGHT (2048<<4)
#endif

static void reset_default_mapstate(void)
{
    pos.x = 32768;          //new board!
    pos.y = 32768;
    pos.z = 0;
    ang = 1536;
    cursectnum = -1;

    numsectors = 0;
    numwalls = 0;

    editorzrange[0] = INT32_MIN;
    editorzrange[1] = INT32_MAX;

    initspritelists();
    taglab_init();

#ifdef YAX_ENABLE
    yax_resetbunchnums();
#endif
}

static void m32_keypresscallback(int32_t code, int32_t downp)
{
    UNREFERENCED_PARAMETER(downp);

    g_iReturnVar = code;
    VM_OnEvent(EVENT_KEYPRESS, -1);
}

void M32_ResetFakeRORTiles(void)
{
#ifdef POLYMER
# ifdef YAX_ENABLE
        // END_TWEAK ceiling/floor fake 'TROR' pics, see BEGIN_TWEAK in engine.c
        if (rendmode==4 && showinvisibility)
        {
            int32_t i;

            for (i=0; i<numyaxbunches; i++)
            {
                yax_tweakpicnums(i, YAX_CEILING, 1);
                yax_tweakpicnums(i, YAX_FLOOR, 1);
            }
        }
# endif
#endif
}

void M32_DrawRoomsAndMasks(void)
{
    yax_preparedrawrooms();
    drawrooms(pos.x,pos.y,pos.z,ang,horiz,cursectnum);
    yax_drawrooms(ExtAnalyzeSprites, horiz, cursectnum);

    ExtAnalyzeSprites();
    drawmasks();
    M32_ResetFakeRORTiles();

#ifdef POLYMER
    if (rendmode == 4 && searchit == 2)
    {
        polymer_editorpick();
        drawrooms(pos.x,pos.y,pos.z,ang,horiz,cursectnum);
        ExtAnalyzeSprites();
        drawmasks();
        M32_ResetFakeRORTiles();
    }
#endif
}

#undef STARTUP_SETUP_WINDOW
#if defined RENDERTYPEWIN || (defined RENDERTYPESDL && ((defined __APPLE__ && defined OSX_STARTUPWINDOW) || defined HAVE_GTK2))
# define STARTUP_SETUP_WINDOW
#endif

int32_t app_main(int32_t argc, const char **argv)
{
#ifdef STARTUP_SETUP_WINDOW
    char cmdsetup = 0;
#endif
    char quitflag;
    int32_t i, k;

    pathsearchmode = 1;		// unrestrict findfrompath so that full access to the filesystem can be had

#ifdef USE_OPENGL
    OSD_RegisterFunction("restartvid","restartvid: reinitialize the video mode",osdcmd_restartvid);
    OSD_RegisterFunction("vidmode","vidmode <xdim> <ydim> <bpp> <fullscreen>: immediately change the video mode",osdcmd_vidmode);
#else
    OSD_RegisterFunction("vidmode","vidmode <xdim> <ydim>: immediately change the video mode",osdcmd_vidmode);
#endif
    wm_setapptitle("Mapster32");

    editstatus = 1;
    newaspect_enable = 1;

    if ((i = ExtPreInit(argc,argv)) < 0) return -1;

#ifdef RENDERTYPEWIN
    backgroundidle = 1;
#endif

    boardfilename[0] = 0;
    for (i=1; i<argc; i++)
    {
        if (argv[i][0] == '-')
        {
#ifdef STARTUP_SETUP_WINDOW
            if (!Bstrcmp(argv[i], "-setup")) cmdsetup = 1;
            else
#endif
            if (!Bstrcmp(argv[i], "-help") || !Bstrcmp(argv[i], "--help") || !Bstrcmp(argv[i], "-?"))
            {
                char *s =
                    "Mapster32\n"
                    "Syntax: mapster32 [options] mapname\n"
                    "Options:\n"
                    "\t-grp\tUse an extra GRP or ZIP file.\n"
                    "\t-g\tSame as above.\n"
#ifdef STARTUP_SETUP_WINDOW
                    "\t-setup\tDisplays the configuration dialogue box before entering the editor.\n"
#endif
                    ;
#ifdef STARTUP_SETUP_WINDOW
                wm_msgbox("Mapster32","%s",s);
#else
                puts(s);
#endif
                return 0;
            }
            continue;
        }

        if (!boardfilename[0])
            Bstrncpyz(boardfilename, argv[i], BMAX_PATH);
    }
    if (boardfilename[0] == 0)
    {
        Bstrcpy(boardfilename,"newboard.map");
    }
    else if (Bstrchr(boardfilename,'.') == 0)
    {
        Bstrcat(boardfilename, ".map");
    }
    //Bcanonicalisefilename(boardfilename,0);

    if ((i = ExtInit()) < 0) return -1;
#ifdef STARTUP_SETUP_WINDOW
    if (i || forcesetup || cmdsetup)
    {
        extern int32_t startwin_run(void);

        if (quitevent || !startwin_run())
        {
            uninitengine();
            exit(0);
        }
    }
#endif

    loadnames(g_namesFileName, 1);

    if (initinput()) return -1;

    initmouse();

    inittimer(TIMERINTSPERSECOND);
    installusertimercallback(keytimerstuff);

    loadpics("tiles000.art", g_maxCacheSize);

#ifdef YAX_ENABLE
    // init dummy texture for YAX
    // must be after loadpics(), which inits BUILD's cache

    i = MAXTILES-1;
    if (tilesizx[i]==0 && tilesizy[i]==0)
    {
        static char R[8*16] = { //
            0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
            0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
            0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
            0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0,
            0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0,
            0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0,
            0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0,
        };

        char *newtile;
        int32_t sx=32, sy=32, col, j;

        walock[i] = 255; // permanent tile
        picsiz[i] = 5 + (5<<4);
        tilesizx[i] = sx; tilesizy[i] = sy;
        allocache(&waloff[i], sx*sy, &walock[i]);
        newtile = (char *)waloff[i];

        col = getclosestcol(128>>2, 128>>2, 0);
        for (j=0; j<(signed)sizeof(R); j++)
            R[j] *= col;

        Bmemset(newtile, 0, sx*sy);
        for (j=0; j<8; j++)
            Bmemcpy(&newtile[32*j], &R[16*j], 16);
    }
#endif

    Bstrcpy(kensig,"Uses BUILD technology by Ken Silverman");
    initcrc();

    if (!loaddefinitionsfile(g_defNamePtr))
        initprintf("Definitions file loaded.\n");

    for (i=0; i < g_defModulesNum; ++i)
        Bfree (g_defModules[i]);
    Bfree (g_defModules);
    g_defModules = NULL;  // be defensive...

    // Here used to be the 'whitecol' calculation

#ifdef HAVE_CLIPSHAPE_FEATURE
    k = clipmapinfo_load();
    if (k>0)
        initprintf("There was an error loading the sprite clipping map (status %d).\n", k);

    for (i=0; i < g_clipMapFilesNum; ++i)
        Bfree (g_clipMapFiles[i]);
    Bfree (g_clipMapFiles);
    g_clipMapFiles = NULL;
#endif

    taglab_init();

    mkonwinvalid();

    if (LoadBoard(boardfilename, 1))
        reset_default_mapstate();

    totalclock = 0;

    updatesector(pos.x,pos.y,&cursectnum);

    setkeypresscallback(&m32_keypresscallback);

    if (cursectnum == -1)
    {
        vid_gamma_3d = vid_gamma;
        vid_brightness_3d = vid_brightness;
        vid_contrast_3d = vid_contrast;

        vid_gamma = vid_contrast = 1.0;
        vid_brightness = 0.0;

        setbrightness(0,0,0);
        if (setgamemode(fullscreen, xdim2d, ydim2d, 8) < 0)
        {
            ExtUnInit();
            uninitengine();
            Bprintf("%d * %d not supported in this graphics mode\n",xdim2d,ydim2d);
            exit(0);
        }

        // executed once per init, but after setgamemode so that OSD has the right width
        OSD_Exec("m32_autoexec.cfg");

        overheadeditor();
        keystatus[buildkeys[BK_MODE2D_3D]] = 0;

        vid_gamma = vid_gamma_3d;
        vid_contrast = vid_contrast_3d;
        vid_brightness = vid_brightness_3d;

        vid_gamma_3d = vid_contrast_3d = vid_brightness_3d = -1;

        setbrightness(GAMMA_CALC,0,0);
    }
    else
    {
        if (setgamemode(fullscreen, xdimgame, ydimgame, bppgame) < 0)
        {
            ExtUnInit();
            uninitengine();
            Bprintf("%d * %d not supported in this graphics mode\n",xdim,ydim);
            exit(0);
        }

        // executed once per init, but after setgamemode so that OSD has the right width
        OSD_Exec("m32_autoexec.cfg");

        setbrightness(GAMMA_CALC,0,0);
    }

CANCEL:
    quitflag = 0;
    while (quitflag == 0)
    {
        if (handleevents())
        {
            if (quitevent)
            {
                keystatus[1] = 1;
                quitevent = 0;
            }
        }

        OSD_DispatchQueued();

        nextpage();
        synctics = totalclock-lockclock;
        lockclock += synctics;

        ExtPreCheckKeys();

        M32_DrawRoomsAndMasks();

#ifdef M32_SHOWDEBUG
        if (searchstat>=0 && (searchwall<0 || searchsector<0))
        {
            if (m32_numdebuglines<64)
                Bsprintf(m32_debugstr[m32_numdebuglines++], "inconsistent search variables!");
            searchstat = -1;
        }

        M32_drawdebug();
#endif
        ExtCheckKeys();


        if (keystatus[1])
        {
            keystatus[1] = 0;

            printext256(0,0,whitecol,0,"Are you sure you want to quit?",0);

            showframe(1);
            synctics = totalclock-lockclock;
            lockclock += synctics;

            while ((keystatus[1]|keystatus[0x1c]|keystatus[0x39]|keystatus[0x31]) == 0)
            {
                idle_waitevent();
                if (handleevents())
                {
                    if (quitevent)
                    {
                        quitflag = 1;
                        break;
                    }
                }

                if (keystatus[0x15]||keystatus[0x1c]) // Y or ENTER
                {
                    keystatus[0x15] = 0;
                    keystatus[0x1c] = 0;
                    quitflag = 1; break;
                }
            }
            while (keystatus[1])
            {
                keystatus[1] = 0;
                quitevent = 0;
                goto CANCEL;
            }
        }
    }

    if (asksave)
    {
        i = CheckMapCorruption(4, 0);

        printext256(0,8,whitecol,0,i<4?"Save changes?":"Map is heavily corrupt. Save changes?",0);
        showframe(1);

        while ((keystatus[1]|keystatus[0x1c]|keystatus[0x39]|keystatus[0x31]|keystatus[0x2e]) == 0)
        {
            idle_waitevent();
            if (handleevents()) { if (quitevent) break;	} // like saying no

            if (keystatus[0x15] || keystatus[0x1c]) // Y or ENTER
            {
                keystatus[0x15] = keystatus[0x1c] = 0;

                SaveBoard(NULL, 0);

                break;
            }
        }
        while (keystatus[1]||keystatus[0x2e])
        {
            keystatus[1] = keystatus[0x2e] = 0;
            quitevent = 0;
            goto CANCEL;
        }
    }


    ExtUnInit();
//    clearfilenames();
    uninitengine();

    return(0);
}

/*
void showmouse(void)
{
    int32_t i;

    for (i=1;i<=4;i++)
    {
        plotpixel(searchx+i,searchy,whitecol);
        plotpixel(searchx-i,searchy,whitecol);
        plotpixel(searchx,searchy-i,whitecol);
        plotpixel(searchx,searchy+i,whitecol);
    }
}
*/

static int32_t mhk=0;
static void loadmhk(int32_t domessage)
{
    char *p; char levname[BMAX_PATH];

    if (!mhk)
        return;

    Bstrcpy(levname, boardfilename);
    p = Bstrrchr(levname,'.');
    if (!p)
        Bstrcat(levname,".mhk");
    else
    {
        p[1]='m';
        p[2]='h';
        p[3]='k';
        p[4]=0;
    }

    if (!loadmaphack(levname))
    {
        if (domessage)
            message("Loaded map hack file \"%s\"",levname);
        else
            initprintf("Loaded map hack file \"%s\"\n",levname);
    }
    else
    {
        mhk=2;
        if (domessage)
            message("No maphack found for map \"%s\"",boardfilename);
    }
}

// this is spriteon{ceiling,ground}z from astub.c packed into
// one convenient function
void spriteoncfz(int32_t i, int32_t *czptr, int32_t *fzptr)
{
    int32_t cz,fz, height, zofs;

    getzsofslope(sprite[i].sectnum, sprite[i].x,sprite[i].y, &cz, &fz);
    spriteheightofs(i, &height, &zofs, 0);

    if (czptr)
        *czptr = cz + height - zofs;
    if (fzptr)
        *fzptr = fz - zofs;
}

static void move_and_update(int32_t xvect, int32_t yvect, int32_t addshr)
{
    if (m32_clipping==0)
    {
        pos.x += xvect>>(14+addshr);
        pos.y += yvect>>(14+addshr);
        updatesector(pos.x,pos.y, &cursectnum);
    }
    else
    {
        clipmove(&pos,&cursectnum, xvect>>addshr,yvect>>addshr,
                 128,4<<8,4<<8, (m32_clipping==1) ? 0 : CLIPMASK0);
    }
}

static void mainloop_move(void)
{
    int32_t xvect, yvect, doubvel;

    if (angvel != 0)  //ang += angvel * constant
    {
        //ENGINE calculates angvel for you

        //Lt. shift makes turn velocity 50% faster
        doubvel = (synctics + DOWN_BK(RUN)*(synctics>>1));

        ang += ((angvel*doubvel)>>4);
        ang &= 2047;
    }
    if ((vel|svel) != 0)
    {
        //Lt. shift doubles forward velocity
        doubvel = (1+(DOWN_BK(RUN)))*synctics;

        xvect = 0;
        yvect = 0;

        if (vel != 0)
        {
            xvect += (vel*doubvel*(int32_t)sintable[(ang+2560)&2047])>>3;
            yvect += (vel*doubvel*(int32_t)sintable[(ang+2048)&2047])>>3;
        }
        if (svel != 0)
        {
            xvect += (svel*doubvel*(int32_t)sintable[(ang+2048)&2047])>>3;
            yvect += (svel*doubvel*(int32_t)sintable[(ang+1536)&2047])>>3;
        }

        move_and_update(xvect, yvect, 0);
    }
}

static void handle_sprite_in_clipboard(int32_t i)
{
    if (somethingintab == 3)
    {
        int32_t j, k;

        sprite[i].picnum = temppicnum;
        if (tilesizx[temppicnum] <= 0 || tilesizy[temppicnum] <= 0)
        {
            j = 0;
            for (k=0; k<MAXTILES; k++)
                if (tilesizx[k] > 0 && tilesizy[k] > 0)
                {
                    j = k;
                    break;
                }
            sprite[i].picnum = j;
        }
        sprite[i].shade = tempshade;
        sprite[i].pal = temppal;
        sprite[i].xrepeat = max(tempxrepeat, 1);
        sprite[i].yrepeat = max(tempyrepeat, 1);
        sprite[i].cstat = tempcstat;
    }
}


void editinput(void)
{
    int32_t mousz, bstatus;
    int32_t i, tempint=0;
    int32_t goalz, xvect, yvect, hiz, loz, oposz;
    int32_t dax, day, hihit, lohit, omlook=mlook;

// 3B  3C  3D  3E   3F  40  41  42   43  44  57  58          46
// F1  F2  F3  F4   F5  F6  F7  F8   F9 F10 F11 F12        SCROLL

    mousz = 0;
    getmousevalues(&mousx,&mousy,&bstatus);
    mousx = (mousx<<16) + mousexsurp;
    mousy = (mousy<<16) + mouseysurp;

    if (unrealedlook && !mskip)
    {
        if (mlook==0 && (bstatus&(1|2|4))==2)
            mlook = 3;
        else if ((bstatus&(1|2|4))==1)
            mlook = 3;
    }

    {
        ldiv_t ld;
        if (mlook)
        {
            ld = ldiv(mousx, (int32_t)((1<<16)/(msens*0.5f))); mousx = ld.quot; mousexsurp = ld.rem;
            ld = ldiv(mousy, (int32_t)((1<<16)/(msens*0.25f))); mousy = ld.quot; mouseysurp = ld.rem;
        }
        else
        {
            ld = ldiv(mousx, (int32_t)((1<<16)/msens)); mousx = ld.quot; mousexsurp = ld.rem;
            ld = ldiv(mousy, (int32_t)((1<<16)/msens)); mousy = ld.quot; mouseysurp = ld.rem;
        }
    }

    if (mlook == 3)
        mlook = omlook;

    // UnrealEd:
    // rmb: mouselook
    // lbm: x:turn y:fwd/back local x
    // lmb&rmb: x:strafe y:up/dn (move in local yz plane)
    // mmb: fwd/back in viewing vector

    if (unrealedlook && !mskip)    //PK
    {
        if ((bstatus&(1|2|4))==1)
        {
            ang += mousx;
            xvect = -((mousy*(int32_t)sintable[(ang+2560)&2047])<<(3+pk_uedaccel));
            yvect = -((mousy*(int32_t)sintable[(ang+2048)&2047])<<(3+pk_uedaccel));

            move_and_update(xvect, yvect, 0);
        }
        else if (!mlook && (bstatus&(1|2|4))==2)
        {
            mlook=2;
        }
        else if ((bstatus&(1|2|4))==(1|2))
        {
            zmode = 2;
            xvect = -((mousx*(int32_t)sintable[(ang+2048)&2047])<<pk_uedaccel);
            yvect = -((mousx*(int32_t)sintable[(ang+1536)&2047])<<pk_uedaccel);
            pos.z += mousy<<(4+pk_uedaccel);

            move_and_update(xvect, yvect, 0);
        }
        else if ((bstatus&(1|2|4))==4)
        {
            zmode = 2;

            // horiz-100 of 200 is viewing at 326.4 build angle units (=atan(200/128)) upward
            tempint = getangle(128, horiz-100);

            xvect = -((mousy*
                       ((int32_t)sintable[(ang+2560)&2047]>>6)*
                       ((int32_t)sintable[(tempint+512)&2047])>>6)
                      <<pk_uedaccel);
            yvect = -((mousy*
                       ((int32_t)sintable[(ang+2048)&2047]>>6)*
                       ((int32_t)sintable[(tempint+512)&2047])>>6)
                      <<pk_uedaccel);

            pos.z += mousy*(((int32_t)sintable[(tempint+2048)&2047])>>(10-pk_uedaccel));

            move_and_update(xvect, yvect, 2);
        }
    }

    if (mskip)
    {
        // mskip was set in astub.c to not trigger UEd mouse movements.
        // Reset now.
        mskip = 0;
    }
    else
    {
        if (mlook && (unrealedlook==0 || (bstatus&(1|4))==0))
        {
            ang += mousx;
            horiz -= mousy;

            /*
            if (mousy && !(mousy/4))
                horiz--;
            if (mousx && !(mousx/2))
                ang++;
            */

            inpclamp(&horiz, -99, 299);

            if (mlook == 1)
            {
                searchx = xdim>>1;
                searchy = ydim>>1;
            }
            osearchx = searchx-mousx;
            osearchy = searchy-mousy;
        }
        else if (unrealedlook==0 || (bstatus&(1|2|4))==0)
        {
            osearchx = searchx;
            osearchy = searchy;
            searchx += mousx;
            searchy += mousy;

            inpclamp(&searchx, 12, xdim-13);
            inpclamp(&searchy, 12, ydim-13);
        }
    }

//    showmouse();

    if (keystatus[0x43])  // F9
    {
        if (mhk)
        {
            Bmemset(spriteext, 0, sizeof(spriteext_t) * MAXSPRITES);
            Bmemset(spritesmooth, 0, sizeof(spritesmooth_t) * (MAXSPRITES+MAXUNIQHUDID));
            delete_maphack_lights();
            mhk = 0;
            message("Maphacks disabled");
        }
        else
        {
            mhk = 1;
            loadmhk(1);
        }

        keystatus[0x43] = 0;
    }

    mainloop_move();

    getzrange(&pos,cursectnum, &hiz,&hihit, &loz,&lohit, 128, (m32_clipping==1)?0:CLIPMASK0);
/*
{
    int32_t his = !(hihit&32768), los = !(lohit&32768);
    if (m32_numdebuglines<64)
        Bsprintf(m32_debugstr[m32_numdebuglines++], "s%d: cf[%s%d, %s%d] z(%d, %d)", cursectnum,
                 his?"s":"w",hihit&16383, los?"s":"w",lohit&16383, hiz,loz);
}
*/
    oposz = pos.z;
    if (zmode == 0)
    {
        goalz = loz-(kensplayerheight<<8);  //playerheight pixels above floor
        if (goalz < hiz+(16<<8))  //ceiling&floor too close
            goalz = (loz+hiz)>>1;
        goalz += mousz;

        if (DOWN_BK(MOVEUP))  //A (stand high)
        {
            goalz -= (16<<8);
            if (DOWN_BK(RUN))
                goalz -= (24<<8);
        }
        if (DOWN_BK(MOVEDOWN))  //Z (stand low)
        {
            goalz += (12<<8);
            if (DOWN_BK(RUN))
                goalz += (12<<8);
        }

        if (goalz != pos.z)
        {
            if (pos.z < goalz) hvel += 64;
            if (pos.z > goalz) hvel = ((goalz-pos.z)>>3);

            pos.z += hvel;
            if (pos.z > loz-(4<<8)) pos.z = loz-(4<<8), hvel = 0;
            if (pos.z < hiz+(4<<8)) pos.z = hiz+(4<<8), hvel = 0;
        }
    }
    else
    {
        goalz = pos.z;
        if (DOWN_BK(MOVEUP))  //A
        {
            if (eitherCTRL)
            {
                horiz = max(-100,horiz-((DOWN_BK(RUN)+1)*synctics*2));
            }
            else
            {
                if (zmode != 1)
                    goalz -= (8<<8);
                else
                {
                    zlock += (4<<8);
                    DOWN_BK(MOVEUP) = 0;
                }
            }
        }
        if (DOWN_BK(MOVEDOWN))  //Z (stand low)
        {
            if (eitherCTRL)
            {
                horiz = min(300,horiz+((DOWN_BK(RUN)+1)*synctics*2));
            }
            else
            {
                if (zmode != 1)
                    goalz += (8<<8);
                else if (zlock > 0)
                {
                    zlock -= (4<<8);
                    DOWN_BK(MOVEDOWN) = 0;
                }
            }
        }

        if (m32_clipping)
            inpclamp(&goalz, hiz+(4<<8), loz-(4<<8));

        if (zmode == 1) goalz = loz-zlock;
        if (m32_clipping && (goalz < hiz+(4<<8)))
            goalz = ((loz+hiz)>>1);  //ceiling&floor too close
        if (zmode == 1) pos.z = goalz;

        if (goalz != pos.z)
        {
            //if (pos.z < goalz) hvel += (32<<DOWN_BK(RUN));
            //if (pos.z > goalz) hvel -= (32<<DOWN_BK(RUN));
            if (pos.z < goalz)
                hvel = ((192*synctics)<<DOWN_BK(RUN));
            else
                hvel = -((192*synctics)<<DOWN_BK(RUN));

            pos.z += hvel;

            if (m32_clipping)
            {
                if (pos.z > loz-(4<<8)) pos.z = loz-(4<<8), hvel = 0;
                if (pos.z < hiz+(4<<8)) pos.z = hiz+(4<<8), hvel = 0;
            }
        }
        else
            hvel = 0;
    }

    {
        int16_t ocursectnum = cursectnum;
        updatesectorz(pos.x,pos.y,pos.z, &cursectnum);
        if (cursectnum<0)
        {
            if (zmode != 2)
                pos.z = oposz;  // don't allow to fall into infinity when in void space
            cursectnum = ocursectnum;
        }
    }

    searchit = 2;
    if (searchstat >= 0)
    {
        if ((bstatus&(1|2|4)) || keystatus[0x39])  // SPACE
            searchit = 0;

        if (keystatus[0x1f])  //S (insert sprite) (3D)
        {
            hitdata_t hitinfo;

            dax = 16384;
            day = divscale14(searchx-(xdim>>1), xdim>>1);
            rotatepoint(0,0, dax,day, ang, &dax,&day);

            hitscan((const vec3_t *)&pos,cursectnum,              //Start position
                    dax,day,(scale(searchy,200,ydim)-horiz)*2000, //vector of 3D ang
                    &hitinfo,CLIPMASK1);

            if (hitinfo.hitsect >= 0)
            {
                dax = hitinfo.pos.x;
                day = hitinfo.pos.y;
                if (gridlock && grid > 0)
                {
                    if (AIMING_AT_WALL || AIMING_AT_MASKWALL)
                        hitinfo.pos.z &= 0xfffffc00;
                    else
                        locktogrid(&dax, &day);
                }

                i = insert_sprite_common(hitinfo.hitsect, dax, day);

                if (i < 0)
                    message("Couldn't insert sprite.");
                else
                {
                    int32_t cz, fz;

                    handle_sprite_in_clipboard(i);

                    getzsofslope(hitinfo.hitsect, hitinfo.pos.x, hitinfo.pos.y, &cz, &fz);

                    spriteoncfz(i, &cz, &fz);
                    sprite[i].z = clamp(hitinfo.pos.z, cz, fz);

                    if (AIMING_AT_WALL || AIMING_AT_MASKWALL)
                    {
                        sprite[i].cstat &= ~48;
                        sprite[i].cstat |= (16+64);

                        correct_ornamented_sprite(i, hitinfo.hitwall);
                    }
                    else
                        sprite[i].cstat |= (tilesizy[sprite[i].picnum]>=32);

                    correct_sprite_yoffset(i);

                    asksave = 1;

                    VM_OnEvent(EVENT_INSERTSPRITE3D, i);
                }
            }

            keystatus[0x1f] = 0;
        }

        if (keystatus[0x3f]||keystatus[0x40])  //F5,F6
        {
            switch (searchstat)
            {
            case SEARCH_CEILING:
            case SEARCH_FLOOR:
                ExtShowSectorData(searchsector); break;
            case SEARCH_WALL:
            case SEARCH_MASKWALL:
                ExtShowWallData(searchwall); break;
            case SEARCH_SPRITE:
                ExtShowSpriteData(searchwall); break;
            }

            keystatus[0x3f] = keystatus[0x40] = 0;
        }
        if (keystatus[0x41]||keystatus[0x42])  //F7,F8
        {
            switch (searchstat)
            {
            case SEARCH_CEILING:
            case SEARCH_FLOOR:
                ExtEditSectorData(searchsector); break;
            case SEARCH_WALL:
            case SEARCH_MASKWALL:
                ExtEditWallData(searchwall); break;
            case SEARCH_SPRITE:
                ExtEditSpriteData(searchwall); break;
            }

            keystatus[0x41] = keystatus[0x42] = 0;
        }

    }

    if (keystatus[buildkeys[BK_MODE2D_3D]])  // Enter
    {

        vid_gamma_3d = vid_gamma;
        vid_contrast_3d = vid_contrast;
        vid_brightness_3d = vid_brightness;

        vid_gamma = vid_contrast = 1.0;
        vid_brightness = 0.0;

        setbrightness(0,0,0);

        keystatus[buildkeys[BK_MODE2D_3D]] = 0;
        overheadeditor();
        keystatus[buildkeys[BK_MODE2D_3D]] = 0;

        vid_gamma = vid_gamma_3d;
        vid_contrast = vid_contrast_3d;
        vid_brightness = vid_brightness_3d;

        vid_gamma_3d = vid_contrast_3d = vid_brightness_3d = -1;

        setbrightness(GAMMA_CALC,0,0);
    }
}

char changechar(char dachar, int32_t dadir, char smooshyalign, char boundcheck)
{
    if (dadir < 0)
    {
        if ((dachar > 0) || (boundcheck == 0))
        {
            dachar--;
            if (smooshyalign > 0)
                dachar = (dachar&0xf8);
        }
    }
    else if (dadir > 0)
    {
        if ((dachar < 255) || (boundcheck == 0))
        {
            dachar++;
            if (smooshyalign > 0)
            {
                if (dachar >= 256-8) dachar = 255;
                else dachar = ((dachar+7)&0xf8);
            }
        }
    }
    return(dachar);
}


////////////////////// OVERHEADEDITOR //////////////////////

// some 2d mode state
static struct overheadstate
{
    // number of backed up drawn walls
    int32_t bak_wallsdrawn;

    // state related to line drawing
    int16_t suckwall, split;
    int16_t splitsect;
    int16_t splitstartwall;
} ovh;

int32_t inside_editor_curpos(int16_t sectnum)
{
    // TODO: take care: mous[xy]plc global vs overheadeditor auto
    return inside_editor(&pos, searchx,searchy, zoom, mousxplc,mousyplc, sectnum);
}

int32_t inside_editor(const vec3_t *pos, int32_t searchx, int32_t searchy, int32_t zoom,
                      int32_t x, int32_t y, int16_t sectnum)
{
    if (!m32_sideview)
        return inside(x, y, sectnum);

    // if in side-view mode, use the screen coords instead
    {
        int32_t dst = MAXSECTORS+M32_FIXME_SECTORS-1, i, oi;
        int32_t srcw=sector[sectnum].wallptr, dstw=MAXWALLS;
        int32_t ret;

        if (sector[sectnum].wallnum > M32_FIXME_WALLS)
            return -1;

        Bmemcpy(&sector[dst], &sector[sectnum], sizeof(sectortype));
        sector[dst].wallptr = dstw;

        Bmemcpy(&wall[dstw], &wall[srcw], sector[dst].wallnum*sizeof(walltype));
        for (i=dstw, oi=srcw; i<dstw+sector[dst].wallnum; i++, oi++)
        {
            wall[i].point2 += dstw-srcw;

            screencoords(&wall[i].x, &wall[i].y, wall[i].x-pos->x, wall[i].y-pos->y, zoom);
            wall[i].y += getscreenvdisp(getflorzofslope(sectnum,wall[oi].x,wall[oi].y)-pos->z, zoom);
            wall[i].x += halfxdim16;
            wall[i].y += midydim16;
        }

        i = numsectors;
        numsectors = dst+1;
        ret = inside(searchx, searchy, dst);
        numsectors = i;
        return ret;
    }
}

static inline void drawline16base(int32_t bx, int32_t by, int32_t x1, int32_t y1, int32_t x2, int32_t y2, char col)
{
    drawline16(bx+x1, by+y1, bx+x2, by+y2, col);
}

void drawsmallabel(const char *text, char col, char backcol, int32_t dax, int32_t day, int32_t daz)
{
    int32_t x1, y1, x2, y2;

    screencoords(&dax,&day, dax-pos.x,day-pos.y, zoom);
    if (m32_sideview)
        day += getscreenvdisp(daz-pos.z, zoom);

    x1 = halfxdim16+dax-(Bstrlen(text)<<1);
    y1 = midydim16+day-4;
    x2 = x1 + (Bstrlen(text)<<2)+2;
    y2 = y1 + 7;

    if ((x1 > 3) && (x2 < xdim) && (y1 > 1) && (y2 < ydim16))
    {
        printext16(x1,y1, col,backcol, text,1);
        drawline16(x1-1,y1-1, x2-3,y1-1, backcol);
        drawline16(x1-1,y2+1, x2-3,y2+1, backcol);

        drawline16(x1-2,y1, x1-2,y2, backcol);
        drawline16(x2-2,y1, x2-2,y2, backcol);
        drawline16(x2-3,y1, x2-3,y2, backcol);
    }
}

static void free_n_ptrs(void **ptrptr, int32_t n)
{
    int32_t i;

    for (i=0; i<n; i++)
    {
        Bfree(ptrptr[i]);
        ptrptr[i] = NULL;
    }
}

// backup highlighted sectors with sprites as mapinfo for later restoration
// return values:
//  -1: highlightsectorcnt<=0
//  -2: out of mem
//   0: ok
static int32_t backup_highlighted_map(mapinfofull_t *mapinfo)
{
    int32_t i, j, k, m, tmpnumwalls=0, tmpnumsprites=0;
    int16_t *const otonsect = (int16_t *)tempxyar;  // STRICTALIASING
    int16_t *const otonwall = ((int16_t *)tempxyar) + MAXWALLS;
#ifdef YAX_ENABLE
    int16_t otonbunch[YAX_MAXBUNCHES];
    int16_t numsectsofbunch[YAX_MAXBUNCHES];  // ceilings + floors
#endif
    int32_t np = 0;
    void *ptrs[5];

    if (highlightsectorcnt <= 0)
        return -1;

#ifdef YAX_ENABLE
    for (i=0; i<numyaxbunches; i++)
        numsectsofbunch[i] = 0;
#endif

    // set up old-->new mappings
    j = 0;
    k = 0;
    for (i=0; i<numsectors; i++)
    {
        int32_t startwall, endwall;

        if (hlsectorbitmap[i>>3]&(1<<(i&7)))
        {
#ifdef YAX_ENABLE
            int16_t bn[2], cf;

            yax_getbunches(i, &bn[0], &bn[1]);
            for (cf=0; cf<2; cf++)
                if (bn[cf] >= 0)
                    numsectsofbunch[bn[cf]]++;
#endif
            otonsect[i] = j++;

            for (WALLS_OF_SECTOR(i, m))
                otonwall[m] = k++;
        }
        else
        {
            otonsect[i] = -1;

            for (WALLS_OF_SECTOR(i, m))
                otonwall[m] = -1;
        }
    }

#ifdef YAX_ENABLE
    j = 0;
    for (i=0; i<numyaxbunches; i++)
    {
        // only back up complete bunches
        if (numsectsofbunch[i] == yax_numsectsinbunch(i, 0)+yax_numsectsinbunch(i, 1))
            otonbunch[i] = j++;
        else
            otonbunch[i] = -1;
    }
    mapinfo->numyaxbunches = j;
#endif

    // count walls & sprites
    for (i=0; i<highlightsectorcnt; i++)
    {
        tmpnumwalls += sector[highlightsector[i]].wallnum;

        m = headspritesect[highlightsector[i]];
        while (m != -1)
        {
            tmpnumsprites++;
            m = nextspritesect[m];
        }
    }

    // allocate temp storage
    ptrs[np++] = mapinfo->sector = Bmalloc(highlightsectorcnt * sizeof(sectortype));
    if (!mapinfo->sector) return -2;

    ptrs[np++] = mapinfo->wall = Bmalloc(tmpnumwalls * sizeof(walltype));
    if (!mapinfo->wall) { free_n_ptrs(ptrs, np-1); return -2; }

#ifdef YAX_ENABLE
    if (mapinfo->numyaxbunches > 0)
    {
        ptrs[np++] = mapinfo->bunchnum = Bmalloc(highlightsectorcnt*2*sizeof(int16_t));
        if (!mapinfo->bunchnum) { free_n_ptrs(ptrs, np-1); return -2; }

        ptrs[np++] = mapinfo->ynextwall = Bmalloc(tmpnumwalls*2*sizeof(int16_t));
        if (!mapinfo->ynextwall) { free_n_ptrs(ptrs, np-1); return -2; }
    }
    else
    {
        mapinfo->bunchnum = mapinfo->ynextwall = NULL;
    }
#endif

    if (tmpnumsprites>0)
    {
        ptrs[np++] = mapinfo->sprite = Bmalloc(tmpnumsprites * sizeof(spritetype));
        if (!mapinfo->sprite) { free_n_ptrs(ptrs, np-1); return -2; }
    }
    else
    {
        // would never be accessed because mapinfo->numsprites is 0, but cleaner
        mapinfo->sprite = NULL;
    }


    // copy everything over
    tmpnumwalls = 0;
    tmpnumsprites = 0;
    for (i=0; i<highlightsectorcnt; i++)
    {
        k = highlightsector[i];
        Bmemcpy(&mapinfo->sector[i], &sector[k], sizeof(sectortype));
        mapinfo->sector[i].wallptr = tmpnumwalls;

#ifdef YAX_ENABLE
        if (mapinfo->numyaxbunches > 0)
        {
            int16_t bn[2];

            yax_getbunches(k, &bn[0], &bn[1]);
            for (j=0; j<2; j++)
                mapinfo->bunchnum[2*i + j] = (bn[j]>=0) ? otonbunch[bn[j]] : -1;
        }
#endif

        for (j=0; j<sector[k].wallnum; j++)
        {
            m = sector[k].wallptr;
            Bmemcpy(&mapinfo->wall[tmpnumwalls+j], &wall[m+j], sizeof(walltype));
            mapinfo->wall[tmpnumwalls+j].point2 += (tmpnumwalls-m);

#ifdef YAX_ENABLE
            if (mapinfo->numyaxbunches > 0)
            {
                int32_t ynw, cf;

                for (cf=0; cf<2; cf++)
                {
                    ynw = yax_getnextwall(m+j, cf);
                    mapinfo->ynextwall[2*(tmpnumwalls+j) + cf] = (ynw >= 0) ? otonwall[ynw] : -1;
                }
            }
#endif
            m = mapinfo->wall[tmpnumwalls+j].nextsector;
            if (m < 0 || otonsect[m] < 0)
            {
                mapinfo->wall[tmpnumwalls+j].nextsector = -1;
                mapinfo->wall[tmpnumwalls+j].nextwall = -1;
            }
            else
            {
                mapinfo->wall[tmpnumwalls+j].nextsector = otonsect[m];
                m = mapinfo->wall[tmpnumwalls+j].nextwall;
                mapinfo->wall[tmpnumwalls+j].nextwall = otonwall[m];
            }
        }
        tmpnumwalls += j;

        m = headspritesect[highlightsector[i]];
        while (m != -1)
        {
            Bmemcpy(&mapinfo->sprite[tmpnumsprites], &sprite[m], sizeof(spritetype));
            mapinfo->sprite[tmpnumsprites].sectnum = otonsect[highlightsector[i]];
            m = nextspritesect[m];
            tmpnumsprites++;
        }
    }


    mapinfo->numsectors = highlightsectorcnt;
    mapinfo->numwalls = tmpnumwalls;
    mapinfo->numsprites = tmpnumsprites;

    return 0;
}

static void mapinfofull_free(mapinfofull_t *mapinfo)
{
    Bfree(mapinfo->sector);
#ifdef YAX_ENABLE
    if (mapinfo->numyaxbunches > 0)
    {
        Bfree(mapinfo->bunchnum);
        Bfree(mapinfo->ynextwall);
    }
#endif
    Bfree(mapinfo->wall);
    if (mapinfo->numsprites>0)
        Bfree(mapinfo->sprite);
}

// restore map saved with backup_highlighted_map, also
// frees mapinfo's sector, wall, (sprite) in any case.
// return values:
//  -1: limits exceeded
//   0: ok
// forreal: if 0, only test if we have enough space (same return values)
static int32_t restore_highlighted_map(mapinfofull_t *mapinfo, int32_t forreal)
{
    int32_t i, j, sect, onumsectors=numsectors, newnumsectors, newnumwalls;

    if (numsectors+mapinfo->numsectors>MAXSECTORS || numwalls+mapinfo->numwalls>MAXWALLS
#ifdef YAX_ENABLE
            || numyaxbunches+mapinfo->numyaxbunches > YAX_MAXBUNCHES
#endif
            || Numsprites+mapinfo->numsprites>MAXSPRITES)
    {
        mapinfofull_free(mapinfo);
        return -1;
    }

    if (!forreal)
        return 0;

    newnumsectors = numsectors + mapinfo->numsectors;
    newnumwalls = numwalls + mapinfo->numwalls;

    // copy sectors & walls
    Bmemcpy(&sector[numsectors], mapinfo->sector, mapinfo->numsectors*sizeof(sectortype));
    Bmemcpy(&wall[numwalls], mapinfo->wall, mapinfo->numwalls*sizeof(walltype));

    // tweak index members
    for (i=numwalls; i<newnumwalls; i++)
    {
        wall[i].point2 += numwalls;

        if (wall[i].nextsector >= 0)
        {
            wall[i].nextsector += numsectors;
            wall[i].nextwall += numwalls;
        }
#ifdef YAX_ENABLE
        for (j=0; j<2; j++)
        {
            if (mapinfo->numyaxbunches > 0)
                yax_setnextwall(i, j, mapinfo->ynextwall[2*(i-numwalls) + j]>=0 ?
                                numwalls+mapinfo->ynextwall[2*(i-numwalls) + j] : -1);
            else
                yax_setnextwall(i, j, -1);
        }
#endif
    }
    for (i=numsectors; i<newnumsectors; i++)
        sector[i].wallptr += numwalls;

    // highlight copied sectors

    numsectors = newnumsectors;

    Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
    for (i=onumsectors; i<newnumsectors; i++)
    {
        hlsectorbitmap[i>>3] |= (1<<(i&7));

#ifdef YAX_ENABLE
        for (j=0; j<2; j++)
            if (mapinfo->numyaxbunches > 0)
                yax_setbunch(i, j, mapinfo->bunchnum[2*(i-onumsectors)+j] >= 0 ?
                             numyaxbunches + mapinfo->bunchnum[2*(i-onumsectors)+j] : -2);
            else
                yax_setbunch(i, j, -2);
        // -2 clears forward yax-nextwall links
#endif
    }

    // insert sprites
    for (i=0; i<mapinfo->numsprites; i++)
    {
        sect = onumsectors+mapinfo->sprite[i].sectnum;
        j = insertsprite(sect, mapinfo->sprite[i].statnum);
        Bmemcpy(&sprite[j], &mapinfo->sprite[i], sizeof(spritetype));
        sprite[j].sectnum = sect;
    }

    mapinfofull_free(mapinfo);

    numwalls = newnumwalls;

    update_highlightsector();

#ifdef YAX_ENABLE
    if (mapinfo->numyaxbunches > 0)
        yax_update(0);
#endif
    yax_updategrays(pos.z);

    return 0;
}


static int32_t newnumwalls=-1;

void ovh_whiteoutgrab(int32_t restoreredwalls)
{
    int32_t i, j, k, startwall, endwall;
#if 0
//def YAX_ENABLE
    int16_t cb, fb;
#endif

    if (restoreredwalls)
    {
        // restore onextwalls first
        for (i=0; i<numsectors; i++)
            for (WALLS_OF_SECTOR(i, j))
                checksectorpointer(j, i);
    }

    for (i=0; i<MAXWALLS; i++)
        onextwall[i] = -1;

    //White out all bordering lines of grab that are
    //not highlighted on both sides
    for (i=highlightsectorcnt-1; i>=0; i--)
        for (WALLS_OF_SECTOR(highlightsector[i], j))
        {
            if (wall[j].nextwall < 0)
                continue;

            k = wall[j].nextsector;

            if (hlsectorbitmap[k>>3]&(1<<(k&7)))
                continue;
#if 0
//def YAX_ENABLE
            // internal red walls are kept red
            yax_getbunches(highlightsector[i], &cb, &fb);
            if (cb>=0 && yax_getbunch(k, YAX_CEILING)>=0)
                continue;
            if (fb>=0 && yax_getbunch(k, YAX_FLOOR)>=0)
                continue;
#endif
            onextwall[j] = wall[j].nextwall;

            NEXTWALL(j).nextwall = -1;
            NEXTWALL(j).nextsector = -1;
            wall[j].nextwall = -1;
            wall[j].nextsector = -1;
        }

    if (highlightsectorcnt > 0)
        mkonwvalid();
    else
        mkonwinvalid();
}

static void duplicate_selected_sectors(void)
{
    mapinfofull_t mapinfo;
    int32_t i, j, onumsectors;
#ifdef YAX_ENABLE
    int32_t onumyaxbunches;
#endif
    int32_t minx=INT32_MAX, maxx=INT32_MIN, miny=INT32_MAX, maxy=INT32_MIN, dx, dy;

    i = backup_highlighted_map(&mapinfo);

    if (i < 0)
    {
        message("Out of memory!");
        return;
    }

    i = restore_highlighted_map(&mapinfo, 0);
    if (i < 0)
    {
        // XXX: no, might be another limit too.  Better message needed.
        printmessage16("Copying sectors would exceed sector or wall limit.");
        return;
    }

    // restoring would succeed, tweak things...
    Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
    for (i=0; i<highlightsectorcnt; i++)
    {
        int32_t startwall, endwall;

        // first, make red lines of old selected sectors, effectively
        // restoring the original state
        for (WALLS_OF_SECTOR(highlightsector[i], j))
        {
            if (wall[j].nextwall >= 0)
                checksectorpointer(wall[j].nextwall,wall[j].nextsector);
            checksectorpointer(j, highlightsector[i]);

            minx = min(minx, wall[j].x);
            maxx = max(maxx, wall[j].x);
            miny = min(miny, wall[j].y);
            maxy = max(maxy, wall[j].y);
        }
    }

    // displace walls & sprites of new sectors by a small amount:
    // calculate displacement
    if (grid>0 && grid<9)
        dx = max(2048>>grid, 128);
    else
        dx = 512;
    dy = -dx;
    if (maxx+dx >= editorgridextent) dx*=-1;
    if (minx+dx <= -editorgridextent) dx*=-1;
    if (maxy+dy >= editorgridextent) dy*=-1;
    if (miny+dy <= -editorgridextent) dy*=-1;

    onumsectors = numsectors;
#ifdef YAX_ENABLE
    onumyaxbunches = numyaxbunches;
#endif
    // restore! this will not fail.
    restore_highlighted_map(&mapinfo, 1);

    // displace
    for (i=onumsectors; i<numsectors; i++)
    {
        for (j=sector[i].wallptr; j<sector[i].wallptr+sector[i].wallnum; j++)
        {
            wall[j].x += dx;
            wall[j].y += dy;
        }

        for (j=headspritesect[i]; j>=0; j=nextspritesect[j])
        {
            sprite[j].x += dx;
            sprite[j].y += dy;
        }
    }

#ifdef YAX_ENABLE
    if (numyaxbunches > onumyaxbunches)
        printmessage16("Sectors duplicated, creating %d new bunches.", numyaxbunches-onumyaxbunches);
    else
#endif
        printmessage16("Sectors duplicated and stamped.");
    asksave = 1;

#ifdef YAX_ENABLE
    if (numyaxbunches > onumyaxbunches)
        yax_update(0);
#endif
    yax_updategrays(pos.z);
}


static void duplicate_selected_sprites(void)
{
    int32_t i, j, k=0;

    for (i=0; i<highlightcnt; i++)
        if ((highlight[i]&0xc000) == 16384)
            k++;

    if (Numsprites + k <= MAXSPRITES)
    {
        for (i=0; i<highlightcnt; i++)
            if ((highlight[i]&0xc000) == 16384)
            {
                //duplicate sprite
                k = (highlight[i]&16383);
                j = insertsprite(sprite[k].sectnum,sprite[k].statnum);
                Bmemcpy(&sprite[j],&sprite[k],sizeof(spritetype));
//                sprite[j].sectnum = sprite[k].sectnum;   //Don't let memcpy overwrite sector!
//                setsprite(j,(vec3_t *)&sprite[j]);
            }

        printmessage16("Sprites duplicated and stamped.");
        asksave = 1;
    }
    else
    {
        printmessage16("Copying sprites would exceed sprite limit.");
    }
}

static void correct_ornamented_sprite(int32_t i, int32_t hitw)
{
    int32_t j;

    if (hitw >= 0)
        sprite[i].ang = (getangle(POINT2(hitw).x-wall[hitw].x,
                                  POINT2(hitw).y-wall[hitw].y)+512)&2047;

    //Make sure sprite's in right sector
    if (inside(sprite[i].x, sprite[i].y, sprite[i].sectnum) == 0)
    {
        j = wall[hitw].point2;
        sprite[i].x -= ksgn(wall[j].y-wall[hitw].y);
        sprite[i].y += ksgn(wall[j].x-wall[hitw].x);
    }
}

void DoSpriteOrnament(int32_t i)
{
    hitdata_t hitinfo;

    hitscan((const vec3_t *)&sprite[i],sprite[i].sectnum,
            sintable[(sprite[i].ang+1536)&2047],
            sintable[(sprite[i].ang+1024)&2047],
            0,
            &hitinfo,CLIPMASK1);

    sprite[i].x = hitinfo.pos.x;
    sprite[i].y = hitinfo.pos.y;
    sprite[i].z = hitinfo.pos.z;
    changespritesect(i, hitinfo.hitsect);

    correct_ornamented_sprite(i, hitinfo.hitwall);
}

void update_highlight(void)
{
    int32_t i;

    highlightcnt = 0;
    for (i=0; i<numwalls; i++)
        if (show2dwall[i>>3]&(1<<(i&7)))
            highlight[highlightcnt++] = i;
    for (i=0; i<MAXSPRITES; i++)
        if (sprite[i].statnum < MAXSTATUS)
        {
            if (show2dsprite[i>>3]&(1<<(i&7)))
                highlight[highlightcnt++] = i+16384;
        }
        else
            show2dsprite[i>>3] &= ~(1<<(i&7));

    if (highlightcnt == 0)
        highlightcnt = -1;
}

void update_highlightsector(void)
{
    int32_t i;

    minhlsectorfloorz = INT32_MAX;
    numhlsecwalls = 0;

    highlightsectorcnt = 0;
    for (i=0; i<numsectors; i++)
        if (hlsectorbitmap[i>>3]&(1<<(i&7)))
        {
            highlightsector[highlightsectorcnt++] = i;
            minhlsectorfloorz = min(minhlsectorfloorz, sector[i].floorz);
            numhlsecwalls += sector[i].wallnum;
        }

    if (highlightsectorcnt==0)
    {
        minhlsectorfloorz = 0;
        highlightsectorcnt = -1;
    }
}

// Get average point of sectors
static void get_sectors_center(const int16_t *sectors, int32_t numsecs, int32_t *cx, int32_t *cy)
{
    int32_t i, j, k=0, dax = 0, day = 0;
    int32_t startwall, endwall;

    for (i=0; i<numsecs; i++)
    {
        for (WALLS_OF_SECTOR(sectors[i], j))
        {
            dax += wall[j].x;
            day += wall[j].y;
            k++;
        }
    }

    if (k > 0)
    {
        dax /= k;
        day /= k;
    }

    *cx = dax;
    *cy = day;
}

static int32_t insert_sprite_common(int32_t sectnum, int32_t dax, int32_t day)
{
    int32_t i, j, k;

    i = insertsprite(sectnum,0);
    if (i < 0)
        return -1;

    sprite[i].x = dax, sprite[i].y = day;
    sprite[i].cstat = DEFAULT_SPRITE_CSTAT;
    sprite[i].shade = 0;
    sprite[i].pal = 0;
    sprite[i].xrepeat = 64, sprite[i].yrepeat = 64;
    sprite[i].xoffset = 0, sprite[i].yoffset = 0;
    sprite[i].ang = 1536;
    sprite[i].xvel = 0; sprite[i].yvel = 0; sprite[i].zvel = 0;
    sprite[i].owner = -1;
    sprite[i].clipdist = 32;
    sprite[i].lotag = 0;
    sprite[i].hitag = 0;
    sprite[i].extra = -1;

    Bmemset(localartfreq, 0, sizeof(localartfreq));
    for (k=0; k<MAXSPRITES; k++)
        if (sprite[k].statnum < MAXSTATUS && k!=i)
            localartfreq[sprite[k].picnum]++;

    j = 0;
    for (k=0; k<MAXTILES; k++)
        if (localartfreq[k] > localartfreq[j])
            j = k;

    if (localartfreq[j] > 0)
        sprite[i].picnum = j;
    else
        sprite[i].picnum = 0;

    return i;
}

void correct_sprite_yoffset(int32_t i)
{
    int32_t tileyofs = (int8_t)((picanm[sprite[i].picnum]>>16)&255);
    int32_t tileysiz = tilesizy[sprite[i].picnum];

    if (klabs(tileyofs) >= tileysiz)
    {
        tileyofs *= -1;
        if (tileyofs == 128)
            tileyofs = 127;

        sprite[i].yoffset = tileyofs;
    }
    else
        sprite[i].yoffset = 0;
}

// keepcol >= 0 && <256: keep that idx-color
// keepcol < 0: keep none
// keepcol >= 256: 0x00ffffff is mask for 3 colors
void fade_editor_screen(int32_t keepcol)
{
    char blackcol=0, greycol=whitecol-25, *cp;
    int32_t pix, i, threecols = (keepcol >= 256);
    char cols[3] = {keepcol&0xff, (keepcol>>8)&0xff, (keepcol>>16)&0xff};

    begindrawing();
    cp = (char *)frameplace;
    for (i=0; i<bytesperline*(ydim-STATUS2DSIZ2); i++, cp++)
    {
        pix = (uint8_t)(*cp);

        if (!threecols && pix == keepcol)
            continue;
        if (threecols)
            if (pix==cols[0] || pix==cols[1] || pix==cols[2])
                continue;

        if (*cp==greycol)
            *cp = blackcol;
        else if (*cp != blackcol)
            *cp = greycol;
    }
    enddrawing();
    showframe(1);
}

static void copy_some_wall_members(int16_t dst, int16_t src, int32_t reset_some)
{
    //                                 x  y  p2 nw ns cs p  op sh pl xr yr xp yp lo hi ex
    static const walltype nullwall = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, -1 };
    walltype *dstwal=&wall[dst];
    const walltype *srcwal = src >= 0 ? &wall[src] : &nullwall;

    if (reset_some)
    {
        dstwal->cstat = srcwal->cstat;
    }
    else
    {
        dstwal->cstat &= ~(4+8+256);
        dstwal->cstat |= (srcwal->cstat&(4+8+256));
    }
    dstwal->shade = srcwal->shade;
    dstwal->yrepeat = srcwal->yrepeat;
    fixrepeats(dst);  // xrepeat
    dstwal->picnum = srcwal->picnum;
    dstwal->overpicnum = srcwal->overpicnum;

    dstwal->pal = srcwal->pal;
    dstwal->xpanning = srcwal->xpanning;
    dstwal->ypanning = srcwal->ypanning;

    if (reset_some)
    {
        dstwal->nextwall = -1;
        dstwal->nextsector = -1;

        dstwal->lotag = 0; //srcwal->lotag;
        dstwal->hitag = 0; //srcwal->hitag;
        dstwal->extra = -1; //srcwal->extra;
#ifdef YAX_ENABLE
        yax_setnextwall(dst, YAX_CEILING, -1);
        yax_setnextwall(dst, YAX_FLOOR, -1);
#endif
    }
}

static void init_new_wall1(int16_t *suckwall_ret, int32_t mousxplc, int32_t mousyplc)
{
    int32_t i;

    Bmemset(&wall[newnumwalls], 0, sizeof(walltype));
    wall[newnumwalls].extra = -1;

    wall[newnumwalls].x = mousxplc;
    wall[newnumwalls].y = mousyplc;
    wall[newnumwalls].nextsector = -1;
    wall[newnumwalls].nextwall = -1;

    for (i=0; i<numwalls; i++)
    {
        YAX_SKIPWALL(i);
        if (wall[i].nextwall >= 0)
            YAX_SKIPWALL(wall[i].nextwall);

        if (wall[i].x == mousxplc && wall[i].y == mousyplc)
            *suckwall_ret = i;
    }

    wall[newnumwalls].point2 = newnumwalls+1;
    newnumwalls++;
}

// helpers for often needed ops:
static int32_t do_while_copyloop1(int16_t startwall, int16_t endwall,
                                  int16_t *danumwalls, int16_t lastpoint2)
{
    int32_t m = startwall;

    do
    {
        if (*danumwalls >= MAXWALLS + M32_FIXME_WALLS)
            return 1;

        Bmemcpy(&wall[*danumwalls], &wall[m], sizeof(walltype));
        wall[*danumwalls].point2 = *danumwalls+1;
        (*danumwalls)++;
        m = wall[m].point2;
    }
    while (m != endwall);

    if (lastpoint2 >= 0)
        wall[(*danumwalls)-1].point2 = lastpoint2;

    return 0;
}

static void updatesprite1(int16_t i)
{
    setsprite(i, (vec3_t *)&sprite[i]);

    if (sprite[i].sectnum>=0)
    {
        int32_t cz, fz;
        spriteoncfz(i, &cz, &fz);
        inpclamp(&sprite[i].z, cz, fz);
    }
}

#ifdef YAX_ENABLE
// highlighted OR grayed-out sectors:
static uint8_t hlorgraysectbitmap[MAXSECTORS>>3];
static int32_t ask_above_or_below(void);
#else
# define hlorgraysectbitmap hlsectorbitmap
#endif

// returns:
//  0: continue
// >0: newnumwalls
// <0: error
// ignore_ret and refsect_ret are for the 'auto-red-wall' feature
static int32_t trace_loop(int32_t j, uint8_t *visitedwall, int16_t *ignore_ret, int16_t *refsect_ret,
                          int16_t trace_loop_yaxcf)
{
    int16_t refsect, ignore;
    int32_t k, n, refwall;
#if 0
//def YAX_ENABLE
    int32_t yaxp = (ignore_ret==NULL);  // bleh
#else
    UNREFERENCED_PARAMETER(trace_loop_yaxcf);
#endif

    if (wall[j].nextwall>=0 || (visitedwall[j>>3]&(1<<(j&7))))
        return 0;

    n=2*MAXWALLS;  // simple inf loop check
    refwall = j;
    k = numwalls;

    ignore = 0;

    if (ignore_ret)
    {
        refsect = -1;
        updatesectorexclude(wall[j].x, wall[j].y, &refsect, hlorgraysectbitmap);
        if (refsect<0)
            return -1;
    }

    do
    {
        if (j!=refwall && visitedwall[j>>3]&(1<<(j&7)))
            ignore = 1;
        visitedwall[j>>3] |= (1<<(j&7));

        if (ignore_ret)
        {
            if (inside(wall[j].x, wall[j].y, refsect) != 1)
                ignore = 1;
        }

        if (!ignore)
        {
            if (k>=MAXWALLS)
            {
                message("Wall limits exceeded while tracing outer loop.");
                return -2;
            }

            if (ignore_ret)  // auto-red wall feature
                onextwall[k] = onextwall[j];

            Bmemcpy(&wall[k], &wall[j], sizeof(walltype));
            wall[k].point2 = k+1;
// TODO: protect lotag/extra; see also hl-sector copying stuff
            wall[k].nextsector = wall[k].nextwall = wall[k].extra = -1;
#ifdef YAX_ENABLE
            if (trace_loop_yaxcf >= 0)
                yax_setnextwall(k, trace_loop_yaxcf, j);
#endif
            k++;
        }

        j = wall[j].point2;
        n--;

        while (wall[j].nextwall>=0 && n>0)
        {
#if 0
//def YAX_ENABLE
            if (yaxp)
            {
                int32_t ns = wall[j].nextsector;
                if ((hlsectorbitmap[ns>>3]&(1<<(ns&7)))==0)
                    break;
            }
#endif
            j = wall[wall[j].nextwall].point2;
//            if (j!=refwall && (visitedwall[j>>3]&(1<<(j&7))))
//                ignore = 1;
//            visitedwall[j>>3] |= (1<<(j&7));
            n--;
        }
    }
    while (j!=refwall && n>0);

    if (j!=refwall)
    {
        message("internal error while tracing outer loop: didn't reach refwall");
        return -3;
    }

    if (ignore_ret)
    {
        *ignore_ret = ignore;
        if (refsect_ret)
            *refsect_ret = refsect;
    }

    return k;
}

// Backup drawn walls for carrying out other operations in the middle.
//  0: back up, set newnumwalls to -1
//  1: restore drawn walls and free mem
//  2: only free memory needed for backing up walls but don't restore walls
//     (use this if the map has been mangled too much for a safe restoration)
// Context that needs special treatment: suckwall, splitsect, splitstartwall
static int32_t backup_drawn_walls(int32_t restore)
{
    static walltype *tmpwall;

    // back up
    if (restore==0)
    {
        // ovh.bak_wallsdrawn should be 0 here

        if (newnumwalls != -1)
        {
            if (newnumwalls <= numwalls)  // shouldn't happen
                return 2;

            tmpwall = Bmalloc((newnumwalls-numwalls) * sizeof(walltype));
            if (!tmpwall)
                return 1;

            ovh.bak_wallsdrawn = newnumwalls-numwalls;

            Bmemcpy(tmpwall, &wall[numwalls], ovh.bak_wallsdrawn*sizeof(walltype));
            newnumwalls = -1;
        }

        return 0;
    }

    // restore/clear
    if (tmpwall)
    {
        if (restore==1)  // really restore
        {
            int32_t i;

            Bmemcpy(&wall[numwalls], tmpwall, ovh.bak_wallsdrawn*sizeof(walltype));

            newnumwalls = numwalls + ovh.bak_wallsdrawn;
            for (i=numwalls; i<newnumwalls; i++)
                wall[i].point2 = i+1;
        }

        Bfree(tmpwall);
        tmpwall = NULL;

        ovh.bak_wallsdrawn = 0;
    }

    return 0;
}

#if 0
#define GETWALCOORD(w) (*(int64_t *)&wall[*(const int32_t *)(w)].x)
static int32_t compare_wall_coords(const void *w1, const void *w2)
{
    if (GETWALCOORD(w1) == GETWALCOORD(w2))
        return 0;
    if (GETWALCOORD(w1) > GETWALCOORD(w2))
        return 1;
    return -1;
}
#undef GETWALCOORD
#endif

#define RESET_EDITOR_VARS() do { \
    sectorhighlightstat = -1; \
    newnumwalls = -1; \
    joinsector[0] = -1; \
    circlewall = -1; \
    circlepoints = 7; \
    } while (0)


#ifdef YAX_ENABLE
static int32_t collnumsects[2];
static int16_t collsectlist[2][MAXSECTORS];
static uint8_t collsectbitmap[2][MAXSECTORS>>3];

static void collect_sectors1(int16_t *sectlist, uint8_t *sectbitmap, int32_t *numsectptr,
                             int16_t startsec, int32_t alsoyaxnext, int32_t alsoonw)
{
    int32_t j, startwall, endwall, sectcnt;

    bfirst_search_init(sectlist, sectbitmap, numsectptr, MAXSECTORS, startsec);

    for (sectcnt=0; sectcnt<*numsectptr; sectcnt++)
    {
        for (WALLS_OF_SECTOR(sectlist[sectcnt], j))
        {
            if (wall[j].nextsector >= 0)
                bfirst_search_try(sectlist, sectbitmap, numsectptr, wall[j].nextsector);
            else if (alsoonw && onextwall[j]>=0)
                bfirst_search_try(sectlist, sectbitmap, numsectptr, sectorofwall(onextwall[j]));
        }

        if (alsoyaxnext)
        {
            int16_t bn[2], cf;
            yax_getbunches(sectlist[sectcnt], &bn[0], &bn[1]);
            for (cf=0; cf<2; cf++)
                if (bn[cf]>=0)
                {
                    for (SECTORS_OF_BUNCH(bn[cf], !cf, j))
                        bfirst_search_try(sectlist, sectbitmap, numsectptr, j);
                }
        }
    }
}


static int32_t sectors_components(int16_t hlsectcnt, const int16_t *hlsectors, int32_t alsoyaxnext, int32_t alsoonw);
static int32_t highlighted_sectors_components(int32_t alsoyaxnext, int32_t alsoonw)
{
    return sectors_components(highlightsectorcnt, highlightsector, alsoyaxnext, alsoonw);
}

// whether all highlighted sectors are in one (returns 1), two (2)
// or more (>2) connected components wrt the nextsector relation
// -1 means error
//  alsoyaxnext: also consider "yax-nextsector" relation
//  alsoonw: also consider "old-nextwall" relation (must be valid)
static int32_t sectors_components(int16_t hlsectcnt, const int16_t *hlsector, int32_t alsoyaxnext, int32_t alsoonw)
{
    int32_t j, k, tmp;

    if (hlsectcnt<1)
        return 0;

    collect_sectors1(collsectlist[0], collsectbitmap[0], &collnumsects[0],
                     hlsector[0], alsoyaxnext, alsoonw);

    for (k=1; k<hlsectcnt; k++)
    {
        j = hlsector[k];
        if ((collsectbitmap[0][j>>3]&(1<<(j&7)))==0)
        {
            // sector j not collected --> more than 1 conn. comp.
            collect_sectors1(collsectlist[1], collsectbitmap[1], &collnumsects[1],
                             j, alsoyaxnext, alsoonw);
            break;
        }
    }

    if (k == hlsectcnt)
        return 1;

    for (k=0; k<hlsectcnt; k++)
    {
        j = hlsector[k];
        tmp = (((collsectbitmap[0][j>>3]&(1<<(j&7)))!=0) + (((collsectbitmap[1][j>>3]&(1<<(j&7)))!=0)<<1));

        if (tmp==3)
            return -1;  // components only weakly connected

        if (tmp==0)
            return 3;  // sector j not reached
    }

    return 2;
}

static int cmpgeomwal1(const int16_t *w1, const int16_t *w2)
{
    const walltype *wal1 = &wall[*w1];
    const walltype *wal2 = &wall[*w2];

    if (wal1->x == wal2->x)
        return wal1->y - wal2->y;

    return wal1->x - wal2->x;
}

static void sort_walls_geometrically(int16_t *wallist, int32_t nwalls)
{
    qsort(wallist, nwalls, sizeof(int16_t), (int(*)(const void *, const void *))&cmpgeomwal1);
}
#endif

void SetFirstWall(int32_t sectnum, int32_t wallnum)
{
#ifdef YAX_ENABLE
    int32_t i, j, k, startwall, endwall;
    int16_t cf, bunchnum, tempsect, tempwall;

    for (i=0; i<numwalls; i++)
        wall[i].cstat &= ~(1<<14);

    for (cf=0; cf<2; cf++)
    {
        tempsect = sectnum;
        tempwall = wallnum;

        while ((bunchnum = yax_getbunch(tempsect, cf)) >= 0 &&
                   (tempsect=yax_is121(bunchnum, cf)) >= 0)
        {
            tempwall = yax_getnextwall(tempwall, cf);
            if (tempwall < 0)
                break;  // corrupt!
            wall[tempwall].cstat |= (1<<14);
        }
    }

    k = 0;
    for (i=0; i<numsectors; i++)
        for (WALLS_OF_SECTOR(i, j))
        {
            if (wall[j].cstat & (1<<14))
            {
                setfirstwall(i, j);
                k++;
                break;
            }
        }

    if (k > 0)
        message("Set first walls (sector[].wallptr) for %d sectors", k+1);
    else
#endif
        message("This wall now sector %d's first wall (sector[].wallptr)", sectnum);

    setfirstwall(sectnum, wallnum);
    mkonwinvalid();
    asksave = 1;
}

static void handlesecthighlight1(int32_t i, int32_t sub, int32_t nograycheck)
{
    int32_t j;

    if (sub)
    {
        hlsectorbitmap[i>>3] &= ~(1<<(i&7));
        for (j=sector[i].wallptr; j<sector[i].wallptr+sector[i].wallnum; j++)
        {
            if (wall[j].nextwall >= 0)
                checksectorpointer(wall[j].nextwall,wall[j].nextsector);
            checksectorpointer(j, i);
        }
    }
    else
    {
        if (nograycheck || (graysectbitmap[i>>3]&(1<<(i&7)))==0)
            hlsectorbitmap[i>>3] |= (1<<(i&7));
    }
}

#ifdef YAX_ENABLE
// 1: good, 0: bad
static int32_t hl_all_bunch_sectors_p()
{
    uint8_t *const havebunch = visited;
    int16_t cf, cb, fb;
    int32_t i, j;

    if (numyaxbunches > 0)
    {
        Bmemset(havebunch, 0, (numyaxbunches+7)>>3);
        for (i=0; i<highlightsectorcnt; i++)
        {
            yax_getbunches(highlightsector[i], &cb, &fb);
            if (cb>=0)
                havebunch[cb>>3] |= (1<<(cb&7));
            if (fb>=0)
                havebunch[fb>>3] |= (1<<(fb&7));
        }

        for (i=0; i<numyaxbunches; i++)
        {
            if ((havebunch[i>>3] & (1<<(i&7)))==0)
                continue;

            for (cf=0; cf<2; cf++)
                for (SECTORS_OF_BUNCH(i,cf, j))
                    if ((hlsectorbitmap[j>>3]&(1<<(j&7)))==0)
                        return 0;
        }
    }

    return 1;
}
#endif

static int32_t find_nextwall(int32_t sectnum, int32_t sectnum2)
{
    int32_t j, startwall, endwall;

    if (sectnum<0 || sectnum2<0)
        return -1;

    for (WALLS_OF_SECTOR(sectnum, j))
        if (wall[j].nextsector == sectnum2)
            return j;

    return -1;
}

static int32_t bakframe_fillandfade(char **origframeptr, int32_t sectnum, const char *querystr)
{
    if (!*origframeptr)
    {
        *origframeptr = Bmalloc(xdim*ydim);
        if (*origframeptr)
        {
            begindrawing();
            Bmemcpy(*origframeptr, (char *)frameplace, xdim*ydim);
            enddrawing();
        }
    }
    else
    {
        begindrawing();
        Bmemcpy((char *)frameplace, *origframeptr, xdim*ydim);
        enddrawing();
    }

    fillsector(sectnum, editorcolors[9]);
    fade_editor_screen(editorcolors[9]);

    return ask_if_sure(querystr, 0);
}

// high-level insert point, handles TROR constrained walls too
//  onewnumwalls: old numwalls + drawn walls
//  mapwallnum: see insertpoint()
static int32_t M32_InsertPoint(int32_t thewall, int32_t dax, int32_t day, int32_t onewnumwalls, int32_t *mapwallnum)
{
#ifdef YAX_ENABLE
    int32_t nextw = wall[thewall].nextwall;
    int32_t i, j, k, m, tmpcf;

    if (yax_islockedwall(thewall) || (nextw>=0 && yax_islockedwall(nextw)))
    {
        // yax'ed wall -- first find out which walls are affected
        for (i=0; i<numwalls; i++)
            wall[i].cstat &= ~(1<<14);

        // round 1
        for (YAX_ITER_WALLS(thewall, i, tmpcf))
            wall[i].cstat |= (1<<14);
        if (nextw >= 0)
            for (YAX_ITER_WALLS(nextw, i, tmpcf))
                wall[i].cstat |= (1<<14);
        // round 2 (enough?)
        for (YAX_ITER_WALLS(thewall, i, tmpcf))
            if (wall[i].nextwall >= 0 && (wall[wall[i].nextwall].cstat&(1<<14))==0)
                wall[wall[i].nextwall].cstat |= (1<<14);
        if (nextw >= 0)
            for (YAX_ITER_WALLS(nextw, i, tmpcf))
                if (wall[i].nextwall >= 0 && (wall[wall[i].nextwall].cstat&(1<<14))==0)
                    wall[wall[i].nextwall].cstat |= (1<<14);

        j = 0;
        for (i=0; i<numwalls; i++)
            j += !!(wall[i].cstat&(1<<14));
        if (max(numwalls,onewnumwalls)+j > MAXWALLS)
        {
            return 0;  // no points inserted, would exceed limits
        }

        // the actual insertion!
        m = 0;
        for (i=0; i<numwalls /* rises with ins. */; i++)
        {
            if (wall[i].cstat&(1<<14))
                if (wall[i].nextwall<0 || i<wall[i].nextwall) // || !(NEXTWALL(i).cstat&(1<<14)) ??
                {
                    m += insertpoint(i, dax,day, mapwallnum);
                }
        }

        for (i=0; i<numwalls; i++)
        {
            if (wall[i].cstat&(1<<14))
            {
                wall[i].cstat &= ~(1<<14);
                k = yax_getnextwall(i+1, YAX_CEILING);
                if (k >= 0)
                    yax_setnextwall(i+1, YAX_CEILING, k+1);
                k = yax_getnextwall(i+1, YAX_FLOOR);
                if (k >= 0)
                    yax_setnextwall(i+1, YAX_FLOOR, k+1);
            }
        }

        if (m==j)
            return m;
        else
            return m|(j<<16);
    }
    else
#endif
    {
        insertpoint(thewall, dax,day, mapwallnum);
        return 1;
    }
}


static int32_t lineintersect2v(const vec2_t *p1, const vec2_t *p2,  // line segment 1
                               const vec2_t *q1, const vec2_t *q2,  // line segment 2
                               vec2_t *pint)
{
    int32_t intz;
    return lineintersect(p1->x, p1->y, 0, p2->x, p2->y, 0,
                         q1->x, q1->y, q2->x, q2->y,
                         &pint->x, &pint->y, &intz);
}

static int32_t vec2eq(const vec2_t *v1, const vec2_t *v2)
{
    return (v1->x==v2->x && v1->y==v2->y);
}

// precondition: [numwalls, newnumwalls-1] form a new loop (may be of wrong orientation)
// ret_ofirstwallofs: if != NULL, *ret_ofirstwallofs will contain the offset of the old
//                    first wall from the new first wall of the sector k, and the automatic
//                    restoring of the old first wll will not be carried out
// returns:
//  -1, -2: errors
//   0,  1: OK, 1 means it was an extended sector and an inner loop has been added automatically
static int32_t AddLoopToSector(int32_t k, int32_t *ret_ofirstwallofs)
{
    int32_t extendedSector=0, firstwall, i, j;
#ifdef YAX_ENABLE
    int16_t cbunch, fbunch;
    int32_t newnumwalls2;

    yax_getbunches(k, &cbunch, &fbunch);
    extendedSector = (cbunch>=0 || fbunch>=0);
#endif
    j = newnumwalls-numwalls;
#ifdef YAX_ENABLE
    newnumwalls2 = newnumwalls + j;

    if (extendedSector)
    {
        if ((cbunch>=0 && (sector[k].ceilingstat&2))
            || (fbunch>=0 && (sector[k].floorstat&2)))
        {
            printmessage16("Sloped extended sectors cannot be subdivided.");
            newnumwalls--;
            return -1;
        }

        if (newnumwalls + j > MAXWALLS || numsectors+1 > MAXSECTORS)
        {
            message("Automatically adding inner sector to new extended sector would exceed limits!");
            newnumwalls--;
            return -2;
        }
    }
#endif
    if (clockdir(numwalls) == CLOCKDIR_CW)
        flipwalls(numwalls,newnumwalls);

    sector[k].wallnum += j;
    for (i=k+1; i<numsectors; i++)
        sector[i].wallptr += j;
    firstwall = sector[k].wallptr;

    for (i=0; i<numwalls; i++)
    {
        if (wall[i].nextwall >= firstwall)
            wall[i].nextwall += j;
        if (wall[i].point2 >= firstwall)
            wall[i].point2 += j;
    }
#ifdef YAX_ENABLE
    yax_tweakwalls(firstwall, j);
#endif

    Bmemmove(&wall[firstwall+j], &wall[firstwall], (newnumwalls-firstwall)*sizeof(walltype));
    // add new loop to beginning of sector
    Bmemmove(&wall[firstwall], &wall[newnumwalls], j*sizeof(walltype));

    for (i=firstwall; i<firstwall+j; i++)
    {
        wall[i].point2 += (firstwall-numwalls);

        copy_some_wall_members(i, firstwall+j, 1);
        wall[i].cstat &= ~(1+16+32+64);
    }

    numwalls = newnumwalls;
    newnumwalls = -1;
#ifdef YAX_ENABLE
    if (extendedSector)
    {
        newnumwalls = whitelinescan(k, firstwall);
        if (newnumwalls != newnumwalls2)
            message("AddLoopToSector: newnumwalls != newnumwalls2!!! WTF?");
        for (i=numwalls; i<newnumwalls; i++)
        {
            NEXTWALL(i).nextwall = i;
            NEXTWALL(i).nextsector = numsectors;
        }

        yax_setbunches(numsectors, cbunch, fbunch);

        numwalls = newnumwalls;
        newnumwalls = -1;
        numsectors++;
    }
#endif
    if (ret_ofirstwallofs)
        *ret_ofirstwallofs = j;
    else
        setfirstwall(k, firstwall+j);  // restore old first wall

    return extendedSector;
}

#define EDITING_MAP_P() (newnumwalls>=0 || joinsector[0]>=0 || circlewall>=0 || (bstatus&1))


void overheadeditor(void)
{
    char buffer[80];
    const char *dabuffer;
    int32_t i, j, k, m=0, mousxplc, mousyplc, firstx=0, firsty=0, oposz, col;
    int32_t numwalls_bak;
    int32_t startwall=0, endwall, dax, day, x1, y1, x2, y2, x3, y3; //, x4, y4;
    int32_t highlightx1, highlighty1, highlightx2, highlighty2;
    int16_t bad, joinsector[2];
    int32_t mousx, mousy, bstatus;
    int16_t circlepoints;
    int32_t sectorhighlightx=0, sectorhighlighty=0;
    int16_t cursectorhighlight, sectorhighlightstat;
    int32_t prefixarg = 0, tsign;
    int32_t resetsynctics = 0, lasttick=getticks(), waitdelay=totalclock, lastdraw=getticks();
    int32_t olen[2]={0,0}, dragwall[2] = {-1, -1};

    ovh.suckwall = -1;
    ovh.split = 0;
    ovh.splitsect = -1;
    ovh.splitstartwall = -1;

    qsetmodeany(xdim2d,ydim2d);
    xdim2d = xdim;
    ydim2d = ydim;

    osearchx = searchx;
    osearchy = searchy;

    searchx = clamp(scale(searchx,xdim2d,xdimgame), 8, xdim2d-8-1);
    searchy = clamp(scale(searchy,ydim2d-STATUS2DSIZ2,ydimgame), 8, ydim2d-STATUS2DSIZ-8-1);
    oposz = pos.z;

    yax_updategrays(pos.z);

    begindrawing(); //{{{
    CLEARLINES2D(0, ydim, 0);
    enddrawing(); //}}}

    ydim16 = ydim-STATUS2DSIZ2;

    cursectorhighlight = -1;
    lastpm16time = -1;

    update_highlightsector();
    ovh_whiteoutgrab(0);

    highlightcnt = -1;
    Bmemset(show2dwall, 0, sizeof(show2dwall));  //Clear all highlights
    Bmemset(show2dsprite, 0, sizeof(show2dsprite));

    RESET_EDITOR_VARS();
    bstatus = 0;

    while ((keystatus[buildkeys[BK_MODE2D_3D]]>>1) == 0)
    {
        if (!((vel|angvel|svel) //DOWN_BK(MOVEFORWARD) || DOWN_BK(MOVEBACKWARD) || DOWN_BK(TURNLEFT) || DOWN_BK(TURNRIGHT)
                || DOWN_BK(MOVEUP) || DOWN_BK(MOVEDOWN) || keystatus[0x10] || keystatus[0x11]
                || keystatus[0x48] || keystatus[0x4b] || keystatus[0x4d] || keystatus[0x50]  // keypad keys
                || bstatus || OSD_IsMoving()))
        {
            if (totalclock > waitdelay)
            {
                uint32_t ms = (highlightsectorcnt>0) ? 75 : 200;
                // wait for event, timeout after 200 ms - (last loop time)
                idle_waitevent_timeout(ms - min(getticks()-lasttick, ms));
                // have synctics reset to 0 after we've slept to avoid zooming out to the max instantly
                resetsynctics = 1;
            }
        }
        else waitdelay = totalclock + 30; // should be 250 ms

        lasttick = getticks();

        if (handleevents())
        {
            if (quitevent)
            {
                keystatus[1] = 1;
                quitevent = 0;
            }
        }

        if (resetsynctics)
        {
            resetsynctics = 0;
            lockclock = totalclock;
            synctics = 0;
        }

        OSD_DispatchQueued();

        if (totalclock < 120*3)
            printmessage16("Uses BUILD technology by Ken Silverman.");
        else if (totalclock < 120*6)
        {
            printmessage16("Press F1 for help.  This is a test release; always keep backups of your maps.");
            //        printext16(8L,ydim-STATUS2DSIZ+32L,editorcolors[9],-1,kensig,0);
        }

        oldmousebstatus = bstatus;
        getmousevalues(&mousx,&mousy,&bstatus);
        mousx = (mousx<<16)+mousexsurp;
        mousy = (mousy<<16)+mouseysurp;
        {
            ldiv_t ld;
            ld = ldiv(mousx, 1<<16); mousx = ld.quot; mousexsurp = ld.rem;
            ld = ldiv(mousy, 1<<16); mousy = ld.quot; mouseysurp = ld.rem;
        }
        searchx += mousx;
        searchy += mousy;

        inpclamp(&searchx, 8, xdim-8-1);
        inpclamp(&searchy, 8, ydim-8-1);

        mainloop_move();

        getpoint(searchx,searchy,&mousxplc,&mousyplc);
        linehighlight = getlinehighlight(mousxplc, mousyplc, linehighlight);

        if (newnumwalls >= numwalls)
        {
            // if we're in the process of drawing a wall, set the end point's coordinates
            dax = mousxplc;
            day = mousyplc;
            adjustmark(&dax,&day,numwalls+!ovh.split);
            wall[newnumwalls].x = dax;
            wall[newnumwalls].y = day;
        }

        ydim16 = ydim;// - STATUS2DSIZ2;
        midydim16 = ydim>>1;

        numwalls_bak = numwalls;
        numwalls = newnumwalls;
        if (numwalls < 0)
            numwalls = numwalls_bak;

        if ((getticks() - lastdraw) >= 5 || (vel|angvel|svel) || DOWN_BK(MOVEUP) || DOWN_BK(MOVEDOWN)
                || mousx || mousy || bstatus || keystatus[0x10] || keystatus[0x11]
                || newnumwalls>=0 || OSD_IsMoving())
        {
            lastdraw = getticks();

            clear2dscreen();

            setup_sideview_sincos();

            if (graphicsmode && !m32_sideview)
            {
                Bmemset(show2dsector, 0, sizeof(show2dsector));
                for (i=0; i<numsectors; i++)
                {
                    YAX_SKIPSECTOR(i);
                    show2dsector[i>>3] |= (1<<(i&7));
                }

                setview(0, 0, xdim-1, ydim16-1);

                if (graphicsmode == 2)
                    totalclocklock = totalclock;

                drawmapview(pos.x, pos.y, zoom, 1536);
            }

            draw2dgrid(pos.x,pos.y,pos.z,cursectnum,ang,zoom,grid);

            ExtPreCheckKeys();

            {
                int32_t cx, cy;

                // Draw brown arrow (start)
                screencoords(&x2, &y2, startposx-pos.x,startposy-pos.y, zoom);
                if (m32_sideview)
                    y2 += getscreenvdisp(startposz-pos.z, zoom);

                cx = halfxdim16+x2;
                cy = midydim16+y2;
                if ((cx >= 2 && cx <= xdim-3) && (cy >= 2 && cy <= ydim16-3))
                {
                    int16_t angofs = m32_sideview ? m32_sideang : 0;
                    x1 = mulscale11(sintable[(startang+angofs+2560)&2047],zoom) / 768;
                    y1 = mulscale11(sintable[(startang+angofs+2048)&2047],zoom) / 768;
                    i = scalescreeny(x1);
                    j = scalescreeny(y1);
                    begindrawing();	//{{{
                    drawline16base(cx,cy, x1,j, -x1,-j, editorcolors[2]);
                    drawline16base(cx,cy, x1,j, +y1,-i, editorcolors[2]);
                    drawline16base(cx,cy, x1,j, -y1,+i, editorcolors[2]);
                    enddrawing();	//}}}
                }
            }

            draw2dscreen(&pos,cursectnum,ang,zoom,grid);

            begindrawing();	//{{{
            if (showtags)
            {
                if (zoom >= 768)
                {
                    for (i=0; i<numsectors; i++)
                    {
                        int16_t secshort = i;

                        YAX_SKIPSECTOR(i);

                        dabuffer = ExtGetSectorCaption(i);
                        if (dabuffer[0] == 0)
                            continue;

                        get_sectors_center(&secshort, 1, &dax, &day);

                        drawsmallabel(dabuffer, editorcolors[0], editorcolors[7],
                                       dax, day, getflorzofslope(i,dax,day));
                    }
                }

                x3 = pos.x + divscale14(-halfxdim16,zoom);
                y3 = pos.y + divscale14(-(midydim16-4),zoom);
//                x4 = pos.x + divscale14(halfxdim16,zoom);
//                y4 = pos.y + divscale14(ydim16-(midydim16-4),zoom);

                if (newnumwalls >= 0)
                {
                    for (i=newnumwalls; i>=numwalls_bak; i--)
                        wall[i].cstat |= (1<<14);
                }

                i = numwalls-1;
                j = numsectors-1;  // might be -1 if empty map!
                if (newnumwalls >= 0)
                    i = newnumwalls-1;
                for (; i>=0; i--)
                {
                    const walltype *wal = &wall[i];

                    if (j>=0 && sector[j].wallptr > i)
                        j--;

                    if (zoom < 768 && !(wal->cstat & (1<<14)))
                        continue;

                    YAX_SKIPWALL(i);

                    //Get average point of wall
//                    if ((dax > x3) && (dax < x4) && (day > y3) && (day < y4))
                    {
                        dabuffer = ExtGetWallCaption(i);
                        if (dabuffer[0] == 0)
                            continue;

                        dax = (wal->x+wall[wal->point2].x)>>1;
                        day = (wal->y+wall[wal->point2].y)>>1;
                        drawsmallabel(dabuffer, editorcolors[0], editorcolors[31],
                                      dax, day, (i >= numwalls || j<0) ? 0 : getflorzofslope(j, dax,day));
                    }
                }

                if (zoom >= 768)
                {
                    int32_t alwaysshowgray =
                        (showinnergray || !(editorzrange[0]==INT32_MIN && editorzrange[1]==INT_MAX));

                    for (i=0, k=0; (m32_sideview && k<m32_swcnt) || (!m32_sideview && i<MAXSPRITES); i++, k++)
                    {
                        if (m32_sideview)
                        {
                            i = m32_wallsprite[k];
                            if (i<MAXWALLS)
                                continue;
                            i = i-MAXWALLS;
                        }
                        else
                            if (sprite[i].statnum == MAXSTATUS)
                                continue;

                        if ((!m32_sideview || !alwaysshowgray) && sprite[i].sectnum >= 0)
                            YAX_SKIPSECTOR(sprite[i].sectnum);

                        dabuffer = ExtGetSpriteCaption(i);
                        if (dabuffer[0] != 0)
                        {
                            int32_t blocking = (sprite[i].cstat&1);

                            col = 3 + 2*blocking;
                            if (spritecol2d[sprite[i].picnum][blocking])
                                col = spritecol2d[sprite[i].picnum][blocking];

                            if ((i == pointhighlight-16384) && (totalclock & 32))
                                col += (2<<2);

                            drawsmallabel(dabuffer, editorcolors[0], editorcolors[col],
                                          sprite[i].x, sprite[i].y, sprite[i].z);
                        }
                    }
                }
            }

            // stick this event right between begin- end enddrawing()...
            // also after the above label stuff so users can redefine them
            VM_OnEvent(EVENT_DRAW2DSCREEN, -1);

            printcoords16(pos.x,pos.y,ang);

            numwalls = numwalls_bak;

            if (highlightsectorcnt >= 0)
            {
                for (i=0; i<numsectors; i++)
                    if (hlsectorbitmap[i>>3]&(1<<(i&7)))
                        fillsector(i, -1);
            }

            if (keystatus[0x2a]) // FIXME
            {
                drawlinepat = 0x00ff00ff;
                drawline16(searchx,0, searchx,ydim2d-1, editorcolors[15]);
                drawline16(0,searchy, xdim2d-1,searchy, editorcolors[15]);
                drawlinepat = 0xffffffff;

                _printmessage16("(%d,%d)",mousxplc,mousyplc);
#if 0
                i = (Bstrlen(tempbuf)<<3)+6;
                if ((searchx+i) < (xdim2d-1))
                    i = 0;
                else i = (searchx+i)-(xdim2d-1);
                if ((searchy+16) < (ydim2d-STATUS2DSIZ2-1))
                    j = 0;
                else j = (searchy+16)-(ydim2d-STATUS2DSIZ2-1);
                printext16(searchx+6-i,searchy+6-j,editorcolors[11],-1,tempbuf,0);
#endif
            }
            drawline16(searchx,0, searchx,8, editorcolors[15]);
            drawline16(0,searchy, 8,searchy, editorcolors[15]);

            ////// draw mouse pointer
            col = editorcolors[15 - 3*gridlock];
            if (joinsector[0] >= 0)
                col = editorcolors[11];

            if (numcorruptthings>0)
            {
                static char cbuf[64];

                if ((pointhighlight&16384)==0)
                {
                    for (i=0; i<numcorruptthings; i++)
                        if ((corruptthings[i]&CORRUPT_MASK)==CORRUPT_WALL &&
                            (corruptthings[i]&(MAXWALLS-1))==pointhighlight)
                        {
                            col = editorcolors[13];
                            printext16(searchx+6,searchy-6-8,editorcolors[13],editorcolors[0],"corrupt wall",0);
                            break;
                        }
                }

                Bsprintf(cbuf, "Map corrupt (level %d): %s%d errors", corruptlevel,
                         numcorruptthings>=MAXCORRUPTTHINGS ? ">=":"", numcorruptthings);
                printext16(8,8, editorcolors[13],editorcolors[0],cbuf,0);
            }

            if (highlightsectorcnt==0 || highlightcnt==0)
            {
                if (keystatus[0x27] || keystatus[0x28])  // ' and ;
                {
                    col = editorcolors[14];

                    drawline16base(searchx+16, searchy-16, -4,0, +4,0, col);
                    if (keystatus[0x28])
                        drawline16base(searchx+16, searchy-16, 0,-4, 0,+4, col);
                }

                if (highlightsectorcnt == 0)
                    if (keystatus[0x36])
                        printext16(searchx+6, searchy-2+8,editorcolors[12],-1,"ALL",0);

                if (highlightcnt == 0)
                {
                    if (eitherCTRL && (highlightx1!=highlightx2 || highlighty1!=highlighty2))
                        printext16(searchx+6,searchy-6-8,editorcolors[12],-1,"SPR ONLY",0);
#ifdef YAX_ENABLE
                    if (keystatus[0xcf])  // End
                        printext16(searchx+6,searchy-2+8,editorcolors[12],-1,"ALL",0);
#endif
                }
            }

            drawline16base(searchx,searchy, +0,-8, +0,-1, col);
            drawline16base(searchx,searchy, +1,-8, +1,-1, col);
            drawline16base(searchx,searchy, +0,+2, +0,+9, col);
            drawline16base(searchx,searchy, +1,+2, +1,+9, col);
            drawline16base(searchx,searchy, -8,+0, -1,+0, col);
            drawline16base(searchx,searchy, -8,+1, -1,+1, col);
            drawline16base(searchx,searchy, +2,+0, +9,+0, col);
            drawline16base(searchx,searchy, +2,+1, +9,+1, col);

            ////// Draw the white pixel closest to mouse cursor on linehighlight
            if (linehighlight>=0)
            {
                char col = wall[linehighlight].nextsector >= 0 ? editorcolors[15] : editorcolors[5];

                if (m32_sideview)
                {
                    getclosestpointonwall(searchx,searchy, linehighlight, &dax,&day, 1);
                    drawline16base(dax,day, 0,0, 0,0, col);
                }
                else
                {
                    getclosestpointonwall(mousxplc,mousyplc, linehighlight, &dax,&day, 0);
                    x2 = mulscale14(dax-pos.x,zoom);
                    y2 = mulscale14(day-pos.y,zoom);

                    drawline16base(halfxdim16+x2,midydim16+y2, 0,0, 0,0, col);
                }
            }

            enddrawing();	//}}}

            OSD_Draw();
        }

        VM_OnEvent(EVENT_PREKEYS2D, -1);
        ExtCheckKeys(); // TX 20050101, it makes more sense to have this here so keys can be overwritten with new functions in bstub.c

        // Flip/mirror sector Ed Coolidge
        if (keystatus[0x2d] || keystatus[0x15])  // X or Y (2D)
        {
            int32_t about_x=keystatus[0x2d];
            int32_t doMirror = eitherALT;  // mirror walls and wall/floor sprites

#ifdef YAX_ENABLE
            if (highlightsectorcnt > 0 && !hl_all_bunch_sectors_p())
            {
                printmessage16("To flip extended sectors, all sectors of a bunch must be selected");
                keystatus[0x2d] = keystatus[0x15] = 0;
            }
            else
#endif
            if (highlightsectorcnt > 0)
            {
                int16_t *const otonwall = onextwall;  // OK, since we make old-nextwalls invalid

                mkonwinvalid();

                keystatus[0x2d] = keystatus[0x15] = 0;

                for (j=0; j<numwalls; j++)
                    otonwall[j] = j;

                get_sectors_center(highlightsector, highlightsectorcnt, &dax, &day);

                if (gridlock && grid > 0)
                    locktogrid(&dax, &day);

                for (i=0; i<highlightsectorcnt; i++)
                {
                    int32_t startofloop, endofloop;
                    int32_t numtoswap = -1;
                    int32_t w=0;
                    walltype tempwall;

                    startofloop = startwall = sector[highlightsector[i]].wallptr;
                    endofloop = endwall = startwall+sector[highlightsector[i]].wallnum-1;
#if 0
                    if (doMirror)
                    {
                        //mirror sector textures
                        sector[highlightsector[i]].ceilingstat ^= 0x10;
                        sector[highlightsector[i]].floorstat ^= 0x10;
                    }
#endif
                    //save position of wall at start of loop
                    x3 = wall[startofloop].x;
                    y3 = wall[startofloop].y;

                    for (j=startwall; j<=endwall; j++)
                    {
                        //fix position of walls
                        if (about_x)
                        {
                            wall[j].x = dax-POINT2(j).x+dax; //flip wall.x about dax
                            wall[j].y = POINT2(j).y;
                        }
                        else
                        {
                            wall[j].x = POINT2(j).x;
                            wall[j].y = day-POINT2(j).y+day; //flip wall.y about day
                        }

                        if (doMirror)
                            wall[j].cstat ^= 8;  //mirror walls about dax/day

                        if (wall[j].point2==startofloop) //check if j is end of loop
                        {
                            endofloop = j;
                            if (about_x)
                            {
                                wall[endofloop].x = dax-x3+dax; //flip wall.x about dax
                                wall[endofloop].y = y3;
                            }
                            else
                            {
                                wall[endofloop].x = x3;
                                wall[endofloop].y = day-y3+day; //flip wall.y about dax
                            }

                            //correct order of walls in loop to maintain player space (right-hand rule)
                            numtoswap = (endofloop-startofloop)>>1;
                            for (w=1; w<=numtoswap; w++)
                            {
                                Bmemcpy(&tempwall, &wall[startofloop+w], sizeof(walltype));
                                Bmemcpy(&wall[startofloop+w], &wall[endofloop-w+1], sizeof(walltype));
                                Bmemcpy(&wall[endofloop-w+1], &tempwall, sizeof(walltype));

                                otonwall[startofloop+w] = endofloop-w+1;
                                otonwall[endofloop-w+1] = startofloop+w;
                            }

                            //make point2 point to next wall in loop
                            for (w=startofloop; w<endofloop; w++)
                                wall[w].point2 = w+1;
                            wall[endofloop].point2 = startofloop;

                            startofloop = endofloop+1; //set first wall of next loop
                            //save position of wall at start of loop
                            x3 = wall[startofloop].x;
                            y3 = wall[startofloop].y;
                        }
                    }

                    j = headspritesect[highlightsector[i]];
                    while (j != -1)
                    {
                        if (about_x)
                        {
                            x3 = sprite[j].x;
                            sprite[j].x = dax-x3+dax; //flip sprite.x about dax
                            sprite[j].ang = (1024+2048-sprite[j].ang)&2047; //flip ang about 512
                        }
                        else
                        {
                            y3 = sprite[j].y;
                            sprite[j].y = day-y3+day; //flip sprite.y about day
                            sprite[j].ang = (2048-sprite[j].ang)&2047; //flip ang about 512
                        }

                        if (doMirror && (sprite[j].cstat & 0x30))
                            sprite[j].cstat ^= 4;  // mirror sprites about dax/day (don't mirror monsters)

                        j = nextspritesect[j];
                    }
                }

                // finally, construct the nextwalls and yax-nextwalls
                // for the new arrangement!
                for (i=0; i<highlightsectorcnt; i++)
                {
                    for (WALLS_OF_SECTOR(i, j))
                    {
                        if (wall[j].nextwall >= 0)
                            wall[j].nextwall = otonwall[wall[j].nextwall];
#ifdef YAX_ENABLE
                        {
                            int32_t cf, ynw;
                            for (cf=0; cf<2; cf++)
                                if ((ynw = yax_getnextwall(j, cf)) >= 0)
                                    yax_setnextwall(j, cf, otonwall[ynw]);
                        }
#endif
                    }
                }

                printmessage16("Selected sector(s) flipped");
                asksave = 1;
            }
        }
        // end edit for sector flip

        if (keystatus[88])   //F12
        {
            keystatus[88] = 0;
//__clearscreen_beforecapture__

            Bsprintf(tempbuf, "Mapster32 %s", ExtGetVer());
            screencapture("captxxxx.tga", eitherSHIFT, tempbuf);

            showframe(1);
        }
        if (keystatus[0x30])  // B (clip Blocking xor) (2D)
        {
            pointhighlight = getpointhighlight(mousxplc, mousyplc, pointhighlight);
            linehighlight = getlinehighlight(mousxplc, mousyplc, linehighlight);

            if ((pointhighlight&0xc000) == 16384)
            {
                sprite[pointhighlight&16383].cstat ^= 1;
                sprite[pointhighlight&16383].cstat &= ~256;
                sprite[pointhighlight&16383].cstat |= ((sprite[pointhighlight&16383].cstat&1)<<8);
                asksave = 1;
            }
            else if (linehighlight >= 0)
            {
                wall[linehighlight].cstat ^= 1;
                wall[linehighlight].cstat &= ~64;
                if ((wall[linehighlight].nextwall >= 0) && !eitherSHIFT)
                {
                    NEXTWALL(linehighlight).cstat &= ~(1+64);
                    NEXTWALL(linehighlight).cstat |= (wall[linehighlight].cstat&1);
                }
                asksave = 1;
            }
            keystatus[0x30] = 0;
        }
        if (keystatus[0x21])  //F (F alone does nothing in 2D right now)
        {
            keystatus[0x21] = 0;
            if (eitherALT)  //ALT-F (relative alignmment flip)
            {
                linehighlight = getlinehighlight(mousxplc, mousyplc, linehighlight);
                if (linehighlight >= 0)
                    SetFirstWall(sectorofwall(linehighlight), linehighlight);
            }
        }

        if (keystatus[0x18])  // O (ornament onto wall) (2D)
        {
            keystatus[0x18] = 0;
            if ((pointhighlight&0xc000) == 16384)
            {
                asksave = 1;
                DoSpriteOrnament(pointhighlight&16383);
            }
        }


        tsign = 0;
        if (keystatus[0x33] || (bstatus&33)==33)  // , (2D)
            tsign = +1;
        if (keystatus[0x34] || (bstatus&17)==17)  // . (2D)
            tsign = -1;

        if (tsign)
        {
#ifdef YAX_ENABLE
            if (highlightsectorcnt > 0 && !hl_all_bunch_sectors_p())
            {
                printmessage16("To rotate ext. sectors, all sectors of a bunch must be selected");
            }
            else
#endif
            if (highlightsectorcnt > 0)
            {
                int32_t smoothRotation = !eitherSHIFT;

                get_sectors_center(highlightsector, highlightsectorcnt, &dax, &day);

                if (smoothRotation)
                {
                    if (gridlock && grid > 0)
                        locktogrid(&dax, &day);
                }

                for (i=0; i<highlightsectorcnt; i++)
                {
                    for (WALLS_OF_SECTOR(highlightsector[i], j))
                    {
                        if (smoothRotation)
                        {
                            x3 = wall[j].x;
                            y3 = wall[j].y;
                            wall[j].x = dax + tsign*(day-y3);
                            wall[j].y = day + tsign*(x3-dax);
                        }
                        else
                            rotatepoint(dax,day, wall[j].x,wall[j].y, tsign&2047, &wall[j].x,&wall[j].y);
                    }

                    for (j=headspritesect[highlightsector[i]]; j != -1; j=nextspritesect[j])
                    {
                        if (smoothRotation)
                        {
                            x3 = sprite[j].x;
                            y3 = sprite[j].y;
                            sprite[j].x = dax + tsign*(day-y3);
                            sprite[j].y = day + tsign*(x3-dax);
                            sprite[j].ang = (sprite[j].ang+tsign*512)&2047;
                        }
                        else
                        {
                            rotatepoint(dax,day, sprite[j].x,sprite[j].y, tsign&2047, &sprite[j].x,&sprite[j].y);
                            sprite[j].ang = (sprite[j].ang+tsign)&2047;
                        }
                    }
                }

                if (smoothRotation)
                    keystatus[0x33] = keystatus[0x34] = 0;

                mouseb &= ~(16|32);
                bstatus &= ~(16|32);
                asksave = 1;
            }
            else
            {
                if (pointhighlight >= 16384)
                {
                    i = pointhighlight-16384;
                    if (eitherSHIFT)
                        sprite[i].ang = (sprite[i].ang-tsign)&2047;
                    else
                    {
                        sprite[i].ang = (sprite[i].ang-128*tsign)&2047;
                        keystatus[0x33] = keystatus[0x34] = 0;
                    }

                    mouseb &= ~(16|32);
                    bstatus &= ~(16|32);
                }
            }
        }

        if (keystatus[0x46])  //Scroll lock (set starting position)
        {
            startposx = pos.x;
            startposy = pos.y;
            startposz = pos.z;
            startang = ang;
            startsectnum = cursectnum;
            keystatus[0x46] = 0;
            asksave = 1;
        }
#if 1
        if (keystatus[0x3f])  //F5
        {
            ExtShowSectorData(0);
        }
        if (keystatus[0x40])  //F6
        {
            if (pointhighlight >= 16384)
                ExtShowSpriteData(pointhighlight-16384);
            else if (linehighlight >= 0)
                ExtShowWallData(linehighlight);
            else
                ExtShowWallData(0);
        }
        if (keystatus[0x41])  //F7
        {
            keystatus[0x41] = 0;

            for (i=0; i<numsectors; i++)
                if (inside_editor_curpos(i) == 1)
                {
                    YAX_SKIPSECTOR(i);

                    ExtEditSectorData(i);
                    break;
                }
        }
        if (keystatus[0x42])  //F8
        {
            keystatus[0x42] = 0;

            if (pointhighlight >= 16384)
                ExtEditSpriteData(pointhighlight-16384);
            else if (linehighlight >= 0)
                ExtEditWallData(linehighlight);
        }
#endif

        if (keystatus[0x23])  //H (Hi 16 bits of tag)
        {
            keystatus[0x23] = 0;
            if (eitherCTRL)  //Ctrl-H
            {
                pointhighlight = getpointhighlight(mousxplc, mousyplc, pointhighlight);
                linehighlight = getlinehighlight(mousxplc, mousyplc, linehighlight);

                if ((pointhighlight&0xc000) == 16384)
                {
                    sprite[pointhighlight&16383].cstat ^= 256;
                    asksave = 1;
                }
                else if (linehighlight >= 0)
                {
                    wall[linehighlight].cstat ^= 64;
                    if ((wall[linehighlight].nextwall >= 0) && !eitherSHIFT)
                    {
                        NEXTWALL(linehighlight).cstat &= ~64;
                        NEXTWALL(linehighlight).cstat |= (wall[linehighlight].cstat&64);
                    }
                    asksave = 1;
                }
            }
            else if (eitherALT)  //ALT
            {
                if (pointhighlight >= 16384)
                {
                    i = pointhighlight-16384;
                    j = taglab_linktags(1, i);
                    j = 2*(j&2);
                    Bsprintf(buffer, "Sprite (%d) Hi-tag: ", i);
                    sprite[i].hitag = getnumber16(buffer, sprite[i].hitag, BTAG_MAX, 0+j);
                }
                else if (linehighlight >= 0)
                {
                    i = linehighlight;
                    j = taglab_linktags(1, i);
                    j = 2*(j&2);
                    Bsprintf(buffer, "Wall (%d) Hi-tag: ", i);
                    wall[i].hitag = getnumber16(buffer, wall[i].hitag, BTAG_MAX, 0+j);
                }
            }
            else
            {
                for (i=0; i<numsectors; i++)
                    if (inside_editor_curpos(i) == 1)
                    {
                        YAX_SKIPSECTOR(i);

                        Bsprintf(buffer, "Sector (%d) Hi-tag: ", i);
                        sector[i].hitag = getnumber16(buffer, sector[i].hitag, BTAG_MAX, 0);
                        break;
                    }
            }
            // printmessage16("");
        }
        if (keystatus[0x19])  // P (palookup #)
        {
            keystatus[0x19] = 0;

            for (i=0; i<numsectors; i++)
                if (inside_editor_curpos(i) == 1)
                {
                    YAX_SKIPSECTOR(i);

                    Bsprintf(buffer, "Sector (%d) Ceilingpal: ", i);
                    sector[i].ceilingpal = getnumber16(buffer, sector[i].ceilingpal, M32_MAXPALOOKUPS, 0);

                    Bsprintf(buffer, "Sector (%d) Floorpal: ", i);
                    sector[i].floorpal = getnumber16(buffer, sector[i].floorpal, M32_MAXPALOOKUPS, 0);
                    break;
                }
        }
        if (keystatus[0x12])  // E (status list)
        {
            keystatus[0x12] = 0;

            if (!eitherCTRL)
            {
                if (pointhighlight >= 16384)
                {
                    i = pointhighlight-16384;
                    Bsprintf(buffer, "Sprite (%d) Status list: ", i);
                    changespritestat(i, getnumber16(buffer, sprite[i].statnum, MAXSTATUS-1, 0));
                }
            }
#ifdef YAX_ENABLE
            else if (highlightsectorcnt > 0 && newnumwalls < 0)
            {
                ////////// YAX //////////
                static const char *cfs[2] = {"ceiling", "floor"};

                int32_t cf, thez, ulz[2]={0,0};
                int16_t bn, sandwichbunch=-1;

                if (numyaxbunches==YAX_MAXBUNCHES)
                {
                    message("Bunch limit of %d reached, cannot extend", YAX_MAXBUNCHES);
                    goto end_yax;
                }

                if (highlighted_sectors_components(0,0) != 1)
                {
                    message("Sectors to extend must be in one connected component");
                    goto end_yax;
                }

                cf = ask_above_or_below();
                if (cf==-1)
                    goto end_yax;

                thez = SECTORFLD(highlightsector[0],z, cf);
                for (i=0; i<highlightsectorcnt; i++)
                {
                    bn = yax_getbunch(highlightsector[i], cf);

                    if (sandwichbunch >= 0 && bn!=sandwichbunch)
                    {
                        message("When sandwiching extension, must select only sectors of one bunch");
                        goto end_yax;
                    }

                    if (bn >= 0)
                    {
                        if (cf==YAX_FLOOR)
                        {
                            if (sandwichbunch < 0 && i!=0)
                            {
                                message("When sandwiching extension, must select only sectors of the bunch");
                                goto end_yax;
                            }
                            sandwichbunch = bn;
                        }
                        else
                        {
                            message("Sector %d's %s is already extended", highlightsector[i], cfs[cf]);
                            goto end_yax;
                        }
                    }

                    if (SECTORFLD(highlightsector[i],z, cf) != thez)
                    {
                        message("Sector %d's %s height doesn't match sector %d's\n",
                                highlightsector[i], cfs[cf], highlightsector[0]);
                        goto end_yax;
                    }

                    if ((sandwichbunch>=0 || highlightsectorcnt>1) && SECTORFLD(highlightsector[i],stat, cf)&2)
                    {
                        message("Sector %ss must not be sloped%s", cfs[cf],
                                sandwichbunch>=0 ? "" : "if extending more than one");
                        goto end_yax;
                    }
                }

                if (sandwichbunch >= 0)
                {
                    // cf==YAX_FLOOR here

                    int32_t tempz, oldfz, swsecheight = DEFAULT_YAX_HEIGHT/4;
                    // highest floor z of lower sectors, lowest ceiling z of these sectors
                    int32_t minfloorz = INT32_MAX, maxceilz = INT32_MIN;

                    // some preparation for making the sandwich
                    if (highlightsectorcnt != yax_numsectsinbunch(sandwichbunch, YAX_FLOOR))
                    {
                        message("When sandwiching extension, must select all sectors of the bunch");
                        goto end_yax;
                    }

                    // "for i in sectors of sandwichbunch(floor)" is now the same as
                    // "for i in highlighted sectors"

                    oldfz = sector[highlightsector[0]].floorz;

                    // check if enough room in z
                    for (SECTORS_OF_BUNCH(sandwichbunch, YAX_CEILING, i))
                        for (WALLS_OF_SECTOR(i, j))
                        {
                            tempz = getflorzofslope(i, wall[j].x, wall[j].y);
                            minfloorz = min(minfloorz, tempz);
                        }
                    for (SECTORS_OF_BUNCH(sandwichbunch, YAX_FLOOR, i))
                        for (WALLS_OF_SECTOR(i, j))
                        {
                            tempz = getceilzofslope(i, wall[j].x, wall[j].y);
                            maxceilz = max(maxceilz, tempz);
                        }

                    if (minfloorz - maxceilz < 2*swsecheight)
                    {
                        message("Too little z headroom for sandwiching, need at least %d",
                                2*swsecheight);
                        goto end_yax;
                    }

                    if (maxceilz >= oldfz || oldfz >= minfloorz)
                    {
                        message("Internal error while sandwiching: oldfz out of bounds");
                        goto end_yax;
                    }

                    // maxceilz   ---|
                    //   ^           |
                    // ulz[0]        ^
                    //   ^          oldfz
                    // ulz[1]        ^
                    //   ^           |
                    // minfloorz  ---|

                    ulz[0] = oldfz - swsecheight*((double)(oldfz-maxceilz)/(minfloorz-maxceilz));
                    ulz[0] &= ~255;
                    ulz[1] = ulz[0] + swsecheight;

                    if (maxceilz >= ulz[0] || ulz[1] >= minfloorz)
                    {
                        message("Too little z headroom for sandwiching");
                        goto end_yax;
                    }
                }

                m = numwalls;
                Bmemset(visited, 0, sizeof(visited));
                // construct!
                for (i=0; i<highlightsectorcnt; i++)
                    for (WALLS_OF_SECTOR(highlightsector[i], j))
                    {
                        k = trace_loop(j, visited, NULL, NULL, !cf);
                        if (k == 0)
                            continue;
                        else if (k < 0)
                        {
                            numwalls = m;
                            goto end_yax;
                        }
//message("loop");
                        wall[k-1].point2 = numwalls;
                        numwalls = k;
                    }

                for (i=m; i<numwalls; i++)  // try
                {
                    j = YAX_NEXTWALL(i, !cf);
                    if (j < 0)
                    {
                        message("Internal error while constructing sector: "
                                "YAX_NEXTWALL(%d, %d)<0!", i, !cf);
                        numwalls = m;
                        goto end_yax;
                    }
                    if (sandwichbunch >= 0)
                    {
                        if (YAX_NEXTWALL(j, cf) < 0)
                        {
                            message("Internal error while sandwiching (2): "
                                    "YAX_NEXTWALL(%d, %d)<0!", j, cf);
                            numwalls = m;
                            goto end_yax;
                        }
                    }
                }
                for (i=m; i<numwalls; i++)  // do!
                {
                    j = YAX_NEXTWALL(i, !cf);

                    if (sandwichbunch >= 0)
                    {
                        int16_t oynw = YAX_NEXTWALL(j, cf);
                        yax_setnextwall(j, cf, i);
                        yax_setnextwall(i, cf, oynw);
                        yax_setnextwall(oynw, !cf, i);
                    }
                    else
                    {
                        yax_setnextwall(j, cf, i);
                    }
                }

                // create new sector based on first highlighted one
                i = highlightsector[0];
                Bmemcpy(&sector[numsectors], &sector[i], sizeof(sectortype));
                sector[numsectors].wallptr = m;
                sector[numsectors].wallnum = numwalls-m;

                if (sandwichbunch < 0)
                {
                    if (SECTORFLD(i,stat, cf)&2)
                        setslope(numsectors, !cf, SECTORFLD(i,heinum, cf));
                    else
                        setslope(numsectors, !cf, 0);
                    setslope(numsectors, cf, 0);

                    SECTORFLD(numsectors,z, !cf) = SECTORFLD(i,z, cf);
                    SECTORFLD(numsectors,z, cf) = SECTORFLD(i,z, cf) - (1-2*cf)*DEFAULT_YAX_HEIGHT;
                }
                else
                {
                    for (SECTORS_OF_BUNCH(sandwichbunch, cf, i))
                        sector[i].floorz = ulz[0];
                    sector[numsectors].ceilingz = ulz[0];
                    sector[numsectors].floorz = ulz[1];
                    for (SECTORS_OF_BUNCH(sandwichbunch, !cf, i))
                        sector[i].ceilingz = ulz[1];
                }

                newnumwalls = numwalls;
                numwalls = m;

                SECTORFLD(numsectors,stat, !cf) &= ~1;  // no plax

                // restore red walls of the selected sectors
                for (i=0; i<highlightsectorcnt; i++)
                {
                    SECTORFLD(highlightsector[i],stat, cf) &= ~1;  // no plax

                    for (WALLS_OF_SECTOR(highlightsector[i], j))
                        if (wall[j].nextwall < 0)
                            checksectorpointer(j, highlightsector[i]);
                }

                // link
                if (sandwichbunch < 0)
                {
                    yax_setbunch(numsectors, !cf, numyaxbunches);
                    for (i=0; i<highlightsectorcnt; i++)
                        yax_setbunch(highlightsector[i], cf, numyaxbunches);
                }
                else
                {
                    yax_setbunch(numsectors, !cf, sandwichbunch);
                    // also relink
                    yax_setbunch(numsectors, cf, numyaxbunches);
                    for (SECTORS_OF_BUNCH(sandwichbunch, !cf, i))
                        yax_setbunch(i, !cf, numyaxbunches);
                }

                numwalls = newnumwalls;
                newnumwalls = -1;

                numsectors++;
                yax_update(0);
                yax_updategrays(pos.z);

                Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
                update_highlightsector();

                if (sandwichbunch < 0)
                    message("Extended %ss of highlighted sectors, creating bunch %d",
                            cfs[cf], numyaxbunches-1);
                else
                    message("Sandwiched bunch %d, creating bunch %d",
                            sandwichbunch, numyaxbunches-1);
                asksave = 1;
            }
            else if (highlightcnt > 0)
            {
                /// 'punch' wall loop through extension

                int32_t loopstartwall = -1, numloopwalls, cf;
                int32_t srcsect, dstsect, ofirstwallofs;
                int16_t cb, fb, bunchnum;

                if (EDITING_MAP_P())
                {
                    printmessage16("Must not be editing map to punch loop");
                    goto end_yax;
                }

                if (numyaxbunches >= YAX_MAXBUNCHES)
                {
                    message("TROR bunch limit reached, cannot punch loop");
                    goto end_yax;
                }

                // determine start wall
                for (i=0; i<highlightcnt; i++)
                {
                    j = highlight[i];

                    if (j&16384)
                        continue;

                    // we only want loop-starting walls
                    if (j>0 && wall[j-1].point2==j)
                        continue;

                    if (clockdir(j)==CLOCKDIR_CCW)
                    {
                        YAX_SKIPWALL(j);

                        if (loopstartwall >= 0)
                        {
                            message("Must have a unique highlighted CCW loop to punch");
                            goto end_yax;
                        }

                        loopstartwall = j;
                    }
                }

                if (loopstartwall == -1)
                {
                    message("Didn't find any non-grayed out loop start walls");
                    goto end_yax;
                }

                // determine sector
                srcsect = sectorofwall(loopstartwall);
                yax_getbunches(srcsect, &cb, &fb);
                if (cb < 0 && fb < 0)
                {
                    message("Ceiling or floor must be extended to punch loop");
                    goto end_yax;
                }

                /// determine c/f
                cf = -1;
                if (fb < 0)
                    cf = YAX_CEILING;
                else if (cb < 0)
                    cf = YAX_FLOOR;

                // query top/bottom
                if (cf == -1)
                {
                    char dachars[2] = {'a', 'z'};
                    cf = editor_ask_function("Punch loop above (a) or below (z)?", dachars, 2);
                    if (cf == -1)
                        goto end_yax;
                }
                else
                {
                    // ask even if only one choice -- I find it more
                    // consistent with 'extend sector' this way
                    if (-1 == editor_ask_function(cf==YAX_CEILING ? "Punch loop above (a)?" :
                                                  "Punch loop below (z)?", cf==YAX_CEILING?"a":"z", 1))
                        goto end_yax;
                }

                bunchnum = (cf==YAX_CEILING) ? cb : fb;

                // check 1
                j = loopstartwall;  // will be real start wall of loop
                numloopwalls = 1;  // will be number of walls in loop
                for (i=wall[loopstartwall].point2; i!=loopstartwall; i=wall[i].point2)
                {
                    numloopwalls++;
                    if (i < j)
                        j = i;

                    if ((show2dwall[i>>3]&(1<<(i&7)))==0)
                    {
                        message("All loop points must be highlighted to punch");
                        goto end_yax;
                    }

                    if (yax_getnextwall(loopstartwall, cf) >= 0 || yax_getnextwall(i, cf) >= 0)
                    {
                        // somewhat redundant, since it would also be caught by check 2
                        message("Loop walls must not already have TROR neighbors");
                        goto end_yax;
                    }

                    if (wall[loopstartwall].nextwall < 0 || wall[i].nextwall < 0)
                    {
                        message("INTERNAL ERROR: All loop walls are expected to be red");
                        goto end_yax;
                    }
                }
                loopstartwall = j;

                if (numwalls + 2*numloopwalls > MAXWALLS || numsectors+1 > MAXSECTORS)
                {
                    message("Punching loop through extension would exceed limits");
                    goto end_yax;
                }

                // get other-side sector, j==loopstartwall
                dstsect = yax_getneighborsect(wall[j].x, wall[j].y, srcsect, cf, NULL);
                if (dstsect < 0)
                {
                    message("Punch loop INTERNAL ERROR: dstsect < 0. Map corrupt?");
                    goto end_yax;
                }

                // check 2
                i = loopstartwall;
                do
                {
                    j = wall[i].point2;

                    for (WALLS_OF_SECTOR(dstsect, k))
                    {
                        vec2_t pint;
                        if (lineintersect2v((vec2_t *)&wall[i], (vec2_t *)&wall[j],
                                                (vec2_t *)&wall[k], (vec2_t *)&POINT2(k), &pint))
                        {
                            message("Loop lines must not intersect any destination sector's walls");
                            goto end_yax;
                        }
                    }
                }
                while ((i = j) != loopstartwall);

                // construct new loop and (dummy yet) sector
                Bmemcpy(&wall[numwalls], &wall[loopstartwall], numloopwalls*sizeof(walltype));
                newnumwalls = numwalls+numloopwalls;

                for (i=numwalls; i<newnumwalls; i++)
                {
                    wall[i].point2 += (numwalls - loopstartwall);
                    wall[i].nextsector = wall[i].nextwall = -1;
                }

                sector[numsectors].wallptr = numwalls;
                sector[numsectors].wallnum = numloopwalls;
                numsectors++; // temp

                // check 3
                for (SECTORS_OF_BUNCH(bunchnum, !cf, i))
                    for (WALLS_OF_SECTOR(i, j))
                    {
                        if (inside(wall[j].x, wall[j].y, numsectors-1)==1)
                        {
                            numsectors--;
                            newnumwalls = -1;
                            message("A point of bunch %d's sectors lies inside the loop to punch",
                                    bunchnum);
                            goto end_yax;
                        }
                    }

                numsectors--;

                // clear wall & sprite highlights
                //  TODO: see about consistency with update_highlight() after other ops
                Bmemset(show2dwall, 0, sizeof(show2dwall));
                Bmemset(show2dsprite, 0, sizeof(show2dsprite));
                update_highlight();

                // construct the loop!
                i = AddLoopToSector(dstsect, &ofirstwallofs);

                if (i <= 0)
                {
                    message("Punch loop INTERNAL ERROR with AddLoopToSector!");
                }
                else
                {
                    int32_t oneinnersect = -1, innerdstsect = numsectors-1;

                    if (dstsect < srcsect)
                        loopstartwall += numloopwalls;

                    /// handle bunchnums! (specifically, create a new one)

                    // collect sectors inside source loop; for that, first break the
                    // inner->outer nextwall links
                    for (i=loopstartwall; i<loopstartwall+numloopwalls; i++)
                    {
                        // all src loop walls are red!
                        NEXTWALL(i).nextwall = NEXTWALL(i).nextsector = -1;
                        oneinnersect = wall[i].nextsector;
                    }

                    // vvv
                    // expect oneinnersect >= 0 here!  Assumption: we collect exactly
                    // one connected component of sectors
                    collect_sectors1(collsectlist[0], collsectbitmap[0],
                                     &collnumsects[0], oneinnersect, 0, 0);

                    // set new bunchnums
                    for (i=0; i<collnumsects[0]; i++)
                        yax_setbunch(collsectlist[0][i], cf, numyaxbunches);
                    yax_setbunch(innerdstsect, !cf, numyaxbunches);
                    // ^^^

                    // restore inner->outer nextwall links
                    for (i=loopstartwall; i<loopstartwall+numloopwalls; i++)
                    {
                        NEXTWALL(i).nextwall = i;
                        NEXTWALL(i).nextsector = srcsect;

                        // set yax-nextwalls!
                        j = (i-loopstartwall) + sector[dstsect].wallptr;
                        yax_setnextwall(i, cf, j);
                        yax_setnextwall(j, !cf, i);

                        yax_setnextwall(wall[i].nextwall, cf, wall[j].nextwall);
                        yax_setnextwall(wall[j].nextwall, !cf, wall[i].nextwall);
                    }

                    setfirstwall(dstsect, sector[dstsect].wallptr+ofirstwallofs);

                    message("Punched loop starting w/ wall %d into %s sector %d%s",
                            loopstartwall, cf==YAX_CEILING?"upper":"lower", dstsect,
                            (oneinnersect>=0) ? "" : " (ERRORS)");
                }

                mkonwinvalid();
                asksave = 1;

                yax_update(0);
                yax_updategrays(pos.z);
            }
end_yax: ;
#endif
        }

        if (highlightsectorcnt < 0)
        {
            if (keystatus[0x36])  //Right shift (point highlighting)
            {
                if (highlightcnt == 0)
                {
                    int32_t xx[] = { highlightx1, highlightx1, searchx, searchx, highlightx1 };
                    int32_t yy[] = { highlighty1, searchy, searchy, highlighty1, highlighty1 };

                    highlightx2 = searchx;
                    highlighty2 = searchy;
                    ydim16 = ydim-STATUS2DSIZ2;

                    plotlines2d(xx, yy, 5, editorcolors[5]);
                }
                else
                {
                    highlightcnt = 0;

                    highlightx1 = searchx;
                    highlighty1 = searchy;
                    highlightx2 = searchx;
                    highlighty2 = searchy;
                }
            }
            else
            {
                if (highlightcnt == 0)
                {
                    int32_t add=keystatus[0x28], sub=(!add && keystatus[0x27]), setop=(add||sub);

                    if (!m32_sideview)
                    {
                        getpoint(highlightx1,highlighty1, &highlightx1,&highlighty1);
                        getpoint(highlightx2,highlighty2, &highlightx2,&highlighty2);
                    }

                    if (highlightx1 > highlightx2)
                        swaplong(&highlightx1, &highlightx2);
                    if (highlighty1 > highlighty2)
                        swaplong(&highlighty1, &highlighty2);

                    // Ctrl+RShift: select all wall-points of highlighted wall's loop:
                    if (eitherCTRL && highlightx1==highlightx2 && highlighty1==highlighty2)
                    {
                        if (!setop)
                        {
                            Bmemset(show2dwall, 0, sizeof(show2dwall));
                            Bmemset(show2dsprite, 0, sizeof(show2dsprite));
                        }

                        if (linehighlight >= 0 && linehighlight < MAXWALLS)
                        {
                            i = linehighlight;
                            do
                            {
                                if (!sub)
                                    show2dwall[i>>3] |= (1<<(i&7));
                                else
                                    show2dwall[i>>3] &= ~(1<<(i&7));

                                // XXX: this selects too many walls, need something more like
                                //      those of dragpoint() -- could be still too many for
                                //      loop punching though
                                for (j=0; j<numwalls; j++)
                                    if (j!=i && wall[j].x==wall[i].x && wall[j].y==wall[i].y)
                                    {
                                        if (!sub)
                                            show2dwall[j>>3] |= (1<<(j&7));
                                        else
                                            show2dwall[j>>3] &= ~(1<<(j&7));
                                    }

                                i = wall[i].point2;
                            }
                            while (i != linehighlight);
                        }

                        update_highlight();
                    }
                    else
                    {
                        int32_t tx, ty, onlySprites=eitherCTRL;

                        if (!setop)
                        {
                            Bmemset(show2dwall, 0, sizeof(show2dwall));
                            Bmemset(show2dsprite, 0, sizeof(show2dsprite));
                        }

                        for (i=0; i<numwalls; i++)
                            wall[i].cstat &= ~(1<<14);

                        for (i=0; i<numwalls; i++)
                        {
                            if (onlySprites)
                                break;

                            YAX_SKIPWALL(i);

                            if (!m32_sideview)
                            {
                                tx = wall[i].x;
                                ty = wall[i].y;
//                                wall[i].cstat &= ~(1<<14);
                            }
                            else
                            {
                                screencoords(&tx,&ty, wall[i].x-pos.x,wall[i].y-pos.y, zoom);
                                ty += getscreenvdisp(
                                    getflorzofslope(sectorofwall(i), wall[i].x,wall[i].y)-pos.z, zoom);
                                tx += halfxdim16;
                                ty += midydim16;
                            }

                            if (tx >= highlightx1 && tx <= highlightx2 &&
                                    ty >= highlighty1 && ty <= highlighty2)
                            {
                                if (!sub)
                                {
                                    if (numgraysects > 0 || m32_sideview)
                                    {
                                        dragpoint(i, wall[i].x, wall[i].y);
                                        dragpoint_noreset = 1;  // vvv
                                    }
                                    else
                                        show2dwall[i>>3] |= (1<<(i&7));
                                }
                                else
                                    show2dwall[i>>3] &= ~(1<<(i&7));
                            }
                        }
                        dragpoint_noreset = 0;  // ^^^

                        if (!sub && (numgraysects > 0 || m32_sideview))
                        {
                            for (i=0; i<numwalls; i++)
                                if (wall[i].cstat&(1<<14))
                                    show2dwall[i>>3] |= (1<<(i&7));
                        }

                        for (i=0; i<MAXSPRITES; i++)
                        {
                            if (sprite[i].statnum == MAXSTATUS)
                                continue;

                            //  v v v: if END pressed, also permit sprites from grayed out sectors
                            if (!keystatus[0xcf] && (unsigned)sprite[i].sectnum < MAXSECTORS)
                                YAX_SKIPSECTOR(sprite[i].sectnum);

                            if (!m32_sideview)
                            {
                                tx = sprite[i].x;
                                ty = sprite[i].y;
                            }
                            else
                            {
                                screencoords(&tx,&ty, sprite[i].x-pos.x,sprite[i].y-pos.y, zoom);
                                ty += getscreenvdisp(sprite[i].z-pos.z, zoom);
                                tx += halfxdim16;
                                ty += midydim16;
                            }

                            if (tx >= highlightx1 && tx <= highlightx2 &&
                                    ty >= highlighty1 && ty <= highlighty2)
                            {
                                if (!sub)
                                {
                                    if (sprite[i].sectnum >= 0)  // don't allow to select sprites in null space
                                        show2dsprite[i>>3] |= (1<<(i&7));
                                }
                                else
                                    show2dsprite[i>>3] &= ~(1<<(i&7));
                            }
                        }

                        update_highlight();

                        for (i=0; i<numwalls; i++)
                            wall[i].cstat &= ~(1<<14);
                    }
                }
            }
        }

        if (highlightcnt < 0)
        {
            if (keystatus[0xb8])  //Right alt (sector highlighting)
            {
                if (highlightsectorcnt == 0)
                {
                    if (!eitherCTRL)
                    {
                        int32_t xx[] = { highlightx1, highlightx1, searchx, searchx, highlightx1 };
                        int32_t yy[] = { highlighty1, searchy, searchy, highlighty1, highlighty1 };

                        highlightx2 = searchx;
                        highlighty2 = searchy;
                        ydim16 = ydim-STATUS2DSIZ2;

                        plotlines2d(xx, yy, 5, editorcolors[10]);
                    }
                }
                else
                {
                    // didmakered: 'bad'!
                    int32_t didmakered = (highlightsectorcnt<0), hadouterpoint=0;
#ifdef YAX_ENABLE
                    for (i=0; i<MAXSECTORS>>3; i++)
                        hlorgraysectbitmap[i] = hlsectorbitmap[i]|graysectbitmap[i];
#endif
                    for (i=0; i<highlightsectorcnt; i++)
                    {
                        int16_t tmpsect = -1;

                        for (WALLS_OF_SECTOR(highlightsector[i], j))
                        {
//                            if (wall[j].nextwall >= 0)
//                                checksectorpointer(wall[j].nextwall,wall[j].nextsector);
                            if (wall[j].nextwall < 0)
                                didmakered |= !!checksectorpointer(j, highlightsector[i]);

                            if (!didmakered)
                            {
                                updatesectorexclude(wall[j].x, wall[j].y, &tmpsect, hlorgraysectbitmap);
                                if (tmpsect<0)
                                    hadouterpoint = 1;
                            }
                        }
#ifdef YAX_ENABLE
                        {
                            int16_t cb, fb;

                            yax_getbunches(highlightsector[i], &cb, &fb);
                            if (cb>=0 || fb>=0)
                            {
                                // TROR stuff in the pasted sectors would really
                                // complicate things, so don't allow this
                                didmakered=1;
                            }
                        }
#endif
                    }

                    if (!didmakered && !hadouterpoint && newnumwalls<0)
                    {
                        // fade the screen to have the user's attention
                        fade_editor_screen(-1);

                        didmakered |= !ask_if_sure("Insert outer loop and make red walls? (Y/N)", 0);
                        clearkeys();
                    }

                    if (!didmakered && !hadouterpoint && newnumwalls<0)
                    {
                        int16_t ignore, refsect;
                        int32_t n;
#ifdef YAX_ENABLE
                        int16_t refsectbn[2]={-1,-1};
                        int32_t refextcf=-1;
#endif
                        Bmemset(visited, 0, sizeof(visited));

                        for (i=0; i<highlightsectorcnt; i++)
                        {
                            for (WALLS_OF_SECTOR(highlightsector[i], j))
                            {
                                k = trace_loop(j, visited, &ignore, &refsect, -1);
                                if (k == 0)
                                    continue;
                                else if (k < 0)
                                    goto end_autoredwall;

                                if (ignore)
                                    continue;

                                // done tracing one outer loop
#ifdef YAX_ENABLE
                                yax_getbunches(refsect, &refsectbn[0], &refsectbn[1]);
                                if (refsectbn[0]>=0 || refsectbn[1]>=0)
                                {
                                    if (refsectbn[0]>=0 && refsectbn[1]>=0)
                                    {
                                        // at least one of ceiling/floor must be non-extended
                                        didmakered = 1;
                                    }
                                    else
                                    {
                                        // ... and the other must be non-sloped
                                        refextcf = (refsectbn[1]>=0);
                                        if (SECTORFLD(refsect,stat, !refextcf)&2)
                                            didmakered = 1;
                                    }
                                }

                                if (didmakered)
                                    goto end_autoredwall;

                                if (refextcf >= 0)
                                {
                                    int32_t refz = SECTORFLD(refsect,z, refextcf), tmpsect;
                                    int32_t neededzofs=0;

                                    // the reference sector is extended on one side
                                    // (given by refextcf) and non-sloped on the other
                                    if (highlighted_sectors_components(0,0) != 1)
                                    {
                                        message("Highlighted sectors must be in one connected component");
                                        goto end_autoredwall;
                                    }

                                    for (m=0; m<highlightsectorcnt; m++)
                                    {
                                        tmpsect = highlightsector[m];
                                        yax_setbunch(tmpsect, refextcf, refsectbn[refextcf]);
                                        // walls: not needed, since they're all inner to the bunch

                                        SECTORFLD(tmpsect,z, refextcf) = refz;
                                        setslope(tmpsect, refextcf, 0);
                                        if (refextcf==0)
                                            neededzofs = max(neededzofs, refz-sector[tmpsect].floorz);
                                        else
                                            neededzofs = max(neededzofs, sector[tmpsect].ceilingz-refz);
                                    }

                                    if (neededzofs > 0)
                                    {
                                        neededzofs += ksgn(neededzofs)*(512<<4);
                                        neededzofs &= ~((256<<4)-1);
                                        if (refextcf==1)
                                            neededzofs *= -1;
                                        for (m=0; m<highlightsectorcnt; m++)
                                            SECTORFLD(highlightsector[m],z, !refextcf) += neededzofs;
                                    }
                                }
#endif
                                wall[k-1].point2 = numwalls;  // close the loop
                                newnumwalls = k;
                                n = (newnumwalls-numwalls);  // number of walls in just constructed loop

                                if (clockdir(numwalls)==CLOCKDIR_CW)
                                {
                                    int16_t begwalltomove = sector[refsect].wallptr+sector[refsect].wallnum;
                                    int32_t onwwasvalid = onwisvalid();

                                    flipwalls(numwalls, newnumwalls);

                                    sector[refsect].wallnum += n;
                                    if (refsect != numsectors-1)
                                    {
                                        walltype *tmpwall = Bmalloc(n * sizeof(walltype));
                                        int16_t *tmponw = Bmalloc(n * sizeof(int16_t));

                                        if (!tmpwall || !tmponw)
                                        {
                                            message("out of memory!");
                                            ExtUnInit();
//                                            clearfilenames();
                                            uninitengine();
                                            exit(1);
                                        }

                                        for (m=0; m<numwalls; m++)
                                        {
                                            if (wall[m].nextwall >= begwalltomove)
                                                wall[m].nextwall += n;
                                        }
#ifdef YAX_ENABLE
                                        yax_tweakwalls(begwalltomove, n);
#endif
                                        for (m=refsect+1; m<numsectors; m++)
                                            sector[m].wallptr += n;
                                        for (m=begwalltomove; m<numwalls; m++)
                                            wall[m].point2 += n;
                                        for (m=numwalls; m<newnumwalls; m++)
                                            wall[m].point2 += (begwalltomove-numwalls);

                                        Bmemcpy(tmponw, &onextwall[numwalls], n*sizeof(int16_t));
                                        Bmemmove(&onextwall[begwalltomove+n], &onextwall[begwalltomove],
                                                 (numwalls-begwalltomove)*sizeof(int16_t));
                                        Bmemcpy(&onextwall[begwalltomove], tmponw, n*sizeof(int16_t));

                                        Bmemcpy(tmpwall, &wall[numwalls], n*sizeof(walltype));
                                        Bmemmove(&wall[begwalltomove+n], &wall[begwalltomove],
                                                 (numwalls-begwalltomove)*sizeof(walltype));
                                        Bmemcpy(&wall[begwalltomove], tmpwall, n*sizeof(walltype));

                                        Bfree(tmpwall);
                                        Bfree(tmponw);
                                    }
                                    numwalls = newnumwalls;
                                    newnumwalls = -1;

                                    mkonwinvalid();

                                    for (m=begwalltomove; m<begwalltomove+n; m++)
                                        if (checksectorpointer(m, refsect) > 0)
                                            if (onwwasvalid && onextwall[wall[m].nextwall]>=0)
                                            {
//initprintf("%d %d\n", m, onextwall[wall[m].nextwall]);
                                                copy_some_wall_members(m, onextwall[wall[m].nextwall], 0);
                                            }

#ifndef YAX_ENABLE
                                    message("Attached new inner loop to sector %d", refsect);
#else
                                    {
                                        const char *cfstr[2] = {"ceiling","floor"};
                                        message("Attached new inner loop to %s%ssector %d",
                                                refextcf>=0 ? cfstr[refextcf] : "",
                                                refextcf>=0 ? "-extended " : "", refsect);
                                    }

                                    asksave = 1;

                                    if (refextcf >= 0)
                                    {
                                        yax_update(0);
                                        goto end_autoredwall;
                                    }
#endif
                                }
                            }
                        }
end_autoredwall:
                        newnumwalls = -1;
#ifdef YAX_ENABLE
                        yax_updategrays(pos.z);
#endif
                    }

                    highlightx1 = searchx;
                    highlighty1 = searchy;
                    highlightx2 = searchx;
                    highlighty2 = searchy;
                    highlightsectorcnt = 0;
                }
            }
            else
            {
                if (highlightsectorcnt == 0)
                {
                    int32_t add=keystatus[0x28], sub=(!add && keystatus[0x27]), setop=(add||sub);
                    int32_t tx,ty, pointsel = eitherCTRL;
#ifdef YAX_ENABLE
                    // home: ceilings, end: floors
                    int32_t fb, bunchsel = keystatus[0xcf] ? 1 : (keystatus[0xc7] ? 0 : -1);
                    uint8_t bunchbitmap[YAX_MAXBUNCHES>>3];
                    Bmemset(bunchbitmap, 0, sizeof(bunchbitmap));
#endif
                    if (!m32_sideview)
                    {
                        getpoint(highlightx1,highlighty1, &highlightx1,&highlighty1);
                        getpoint(highlightx2,highlighty2, &highlightx2,&highlighty2);
                    }

                    if (!pointsel)
                    {
                        if (highlightx1 > highlightx2)
                            swaplong(&highlightx1, &highlightx2);
                        if (highlighty1 > highlighty2)
                            swaplong(&highlighty1, &highlighty2);
                    }

                    if (!setop)
                        Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));

                    for (i=0; i<numsectors; i++)
                    {
                        if (pointsel)
                        {
                            bad = (inside_editor(&pos, searchx,searchy, zoom, highlightx2, highlighty2, i)!=1);
                        }
                        else
                        {
                            bad = 0;
                            for (WALLS_OF_SECTOR(i, j))
                            {
                                if (!m32_sideview)
                                {
                                    tx = wall[j].x;
                                    ty = wall[j].y;
                                }
                                else
                                {
                                    screencoords(&tx,&ty, wall[j].x-pos.x,wall[j].y-pos.y, zoom);
                                    ty += getscreenvdisp(getflorzofslope(i, wall[j].x,wall[j].y)-pos.z, zoom);
                                    tx += halfxdim16;
                                    ty += midydim16;
                                }

                                if (tx < highlightx1 || tx > highlightx2) bad = 1;
                                if (ty < highlighty1 || ty > highlighty2) bad = 1;
                                if (bad == 1) break;
                            }
                        }

                        if (bad == 0)
                        {
#ifdef YAX_ENABLE
                            if (bunchsel!=-1 && (fb = yax_getbunch(i, YAX_FLOOR))>=0)
                            {
                                if ((sub || (graysectbitmap[i>>3]&(1<<(i&7)))==0) &&
                                        (bunchbitmap[fb>>3]&(1<<(fb&7)))==0)
                                {
                                    bunchbitmap[fb>>3] |= (1<<(fb&7));
                                    for (SECTORS_OF_BUNCH(fb, bunchsel, j))
                                        handlesecthighlight1(j, sub, 1);
                                }
                            }
                            else
#endif
                                handlesecthighlight1(i, sub, eitherSHIFT);
                        }
                    }

                    update_highlightsector();
                    ovh_whiteoutgrab(0);
                }
            }
        }

        if (((bstatus&1) < (oldmousebstatus&1)) && highlightsectorcnt < 0)  //after dragging
        {
            int32_t runi, numdelpoints=0;
            int32_t havedrawnwalls = (newnumwalls!=-1), restorestat=1;

            // restorestat is set to 2 whenever the drawn walls should NOT be
            // restored afterwards

            int32_t err = backup_drawn_walls(0);

            if (err)
            {
                message("Error backing up drawn walls (code %d)!", err);
                goto end_after_dragging;
            }

            j = 1;
            for (i=0; i<highlightcnt; i++)
                if (pointhighlight == highlight[i])
                {
                    j = 0;
                    break;
                }

            if (j == 0)
            {
                for (i=0; i<highlightcnt; i++)
                    if ((highlight[i]&0xc000) == 16384)
                        updatesprite1(highlight[i]&16383);
            }
            else if ((pointhighlight&0xc000) == 16384)
            {
                updatesprite1(pointhighlight&16383);
            }

            if ((pointhighlight&0xc000) == 0)
            {
                dax = wall[pointhighlight].x;
                day = wall[pointhighlight].y;

                for (i=0; i<2; i++)
                {
                    if (dragwall[i] < 0)
                        break;

                    if (olen[i] != 0)
                    {
#ifndef YAX_ENABLE
                        j = dragwall[i];
#else
                        int32_t cf;
                        for (YAX_ITER_WALLS(dragwall[i], j, cf))
#endif
                        {
                            int32_t nw = wall[j].nextwall;

                            k = getlenbyrep(olen[i], wall[j].xrepeat);
                            fixxrepeat(j, k);
                            if (nw >= 0)
                            {
                                k = getlenbyrep(olen[i], wall[nw].xrepeat);
                                fixxrepeat(nw, k);
                            }
                        }
                    }
                }
            }
            else if ((pointhighlight&0xc000) == 16384)
            {
                dax = sprite[pointhighlight&16383].x;
                day = sprite[pointhighlight&16383].y;
            }

            dragwall[0] = dragwall[1] = -1;

            // attemt to delete some points
            for (runi=0; runi<3; runi++)  // check, tweak, carry out
                for (i=numwalls-1; i>=0; i--)
                {
                    if (runi==0)
                        wall[i].cstat &= ~(1<<14);

                    if (wall[i].x == POINT2(i).x && wall[i].y == POINT2(i).y)
                    {
                        if (havedrawnwalls)
                        {
                            if (i==ovh.suckwall || (ovh.split && i==ovh.splitstartwall))
                            {
                                // if we're about to delete a wall that participates
                                // in splitting, discard the already drawn walls
                                restorestat = 2;
                            }
                            else if (runi == 1)
                            {
                                // correct drawn wall anchors
                                if (ovh.suckwall > i)
                                    ovh.suckwall--;
                                if (ovh.split && ovh.splitstartwall > i)
                                    ovh.splitstartwall--;
                            }
                        }

                        if (runi == 0)
                        {
                            int32_t sectnum = sectorofwall(i);
                            if (sector[sectnum].wallnum <= 3)
                            {
                                message("Deleting wall %d would leave sector %d with %d walls.",
                                        i, sectnum, sector[sectnum].wallnum-1);
                                goto end_after_dragging;
                            }

                            sectnum = wall[i].nextsector;
                            if (sectnum >= 0 && sector[sectnum].wallnum <= 3)
                            {
                                message("Deleting wall %d would leave sector %d with %d walls.",
                                        i, sectnum, sector[sectnum].wallnum-1);
                                goto end_after_dragging;
                            }
                        }
                        else
                        {
                            deletepoint(i, runi);
                            if (runi==2)
                                numdelpoints++;
                        }
                    }
                }

            if (numdelpoints)
            {
                if (numdelpoints > 1)
                    message("Deleted %d points%s", numdelpoints,
                            (havedrawnwalls && restorestat==2) ? " and cleared drawn walls":"");
                else
                    printmessage16("Point deleted%s", (havedrawnwalls && restorestat==2) ?
                                   ", cleared drawn walls":"");
                asksave = 1;
            }
            else
            {
                for (i=0; i<numwalls; i++)     //make new red lines?
                {
                    YAX_SKIPWALL(i);

                    if ((wall[i].x == dax && wall[i].y == day)
                        || (POINT2(i).x == dax && POINT2(i).y == day))
                    {
                        checksectorpointer(i, sectorofwall(i));
//                    fixrepeats(i);
                        asksave = 1;
                    }
                }
            }
#ifdef YAX_ENABLE
            yax_update(0);
            yax_updategrays(pos.z);
#endif
end_after_dragging:
            backup_drawn_walls(restorestat);
        }

        if ((bstatus&1) > 0)                //drag points
        {
            if (highlightsectorcnt > 0)
            {
                if ((bstatus&1) > (oldmousebstatus&1))
                {
                    newnumwalls = -1;
                    sectorhighlightstat = -1;

//                    updatesector(mousxplc,mousyplc,&cursectorhighlight);
                    cursectorhighlight = -1;
                    for (i=0; i<highlightsectorcnt; i++)
                        if (inside_editor_curpos(highlightsector[i])==1)
                        {
                            cursectorhighlight = highlightsector[i];
                            break;
                        }

                    if (cursectorhighlight >= 0 && cursectorhighlight < numsectors)
                    {
                        //You clicked inside one of the flashing sectors!
                        sectorhighlightstat = 1;

                        dax = mousxplc;
                        day = mousyplc;
                        if (gridlock && grid > 0)
                            locktogrid(&dax, &day);

                        sectorhighlightx = dax;
                        sectorhighlighty = day;
                    }
                }
                else if (sectorhighlightstat == 1)
                {
                    dax = mousxplc;
                    day = mousyplc;
                    if (gridlock && grid > 0)
                        locktogrid(&dax, &day);

                    dax -= sectorhighlightx;
                    day -= sectorhighlighty;
                    sectorhighlightx += dax;
                    sectorhighlighty += day;
#ifdef YAX_ENABLE
                    if (!hl_all_bunch_sectors_p())
                        printmessage16("To drag extended sectors, all sectors of a bunch must be selected");
                    else
#endif
                    for (i=0; i<highlightsectorcnt; i++)
                    {
                        for (WALLS_OF_SECTOR(highlightsector[i], j))
                            { wall[j].x += dax; wall[j].y += day; }

                        for (j=headspritesect[highlightsector[i]]; j>=0; j=nextspritesect[j])
                            { sprite[j].x += dax; sprite[j].y += day; }
                    }

                    //for(i=0;i<highlightsectorcnt;i++)
                    //{
                    //   startwall = sector[highlightsector[i]].wallptr;
                    //   endwall = startwall+sector[highlightsector[i]].wallnum-1;
                    //   for(j=startwall;j<=endwall;j++)
                    //   {
                    //      if (wall[j].nextwall >= 0)
                    //         checksectorpointer(wall[j].nextwall,wall[j].nextsector);
                    //     checksectorpointer((short)j,highlightsector[i]);
                    //   }
                    //}
                    asksave = 1;
                }

            }
            else  //if (highlightsectorcnt <= 0)
            {
                if ((bstatus&1) > (oldmousebstatus&1))
                {
                    pointhighlight = getpointhighlight(mousxplc, mousyplc, pointhighlight);

                    if (pointhighlight >= 0 && (pointhighlight&0xc000)==0)
                    {
                        dragwall[0] = lastwall(pointhighlight);
                        dragwall[1] = pointhighlight;
                        olen[0] = wallength(dragwall[0]);
                        olen[1] = wallength(dragwall[1]);
                    }
                }

                if (pointhighlight >= 0 && (!m32_sideview || m32_sideelev>=32))
                {
                    if (m32_sideview)
                    {
                        int32_t dz;
                        if (pointhighlight>=16384)
                            dz = sprite[pointhighlight&16383].z - pos.z;
                        else
                            dz = getflorzofslope(sectorofwall(pointhighlight),
                                                 wall[pointhighlight].x, wall[pointhighlight].y) - pos.z;
                        getinvdisplacement(&dax,&day, -dz);
                        dax += mousxplc;
                        day += mousyplc;
                    }
                    else
                    {
                        dax = mousxplc;
                        day = mousyplc;
                    }

                    if (gridlock && grid > 0)
                        locktogrid(&dax, &day);

                    j = 1;
                    if (highlightcnt > 0)
                        for (i=0; i<highlightcnt; i++)
                            if (pointhighlight == highlight[i])
                                { j = 0; break; }

                    if (j == 0)
                    {
                        if ((pointhighlight&0xc000) == 0)
                        {
                            dax -= wall[pointhighlight].x;
                            day -= wall[pointhighlight].y;
                        }
                        else
                        {
                            dax -= sprite[pointhighlight&16383].x;
                            day -= sprite[pointhighlight&16383].y;
                        }

                        for (i=0; i<highlightcnt; i++)
                        {
                            if ((highlight[i]&0xc000) == 0)
                            {
                                wall[highlight[i]].x += dax;
                                wall[highlight[i]].y += day;
                            }
                            else
                            {
                                spritetype *daspr = &sprite[highlight[i]&16383];

                                daspr->x += dax;
                                daspr->y += day;
                                setspritez(daspr-sprite, (const vec3_t *)daspr);
                            }
                        }
                    }
                    else
                    {
                        if ((pointhighlight&0xc000) == 0)
                        {
                            if (newnumwalls >= numwalls &&
                                    wall[pointhighlight].x==firstx && wall[pointhighlight].y==firsty)
                            {
                                printmessage16("Can't drag point where drawing started.");
                                goto end_point_dragging;
                            }

                            dragpoint(pointhighlight,dax,day);
                            if ((unsigned)linehighlight < MAXWALLS)
                                wall[linehighlight].cstat |= (1<<14);
                            wall[lastwall(pointhighlight)].cstat |= (1<<14);
                        }
                        else if ((pointhighlight&0xc000) == 16384)
                        {
                            int32_t daspr=pointhighlight&16383;
                            int16_t osec=sprite[daspr].sectnum, nsec=osec;
                            vec3_t vec, ovec;

                            Bmemcpy(&ovec, (vec3_t *)&sprite[daspr], sizeof(vec3_t));
                            vec.x = dax;
                            vec.y = day;
                            vec.z = sprite[daspr].z;
                            if (setspritez(daspr, &vec) == -1 && osec>=0)
                            {
                                updatesector_onlynextwalls(dax, day, &nsec);

                                if (nsec >= 0)
                                {
                                    sprite[daspr].x = dax;
                                    sprite[daspr].y = day;
                                    // z updating is after we released the mouse button
                                    if (sprite[daspr].sectnum != nsec)
                                        changespritesect(daspr, nsec);
                                }
                                else
                                    Bmemcpy(&sprite[daspr], &ovec, sizeof(vec3_t));
                            }
                        }
                    }

                    asksave = 1;
                }
            }
        }
        else //if ((bstatus&1) == 0)
        {
            pointhighlight = getpointhighlight(mousxplc, mousyplc, pointhighlight);
            sectorhighlightstat = -1;
        }
end_point_dragging:

        if (bstatus&(2|4))  // change arrow position
        {
            if (eitherCTRL)
            {
                int16_t cursectornum;

                for (cursectornum=0; cursectornum<numsectors; cursectornum++)
                    if (inside_editor_curpos(cursectornum) == 1)
                        break;

                if (cursectornum < numsectors)
                {
                    if (pointhighlight >= 16384)
                        ExtEditSpriteData(pointhighlight-16384);
                    else if ((linehighlight >= 0) && ((bstatus&1) || sectorofwall(linehighlight) == cursectornum))
                        ExtEditWallData(linehighlight);
                    else if (cursectornum >= 0)
                        ExtEditSectorData(cursectornum);
                }

                bstatus &= ~6;
            }
            else
            {
                if (m32_sideview && (bstatus&4))
                {
                    pos.z += divscale18(searchy-midydim16,zoom);
                    getpoint(searchx,midydim16, &pos.x, &pos.y);
#ifdef YAX_ENABLE
                    yax_updategrays(pos.z);
#endif
                }
                else
                {
                    pos.x = mousxplc;
                    pos.y = mousyplc;
                }

                if (m32_sideview)
                {
                    int32_t opat=drawlinepat;

                    y1 = INT32_MAX;

                    for (i=0; i<numsectors; i++)
                    {
                        if (inside(pos.x, pos.y, i)==1)
                        {
                            day = getscreenvdisp(getflorzofslope(i, pos.x, pos.y)-pos.z, zoom);

                            x2 = max(4, mulscale14(64,zoom));
                            y2 = scalescreeny(x2);

                            if (klabs(day) < y1)
                                y1 = day;

                            drawline16base(halfxdim16, midydim16+day, -x2,-y2, x2,y2, editorcolors[14]);
                            drawline16base(halfxdim16, midydim16+day, -x2,y2, x2,-y2, editorcolors[14]);
                        }
                    }

                    drawlinepat = 0x11111111;
                    if (y1 != INT32_MAX)
                        drawline16base(halfxdim16,midydim16, 0,0, 0,y1, editorcolors[14]);
//                    else
//                        drawline16base(halfxdim16,midydim16, 0,0, 0,getscreenvdisp(-pos.z, zoom), editorcolors[14]);
                    drawlinepat = opat;
                }

                searchx = halfxdim16;
                searchy = midydim16;
            }
        }
        else if ((oldmousebstatus&6) > 0)
            updatesectorz(pos.x,pos.y,pos.z,&cursectnum);

        if (circlewall != -1 && (keystatus[0x4a] || ((bstatus&32) && !eitherCTRL)))  // -, mousewheel down
        {
            if (circlepoints > 1)
                circlepoints--;
            keystatus[0x4a] = 0;
            mouseb &= ~32;
            bstatus &= ~32;
        }
        if (circlewall != -1 && (keystatus[0x4e] || ((bstatus&16) && !eitherCTRL)))  // +, mousewheel up
        {
            if (circlepoints < 63)
                circlepoints++;
            keystatus[0x4e] = 0;
            mouseb &= ~16;
            bstatus &= ~16;
        }

        if (keystatus[0x3d])  // F3
        {
            keystatus[0x3d]=0;
            if (!m32_sideview && EDITING_MAP_P())
                message("Must not be editing map while switching to side view mode.");
            else
            {
                m32_sideview = !m32_sideview;
                printmessage16("Side view %s", m32_sideview?"enabled":"disabled");
            }
        }

        if (m32_sideview && (keystatus[0x10] || keystatus[0x11]))
        {
            if (eitherCTRL)
            {
                if (m32_sideang&63)
                {
                    m32_sideang += (1-2*keystatus[0x10])*(1-2*sideview_reversehrot)*32;
                    m32_sideang &= (2047&~63);
                }
                else
                {
                    m32_sideang += (1-2*keystatus[0x10])*(1-2*sideview_reversehrot)*64;
                    m32_sideang &= 2047;
                }

                keystatus[0x10] = keystatus[0x11] = 0;
            }
            else
            {
                m32_sideang += (1-2*keystatus[0x10])*(1-2*sideview_reversehrot)*synctics<<(eitherSHIFT*2);
                m32_sideang &= 2047;
            }
            _printmessage16("Sideview angle: %d", (int32_t)m32_sideang);
        }

        if (m32_sideview && (eitherSHIFT || (bstatus&(16|32))))
        {
            if ((DOWN_BK(MOVEUP) || (bstatus&16)) && m32_sideelev < 512)
            {
                m32_sideelev += synctics<<(1+!!(bstatus&16));
                if (m32_sideelev > 512)
                    m32_sideelev = 512;
                _printmessage16("Sideview elevation: %d", m32_sideelev);
            }
            if ((DOWN_BK(MOVEDOWN) || (bstatus&32)) && m32_sideelev > 0)
            {
                m32_sideelev -= synctics<<(1+!!(bstatus&32));
                if (m32_sideelev < 0)
                    m32_sideelev = 0;
                _printmessage16("Sideview elevation: %d", m32_sideelev);
            }
        }
        else
        {
            int32_t didzoom=0;

            if ((DOWN_BK(MOVEUP) || (bstatus&16)) && zoom < 65536)
            {
                zoom += synctics*(zoom>>4);
                if (zoom < 24) zoom += 2;
                didzoom = 1;
            }
            if ((DOWN_BK(MOVEDOWN) || (bstatus&32)) && zoom > 8)
            {
                zoom -= synctics*(zoom>>4);
                didzoom = 1;
            }

            if (didzoom)
            {
                if (eitherALT)
                {
                    searchx = halfxdim16;
                    searchy = midydim16;
                    pos.x = mousxplc;
                    pos.y = mousyplc;
                }
                zoom = clamp(zoom, 8, 65536);
                _printmessage16("Zoom: %d",zoom);
            }
        }

        if (keystatus[0x22])  // G (grid on/off)
        {
            keystatus[0x22] = 0;
            grid++;
            if (grid == 7) grid = 0;
        }
        if (keystatus[0x26])  // L (grid lock)
        {
            keystatus[0x26] = 0;
            gridlock = !gridlock;
            printmessage16("Grid locking %s", gridlock?"on":"off");
        }

        if (keystatus[0x24])  // J (join sectors)
        {
            keystatus[0x24] = 0;

            if (newnumwalls >= 0)
            {
                printmessage16("Can't join sectors while editing.");
                goto end_join_sectors;
            }

#ifdef YAX_ENABLE
            if (highlightsectorcnt > 0 && eitherCTRL)
            {
                // [component][ceiling(0) or floor(1)]
                // compstat: &1: "has extension", &2: "differ in z", &4: "sloped", -1: "uninited"
                int32_t cf, comp, compstat[2][2]={{-1,-1},{-1,-1}}, compcfz[2][2];

                // joinstat: join what to what?
                //  &1: ceil(comp 0) <-> flor(comp 1),  &2: flor(comp 0) <-> ceil(comp 1)
                //  (doesn't yet say which is stationary)
                // movestat: which component can be displaced?
                //  &1: first,  &2: second
                int32_t askres, joinstat, needsdisp, moveonwp;
                int32_t movestat, dx=0,dy=0,dz, delayerr=0;

                int32_t numouterwalls[2] = {0,0}, numowals;
                static int16_t outerwall[2][MAXWALLS];
                const walltype *wal0, *wal1, *wal0p2, *wal1p2;

                // join sector ceilings/floors to a new bunch
                if (numyaxbunches==YAX_MAXBUNCHES)
                {
                    message("Bunch limit of %d reached, cannot join", YAX_MAXBUNCHES);
                    goto end_join_sectors;
                }

                // first, see whether we have exactly two connected components
                // wrt wall[].nextsector
                if (highlighted_sectors_components(0,0) != 2)
                {
                    message("Sectors must be partitioned in two components to join");
                    goto end_join_sectors;
                }

                for (k=0; k<highlightsectorcnt; k++)
                {
                    j = highlightsector[k];
                    comp = !!(collsectbitmap[1][j>>3]&(1<<(j&7)));

                    for (cf=0; cf<2; cf++)
                    {
                        if (compstat[comp][cf]==-1)
                        {
                            compstat[comp][cf] = 0;
                            compcfz[comp][cf] = SECTORFLD(j,z, cf);
                        }

                        if (yax_getbunch(j, cf)>=0)
                            compstat[comp][cf] |= 1;
                        if (SECTORFLD(j,z, cf) != compcfz[comp][cf])
                            compstat[comp][cf] |= 2;
                        if (SECTORFLD(j,stat, cf)&2)
                            compstat[comp][cf] |= 4;

                        compcfz[comp][cf] = SECTORFLD(j,z, cf);
                    }
                }

                // check for consistency
                joinstat = 0;
                if (!compstat[0][YAX_CEILING] && !compstat[1][YAX_FLOOR])
                    joinstat |= 1;
                if (!compstat[0][YAX_FLOOR] && !compstat[1][YAX_CEILING])
                    joinstat |= 2;

                if (joinstat==0)
                {
                    message("No consistent joining combination found");
                    OSD_Printf("comp0: c=%d,f=%d;  comp1: c=%d,f=%d  (1:extended, 2:z mismatch, 4:sloped)\n",
                               compstat[0][YAX_CEILING], compstat[0][YAX_FLOOR],
                               compstat[1][YAX_CEILING], compstat[1][YAX_FLOOR]);
                    //for (i=0; i<2; i++) for (j=0; j<2; j++) message("%d", compstat[i][j]);
                    goto end_join_sectors;
                }
                if (joinstat==3)
                {
                    if (compcfz[0][YAX_CEILING] != compstat[1][YAX_FLOOR])
                        joinstat &= 1;
                    if (compcfz[0][YAX_CEILING] != compstat[1][YAX_FLOOR])
                        joinstat &= 2;

                    if (joinstat == 0)
                        joinstat = 3;  // we couldn't disambiguate
                }

                for (comp=0; comp<2; comp++)
                    for (k=0; k<collnumsects[comp]; k++)
                    {
                        i = collsectlist[comp][k];
                        for (WALLS_OF_SECTOR(i, j))
                            if (wall[j].nextwall < 0)
                                outerwall[comp][numouterwalls[comp]++] = j;
                    }

                if (numouterwalls[0] != numouterwalls[1])
                {
                    message("Number of outer walls must be equal for both components"
                            " (have %d and %d)", numouterwalls[0], numouterwalls[1]);
                    if (numouterwalls[0]>0 && numouterwalls[1]>0)
                        delayerr = 1;
                    else
                        goto end_join_sectors;
                }
                numowals = min(numouterwalls[0], numouterwalls[1]);

                // now sort outer walls 'geometrically'
                for (comp=0; comp<2; comp++)
                    sort_walls_geometrically(outerwall[comp], numouterwalls[comp]);

                for (k=0; k<numowals; k++)
                {
                    wal0 = &wall[outerwall[0][k]];
                    wal1 = &wall[outerwall[1][k]];

                    wal0p2 = &wall[wal0->point2];
                    wal1p2 = &wall[wal1->point2];

                    if (k==0)
                    {
                        dx = wal1->x - wal0->x;
                        dy = wal1->y - wal0->y;
                    }

                    if (wal1->x - wal0->x != dx || wal1->y - wal0->y != dy ||
                            wal1p2->x - wal0p2->x != dx || wal1p2->y - wal0p2->y != dy)
                    {
                        pos.x = wal0->x + (wal0p2->x - wal0->x)/4;
                        pos.y = wal0->y + (wal0p2->y - wal0->y)/4;
                        pos.z = getflorzofslope(sectorofwall(wal0-wall), pos.x, pos.y);

                        if (!delayerr)
                            message("Outer wall coordinates must coincide for both components");
                        OSD_Printf("wal0:%d (%d,%d)--(%d,%d)\n",(int)(wal0-wall),
                                   wal0->x,wal0->y, wal0p2->x,wal0p2->y);
                        OSD_Printf("wal1:%d (%d,%d)--(%d,%d)\n",(int)(wal1-wall),
                                   wal1->x,wal1->y, wal1p2->x,wal1p2->y);

                        goto end_join_sectors;
                    }
                }

                if (delayerr)
                    goto end_join_sectors;

                if (joinstat == 3)
                {
                    char askchars[2] = {'1', 'v'};

                    // now is a good time to ask...
                    for (comp=0; comp<2; comp++)
                        for (k=0; k<collnumsects[comp]; k++)
                            fillsector(collsectlist[comp][k], comp==0 ? 159 : editorcolors[11]);

                    fade_editor_screen(editorcolors[11] | (159<<8));
                    askres = editor_ask_function("Connect yellow ceil w/ blue floor (1) or (v)ice versa?", askchars, 2);
                    if (askres==-1)
                        goto end_join_sectors;
                    joinstat &= (1<<askres);
                }

                joinstat--;  // 0:ceil(0)<->flor(1), 1:ceil(1)<->flor(0)

                dz = compcfz[1][!joinstat] - compcfz[0][joinstat];
                needsdisp = (dx || dy || dz);

                if (needsdisp)
                {
                    // a component is more likely to be displaced if it's not
                    // extended on the non-joining side
                    movestat = (!(compstat[0][!joinstat]&1)) | ((!(compstat[1][joinstat]&1))<<1);
                    if (!movestat)
                    {
                        movestat = 3;
//                        message("Internal error while TROR-joining: movestat inconsistent!");
//                        goto end_join_sectors;
                    }

                    if (movestat==3)
                    {
                        char askchars[2] = {'y', 'b'};

                        for (comp=0; comp<2; comp++)
                            for (k=0; k<collnumsects[comp]; k++)
                                fillsector(collsectlist[comp][k], comp==0 ? 159 : editorcolors[11]);

                        fade_editor_screen(editorcolors[11] | (159<<8));
                        askres = editor_ask_function("Move (y)ellow or (b)lue component?", askchars, 2);
                        if (askres==-1)
                            goto end_join_sectors;
                        movestat &= (1<<askres);
                    }

                    movestat--;  // 0:move 1st, 1:move 2nd component
                    if (movestat==1)
                        dx*=-1, dy*=-1, dz*=-1;

                    moveonwp = 0;
                    if (onwisvalid())
                    {
                        static int16_t ocollsectlist[MAXSECTORS];
                        static uint8_t tcollbitmap[MAXSECTORS>>3];
                        int16_t ocollnumsects=collnumsects[movestat], tmpsect;

                        Bmemcpy(ocollsectlist, collsectlist[movestat], ocollnumsects*sizeof(int16_t));
                        Bmemset(tcollbitmap, 0, sizeof(tcollbitmap));

                        for (k=0; k<ocollnumsects; k++)
                            for (WALLS_OF_SECTOR(ocollsectlist[k], j))
                            {
                                if (onextwall[j] < 0)
                                    continue;

                                tmpsect = sectorofwall(onextwall[j]);
                                sectors_components(1, &tmpsect, 1,0);

                                for (m=0; m<(numsectors+7)>>3; m++)
                                    tcollbitmap[m] |= collsectbitmap[0][m];
                                moveonwp = 1;
                            }

                        if (moveonwp)
                        {
                            int32_t movecol = movestat==0 ? 159 : editorcolors[11];
                            for (i=0; i<numsectors; i++)
                                if (tcollbitmap[i>>3]&(1<<(i&7)))
                                    fillsector(i, editorcolors[12]);

                            fade_editor_screen(editorcolors[12] | (movecol<<8));
                            moveonwp = ask_if_sure("Also move formerly wall-connected sectors?",0);
                            if (moveonwp==-1)
                                goto end_join_sectors;
                        }
                    }

                    // now need to collect them wrt. the nextsector but also
                    // the yax-nextsector relation
                    if (highlighted_sectors_components(1,moveonwp) != 2)
                    {
                        message("Must not have TROR connections between the two components");
                        goto end_join_sectors;
                    }

                    // displace!
                    for (k=0; k<collnumsects[movestat]; k++)
                    {
                        i = collsectlist[movestat][k];

                        sector[i].floorz += dz;
                        sector[i].ceilingz += dz;

                        for (WALLS_OF_SECTOR(i, j))
                        {
                            wall[j].x += dx;
                            wall[j].y += dy;
                        }

                        for (j=headspritesect[i]; j>=0; j=nextspritesect[j])
                        {
                            sprite[j].x += dx;
                            sprite[j].y += dy;
                            sprite[j].z += dz;
                        }
                    }

                    // restore old components, i.e. only the bunch sectors
                    highlighted_sectors_components(0,0);

                }  // end if (needsdisp)

                /*** construct the YAX connection! ***/
                for (comp=0; comp<2; comp++)
                {
                    // walls
                    for (j=0; j<numowals; j++)
                        yax_setnextwall(outerwall[comp][j], comp^joinstat, outerwall[!comp][j]);

                    // sectors
                    for (k=0; k<collnumsects[comp]; k++)
                    {
                        i = collsectlist[comp][k];
                        yax_setbunch(i, comp^joinstat, numyaxbunches);
                        SECTORFLD(i,stat, comp^joinstat) &= ~1;  // no plax

                        // restore red walls AFTER setting nextwalls
                        // (see checksectorpointer() for why)
                        for (WALLS_OF_SECTOR(i, j))
                            if (wall[j].nextwall < 0)
                                checksectorpointer(j, i);
                    }
                }

                Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
                update_highlightsector();

                yax_update(0);
                yax_updategrays(pos.z);

                message("Joined highlighted sectors to new bunch %d", numyaxbunches);
                asksave = 1;
            }
            else
#endif  // defined YAX_ENABLE
            if (joinsector[0] < 0)
            {
                int32_t numjoincandidates = 0;
                char *origframe=NULL;

                for (i=0; i<numsectors; i++)
                {
                    YAX_SKIPSECTOR(i);
                    numjoincandidates += (inside_editor_curpos(i) == 1);
                }

                for (i=0; i<numsectors; i++)
                {
                    YAX_SKIPSECTOR(i);
                    if (inside_editor_curpos(i) == 1)
                    {
                        if (numjoincandidates > 1)
                        {
                            if (!bakframe_fillandfade(&origframe, i, "Use this as first joining sector? (Y/N)"))
                                continue;
                        }

                        joinsector[0] = i;
                        printmessage16("Join sector - press J again on sector to join with.");
                        break;
                    }
                }

                if (origframe)
                    Bfree(origframe);
            }
            else
            {
                int32_t joink, s1to0wall, s0to1wall;
#ifdef YAX_ENABLE
                int16_t jbn[2][2];  // [join index][c/f]
                int32_t uneqbn;  // unequal bunchnums (bitmap): 1:above, 2:below
#endif
                char *origframe = NULL;
                int32_t numjoincandidates = 0;

                joinsector[1] = -1;

                for (i=0; i<numsectors; i++)
                {
                    YAX_SKIPSECTOR(i);
                    numjoincandidates += (inside_editor_curpos(i) == 1);
                }

                for (i=0; i<numsectors; i++)
                {
                    YAX_SKIPSECTOR(i);

                    if (inside_editor_curpos(i) == 1)
                    {
                        if (numjoincandidates > 1)
                        {
                            if (!bakframe_fillandfade(&origframe, i, "Use this as second joining sector? (Y/N)"))
                                continue;

                            if (origframe)
                            {
                                Bfree(origframe);
                                origframe = NULL;
                            }
                        }

                        s1to0wall = find_nextwall(i, joinsector[0]);
                        s0to1wall = wall[s1to0wall].nextwall;
                        joinsector[1] = i;
#ifdef YAX_ENABLE
                        for (k=0; k<2; k++)
                            yax_getbunches(joinsector[k], &jbn[k][YAX_CEILING], &jbn[k][YAX_FLOOR]);
#endif
                        // pressing J into the same sector is the same as saying 'no'
                        //                     v----------------v
                        if (s1to0wall == -1 && i != joinsector[0])
                        {
#ifdef YAX_ENABLE
                            if (jbn[0][0]>=0 || jbn[0][1]>=0 || jbn[1][0]>=0 || jbn[1][1]>=0)
                            {
                                message("Joining non-adjacent extended sectors not allowed!");
                                joinsector[0] = joinsector[1] = -1;
                                goto end_join_sectors;
                            }
#endif
                            {
                                fillsector(i, editorcolors[9]);
                                fillsector(joinsector[0], editorcolors[9]);
                                fade_editor_screen(editorcolors[9]);

                                if (!ask_if_sure("Join non-adjacent sectors? (Y/N)", 0))
                                    joinsector[1] = joinsector[0];
                            }
                        }
#ifdef YAX_ENABLE
                        uneqbn = (jbn[0][YAX_CEILING]!=jbn[1][YAX_CEILING]) |
                            ((jbn[0][YAX_FLOOR]!=jbn[1][YAX_FLOOR])<<1);
                        if (uneqbn)
                        {
                            const int32_t cf=YAX_FLOOR;
                            int32_t whybad=0, jsynw[2];

                            if (uneqbn == 1)
                            {
                                OSD_Printf("Can't join two sectors with different ceiling bunchnums."
                                           " To make them equal, join their upper neighbor's floors.");
                                printmessage16("Can't join two sectors with different ceiling bunchnums. See OSD");
                                joinsector[0] = joinsector[1] = -1;
                                goto end_join_sectors;
                            }
                            if (s0to1wall < 0)
                            {
                                printmessage16("INTERNAL ERROR: nextwalls inconsistent!");
                                joinsector[0] = joinsector[1] = -1;
                                goto end_join_sectors;
                            }

                            // both must be extended
                            if (jbn[0][cf]<0 || jbn[1][cf]<0)
                                uneqbn &= ~(1<<cf), whybad|=1;
                            // if any sloped, can't join
                            if ((SECTORFLD(joinsector[0],stat, cf)&2) || (SECTORFLD(joinsector[1],stat, cf)&2))
                                uneqbn &= ~(1<<cf), whybad|=2;
                            // if on unequal heights, can't join either
                            if (SECTORFLD(joinsector[0],z, cf) != SECTORFLD(joinsector[1],z, cf))
                                uneqbn &= ~(1<<cf), whybad|=4;

                            // check whether the lower neighbors have a red-wall link to each other
                            jsynw[1] = yax_getnextwall(s1to0wall, cf);
                            jsynw[0] = yax_getnextwall(s0to1wall, cf);
                            if (jsynw[0]<0 || jsynw[1]<0)  // this shouldn't happen
                                uneqbn &= ~(1<<cf), whybad|=8;
                            else if (wall[jsynw[1]].nextwall != jsynw[0])
                            {
//                                if (find_nextwall(sectorofwall(jsynw[1]), sectorofwall(jsynw[0])) < 0)
                                    uneqbn &= ~(1<<cf), whybad|=16;
                            }

                            if ((uneqbn&2)==0)
                            {
#if 0
                                if (whybad==1+8 && jbn[0][cf]>=0 && jbn[1][cf]<0)
                                {
                                    // 1st join sector extended, 2nd not... let's see
                                    // if the latter is inner to the former one

                                    int32_t lowerstartsec = yax_vnextsec(s0to1wall, cf);

                                    m = (lowerstartsec < 0)<<1;
                                    for (WALLS_OF_SECTOR(joinsector[1], k))
                                    {
                                        if (m) break;

                                        m |= (wall[k].nextsector>=0 && wall[k].nextsector != joinsector[0]);
                                        m |= (wall[k].nextwall>=0 && yax_vnextsec(wall[k].nextwall, cf)!=lowerstartsec)<<1;
                                    }

                                    if (m==0)
                                    {
                                        yax_setbunch(joinsector[1], YAX_FLOOR, jbn[0][cf]);
                                        yax_update(0);
                                        yax_updategrays(pos.z);
                                        asksave = 1;

                                        printmessage16("Added sector %d's floor to bunch %d",
                                                       joinsector[1], jbn[0][cf]);
                                    }
                                    else
                                    {
                                        if (m&1)
                                        {
                                            message("Can't add sector %d's floor to bunch %d: not inner to sector %d",
                                                    joinsector[1], jbn[0][cf], joinsector[0]);
                                        }
                                        else // if (m&2)
                                        {
                                            message("Can't add sector %d's floor to bunch %d: must have lower neighbor",
                                                    joinsector[1], jbn[0][cf]);
                                        }
                                    }
                                }
                                else
#endif
                                {
                                    if (whybad&1)
                                        message("Can't make floor bunchnums equal: both floors must be extended");
                                    else if (whybad&2)
                                        message("Can't make floor bunchnums equal: both floors must be non-sloped");
                                    else if (whybad&4)
                                        message("Can't make floor bunchnums equal: both floors must have equal height");
                                    else if (whybad&8)
                                        message("Can't make floor bunchnums equal: INTERNAL ERROR");
                                    else if (whybad&16)
                                        message("Can't make floor bunchnums equal: lower neighbors must be linked");
                                }
                            }
                            else
                            {
                                int32_t vcf, newbn, ynw;

                                // we're good to go for making floor bunchnums equal
                                for (SECTORS_OF_BUNCH(jbn[1][cf], YAX_FLOOR, k))
                                    yax_setbunch(k, YAX_FLOOR, jbn[0][cf]);
                                for (SECTORS_OF_BUNCH(jbn[1][cf], YAX_CEILING, k))
                                    yax_setbunch(k, YAX_CEILING, jbn[0][cf]);

                                yax_update(0);
                                // now we can iterate the sectors with the new bunchnums
                                newbn = yax_getbunch(joinsector[0], cf);

                                // clear all yax-nextwall links on walls that are inside the bunch
                                for (vcf=0; vcf<2; vcf++)
                                    for (SECTORS_OF_BUNCH(newbn, vcf, k))
                                        for (WALLS_OF_SECTOR(k, m))
                                        {
                                            ynw = yax_getnextwall(m, vcf);
                                            if (ynw < 0 || wall[m].nextsector < 0)
                                                continue;

                                            if (yax_getbunch(wall[m].nextsector, vcf) == newbn)
                                            {
                                                yax_setnextwall(ynw, !vcf, -1);
                                                yax_setnextwall(m, vcf, -1);
                                            }
                                        }

                                // shouldn't be needed again for the editor, but can't harm either:
                                yax_update(0);
                                yax_updategrays(pos.z);

                                printmessage16("Made sector %d and %d floor bunchnums equal",
                                               joinsector[0], joinsector[1]);
                                asksave = 1;
                            }

                            joinsector[0] = joinsector[1] = -1;
                            goto end_join_sectors;
                        }
#endif
                        break;
                    }
                }

                if (joinsector[1] < 0 || joinsector[0] == joinsector[1])
                {
                    printmessage16("No sectors joined.");
                    joinsector[0] = -1;
                    goto end_join_sectors;
                }


                for (i=0; i<numwalls; i++)
                    wall[i].cstat &= ~(1<<14);

                newnumwalls = numwalls;

                for (k=0; k<2; k++)
                {
                    for (WALLS_OF_SECTOR(joinsector[k], j))
                    {
                        int32_t loopnum=MAXWALLS*2;

                        if (wall[j].cstat & (1<<14))
                            continue;

                        if (wall[j].nextsector == joinsector[1-k])
                        {
                            wall[j].cstat |= (1<<14);
                            continue;
                        }

                        i = j;
                        m = newnumwalls;
                        joink = k;
                        do
                        {
                            if (newnumwalls >= MAXWALLS + M32_FIXME_WALLS)
                            {
                                message("Joining sectors failed: not enough space beyond wall[]");
                                joinsector[0] = -1;
                                newnumwalls = -1;

                                for (i=0; i<numwalls; i++)
                                    wall[i].cstat &= ~(1<<14);

                                goto end_join_sectors;
                            }

                            Bmemcpy(&wall[newnumwalls], &wall[i], sizeof(walltype));
#ifdef YAX_ENABLE
                            yax_fixreverselinks(newnumwalls, newnumwalls);
#endif
                            wall[newnumwalls].point2 = newnumwalls+1;
                            newnumwalls++;

                            wall[i].cstat |= (1<<14);

                            i = wall[i].point2;
                            if (wall[i].nextsector == joinsector[1-joink])
                            {
                                i = NEXTWALL(i).point2;
                                joink = 1 - joink;
                            }

                            loopnum--;
                        }
                        while (loopnum>0 && ((wall[i].cstat & (1<<14))==0)
                                   && (wall[i].nextsector != joinsector[1-joink]));

                        wall[newnumwalls-1].point2 = m;

                        if (loopnum==0)
                        {
                            message("internal error while joining sectors: infloop!");
                            newnumwalls = -1;
                        }
                    }
                }

                if (newnumwalls > numwalls)
                {
                    Bmemcpy(&sector[numsectors], &sector[joinsector[0]], sizeof(sectortype));
                    sector[numsectors].wallptr = numwalls;
                    sector[numsectors].wallnum = newnumwalls-numwalls;

                    //fix sprites
                    for (i=0; i<2; i++)
                    {
                        j = headspritesect[joinsector[i]];
                        while (j != -1)
                        {
                            k = nextspritesect[j];
                            changespritesect(j, numsectors);
                            j = k;
                        }
                    }

                    numsectors++;

                    for (i=numwalls; i<newnumwalls; i++)
                    {
                        if (wall[i].nextwall >= 0)
                        {
                            NEXTWALL(i).nextwall = i;
                            NEXTWALL(i).nextsector = numsectors-1;
                        }
                    }

                    numwalls = newnumwalls;
                    newnumwalls = -1;

                    // clean out nextwall links for deletesector
                    for (k=0; k<2; k++)
                        for (WALLS_OF_SECTOR(joinsector[k], j))
                        {
                            wall[j].nextwall = wall[j].nextsector = -1;
#ifdef YAX_ENABLE
                            yax_setnextwall(j, YAX_CEILING, -1);
                            yax_setnextwall(j, YAX_FLOOR, -1);
#endif
                        }

                    deletesector(joinsector[0]);
                    if (joinsector[0] < joinsector[1])
                        joinsector[1]--;
                    deletesector(joinsector[1]);

                    printmessage16("Sectors joined.");
                    mkonwinvalid();
                    asksave = 1;
#ifdef YAX_ENABLE
                    yax_update(0);
                    yax_updategrays(pos.z);
#endif
                }

                joinsector[0] = -1;
            }
        }
end_join_sectors:

// PK
        for (i=0x02; i<=0x0b; i++)  // keys '1' to '0' on the upper row
            if (keystatus[i])
            {
                prefixarg = prefixtiles[i-2];
                break;
            }

        if (eitherALT && keystatus[0x1f]) //ALT-S
        {
            keystatus[0x1f] = 0;

            if (linehighlight >= 0 && wall[linehighlight].nextwall == -1)
            {
                newnumwalls = whitelinescan(sectorofwall(linehighlight), linehighlight);
                if (newnumwalls < numwalls)
                {
                    printmessage16("Can't make a sector out there.");
                    newnumwalls = -1;
                }
                else if (newnumwalls > MAXWALLS)
                {
                    printmessage16("Making new sector from inner loop would exceed wall limits.");
                    newnumwalls = -1;
                }
                else
                {
                    for (i=numwalls; i<newnumwalls; i++)
                    {
                        NEXTWALL(i).nextwall = i;
                        NEXTWALL(i).nextsector = numsectors;
#ifdef YAX_ENABLE
                        yax_setnextwall(i, YAX_CEILING, -1);
                        yax_setnextwall(i, YAX_FLOOR, -1);
#endif
                    }
#ifdef YAX_ENABLE
                    yax_setbunches(numsectors, -1, -1);
                    yax_update(0);
                    yax_updategrays(pos.z);
#endif
                    numwalls = newnumwalls;
                    newnumwalls = -1;
                    numsectors++;

                    asksave = 1;
                    printmessage16("Inner loop made into new sector.");
                }
            }
        }
        else if (keystatus[0x1f])  //S
        {
            int16_t sucksect = -1;

            keystatus[0x1f] = 0;

            for (i=0; i<numsectors; i++)
            {
                YAX_SKIPSECTOR(i);
                if (inside_editor_curpos(i) == 1)
                {
                    sucksect = i;
                    break;
                }
            }

            if (sucksect >= 0)
            {
                dax = mousxplc;
                day = mousyplc;
                if (gridlock && grid > 0)
                    locktogrid(&dax, &day);

                i = insert_sprite_common(sucksect, dax, day);

                if (i < 0)
                    printmessage16("Couldn't insert sprite.");
                else
                {
                    sprite[i].z = getflorzofslope(sucksect,dax,day);
// PK
                    if (prefixarg)
                    {
                        sprite[i].picnum = prefixarg;
                        sprite[i].xrepeat = sprite[i].yrepeat = 48;
                        prefixarg = 0;
                    }
                    else handle_sprite_in_clipboard(i);

                    if (tilesizy[sprite[i].picnum] >= 32)
                        sprite[i].cstat |= 1;

                    correct_sprite_yoffset(i);

                    printmessage16("Sprite inserted.");

                    asksave = 1;

                    VM_OnEvent(EVENT_INSERTSPRITE2D, i);
                }
            }
        }

        if (keystatus[0x2e])  // C (make circle of points)
        {
            if (highlightsectorcnt > 0)
                duplicate_selected_sectors();
            else if (highlightcnt > 0)
                duplicate_selected_sprites();
            else if (circlewall >= 0)
            {
                circlewall = -1;
            }
            else if (!m32_sideview)
            {
                if (linehighlight >= 0)
                {
#ifdef YAX_ENABLE
                    j = linehighlight;

                    if (yax_islockedwall(j) ||
                            (wall[j].nextwall >= 0 && yax_islockedwall(wall[j].nextwall)))
                        printmessage16("Can't make circle in wall constrained by sector extension.");
                    else
#endif
                    circlewall = linehighlight;
                }
            }

            keystatus[0x2e] = 0;
        }

        bad = keystatus[0x39] && !m32_sideview;  //Gotta do this to save lots of 3 spaces!

        if (circlewall >= 0)
        {
            int32_t tempint1, tempint2;

            x1 = wall[circlewall].x;
            y1 = wall[circlewall].y;
            x2 = POINT2(circlewall).x;
            y2 = POINT2(circlewall).y;
            x3 = mousxplc;
            y3 = mousyplc;
            adjustmark(&x3,&y3,newnumwalls);
            tempint1 = dmulscale4(x3-x2,x1-x3, y1-y3,y3-y2);
            tempint2 = dmulscale4(y1-y2,x1-x3, y1-y3,x2-x1);

            if (tempint2 != 0)
            {
                int32_t ps = 2, goodtogo, err=0;
                int32_t centerx, centery, circlerad;
                int16_t circleang1, circleang2;

                centerx = ((x1+x2) + scale(y1-y2,tempint1,tempint2))>>1;
                centery = ((y1+y2) + scale(x2-x1,tempint1,tempint2))>>1;

                dax = mulscale14(centerx-pos.x,zoom);
                day = mulscale14(centery-pos.y,zoom);
                drawline16base(halfxdim16+dax,midydim16+day, -2,-2, +2,+2, editorcolors[14]);
                drawline16base(halfxdim16+dax,midydim16+day, -2,+2, +2,-2, editorcolors[14]);

                circleang1 = getangle(x1-centerx,y1-centery);
                circleang2 = getangle(x2-centerx,y2-centery);

//                circleangdir = 1;
                k = ((circleang2-circleang1)&2047);
                if (mulscale4(x3-x1,y2-y1) < mulscale4(x2-x1,y3-y1))
                {
//                    circleangdir = -1;
                    k = -((circleang1-circleang2)&2047);
                }

                circlerad = ksqrt(dmulscale4(centerx-x1,centerx-x1, centery-y1,centery-y1))<<2;
                goodtogo = (numwalls+circlepoints <= MAXWALLS);

                if (bad > 0 && goodtogo)
                {
                    err = backup_drawn_walls(0);

                    if (err)
                    {
                        message("Error backing up drawn walls (code %d)!", err);
                        goodtogo = 0;
                    }
                }

                for (i=circlepoints; i>0; i--)
                {
                    j = (circleang1 + scale(i,k,circlepoints+1))&2047;
                    dax = centerx + mulscale14(sintable[(j+512)&2047],circlerad);
                    day = centery + mulscale14(sintable[j],circlerad);

                    inpclamp(&dax, -editorgridextent, editorgridextent);
                    inpclamp(&day, -editorgridextent, editorgridextent);

                    if (bad > 0 && goodtogo)
                        insertpoint(circlewall, dax,day, &circlewall);

                    dax = mulscale14(dax-pos.x,zoom);
                    day = mulscale14(day-pos.y,zoom);
                    drawline16base(halfxdim16+dax,midydim16+day, -ps,-ps, +ps,-ps, editorcolors[14]);
                    drawline16base(halfxdim16+dax,midydim16+day, +ps,-ps, +ps,+ps, editorcolors[14]);
                    drawline16base(halfxdim16+dax,midydim16+day, +ps,+ps, -ps,+ps, editorcolors[14]);
                    drawline16base(halfxdim16+dax,midydim16+day, -ps,+ps, -ps,-ps, editorcolors[14]);
//                    drawcircle16(halfxdim16+dax, midydim16+day, 3, 14);
                }

                if (bad > 0 && goodtogo)
                    backup_drawn_walls(1);

                if (bad > 0)
                {
                    if (goodtogo)
                    {
                        asksave = 1;
                        printmessage16("Circle points inserted.");
                        circlewall = -1;

                        mkonwinvalid();
                    }
                    else
                        printmessage16("Inserting circle points would exceed wall limit.");
                }
            }

            bad = 0;
            keystatus[0x39] = 0;
        }

        if (bad > 0)   //Space bar test
        {
            keystatus[0x39] = 0;
            adjustmark(&mousxplc,&mousyplc,newnumwalls);

            if (checkautoinsert(mousxplc,mousyplc,newnumwalls) == 1)
            {
                printmessage16("You must insert a point there first.");
                bad = 0;
            }
        }

        if (bad > 0)  //Space
        {
            if (newnumwalls < numwalls)  // starting wall drawing
            {
                if (numwalls >= MAXWALLS-1)
                {
                    // whatever we do, we will need at least two new walls
                    printmessage16("Can't start sector drawing: wall limit reached.");
                    goto end_space_handling;
                }

                if (numsectors >= MAXSECTORS)
                {
                    printmessage16("Can't start sector drawing: sector limit reached.");
                    goto end_space_handling;
                }

                firstx = mousxplc;
                firsty = mousyplc;  //Make first point
                newnumwalls = numwalls;
                ovh.suckwall = -1;
                ovh.split = 0;

                init_new_wall1(&ovh.suckwall, mousxplc, mousyplc);

                printmessage16("Sector drawing started.");
            }
            else  // 2nd point and up...
            {
                //if not back to first point
                if (firstx != mousxplc || firsty != mousyplc)  //nextpoint
                {
                    if (newnumwalls>=MAXWALLS)
                    {
                        printmessage16("Inserting another point would exceed wall limit.");
                        goto end_space_handling;
                    }

                    j = 0;
                    for (i=numwalls; i<newnumwalls; i++)
                        if (mousxplc == wall[i].x && mousyplc == wall[i].y)
                            j = 1;

                    if (j == 0)  // if new point is not on a position of already drawn points
                    {
                        // on the second point insertion, check if starting to split a sector
                        if (newnumwalls == numwalls+1)
                        {
                            dax = (wall[numwalls].x+mousxplc)>>1;
                            day = (wall[numwalls].y+mousyplc)>>1;

                            for (i=0; i<numsectors; i++)
                            {
                                YAX_SKIPSECTOR(i);
                                if (inside(dax,day,i) != 1)
                                    continue;

                                //check if first point at point of sector
                                m = -1;
                                for (WALLS_OF_SECTOR(i, k))
                                    if (wall[k].x==wall[numwalls].x && wall[k].y==wall[numwalls].y)
                                    {
                                        YAX_SKIPWALL(k);
                                        if (wall[k].nextwall >= 0)
                                            YAX_SKIPWALL(wall[k].nextwall);

                                        m = k;
                                        break;
                                    }

                                // if the second insertion is not on a neighboring point of the first one...
                                if (m>=0 && (POINT2(k).x != mousxplc || POINT2(k).y != mousyplc))
                                    if (wall[lastwall(k)].x != mousxplc || wall[lastwall(k)].y != mousyplc)
                                    {
                                        ovh.split = 1;
                                        ovh.splitsect = i;
                                        ovh.splitstartwall = m;
                                        break;
                                    }
                            }
                        }

                        //make new point

                        //make sure not drawing over old red line
                        bad = 0;
                        for (i=0; i<numwalls; i++)
                        {
                            YAX_SKIPWALL(i);

                            if (wall[i].nextwall >= 0)
                            {
                                int32_t lastwalx = wall[newnumwalls-1].x;
                                int32_t lastwaly = wall[newnumwalls-1].y;

                                YAX_SKIPWALL(wall[i].nextwall);

                                if (wall[i].x == mousxplc && wall[i].y == mousyplc)
                                    if (POINT2(i).x == lastwalx && POINT2(i).y == lastwaly)
                                        bad = 1;
                                if (wall[i].x == lastwalx && wall[i].y == lastwaly)
                                    if (POINT2(i).x == mousxplc && POINT2(i).y == mousyplc)
                                        bad = 1;
                            }
                        }

                        if (bad == 0)
                        {
                            init_new_wall1(&ovh.suckwall, mousxplc, mousyplc);
                        }
                        else
                        {
                            printmessage16("You can't draw new lines over red lines.");
                            goto end_space_handling;
                        }
                    }
                }

                ////////// newnumwalls is at most MAXWALLS here //////////

                //if not split and back to first point
                if (!ovh.split && newnumwalls >= numwalls+3
                        && firstx==mousxplc && firsty==mousyplc)
                {
                    wall[newnumwalls-1].point2 = numwalls;

                    if (ovh.suckwall == -1)  //if no connections to other sectors
                    {
                        k = -1;
                        for (i=0; i<numsectors; i++)
                        {
                            YAX_SKIPSECTOR(i);

                            if (inside(firstx,firsty,i) == 1)
                            {
                                // if all points inside that one sector i,
                                // will add an inner loop
                                for (j=numwalls+1; j<newnumwalls; j++)
                                {
                                    if (inside(wall[j].x, wall[j].y, i) == 0)
                                        goto check_next_sector;
                                }

                                k = i;
                                break;
                            }
check_next_sector: ;
                        }

                        if (k == -1)   //if not inside another sector either
                        {
                            //add island sector
                            if (clockdir(numwalls) == CLOCKDIR_CCW)
                                flipwalls(numwalls,newnumwalls);

                            Bmemset(&sector[numsectors], 0, sizeof(sectortype));
                            sector[numsectors].extra = -1;

                            sector[numsectors].wallptr = numwalls;
                            sector[numsectors].wallnum = newnumwalls-numwalls;
                            sector[numsectors].ceilingz = -(32<<8);
                            sector[numsectors].floorz = (32<<8);

                            for (i=numwalls; i<newnumwalls; i++)
                                copy_some_wall_members(i, -1, 1);
#ifdef YAX_ENABLE
                            yax_setbunches(numsectors, -1, -1);
#endif
                            headspritesect[numsectors] = -1;
                            numsectors++;

                            numwalls = newnumwalls;
                            newnumwalls = -1;

                            printmessage16("Created new sector %d", numsectors-1);
                        }
                        else       //else add loop to sector
                        {
                            int32_t ret = AddLoopToSector(k, NULL);

                            if (ret < 0)
                                goto end_space_handling;
#ifdef YAX_ENABLE
                            else if (ret > 0)
                                printmessage16("Added inner loop to sector %d and made new inner sector", k);
                            else
#endif
                                printmessage16("Added inner loop to sector %d", k);
                            mkonwinvalid();
                            asksave = 1;
                        }
                    }
                    else  // if connected to at least one other sector
                    {
                        int16_t sucksect;

                        //add new sector with connections

                        if (clockdir(numwalls) == CLOCKDIR_CCW)
                            flipwalls(numwalls,newnumwalls);

                        for (i=numwalls; i<newnumwalls; i++)
                        {
                            copy_some_wall_members(i, ovh.suckwall, 1);
                            wall[i].cstat &= ~(1+16+32+64);

                            if (checksectorpointer(i, numsectors) > 0)
                            {
                                // if new red line, prefer the other-side wall as base
                                ovh.suckwall = wall[i].nextwall;
                            }
                        }
                        sucksect = sectorofwall(ovh.suckwall);

                        if (numsectors != sucksect)
                            Bmemcpy(&sector[numsectors], &sector[sucksect], sizeof(sectortype));

                        sector[numsectors].wallptr = numwalls;
                        sector[numsectors].wallnum = newnumwalls-numwalls;

                        sector[numsectors].extra = -1;
                        sector[numsectors].lotag = sector[numsectors].hitag = 0;

                        setslope(numsectors, YAX_CEILING, 0);
                        setslope(numsectors, YAX_FLOOR, 0);

                        sector[numsectors].ceilingpal = sector[numsectors].floorpal = 0;
#ifdef YAX_ENABLE
                        yax_setbunches(numsectors, -1, -1);
#endif
                        headspritesect[numsectors] = -1;
                        numsectors++;

                        numwalls = newnumwalls;
                        newnumwalls = -1;

                        message("Created new sector %d based on sector %d", numsectors-1, sucksect);
                    }

                    asksave = 1;
#ifdef YAX_ENABLE
                    yax_update(0);
                    yax_updategrays(pos.z);
#endif

                    goto end_space_handling;
                }
                ////////// split sector //////////
                else if (ovh.split == 1)
                {
                    int16_t danumwalls, splitendwall, doSectorSplit;
                    int16_t secondstartwall=-1;  // used only with splitting
                    int32_t expectedNumwalls = numwalls+2*(newnumwalls-numwalls-1), loopnum;

                    startwall = sector[ovh.splitsect].wallptr;
                    endwall = startwall + sector[ovh.splitsect].wallnum - 1;

//                    OSD_Printf("numwalls: %d, newnumwalls: %d\n", numwalls, newnumwalls);
                    i = -1;
                    for (k=startwall; k<=endwall; k++)
                        if (wall[k].x == wall[newnumwalls-1].x && wall[k].y == wall[newnumwalls-1].y)
                        {
                            i = k;
                            break;
                        }
                    //           vvvv shouldn't happen, but you never know...
                    if (i==-1 || k==ovh.splitstartwall)
                        goto end_space_handling;

                    splitendwall = k;
                    doSectorSplit = (loopnumofsector(ovh.splitsect,ovh.splitstartwall)
                                     == loopnumofsector(ovh.splitsect,splitendwall));

                    if (expectedNumwalls > MAXWALLS)
                    {
                        printmessage16("%s would exceed wall limit.", bad==0 ?
                                       "Splitting sector" : "Joining sector loops");
                        newnumwalls--;
                        goto end_space_handling;
                    }
#ifdef YAX_ENABLE
                    {
                        int16_t cb, fb;

                        yax_getbunches(ovh.splitsect, &cb, &fb);

                        if ((cb>=0 && (sector[ovh.splitsect].ceilingstat&2))
                                || (fb>=0 && (sector[ovh.splitsect].floorstat&2)))
                        {
                            printmessage16("Sloped extended sectors cannot be split.");
                            newnumwalls--;
                            goto end_space_handling;
                        }
                    }
#endif
                    ////////// common code for splitting/loop joining //////////

                    newnumwalls--;  //first fix up the new walls
                    for (i=numwalls; i<newnumwalls; i++)
                    {
                        copy_some_wall_members(i, startwall, 1);
                        wall[i].point2 = i+1;
                    }

                    danumwalls = newnumwalls;  //where to add more walls

                    if (doSectorSplit)
                    {
                        // Copy outer loop of first sector
                        if (do_while_copyloop1(splitendwall, ovh.splitstartwall, &danumwalls, numwalls))
                            goto split_not_enough_walls;

                        //Add other loops for 1st sector
                        i = loopnum = loopnumofsector(ovh.splitsect,ovh.splitstartwall);

                        for (j=startwall; j<=endwall; j++)
                        {
                            k = loopnumofsector(ovh.splitsect,j);
                            if (k == i)
                                continue;

                            if (k == loopnum)
                                continue;

                            i = k;

                            if (loopinside(wall[j].x,wall[j].y, numwalls) != 1)
                                continue;

                            if (do_while_copyloop1(j, j, &danumwalls, danumwalls))
                                goto split_not_enough_walls;
                        }

                        secondstartwall = danumwalls;
                    }
                    else
                    {
                        if (do_while_copyloop1(splitendwall, splitendwall, &danumwalls, -1))
                            goto split_not_enough_walls;
                    }

                    //copy split points for other sector backwards
                    for (j=newnumwalls; j>numwalls; j--)
                    {
                        Bmemcpy(&wall[danumwalls], &wall[j], sizeof(walltype));
                        wall[danumwalls].nextwall = -1;
                        wall[danumwalls].nextsector = -1;
                        wall[danumwalls].point2 = danumwalls+1;
#ifdef YAX_ENABLE
                        yax_setnextwall(danumwalls,YAX_CEILING, -1);
                        yax_setnextwall(danumwalls,YAX_FLOOR, -1);
#endif
                        danumwalls++;
                    }

                    //copy rest of loop next
                    if (doSectorSplit)
                    {
                        if (do_while_copyloop1(ovh.splitstartwall, splitendwall, &danumwalls, secondstartwall))
                            goto split_not_enough_walls;
                    }
                    else
                    {
                        if (do_while_copyloop1(ovh.splitstartwall, ovh.splitstartwall, &danumwalls, numwalls))
                            goto split_not_enough_walls;
                    }

                    //Add other loops for 2nd sector
                    i = loopnum = loopnumofsector(ovh.splitsect,ovh.splitstartwall);

                    for (j=startwall; j<=endwall; j++)
                    {
                        k = loopnumofsector(ovh.splitsect, j);
                        if (k==i)
                            continue;

                        if (doSectorSplit && k==loopnum)
                            continue;
                        if (!doSectorSplit && (k == loopnumofsector(ovh.splitsect,ovh.splitstartwall) || k == loopnumofsector(ovh.splitsect,splitendwall)))
                            continue;

                        i = k;

                        // was loopinside(... , secondstartwall) != 1, but this way there are
                        // no duplicate or left-out loops (can happen with convoluted geometry)
                        if (doSectorSplit && (loopinside(wall[j].x,wall[j].y, numwalls) != 0))
                            continue;

                        if (do_while_copyloop1(j, j, &danumwalls, danumwalls))
                            goto split_not_enough_walls;
                    }

                    //fix all next pointers on old sector line
                    for (j=numwalls; j<danumwalls; j++)
                    {
#ifdef YAX_ENABLE
//                        if (doSectorSplit || (j!=numwalls && j!=danumwalls-1))
                            yax_fixreverselinks(j, j);
#endif
                        if (wall[j].nextwall >= 0)
                        {
                            NEXTWALL(j).nextwall = j;

                            if (!doSectorSplit || j < secondstartwall)
                                NEXTWALL(j).nextsector = numsectors;
                            else
                                NEXTWALL(j).nextsector = numsectors+1;
                        }
                    }

                    //copy sector attributes & fix wall pointers
                    Bmemcpy(&sector[numsectors], &sector[ovh.splitsect], sizeof(sectortype));
                    sector[numsectors].wallptr = numwalls;
                    sector[numsectors].wallnum = (doSectorSplit?secondstartwall:danumwalls) - numwalls;

                    if (doSectorSplit)
                    {
                        //set all next pointers on split
                        for (j=numwalls; j<newnumwalls; j++)
                        {
                            m = secondstartwall+(newnumwalls-1-j);
                            wall[j].nextwall = m;
                            wall[j].nextsector = numsectors+1;
                            wall[m].nextwall = j;
                            wall[m].nextsector = numsectors;
                        }

                        Bmemcpy(&sector[numsectors+1], &sector[ovh.splitsect], sizeof(sectortype));
                        sector[numsectors+1].wallptr = secondstartwall;
                        sector[numsectors+1].wallnum = danumwalls-secondstartwall;
                    }

                    //fix sprites
                    j = headspritesect[ovh.splitsect];
                    while (j != -1)
                    {
                        k = nextspritesect[j];
                        if (!doSectorSplit || loopinside(sprite[j].x,sprite[j].y,numwalls) == 1)
                            changespritesect(j,numsectors);
                        //else if (loopinside(sprite[j].x,sprite[j].y,secondstartwall) == 1)
                        else  //Make sure no sprites get left out & deleted!
                            changespritesect(j,numsectors+1);
                        j = k;
                    }

                    numsectors += 1 + doSectorSplit;

                    k = danumwalls-numwalls;  //Back of number of walls of new sector for later
                    numwalls = danumwalls;

                    //clear out old sector's next pointers for clean deletesector
                    for (j=startwall; j<=endwall; j++)
                    {
#ifdef YAX_ENABLE
                        // same thing for yax-nextwalls (only forward links!)
                        yax_setnextwall(j, YAX_CEILING, -1);
                        yax_setnextwall(j, YAX_FLOOR, -1);
#endif
                        wall[j].nextwall = wall[j].nextsector = -1;
                    }
                    deletesector(ovh.splitsect);

                    //Check pointers
                    for (j=numwalls-k; j<numwalls; j++)
                    {
                        if (wall[j].nextwall >= 0)
                            checksectorpointer(wall[j].nextwall, wall[j].nextsector);
                        checksectorpointer(j, sectorofwall(j));
                    }

                    //k now safe to use as temp

                    if (numwalls==expectedNumwalls)
                    {
                        message("%s", doSectorSplit ? "Sector split." : "Loops joined.");
                    }
                    else
                    {
                        message("%s WARNING: CREATED %d MORE WALLS THAN EXPECTED!",
                                doSectorSplit ? "Sector split." : "Loops joined.",
                                numwalls-expectedNumwalls);
                        // this would display 'num* out of bounds' but this corruption
                        // is almost as bad... (shouldn't happen anymore)
                        if (numcorruptthings < MAXCORRUPTTHINGS)
                            corruptthings[numcorruptthings++] = 0;
                        corruptlevel = 5;
                    }

                    if (0)
                    {
split_not_enough_walls:
                        message("%s failed: not enough space beyond wall[]",
                                doSectorSplit ? "Splitting sectors" : "Joining loops");
                    }

                    newnumwalls = -1;
                    asksave = 1;

                    mkonwinvalid();
#ifdef YAX_ENABLE
                    yax_update(0);
                    yax_updategrays(pos.z);
#endif
                }
            }
        }
end_space_handling:

        if (keystatus[0x1c]) //Left Enter
        {
            keystatus[0x1c] = 0;
            if (keystatus[0x2a] && keystatus[0x1d])  // LCtrl+LShift
            {
#ifdef YAX_ENABLE
                if (numyaxbunches == 0 ||
                    (fade_editor_screen(-1), ask_if_sure("Really check all wall pointers in TROR map?", 0)))
#endif
                {
                    printmessage16("CHECKING ALL POINTERS!");
                    for (i=0; i<numsectors; i++)
                    {
                        startwall = sector[i].wallptr;
                        for (j=startwall; j<numwalls; j++)
                            if (startwall > wall[j].point2)
                                startwall = wall[j].point2;
                        sector[i].wallptr = startwall;
                    }
                    for (i=numsectors-2; i>=0; i--)
                        sector[i].wallnum = sector[i+1].wallptr-sector[i].wallptr;
                    sector[numsectors-1].wallnum = numwalls-sector[numsectors-1].wallptr;

                    for (i=0; i<numwalls; i++)
                    {
                        wall[i].nextsector = -1;
                        wall[i].nextwall = -1;
                    }
                    for (i=0; i<numsectors; i++)
                    {
                        for (WALLS_OF_SECTOR(i, j))
                            checksectorpointer(j, i);
                    }
                    printmessage16("ALL POINTERS CHECKED!");
                    asksave = 1;
                }
            }
            else  // NOT (LCtrl + LShift)
            {
                if (newnumwalls > numwalls)  // batch insert points
                {
                    int32_t numdrawnwalls = newnumwalls-numwalls;
                    vec2_t *point = (vec2_t *)tempxyar;  // [MAXWALLS][2]
                    int32_t insdpoints = 0;

                    // back up the points of the line strip
                    for (i=0; i<numdrawnwalls+1; i++)
                    {
                        point[i].x = wall[numwalls+i].x;
                        point[i].y = wall[numwalls+i].y;
                    }

                    newnumwalls = -1;

                    for (i=0; i<numdrawnwalls; i++)
                    {
                        for (j=numwalls-1; j>=0; j--)  /* j may be modified in loop */
                        {
                            vec2_t pint;
                            int32_t inspts;

                            YAX_SKIPWALL(j);

                            if (!lineintersect2v((vec2_t *)&wall[j], (vec2_t *)&POINT2(j),
                                                 &point[i], &point[i+1], &pint))
                                continue;

                            if (vec2eq(&pint, (vec2_t *)&wall[j]) || vec2eq(&pint, (vec2_t *)&POINT2(j)))
                                continue;

                            inspts = M32_InsertPoint(j, pint.x, pint.y, -1, &j);  /* maybe modify j */

                            if (inspts==0)
                            {
                                printmessage16("Wall limit exceeded while inserting points.");
                                goto end_batch_insert_points;
                            }
                            else if (inspts > 0 && inspts < 65536)
                            {
                                insdpoints += inspts;
                            }
                            else  // inspts >= 65536
                            {
                                message("ERR: Inserted %d points for constr. wall (exp. %d; %d already ins'd)",
                                        inspts&65535, inspts>>16, insdpoints);
                                goto end_batch_insert_points;
                            }
                        }
                    }

                    message("Batch-inserted %d points in total", insdpoints);
end_batch_insert_points:

                    if (insdpoints != 0)
                    {
#ifdef YAX_ENABLE
                        yax_updategrays(pos.z);
#endif
                        mkonwinvalid();
                        asksave = 1;
                    }
                }
                else if (linehighlight >= 0)
                {
                    checksectorpointer(linehighlight,sectorofwall(linehighlight));
                    printmessage16("Checked pointers of highlighted line.");
                    asksave = 1;
                }
            }
        }

        {
            static int32_t backspace_last = 0;

            if (keystatus[0x0e]) //Backspace
            {
                keystatus[0x0e] = 0;

                if (newnumwalls >= numwalls)
                {
                    backspace_last = 1;

                    if (newnumwalls == numwalls+1 || keystatus[0x1d])  // LCtrl: delete all newly drawn walls
                        newnumwalls = -1;
                    else
                        newnumwalls--;
                }
                else if (backspace_last==0)
                {
                    graphicsmode += (1-2*(DOWN_BK(RUN) || keystatus[0x36]))+3;
                    graphicsmode %= 3;
                    printmessage16("2D mode textures %s",
                                   (graphicsmode == 2)?"enabled w/ animation":graphicsmode?"enabled":"disabled");
                }
            }
            else
                backspace_last = 0;
        }

        if (keystatus[0xd3] && eitherCTRL && numwalls > 0)  //sector delete
        {
            int32_t numdelsectors = 0;
            char *origframe=NULL;

#ifdef YAX_ENABLE
            int16_t cb, fb;
            uint8_t bunchbitmap[YAX_MAXBUNCHES>>3];
            Bmemset(bunchbitmap, 0, sizeof(bunchbitmap));
#endif
            keystatus[0xd3] = 0;

            for (i=0; i<numsectors; i++)
            {
                YAX_SKIPSECTOR(i);
                numdelsectors += (inside_editor_curpos(i) == 1);
            }

            for (i=0; i<numsectors; i++)
            {
                if (highlightsectorcnt <= 0 || !keystatus[0x2a])
                {
                    YAX_SKIPSECTOR(i);

                    if (inside_editor_curpos(i) != 1)
                        continue;
                }

                k = 0;
                if (highlightsectorcnt > 0)
                {
                    // LShift: force highlighted sector deleting
                    if (keystatus[0x2a] || (hlsectorbitmap[i>>3]&(1<<(i&7))))
                    {
                        for (j=highlightsectorcnt-1; j>=0; j--)
                        {
#ifdef YAX_ENABLE
                            yax_getbunches(highlightsector[j], &cb, &fb);
                            if (cb>=0) bunchbitmap[cb>>3] |= (1<<(cb&7));
                            if (fb>=0) bunchbitmap[fb>>3] |= (1<<(fb&7));
#endif
                            deletesector(highlightsector[j]);
                            for (k=j-1; k>=0; k--)
                                if (highlightsector[k] >= highlightsector[j])
                                    highlightsector[k]--;
                        }

                        printmessage16("Highlighted sectors deleted.");
                        mkonwinvalid();
                        k = 1;
                    }
                }

                if (k == 0)
                {
                    if (numdelsectors > 1)
                    {
                        if (!bakframe_fillandfade(&origframe, i, "Delete this sector? (Y/N)"))
                            continue;
                    }

#ifdef YAX_ENABLE
                    yax_getbunches(i, &cb, &fb);
                    if (cb>=0) bunchbitmap[cb>>3] |= (1<<(cb&7));
                    if (fb>=0) bunchbitmap[fb>>3] |= (1<<(fb&7));
#endif
                    deletesector(i);
                    mkonwinvalid();
                    printmessage16("Sector deleted.");
                }

                if (origframe)
                    Bfree(origframe);

#ifdef YAX_ENABLE
                for (j=0; j<numsectors; j++)
                {
                    yax_getbunches(j, &cb, &fb);
                    if (cb>=0 && (bunchbitmap[cb>>3] & (1<<(cb&7))))
                        yax_setbunch(j, YAX_CEILING, -1);
                    if (fb>=0 && (bunchbitmap[fb>>3] & (1<<(fb&7))))
                        yax_setbunch(j, YAX_FLOOR, -1);
                }
#endif
                Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
                update_highlightsector();

                Bmemset(show2dwall, 0, sizeof(show2dwall));
                update_highlight();

                newnumwalls = -1;
                asksave = 1;
#ifdef YAX_ENABLE
                yax_update(0);
                yax_updategrays(pos.z);
#endif
                break;
            }
        }

        if (keystatus[0xd3] && (pointhighlight >= 0))
        {
            if ((pointhighlight&0xc000) == 16384)   //Sprite Delete
            {
                deletesprite(pointhighlight&16383);
                printmessage16("Sprite deleted.");

                update_highlight();
                asksave = 1;
            }
            keystatus[0xd3] = 0;
        }

        if (keystatus[0xd2] || keystatus[0x17])  //InsertPoint
        {
            if (highlightsectorcnt > 0)
                duplicate_selected_sectors();
            else if (highlightcnt > 0)
                duplicate_selected_sprites();
            else if (linehighlight >= 0)
            {
                int32_t onewnumwalls = newnumwalls;
                int32_t wallis2sided = (wall[linehighlight].nextwall>=0);

                int32_t err = backup_drawn_walls(0);

                if (err)
                {
                    message("Error backing up drawn walls (code %d)!", err);
                }
                else if (max(numwalls,onewnumwalls) >= MAXWALLS-wallis2sided)
                {
                    printmessage16("Inserting point would exceed wall limit.");
                }
                else
                {
                    getclosestpointonwall(m32_sideview?searchx:mousxplc, m32_sideview?searchy:mousyplc,
                                          linehighlight, &dax,&day, 1);
                    i = linehighlight;
                    if (m32_sideview)
                    {
                        int32_t y_p, d, dx, dy, frac;

                        dx = dax - m32_wallscreenxy[i][0];
                        dy = day - m32_wallscreenxy[i][1];
                        d = max(dx, dy);
                        y_p = (dy>dx);

                        if (d==0)
                            goto point_not_inserted;

                        frac = divscale24(d, m32_wallscreenxy[wall[i].point2][y_p]-m32_wallscreenxy[i][y_p]);
                        dax = POINT2(i).x - wall[i].x;
                        day = POINT2(i).y - wall[i].y;
                        dax = wall[i].x + mulscale24(dax,frac);
                        day = wall[i].y + mulscale24(day,frac);
                    }

                    adjustmark(&dax,&day, newnumwalls);
                    if ((wall[i].x == dax && wall[i].y == day) || (POINT2(i).x == dax && POINT2(i).y == day))
                    {
point_not_inserted:
                        printmessage16("Point not inserted.");
                    }
                    else
                    {
                        int32_t insdpoints = M32_InsertPoint(linehighlight, dax, day, onewnumwalls, NULL);

                        if (insdpoints == 0)
                        {
                            printmessage16("Inserting points would exceed wall limit.");
                            goto end_insert_points;
                        }
                        else if (insdpoints == 1)
                        {
                            printmessage16("Point inserted.");
                        }
                        else if (insdpoints > 1 && insdpoints < 65536)
                        {
                            message("Inserted %d points for constrained wall.", insdpoints);
                        }
                        else  // insdpoints >= 65536
                        {
                            message("Inserted %d points for constrained wall (expected %d, WTF?).",
                                    insdpoints&65535, insdpoints>>16);
                        }

#ifdef YAX_ENABLE
                        yax_updategrays(pos.z);
#endif
                        mkonwinvalid();
                    }
                }

end_insert_points:
                backup_drawn_walls(1);

                asksave = 1;
            }
            keystatus[0xd2] = keystatus[0x17] = 0;
        }

        /*j = 0;
        for(i=22-1;i>=0;i--) updatecrc16(j,kensig[i]);
        if ((j&0xffff) != 0xebf)
        {
        	printf("Don't screw with my name.\n");
        	exit(0);
        }*/
        //printext16(9L,336+9L,4,-1,kensig,0);
        //printext16(8L,336+8L,12,-1,kensig,0);

        showframe(1);
        synctics = totalclock-lockclock;
        lockclock += synctics;

        if (keystatus[buildkeys[BK_MODE2D_3D]])
        {
            updatesector(pos.x,pos.y,&cursectnum);
            if (cursectnum >= 0)
                keystatus[buildkeys[BK_MODE2D_3D]] = 2;
            else
                printmessage16("Arrow must be inside a sector before entering 3D mode.");
        }

// vvv PK ------------------------------------ (LShift) Ctrl-X: (prev) next map
// this is copied from 'L' (load map), but without copying the highlighted sectors

        if (quickmapcycling && keystatus[0x2d])   //X
        {
            if (eitherCTRL)  //Ctrl
            {
nextmap:
//				bad = 0;
                i = menuselect_auto(keystatus[0x2a] ? 0:1); // Left Shift: prev map
                if (i < 0)
                {
                    if (i == -1)
                        message("No more map files.");
                    else if (i == -2)
                        message("No .MAP files found.");
                    else if (i == -3)
                        message("Load map first!");
                }
                else
                {
                    if (LoadBoard(NULL, 4))
                        goto nextmap;

                    RESET_EDITOR_VARS();
                    oposz = pos.z;
                }
                showframe(1);
                keystatus[0x1c] = 0;

                keystatus[0x2d]=keystatus[0x13]=0;

            }
        }
// ^^^ PK ------------------------------------

        if (keystatus[1] && joinsector[0] >= 0)
        {
            keystatus[1]=0;
            joinsector[0]=-1;
            printmessage16("No sectors joined.");
        }

CANCEL:
        if (keystatus[1])
        {
            keystatus[1] = 0;
#if M32_UNDO
            _printmessage16("(N)ew, (L)oad, (S)ave, save (A)s, (T)est map, (U)ndo, (R)edo, (Q)uit");
#else
            _printmessage16("(N)ew, (L)oad, (S)ave, save (A)s, (T)est map, (Q)uit");
#endif
            printext16(16*8, ydim-STATUS2DSIZ2-12, editorcolors[15], -1, GetSaveBoardFilename(), 0);

            showframe(1);
            bflushchars();
            bad = 1;
            while (bad == 1)
            {
                char ch;

                if (handleevents())
                {
                    if (quitevent)
                        quitevent = 0;
                }
                idle();

                ch = bgetchar();

                if (keystatus[1])
                {
                    keystatus[1] = 0;
                    bad = 0;
                    // printmessage16("");
                }
                else if (ch == 'n' || ch == 'N')  //N
                {
                    bad = 0;

                    if (!ask_if_sure("Are you sure you want to start a new board? (Y/N)", 0))
                        break;
                    else
                    {
                        int32_t bakstat=-1;
                        mapinfofull_t bakmap;

                        if (highlightsectorcnt > 0)
                            bakstat = backup_highlighted_map(&bakmap);

//                        Bmemset(hlsectorbitmap, 0, sizeof(hlsectorbitmap));
//                        highlightsectorcnt = -1;

                        highlightcnt = -1;
                        //Clear all highlights
                        Bmemset(show2dwall, 0, sizeof(show2dwall));
                        Bmemset(show2dsprite, 0, sizeof(show2dsprite));

                        for (i=0; i<MAXSECTORS; i++) sector[i].extra = -1;
                        for (i=0; i<MAXWALLS; i++) wall[i].extra = -1;
                        for (i=0; i<MAXSPRITES; i++) sprite[i].extra = -1;

                        RESET_EDITOR_VARS();
                        mkonwinvalid();

                        reset_default_mapstate();

                        Bstrcpy(boardfilename,"newboard.map");
                        ExtLoadMap(boardfilename);
#if M32_UNDO
                        map_undoredo_free();
#endif
                        if (bakstat==0)
                        {
                            bakstat = restore_highlighted_map(&bakmap, 1);
                            if (bakstat == -1)
                                message("Can't copy highlighted portion of old map: limits exceeded.");
                        }

                        CheckMapCorruption(4, 0);

                        break;
                    }

                    // printmessage16("");
                    showframe(1);
                }
                else if (ch == 'l' || ch == 'L')  //L
                {
                    bad = 0;
                    i = menuselect();
                    if (i < 0)
                    {
                        if (i == -2)
                            printmessage16("No .MAP files found.");
                    }
                    else
                    {
                        int32_t bakstat=-1;
                        mapinfofull_t bakmap;

                        if (highlightsectorcnt > 0)
                            bakstat = backup_highlighted_map(&bakmap);
// __old_mapcopy_2__
                        if (LoadBoard(NULL, 4))
                        {
                            message("Invalid map format, nothing loaded.");
                            if (bakstat==0)
                                mapinfofull_free(&bakmap);
                        }
                        else
                        {
                            RESET_EDITOR_VARS();
                            oposz = pos.z;

                            if (bakstat==0)
                            {
                                bakstat = restore_highlighted_map(&bakmap, 1);
                                if (bakstat == -1)
                                    message("Can't copy highlighted portion of old map: limits exceeded.");
                            }
                        }
                    }
                    showframe(1);
                    keystatus[0x1c] = 0;
                }
                else if (ch == 'a' || ch == 'A')  //A
                {
                    int32_t corrupt = CheckMapCorruption(4, 0);

                    bad = 0;

                    Bstrcpy(selectedboardfilename, boardfilename);
                    if (Bstrrchr(boardfilename, '/'))
                        Bstrcpy(boardfilename, Bstrrchr(boardfilename, '/')+1);

                    i = 0;
                    while ((boardfilename[i] != 0) && (i < 64))
                        i++;
                    if (boardfilename[i-4] == '.')
                        i -= 4;
                    boardfilename[i] = 0;

                    bflushchars();
                    while (bad == 0)
                    {
                        _printmessage16("%sSave as: ^011%s%s", corrupt>=4?"(map corrupt) ":"",
                                        boardfilename, (totalclock&32)?"_":"");
                        showframe(1);

                        if (handleevents())
                            quitevent = 0;

                        idle();

                        ch = bgetchar();

                        if (keystatus[1]) bad = 1;
                        else if (ch == 13) bad = 2;
                        else if (ch > 0)
                        {
                            if (i > 0 && (ch == 8 || ch == 127))
                            {
                                i--;
                                boardfilename[i] = 0;
                            }
                            else if (i < 40 && ch > 32 && ch < 128)
                            {
                                boardfilename[i++] = ch;
                                boardfilename[i] = 0;
                            }
                        }
                    }

                    if (bad == 1)
                    {
                        Bstrcpy(boardfilename, selectedboardfilename);
                        keystatus[1] = 0;
                        printmessage16("Operation cancelled");
                        showframe(1);
                    }
                    else if (bad == 2)
                    {
                        char *slash;

                        keystatus[0x1c] = 0;

                        Bstrcpy(&boardfilename[i], ".map");

                        slash = Bstrrchr(selectedboardfilename,'/');
                        Bstrcpy(slash ? slash+1 : selectedboardfilename, boardfilename);

                        SaveBoardAndPrintMessage(selectedboardfilename);

                        Bstrcpy(boardfilename, selectedboardfilename);
                        ExtSetupMapFilename(boardfilename);
                    }
                    bad = 0;
                }
                else if (ch == 's' || ch == 'S')  //S
                {
                    bad = 0;

                    if (CheckMapCorruption(4, 0)>=4)
                    {
                        fade_editor_screen(-1);
                        if (!ask_if_sure("Map is corrupt. Are you sure you want to save? (Y/N)", 0))
                            break;
                    }

                    SaveBoardAndPrintMessage(NULL);

                    showframe(1);
                }
                else if (ch == 't' || ch == 'T')
                {
                    test_map(0);
                }
#if M32_UNDO
                else if (ch == 'u' || ch == 'U')
                {
                    bad = 0;
                    if (map_undoredo(0)) printmessage16("Nothing to undo!");
                    else printmessage16("Revision %d undone",map_revision);
                }
                else if (ch == 'r' || ch == 'R')
                {
                    bad = 0;
                    if (map_undoredo(1)) printmessage16("Nothing to redo!");
                    else printmessage16("Restored revision %d",map_revision-1);
                }
#endif
                else if (ch == 'q' || ch == 'Q')  //Q
                {
                    bad = 0;

                    if (ask_if_sure("Are you sure you want to quit?", 0))
                    {
                        //QUIT!

                        int32_t corrupt = CheckMapCorruption(4, 0);

                        if (ask_if_sure(corrupt<4?"Save changes?":"Map corrupt. Save changes?", 2+(corrupt>=4)))
                            SaveBoard(NULL, 0);

                        while (keystatus[1] || keystatus[0x2e])
                        {
                            keystatus[1] = 0;
                            keystatus[0x2e] = 0;
                            quitevent = 0;
                            printmessage16("Operation cancelled");
                            showframe(1);
                            goto CANCEL;
                        }

                        ExtUnInit();
//                        clearfilenames();
                        uninitengine();

                        exit(0);
                    }

                    // printmessage16("");
                    showframe(1);
                }
            }

            clearkeys();
        }

        VM_OnEvent(EVENT_KEYS2D, -1);

        //nextpage();
    }

    for (i=0; i<highlightsectorcnt; i++)
    {
        for (WALLS_OF_SECTOR(highlightsector[i], j))
        {
            if (wall[j].nextwall >= 0)
                checksectorpointer(wall[j].nextwall, wall[j].nextsector);
            checksectorpointer(j, highlightsector[i]);
        }
    }
    mkonwinvalid();

    fixspritesectors();

    if (setgamemode(fullscreen,xdimgame,ydimgame,bppgame) < 0)
    {
        initprintf("%d * %d not supported in this graphics mode\n",xdim,ydim);
        ExtUnInit();
//        clearfilenames();
        uninitengine();
        exit(1);
    }

    setbrightness(GAMMA_CALC,0,0);

    pos.z = oposz;

    searchx = clamp(scale(searchx,xdimgame,xdim2d), 8, xdimgame-8-1);
    searchy = clamp(scale(searchy,ydimgame,ydim2d-STATUS2DSIZ), 8, ydimgame-8-1);

    VM_OnEvent(EVENT_ENTER3DMODE, -1);
}

// flags:
// 1: quit_is_yes
// 2: don't clear keys on return
int32_t ask_if_sure(const char *query, uint32_t flags)
{
    char ch;
    int32_t ret=-1;

    if (!query)
        _printmessage16("Are you sure?");
    else
        _printmessage16("%s", query);
    showframe(1);
    bflushchars();

    while ((keystatus[1]|keystatus[0x2e]) == 0 && ret==-1)
    {
        if (handleevents())
        {
            if (quitevent)
            {
                if (flags&1)
                    return 1;
                else
                    quitevent = 0;
            }
        }
        idle();

        ch = bgetchar();

        if (ch == 'y' || ch == 'Y')
            ret = 1;
        else if (ch == 'n' || ch == 'N' || ch == 13 || ch == ' ')
            ret = 0;
    }

    if ((flags&2)==0)
        clearkeys();

    if (ret >= 0)
        return ret;

    return 0;
}

int32_t editor_ask_function(const char *question, const char *dachars, int32_t numchars)
{
    char ch;
    int32_t i, ret=-1;

    _printmessage16("%s", question);

    showframe(1);
    bflushchars();

    // 'c' is cancel too, but can be overridden
    while ((keystatus[1]|keystatus[0x2e]) == 0 && ret==-1)
    {
        if (handleevents())
            quitevent = 0;

        idle();
        ch = bgetchar();

        for (i=0; i<numchars; i++)
            if (ch==Btolower(dachars[i]) || ch==Btoupper(dachars[i]))
                ret = i;
    }

    clearkeys();

    return ret;
}

#ifdef YAX_ENABLE
static int32_t ask_above_or_below(void)
{
    char dachars[2] = {'a', 'z'};
    return editor_ask_function("Extend above (a) or below (z)?", dachars, 2);
}
#endif

static void SaveBoardAndPrintMessage(const char *fn)
{
    const char *f;

    _printmessage16("Saving board...");
    showframe(1);

    f = SaveBoard(fn, 0);

    if (f)
    {
        if (saveboard_fixedsprites)
            printmessage16("Saved board %sto %s (changed sectnums of %d sprites).",
                           saveboard_savedtags?"and tags ":"", f, saveboard_fixedsprites);
        else
            printmessage16("Saved board %sto %s.", saveboard_savedtags?"and tags ":"", f);
    }
    else
    {
        if (saveboard_fixedsprites)
            printmessage16("Saving board failed (changed sectnums of %d sprites).",
                           saveboard_fixedsprites);
        else
            printmessage16("Saving board failed.");
    }
}

// get the file name of the file that would be written if SaveBoard(NULL, 0) was called
static const char *GetSaveBoardFilename(void)
{
    const char *fn = boardfilename, *f;

    if (pathsearchmode)
        f = fn;
    else
    {
        // virtual filesystem mode can't save to directories so drop the file into
        // the current directory
        f = Bstrrchr(fn, '/');
        if (!f)
            f = fn;
        else
            f++;
    }

    return f;
}

// flags:  1:no ExtSaveMap (backup.map) and no taglabels saving
const char *SaveBoard(const char *fn, uint32_t flags)
{
    const char *f;
    int32_t ret;

    saveboard_savedtags = 0;

    if (!fn)
        fn = boardfilename;

    if (pathsearchmode)
        f = fn;
    else
    {
        // virtual filesystem mode can't save to directories so drop the file into
        // the current directory
        f = Bstrrchr(fn, '/');
        if (!f)
            f = fn;
        else
            f++;
    }

    saveboard_fixedsprites = ExtPreSaveMap();
    ret = saveboard(f,&startposx,&startposy,&startposz,&startang,&startsectnum);
    if ((flags&1)==0)
    {
        ExtSaveMap(f);
        saveboard_savedtags = !taglab_save(f);
    }

    if (!ret)
        return f;
    else
        return NULL;
}

// flags:  1: for running on Mapster32 init
//         4: passed to loadboard flags (no polymer_loadboard); implies no maphack loading
int32_t LoadBoard(const char *filename, uint32_t flags)
{
    int32_t i, tagstat, loadingflags=(!pathsearchmode&&grponlymode?2:0);

    if (!filename)
        filename = selectedboardfilename;

    if (filename != boardfilename)
        Bstrcpy(boardfilename, filename);

    for (i=0; i<MAXSECTORS; i++) sector[i].extra = -1;
    for (i=0; i<MAXWALLS; i++) wall[i].extra = -1;
    for (i=0; i<MAXSPRITES; i++) sprite[i].extra = -1;

    editorzrange[0] = INT32_MIN;
    editorzrange[1] = INT32_MAX;

    ExtPreLoadMap();
    i = loadboard(boardfilename,(flags&4)|loadingflags, &pos.x,&pos.y,&pos.z,&ang,&cursectnum);
    if (i == -2)
        i = loadoldboard(boardfilename,loadingflags, &pos.x,&pos.y,&pos.z,&ang,&cursectnum);
    if (i < 0)
    {
//        printmessage16("Invalid map format.");
        return i;
    }

    mkonwinvalid();

    highlightcnt = -1;
    Bmemset(show2dwall, 0, sizeof(show2dwall));  //Clear all highlights
    Bmemset(show2dsprite, 0, sizeof(show2dsprite));

    if ((flags&4)==0)
        loadmhk(0);

    tagstat = taglab_load(boardfilename, loadingflags);
    ExtLoadMap(boardfilename);

    if (mapversion < 7)
        message("Map %s loaded successfully and autoconverted to V7!",boardfilename);
    else
    {
        i = CheckMapCorruption(4, 0);
        message("Loaded map %s%s %s", boardfilename, tagstat==0?" w/tags":"",
                i==0?"successfully": (i<4 ? "(moderate corruption)" : "(HEAVY corruption)"));
    }

    startposx = pos.x;      //this is same
    startposy = pos.y;
    startposz = pos.z;
    startang = ang;
    startsectnum = cursectnum;

    return 0;
}

void getpoint(int32_t searchxe, int32_t searchye, int32_t *x, int32_t *y)
{
    inpclamp(&pos.x, -editorgridextent, editorgridextent);
    inpclamp(&pos.y, -editorgridextent, editorgridextent);

    searchxe -= halfxdim16;
    searchye -= midydim16;

    if (m32_sideview)
    {
        if (m32_sidesin!=0)
            searchye = divscale14(searchye, m32_sidesin);
        rotatepoint(0,0, searchxe,searchye, -m32_sideang, &searchxe,&searchye);
    }

    *x = pos.x + divscale14(searchxe,zoom);
    *y = pos.y + divscale14(searchye,zoom);

    inpclamp(x, -editorgridextent, editorgridextent);
    inpclamp(y, -editorgridextent, editorgridextent);
}

static int32_t getlinehighlight(int32_t xplc, int32_t yplc, int32_t line)
{
    int32_t i, j, dst, dist, closest, x1, y1, x2, y2, nx, ny;
    int32_t daxplc, dayplc;

    if (numwalls == 0)
        return -1;

    if (mouseb & 1)
        return line;

    if ((pointhighlight&0xc000) == 16384)
        return -1;

    dist = 1024;
    if (m32_sideview)
    {
        daxplc = searchx;
        dayplc = searchy;
        dist = mulscale14(dist, zoom);
    }
    else
    {
        daxplc = xplc;
        dayplc = yplc;
    }

    closest = -1;
    for (i=0; i<numwalls; i++)
    {
        if (!m32_sideview)
            YAX_SKIPWALL(i);

        getclosestpointonwall(daxplc,dayplc, i, &nx,&ny, 1);
        dst = klabs(daxplc-nx) + klabs(dayplc-ny);
        if (dst <= dist)
        {
            dist = dst;
            closest = i;
        }
    }

    if (closest>=0 && (j = wall[closest].nextwall) >= 0)
#ifdef YAX_ENABLE
    if (m32_sideview || ((graywallbitmap[j>>3]&(1<<(j&7)))==0))
#endif
    {
        //if red line, allow highlighting of both sides
        if (m32_sideview)
        {
            x1 = m32_wallscreenxy[closest][0];
            y1 = m32_wallscreenxy[closest][1];
            x2 = m32_wallscreenxy[wall[closest].point2][0];
            y2 = m32_wallscreenxy[wall[closest].point2][1];
        }
        else
        {
            x1 = wall[closest].x;
            y1 = wall[closest].y;
            x2 = POINT2(closest).x;
            y2 = POINT2(closest).y;
        }

        i = wall[closest].nextwall;
        if (!m32_sideview ||
            ((*(int64_t *)m32_wallscreenxy[closest]==*(int64_t *)m32_wallscreenxy[wall[j].point2]) &&
             (*(int64_t *)m32_wallscreenxy[wall[closest].point2]==*(int64_t *)m32_wallscreenxy[j])))
            if (dmulscale32(daxplc-x1,y2-y1,-(x2-x1),dayplc-y1) >= 0)
                closest = j;
    }

    return closest;
}

int32_t getpointhighlight(int32_t xplc, int32_t yplc, int32_t point)
{
    int32_t i, j, dst, dist = 512, closest = -1;
    int32_t dax,day;
    int32_t alwaysshowgray = (showinnergray || !(editorzrange[0]==INT32_MIN && editorzrange[1]==INT_MAX));

    if (numwalls == 0)
        return -1;

    if (mouseb & 1)
        return point;

    if (grid < 1)
        dist = 0;

    for (i=0; i<numsectors; i++)
    {
        if (!m32_sideview || !alwaysshowgray)
            YAX_SKIPSECTOR(i);

        for (j=sector[i].wallptr; j<sector[i].wallptr+sector[i].wallnum; j++)
        {
            if (!m32_sideview)
                dst = klabs(xplc-wall[j].x) + klabs(yplc-wall[j].y);
            else
            {
                screencoords(&dax,&day, wall[j].x-pos.x,wall[j].y-pos.y, zoom);
                day += getscreenvdisp(getflorzofslope(i, wall[j].x,wall[j].y)-pos.z, zoom);

                if (halfxdim16+dax < 0 || halfxdim16+dax >= xdim || midydim16+day < 0 || midydim16+day >= ydim)
                    continue;

                dst = klabs(halfxdim16+dax-searchx) + klabs(midydim16+day-searchy);
            }

            if (dst <= dist)
            {
                // prefer white walls
                if (dst<dist || closest==-1 || (wall[j].nextwall>=0)-(wall[closest].nextwall>=0) <= 0)
                    dist = dst, closest = j;
            }
        }
    }

    if (zoom >= 256)
        for (i=0; i<MAXSPRITES; i++)
            if (sprite[i].statnum < MAXSTATUS)
            {
                if ((!m32_sideview || !alwaysshowgray) && sprite[i].sectnum >= 0)
                    YAX_SKIPSECTOR(sprite[i].sectnum);

                if (!m32_sideview)
                {
                    dst = klabs(xplc-sprite[i].x) + klabs(yplc-sprite[i].y);
                }
                else
                {
                    screencoords(&dax,&day, sprite[i].x-pos.x,sprite[i].y-pos.y, zoom);
                    day += getscreenvdisp(sprite[i].z-pos.z, zoom);

                    if (halfxdim16+dax < 0 || halfxdim16+dax >= xdim || midydim16+day < 0 || midydim16+day >= ydim)
                        continue;

                    dst = klabs(halfxdim16+dax-searchx) + klabs(midydim16+day-searchy);
                }

                // was (dst <= dist), but this way, when duplicating sprites,
                // the selected ones are dragged first
                if (dst < dist || (dst == dist && (show2dsprite[i>>3]&(1<<(i&7)))))
                    dist = dst, closest = i+16384;
            }

    return closest;
}

static void locktogrid(int32_t *dax, int32_t *day)
{
    *dax = ((*dax+(1024>>grid))&(0xffffffff<<(11-grid)));
    *day = ((*day+(1024>>grid))&(0xffffffff<<(11-grid)));
}

static int32_t adjustmark(int32_t *xplc, int32_t *yplc, int16_t danumwalls)
{
    int32_t i, dst, dist, dax, day, pointlockdist;

    if (danumwalls < 0)
        danumwalls = numwalls;

    pointlockdist = 0;
    if (grid > 0 && gridlock)
        pointlockdist = (256>>grid);

    dist = pointlockdist;
    dax = *xplc;
    day = *yplc;

    for (i=0; i<danumwalls; i++)
    {
        YAX_SKIPWALL(i);

        dst = klabs((*xplc)-wall[i].x) + klabs((*yplc)-wall[i].y);
        if (dst < dist)
        {
            dist = dst;
            dax = wall[i].x;
            day = wall[i].y;
        }
    }
    if (dist == pointlockdist)
        if (gridlock && grid > 0)
            locktogrid(&dax, &day);

    *xplc = dax;
    *yplc = day;

    return(0);
}

static int32_t checkautoinsert(int32_t dax, int32_t day, int16_t danumwalls)
{
    int32_t i, x1, y1, x2, y2;

    if (danumwalls < 0)
        danumwalls = numwalls;

    for (i=0; i<danumwalls; i++)    // Check if a point should be inserted
    {
        YAX_SKIPWALL(i);

        x1 = wall[i].x;
        y1 = wall[i].y;
        x2 = POINT2(i).x;
        y2 = POINT2(i).y;

        if ((x1 != dax || y1 != day) && (x2 != dax || y2 != day))
            if ((x1 <= dax && dax <= x2) || (x2 <= dax && dax <= x1))
                if ((y1 <= day && day <= y2) || (y2 <= day && day <= y1))
                    if ((dax-x1)*(y2-y1) == (day-y1)*(x2-x1))
                        return(1);          //insertpoint((short)i,dax,day,NULL);
    }

    return(0);
}

// wallstart has to be the starting wall of a loop!
static int32_t clockdir(int16_t wallstart)   //Returns: 0 is CW, 1 is CCW
{
    int16_t i, themin;
    int32_t minx, tempint, x0, x1, x2, y0, y1, y2;

    minx = 0x7fffffff;
    themin = -1;
    i = wallstart-1;
    do
    {
        i++;
        if (POINT2(i).x < minx)
        {
            minx = POINT2(i).x;
            themin = i;
        }
    }
    while ((wall[i].point2 != wallstart) && (i < MAXWALLS));

    x0 = wall[themin].x;
    y0 = wall[themin].y;
    x1 = POINT2(themin).x;
    y1 = POINT2(themin).y;
    x2 = POINT2(wall[themin].point2).x;
    y2 = POINT2(wall[themin].point2).y;

    if (y1 >= y2 && y1 <= y0) return(0);
    if (y1 >= y0 && y1 <= y2) return(1);

    tempint = (x0-x1)*(y2-y1) - (x2-x1)*(y0-y1);
    if (tempint < 0)
        return(0);
    else
        return(1);
}

static void flipwalls(int16_t numwalls, int16_t newnumwalls)
{
    int32_t i, j, nume;

    nume = newnumwalls-numwalls;

    for (i=numwalls; i<numwalls+(nume>>1); i++)
    {
        j = numwalls+newnumwalls-i-1;

        swapshort(&onextwall[i], &onextwall[j]);

        swaplong(&wall[i].x, &wall[j].x);
        swaplong(&wall[i].y, &wall[j].y);
    }
}

// returns number of points inserted (1; or 2 if wall had a nextwall)
//  *mapwallnum contains the new wallnum of the former (pre-insertpoint) *mapwallnum
//  (the new one can only be >= than the old one; ptr may be NULL if we don't care)
static int32_t insertpoint(int16_t linehighlight, int32_t dax, int32_t day, int32_t *mapwallnum)
{
    int16_t sucksect;
    int32_t i, j, k;
    uint32_t templenrepquot;

    j = linehighlight;
    sucksect = sectorofwall(j);
    templenrepquot = getlenbyrep(wallength(j), wall[j].xrepeat);

    sector[sucksect].wallnum++;
    for (i=sucksect+1; i<numsectors; i++)
        sector[i].wallptr++;

    if (mapwallnum && *mapwallnum >= j+1)
        (*mapwallnum)++;
    movewalls(j+1, +1);
    Bmemcpy(&wall[j+1], &wall[j], sizeof(walltype));
#ifdef YAX_ENABLE
    wall[j+1].cstat &= ~(1<<14);
#endif
    wall[j].point2 = j+1;
    wall[j+1].x = dax;
    wall[j+1].y = day;
    fixxrepeat(j, templenrepquot);
    AlignWallPoint2(j);
    fixxrepeat(j+1, templenrepquot);

    if (wall[j].nextwall >= 0)
    {
        k = wall[j].nextwall;
        templenrepquot = getlenbyrep(wallength(k), wall[k].xrepeat);

        sucksect = sectorofwall(k);

        sector[sucksect].wallnum++;
        for (i=sucksect+1; i<numsectors; i++)
            sector[i].wallptr++;

        if (mapwallnum && *mapwallnum >= k+1)
            (*mapwallnum)++;
        movewalls(k+1, +1);
        Bmemcpy(&wall[k+1], &wall[k], sizeof(walltype));
#ifdef YAX_ENABLE
        wall[k+1].cstat &= ~(1<<14);
#endif
        wall[k].point2 = k+1;
        wall[k+1].x = dax;
        wall[k+1].y = day;
        fixxrepeat(k, templenrepquot);
        AlignWallPoint2(k);
        fixxrepeat(k+1, templenrepquot);

        j = wall[k].nextwall;
        wall[j].nextwall = k+1;
        wall[j+1].nextwall = k;
        wall[k].nextwall = j+1;
        wall[k+1].nextwall = j;

        return 2;
    }

    return 1;
}

// runi: 0=check (forbidden), 1=prepare, 2=do!
static void deletepoint(int16_t point, int32_t runi)
{
    int32_t i, j, sucksect;

    Bassert(runi != 0);

    if (runi==1)
    {
        i = wall[point].nextwall;
        if (i >= 0)
        {
            NEXTWALL(i).nextwall = NEXTWALL(i).nextsector = -1;
            wall[i].nextwall = wall[i].nextsector = -1;
        }

        return;
    }

    sucksect = sectorofwall(point);

    sector[sucksect].wallnum--;
    for (i=sucksect+1; i<numsectors; i++)
        sector[i].wallptr--;

    j = lastwall(point);
    wall[j].point2 = wall[point].point2;

#if 0
    if (wall[j].nextwall >= 0)
    {
        NEXTWALL(j).nextwall = -1;
        NEXTWALL(j).nextsector = -1;
    }
    if (wall[point].nextwall >= 0)
    {
        NEXTWALL(point).nextwall = -1;
        NEXTWALL(point).nextsector = -1;
    }
#endif
    movewalls(point, -1);

//    checksectorpointer(j, sucksect);

    return;
}

static int32_t deletesector(int16_t sucksect)
{
    int32_t i, j, k, nextk, startwall, endwall;

    while (headspritesect[sucksect] >= 0)
        deletesprite(headspritesect[sucksect]);

    startwall = sector[sucksect].wallptr;
    endwall = startwall + sector[sucksect].wallnum - 1;
    j = sector[sucksect].wallnum;

#ifdef YAX_ENABLE
    yax_setbunches(sucksect, -1, -1);
#endif

    for (i=sucksect; i<numsectors-1; i++)
    {
        k = headspritesect[i+1];
        while (k != -1)
        {
            nextk = nextspritesect[k];
            changespritesect(k, i);
            k = nextk;
        }

        Bmemcpy(&sector[i], &sector[i+1], sizeof(sectortype));
        sector[i].wallptr -= j;
    }
    numsectors--;

    for (i=startwall; i<=endwall; i++)
    {
        if (wall[i].nextwall >= 0)
        {
            NEXTWALL(i).nextwall = -1;
            NEXTWALL(i).nextsector = -1;
        }
    }

    movewalls(startwall, -j);
    for (i=0; i<numwalls; i++)
        if (wall[i].nextwall >= startwall)
            wall[i].nextsector--;

    return(0);
}

int32_t fixspritesectors(void)
{
    int32_t i, j, dax, day, cz, fz;
    int32_t numfixedsprites = 0, printfirsttime = 0;

    for (i=numsectors-1; i>=0; i--)
        if (sector[i].wallnum <= 0 || sector[i].wallptr >= numwalls)
        {
            deletesector(i);
            initprintf("Deleted sector %d which had corrupt .wallnum or .wallptr\n", i);
        }

    if (m32_script_expertmode)
        return 0;

    for (i=0; i<MAXSPRITES; i++)
        if (sprite[i].statnum < MAXSTATUS)
        {
            dax = sprite[i].x;
            day = sprite[i].y;

            if (inside(dax,day,sprite[i].sectnum) != 1)
            {
                spriteoncfz(i, &cz, &fz);

                for (j=0; j<numsectors; j++)
                {
                    if (inside(dax,day, j) != 1)
                        continue;

                    if (cz <= sprite[i].z && sprite[i].z <= fz)
                    {
                        if (fixmaponsave_sprites || (sprite[i].sectnum < 0 || sprite[i].sectnum >= numsectors))
                        {
                            numfixedsprites++;

                            if (printfirsttime == 0)
                            {
                                initprintf("--------------------\n");
                                printfirsttime = 1;
                            }
                            initprintf("Changed sectnum of sprite %d from %d to %d\n", i, sprite[i].sectnum, j);
                            changespritesect(i, j);
                        }
                        break;
                    }
                }
            }
        }

    return numfixedsprites;
}

static int32_t movewalls(int32_t start, int32_t offs)
{
    int32_t i;

    if (offs < 0)  //Delete
    {
        for (i=start; i<numwalls+offs; i++)
            Bmemcpy(&wall[i], &wall[i-offs], sizeof(walltype));
    }
    else if (offs > 0)  //Insert
    {
        for (i=numwalls+offs-1; i>=start+offs; i--)
            Bmemcpy(&wall[i], &wall[i-offs], sizeof(walltype));

        if (ovh.bak_wallsdrawn > 0)
        {
            if (ovh.suckwall >= start)
                ovh.suckwall += offs;
            if (ovh.splitstartwall >= start)
                ovh.splitstartwall += offs;
        }
    }

    numwalls += offs;
    for (i=0; i<numwalls; i++)
    {
        if (wall[i].nextwall >= start) wall[i].nextwall += offs;
        if (wall[i].point2 >= start) wall[i].point2 += offs;
    }
#ifdef YAX_ENABLE
    yax_tweakwalls(start, offs);
#endif

    return(0);
}

int32_t wallength(int16_t i)
{
    int64_t dax = POINT2(i).x - wall[i].x;
    int64_t day = POINT2(i).y - wall[i].y;
#if 1 //def POLYMOST
    int64_t hypsq = dax*dax + day*day;
    if (hypsq > (int64_t)INT32_MAX)
        return (int32_t)sqrt((double)hypsq);
    else
        return ksqrt((int32_t)hypsq);
#else
    return ksqrt(dax*dax + day*day);
#endif
}

void fixrepeats(int16_t i)
{
    int32_t dist = wallength(i);
    int32_t day = wall[i].yrepeat;

    wall[i].xrepeat = clamp(mulscale10(dist,day), 1, 255);
}

uint32_t getlenbyrep(int32_t len, int32_t repeat)
{
    if (repeat <= 0)
        return ((uint32_t)len)<<12;

    return divscale12(len, repeat);
}

void fixxrepeat(int16_t wallnum, uint32_t lenrepquot)  // lenrepquot: divscale12(wallength,xrepeat)
{
    if (lenrepquot != 0)
    {
        uint32_t res = (((wallength(wallnum)<<12)+(1<<11))/lenrepquot);
        wall[wallnum].xrepeat = clamp(res, 1, 255);
    }
}


int32_t overridepm16y = -1;

void clearmidstatbar16(void)
{
    int32_t y = overridepm16y<0 ? STATUS2DSIZ : overridepm16y;

    begindrawing();
    CLEARLINES2D(ydim-y+25, STATUS2DSIZ+2-(25<<1), 0);
    enddrawing();
}

static void clearministatbar16(void)
{
    int32_t i, col = whitecol - 21;
//    static const char *tempbuf = "Mapster32" " " VERSION;
    char tempbuf[16];

    begindrawing();

    for (i=ydim-STATUS2DSIZ2; i<ydim; i++)
    {
//        drawline256(0, i<<12, xdim<<12, i<<12, col);
        CLEARLINES2D(i, 1, (col<<24)|(col<<16)|(col<<8)|col);

        col--;
        if (col <= 0) break;
    }

    CLEARLINES2D(i, ydim-i, 0);

    if (xdim >= 800)
    {
        Bsprintf(tempbuf, "Mapster32 %s", ExtGetVer());
        printext16(xdim2d-(Bstrlen(tempbuf)<<3)-3, ydim2d-STATUS2DSIZ2+10, editorcolors[4],-1, tempbuf, 0);
        printext16(xdim2d-(Bstrlen(tempbuf)<<3)-2, ydim2d-STATUS2DSIZ2+9, editorcolors[12],-1, tempbuf, 0);
    }

    enddrawing();
}

// startwall has to be the starting wall of a loop!
static int16_t loopinside(int32_t x, int32_t y, int16_t startwall)
{
    int32_t x1, y1, x2, y2;
    int16_t i, cnt;

    cnt = clockdir(startwall);
    i = startwall;
    do
    {
        x1 = wall[i].x;
        x2 = POINT2(i).x;

        if (x1 >= x || x2 >= x)
        {
            y1 = wall[i].y;
            y2 = POINT2(i).y;

            if (y1 > y2)
            {
                swaplong(&x1, &x2);
                swaplong(&y1, &y2);
            }
            if (y1 <= y && y2 > y)
                if (x1*(y-y2)+x2*(y1-y) <= x*(y1-y2))
                    cnt ^= 1;
        }
        i = wall[i].point2;
    }
    while (i != startwall);

    return(cnt);
}

#if 0
static int32_t numloopsofsector(int16_t sectnum)
{
    int32_t i, numloops, startwall, endwall;

    numloops = 0;
    startwall = sector[sectnum].wallptr;
    endwall = startwall + sector[sectnum].wallnum;
    for (i=startwall; i<endwall; i++)
        if (wall[i].point2 < i) numloops++;
    return(numloops);
}
#endif

int32_t getnumber_internal1(char ch, int32_t *danumptr, int32_t maxnumber, char sign)
{
    int32_t danum = *danumptr;

    if (ch >= '0' && ch <= '9')
    {
        int64_t nbig;
        if (danum >= 0)
        {
            nbig = ((int64_t)danum*10)+(ch-'0');
            if (nbig <= (int64_t)maxnumber) danum = nbig;
        }
        else if (sign) // this extra check isn't hurting anything
        {
            nbig = ((int64_t)danum*10)-(ch-'0');
            if (nbig >= (int64_t)(-maxnumber)) danum = nbig;
        }
    }
    else if (ch == 8 || ch == 127)  	// backspace
    {
        danum /= 10;
    }
    else if (ch == 13)
    {
        return 1;
    }
    else if (ch == '-' && sign)  	// negate
    {
        danum = -danum;
    }

    *danumptr = danum;
    return 0;
}

int32_t getnumber_autocomplete(const char *namestart, char ch, int32_t *danum, int32_t flags)
{
    if (flags!=1 && flags!=2)
        return 0;

    if (flags==2 && *danum<0)
        return 0;

    if (isalpha(ch))
    {
        char b[2];
        const char *gotstr;
        int32_t i, diddel;

        b[0] = ch;
        b[1] = 0;
        gotstr = getstring_simple(namestart, b, (flags==1)?sizeof(names[0])-1:TAGLAB_MAX-1, flags);
        if (!gotstr || !gotstr[0])
            return 0;

        if (flags==1)
        {
            for (i=0; i<MAXTILES; i++)
                if (!Bstrcasecmp(names[i], gotstr))
                {
                    *danum = i;
                    return 1;
                }
        }
        else
        {
            i = taglab_gettag(gotstr);
//initprintf("taglab: s=%s, i=%d\n",gotstr,i);
            if (i > 0)
            {
                *danum = i;
                return 1;
            }
            else
            {
                // insert new tag
                if (*danum > 0 && *danum<32768)
                {
                    diddel = taglab_add(gotstr, *danum);
                    message("Added label \"%s\" for tag %d%s%s", gotstr, *danum,
                            diddel?", deleting old ":"",
                            (!diddel)?"":(diddel==1?"label":"tag"));
                    return 1;
                }
                else if (*danum==0)
                {
                    i = taglab_getnextfreetag();
                    if (i >= 1)
                    {
                        *danum = i;
                        diddel = taglab_add(gotstr, *danum);
                        message("%sadded label \"%s\" for tag %d%s%s",
                                diddel?"Auto-":"Automatically ", gotstr, *danum,
                                diddel?", deleting old ":"",
                                (!diddel)?"":(diddel==1?"label":"tag"));
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

// sign is now used for more than one flag (also _getnumber256):
//  1: sign
//  2: autocomplete names
//  4: autocomplete taglabels
//  8: return -1 if cancelled
int32_t _getnumber16(const char *namestart, int32_t num, int32_t maxnumber, char sign, void *(func)(int32_t))
{
    char buffer[80], ournamestart[80-17], ch;
    int32_t n, danum, oldnum;
    uint8_t flags = (sign&(2|4|8))>>1;
    sign &= 1;

    danum = num;
    oldnum = danum;

    // need to have 4+11+2==17 chars room at the end
    // ("^011", max. string length of an int32, "_ ")
    Bstrncpyz(ournamestart, namestart, sizeof(ournamestart));

    bflushchars();
    while (keystatus[0x1] == 0)
    {
        if (handleevents())
            quitevent = 0;

        idle();
        ch = bgetchar();

        Bsprintf(buffer, "%s^011%d", ournamestart, danum);
        n = Bstrlen(buffer);  // maximum is 62+4+11 == 77
        if (totalclock & 32)
            Bstrcat(buffer,"_ ");
        // max strlen now 79
        _printmessage16("%s", buffer);

        if (func != NULL)
        {
            Bsnprintf(buffer, sizeof(buffer), "%s", (char *)func(danum));
            // printext16(200L-24, ydim-STATUS2DSIZ+20L, editorcolors[9], editorcolors[0], buffer, 0);
            printext16(n<<3, ydim-STATUS2DSIZ+128, editorcolors[11], -1, buffer,0);
        }

        showframe(1);

        n = 0;
        if (getnumber_internal1(ch, &danum, maxnumber, sign) ||
            (n=getnumber_autocomplete(ournamestart, ch, &danum, flags&(1+2))))
        {
            if (flags==1 || n==0)
                printmessage16("%s", buffer);
            if (danum != oldnum)
                asksave = 1;
            oldnum = danum;
            break;
        }
    }

    if (keystatus[0x1] && (flags&4))
        oldnum = -1;

    clearkeys();

    return oldnum;
}

static void getnumber_clearline(void)
{
    char cbuf[128];
    int32_t i;
    for (i=0; i<min(xdim>>3, (signed)sizeof(cbuf)-1); i++)
        cbuf[i] = ' ';
    cbuf[i] = 0;
    printext256(0, 0, whitecol, 0, cbuf, 0);
}

// sign: |16: don't draw scene
int32_t _getnumber256(const char *namestart, int32_t num, int32_t maxnumber, char sign, void *(func)(int32_t))
{
    char buffer[80], ournamestart[80-13], ch;
    int32_t danum, oldnum;
    uint8_t flags = (sign&(2|4|8|16))>>1;
    sign &= 1;

    danum = num;
    oldnum = danum;

    // need to have 11+2==13 chars room at the end
    // (max. string length of an int32, "_ ")
    Bstrncpyz(ournamestart, namestart, sizeof(ournamestart));

    bflushchars();
    while (keystatus[0x1] == 0)
    {
        if (handleevents())
            quitevent = 0;

        if ((flags&8)==0)
            M32_DrawRoomsAndMasks();

        ch = bgetchar();
        if (keystatus[0x1])
            break;
        clearkeys();

        mouseb = 0;
        searchx = osearchx;
        searchy = osearchy;

        if ((flags&8)==0)
            ExtCheckKeys();

        getnumber_clearline();

        Bsprintf(buffer,"%s%d",ournamestart,danum);
        // max strlen now 66+11==77
        if (totalclock & 32)
            Bstrcat(buffer,"_ ");
        // max strlen now 79
        printmessage256(0, 0, buffer);
        if (func != NULL)
        {
            Bsnprintf(buffer, sizeof(buffer), "%s", (char *)func(danum));
            printmessage256(0, 9, buffer);
        }

        showframe(1);

        if (getnumber_internal1(ch, &danum, maxnumber, sign) ||
            getnumber_autocomplete(ournamestart, ch, &danum, flags&(1+2)))
        {
            if (danum != oldnum)
                asksave = 1;
            oldnum = danum;
            break;
        }
    }

    if (keystatus[0x1] && (flags&4))
        oldnum = -1;

    clearkeys();

    lockclock = totalclock;  //Reset timing

    return oldnum;
}

// querystr: e.g. "Name: ", must be !=NULL
// defaultstr: can be NULL
//  NO overflow checks are done when copying them!
// maxlen: maximum length of entry string, if ==1, enter single char
// completion: 0=none, 1=names[][], 2=taglabels
const char *getstring_simple(const char *querystr, const char *defaultstr, int32_t maxlen, int32_t completion)
{
    static char buf[128];
    int32_t ei=0, qrylen=0, maxidx, havecompl=0;
    char ch;

    bflushchars();
    clearkeys();

    Bmemset(buf, 0, sizeof(buf));

    qrylen = Bstrlen(querystr);
    Bmemcpy(buf, querystr, qrylen);

    if (maxlen==0)
        maxlen = 64;

    maxidx = min((signed)sizeof(buf), xdim>>3);

    ei = qrylen;

    if (defaultstr)
    {
        int32_t deflen = Bstrlen(defaultstr);
        Bmemcpy(&buf[ei], defaultstr, deflen);
        ei += deflen;
    }

    buf[ei] = '_';
    buf[ei+1] = 0;

    while (1)
    {
        if (qsetmode==200)
            getnumber_clearline();

        if (qsetmode==200)
            printext256(0, 0, whitecol, 0, buf, 0);
        else
            _printmessage16("%s", buf);

        showframe(1);

        if (handleevents())
            quitevent = 0;

        idle();
        ch = bgetchar();

        if (ch==13)
        {
            if (maxlen != 1)
                buf[ei] = 0;
            break;
        }
        else if (keystatus[1])
        {
            clearkeys();
            return completion ? NULL : defaultstr;
        }

        if (maxlen!=1)
        {
            // blink...
            if (totalclock&32)
            {
                buf[ei] = '_';
                buf[ei+1] = 0;
            }
            else
            {
                buf[ei] = 0;
            }

            if (ei>qrylen && (ch==8 || ch==127))
            {
                buf[ei--] = 0;
                buf[ei] = '_';
                havecompl = 0;
            }
            else if (ei<maxidx-2 && ei-qrylen<maxlen && isprint(ch))
            {
                if (completion==2 && ch==' ')
                    ch = '_';
                buf[ei++] = ch;
                buf[ei] = '_';
                buf[ei+1] = 0;
            }

            if (completion && ((ei>qrylen && ch==9) || havecompl))  // tab: maybe do auto-completion
            {
                char cmpbuf[128];
                char completions[3][16];
                const char *cmpstr;
                int32_t len=ei-qrylen, i, j, k=len, first=1, numcompl=0;

                Bmemcpy(cmpbuf, &buf[qrylen], len);
                cmpbuf[len] = 0;

                for (i=(completion!=1); i<((completion==1)?MAXTILES:32768); i++)
                {
                    cmpstr = (completion==1) ? names[i] : taglab_getlabel(i);
                    if (!cmpstr)
                        continue;

                    if (Bstrncasecmp(cmpbuf, cmpstr, len) || Bstrlen(cmpstr)==(unsigned)len)  // compare the prefix
                        continue;

                    if (ch==9)
                    {
                        if (first)
                        {
                            Bstrncpy(cmpbuf+len, cmpstr+len, sizeof(cmpbuf)-len);
                            cmpbuf[sizeof(cmpbuf)-1] = 0;
                            first = 0;
                        }
                        else
                        {
                            for (k=len; cmpstr[k] && cmpbuf[k] && Btolower(cmpstr[k])==Btolower(cmpbuf[k]); k++)
                                /* nop */;
                            cmpbuf[k] = 0;
                        }
                    }

                    if (numcompl<3)
                    {
                        Bstrncpyz(completions[numcompl], cmpstr+len, sizeof(completions[0]));

                        for (k=0; completions[numcompl][k]; k++)
                            completions[numcompl][k] = Btolower(completions[numcompl][k]);
                        numcompl++;
                    }

                    for (k=len; cmpbuf[k]; k++)
                        cmpbuf[k] = Btolower(cmpbuf[k]);
                }

                ei = qrylen;
                for (i=0; i<k && i<maxlen && ei<maxidx-2; i++)
                    buf[ei++] = cmpbuf[i];

                if (k==len && numcompl>0)  // no chars autocompleted/completion request
                {
                    buf[ei] = '{';
                    buf[ei+1] = 0;

                    i = ei+1;
                    for (k=0; k<numcompl; k++)
                    {
                        j = 0;
                        while (i<maxidx-1 && completions[k][j])
                            buf[i++] = completions[k][j++];
                        if (i<maxidx-1)
                            buf[i++] = (k==numcompl-1) ? '}' : ',';
                    }
                    buf[i] = 0;

                    havecompl = 1;
                }
                else
                {
                    buf[ei] = '_';
                    buf[ei+1] = 0;
                }
            }
        }
        else
        {
            if (isalnum(ch) || ch==' ')
                buf[ei] = Btoupper(ch);
        }
    }

    clearkeys();

    return buf+qrylen;
}

static int32_t getfilenames(const char *path, const char *kind)
{
    const int32_t addflags = (!pathsearchmode && grponlymode ? CACHE1D_OPT_NOSTACK : 0);

    fnlist_getnames(&fnlist, path, kind, addflags|CACHE1D_FIND_DRIVE, addflags);

    finddirshigh = fnlist.finddirs;
    findfileshigh = fnlist.findfiles;
    currentlist = (findfileshigh != NULL);

    return(0);
}

// vvv PK ------------------------------------
// copied off menuselect

static const char *g_oldpath=NULL;
static int32_t menuselect_auto(int32_t direction) // 20080104: jump to next (direction!=0) or prev (direction==0) file
{
    const char *boardbasename;

    if (!g_oldpath)
        return -3;  // not inited
    else
        Bmemcpy(selectedboardfilename, g_oldpath, BMAX_PATH);

    if (pathsearchmode)
        Bcanonicalisefilename(selectedboardfilename, 1);  // clips off the last token and compresses relative path
    else
        Bcorrectfilename(selectedboardfilename, 1);

    getfilenames(selectedboardfilename, "*.map");
    if (fnlist.numfiles==0)
        return -2;

    boardbasename = Bstrrchr(boardfilename,'/'); // PK
    if (!boardbasename)
        boardbasename=boardfilename;
    else
        boardbasename++;

    for (; findfileshigh; findfileshigh=findfileshigh->next)
        if (!Bstrcmp(findfileshigh->name,boardbasename))
            break;

    if (!findfileshigh)
        findfileshigh = fnlist.findfiles;

    if (direction)
    {
        if (findfileshigh->next)
            findfileshigh=findfileshigh->next;
        else
            return -1;
    }
    else
    {
        if (findfileshigh->prev)
            findfileshigh=findfileshigh->prev;
        else
            return -1;
    }

    Bstrcat(selectedboardfilename, findfileshigh->name);

    return(0);
}
// ^^^ PK ------------------------------------

static int32_t menuselect(void)
{
    int32_t listsize;
    int32_t i;
    char ch, buffer[96], /*PK*/ *boardbasename;
    static char oldpath[BMAX_PATH];
    CACHE1D_FIND_REC *dir;
    int32_t bakpathsearchmode = pathsearchmode;

    g_oldpath=oldpath; //PK: need it in menuselect_auto

    Bstrcpy(selectedboardfilename, oldpath);
    if (pathsearchmode)
        Bcanonicalisefilename(selectedboardfilename, 1);		// clips off the last token and compresses relative path
    else
        Bcorrectfilename(selectedboardfilename, 1);

    getfilenames(selectedboardfilename, "*.map");

    // PK 20080103: start with last selected map
    boardbasename = Bstrrchr(boardfilename,'/');
    if (!boardbasename)
        boardbasename=boardfilename;
    else
        boardbasename++;

    for (; findfileshigh; findfileshigh=findfileshigh->next)
        if (!Bstrcmp(findfileshigh->name,boardbasename))
            break;

    if (!findfileshigh)
        findfileshigh = fnlist.findfiles;

    _printmessage16("Select map file with arrow keys and enter.");

    ydim16 = ydim-STATUS2DSIZ2;
    listsize = (ydim16-32)/9;

    do
    {
        begindrawing(); //{{{

        CLEARLINES2D(0, ydim16, 0);

        if (pathsearchmode)
            Bstrcpy(buffer,"Local filesystem mode.  Ctrl-F: game filesystem");
        else
            Bsprintf(buffer,"Game filesystem %smode.  Ctrl-F: local filesystem, Ctrl-G: %s",
                     grponlymode?"GRP-only ":"", grponlymode?"all files":"GRP contents only");

        printext16(halfxdim16-(8*Bstrlen(buffer)/2), 4, editorcolors[12],editorcolors[0],buffer,0);

        Bsnprintf(buffer,sizeof(buffer)-1,"(%d dirs, %d files) %s",
                  fnlist.numdirs, fnlist.numfiles, selectedboardfilename);
        buffer[sizeof(buffer)-1] = 0;

        printext16(8,ydim16-8-1,editorcolors[8],editorcolors[0],buffer,0);

        if (finddirshigh)
        {
            dir = finddirshigh;
            for (i=(listsize/2)-1; i>=0; i--)
            {
                if (!dir->prev) break;
                else dir=dir->prev;
            }
            for (i=0; ((i<listsize) && dir); i++, dir=dir->next)
            {
                int32_t c = (dir->type == CACHE1D_FIND_DIR ? 2 : 3); //PK
                Bmemset(buffer,0,sizeof(buffer));
                Bstrncpy(buffer,dir->name,25);
                if (Bstrlen(buffer) == 25)
                    buffer[21] = buffer[22] = buffer[23] = '.', buffer[24] = 0;
                if (dir == finddirshigh)
                {
                    if (currentlist == 0) printext16(8,16+9*i,editorcolors[c|8],editorcolors[0],"->",0);
                    printext16(32,16+9*i,editorcolors[c|8],editorcolors[0],buffer,0);
                }
                else
                {
                    printext16(32,16+9*i,editorcolors[c],editorcolors[0],buffer,0);
                }
            }
        }

        if (findfileshigh)
        {
            dir = findfileshigh;
            for (i=(listsize/2)-1; i>=0; i--)
            {
                if (!dir->prev) break;
                else dir=dir->prev;
            }
            for (i=0; ((i<listsize) && dir); i++, dir=dir->next)
            {
                if (dir == findfileshigh)
                {
                    if (currentlist == 1) printext16(240,16+9*i,editorcolors[7|8],editorcolors[0],"->",0);
                    printext16(240+24,16+9*i,editorcolors[7|8],editorcolors[0],dir->name,0);
                }
                else
                {
                    printext16(240+24,16+9*i,editorcolors[7],editorcolors[0],dir->name,0);
                }
            }
        }

        enddrawing(); //}}}
        showframe(1);

        keystatus[0xcb] = 0;
        keystatus[0xcd] = 0;
        keystatus[0xc8] = 0;
        keystatus[0xd0] = 0;
        keystatus[0x1c] = 0;	//enter
        keystatus[0xf] = 0;		//tab
        keystatus[1] = 0;		//esc
        ch = 0;                      //Interesting fakery of ch = getch()
        while (ch == 0)
        {
            if (handleevents())
                if (quitevent)
                {
                    keystatus[1] = 1;
                    quitevent = 0;
                }

            idle();

            ch = bgetchar();

            {
                // JBF 20040208: seek to first name matching pressed character
                CACHE1D_FIND_REC *seeker = currentlist ? fnlist.findfiles : fnlist.finddirs;
                if (keystatus[0xc7]||keystatus[0xcf]) // home/end
                {
                    while (keystatus[0xcf]?seeker->next:seeker->prev)
                        seeker = keystatus[0xcf]?seeker->next:seeker->prev;
                    if (seeker)
                    {
                        if (currentlist) findfileshigh = seeker;
                        else finddirshigh = seeker;
                    }
                    ch = keystatus[0xcf]?80:72;
                    keystatus[0xc7] = keystatus[0xcf] = 0;
                }
                else if (keystatus[0xc9]|keystatus[0xd1]) // page up/down
                {
                    seeker = currentlist?findfileshigh:finddirshigh;
                    i = (ydim2d-STATUS2DSIZ2-48)>>5/*3*/;  //PK

                    while (i>0)
                    {
                        if (keystatus[0xd1]?seeker->next:seeker->prev)
                            seeker = keystatus[0xd1]?seeker->next:seeker->prev;
                        i--;
                    }
                    if (seeker)
                    {
                        if (currentlist) findfileshigh = seeker;
                        else finddirshigh = seeker;
                    }
                    ch = keystatus[0xd1]?80:72;
                    keystatus[0xc9] = keystatus[0xd1] = 0;
                }
                else
                {
                    char ch2;
                    if (ch > 0 && ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                                   (ch >= '0' && ch <= '9') || (ch=='_')))
                    {
#ifdef _WIN32
                        if (ch >= 'a') ch -= ('a'-'A');
#endif
                        while (seeker)
                        {
                            ch2 = seeker->name[0];
#ifdef _WIN32
                            if (ch2 >= 'a' && ch2 <= 'z') ch2 -= ('a'-'A');
#endif
                            if (ch2 == ch) break;
                            seeker = seeker->next;
                        }
                        if (seeker)
                        {
                            if (currentlist) findfileshigh = seeker;
                            else finddirshigh = seeker;
                            continue;
                        }
                    }
                }
            }
            if (keystatus[0xcb]) ch = 9;		// left arr
            if (keystatus[0xcd]) ch = 9;		// right arr
            if (keystatus[0xc8]) ch = 72;   	// up arr
            if (keystatus[0xd0]) ch = 80;   	// down arr
        }

        if (ch==6)  // Ctrl-F
        {
            currentlist = 0;
            pathsearchmode = 1-pathsearchmode;
            if (pathsearchmode)
            {
                Bstrcpy(selectedboardfilename, "");
                Bcanonicalisefilename(selectedboardfilename, 0);
            }
            else Bstrcpy(selectedboardfilename, "/");

            getfilenames(selectedboardfilename, "*.map");
            Bstrcpy(oldpath,selectedboardfilename);
        }
        else if (ch==7)  // Ctrl-G
        {
            if (!pathsearchmode)
            {
                grponlymode = 1-grponlymode;
                getfilenames(selectedboardfilename, "*.map");
                Bstrcpy(oldpath,selectedboardfilename);
            }
        }
        else if (ch == 9)
        {
            if ((currentlist == 0 && fnlist.findfiles) || (currentlist == 1 && fnlist.finddirs))
                currentlist = 1-currentlist;
        }
        else if (keystatus[0xc8] /*(ch == 75) || (ch == 72)*/)
        {
            if (currentlist == 0)
            {
                if (finddirshigh && finddirshigh->prev)
                    finddirshigh = finddirshigh->prev;
            }
            else
            {
                if (findfileshigh && findfileshigh->prev)
                    findfileshigh = findfileshigh->prev;
            }
        }
        else if (keystatus[0xd0] /*(ch == 77) || (ch == 80)*/)
        {
            if (currentlist == 0)
            {
                if (finddirshigh && finddirshigh->next)
                    finddirshigh = finddirshigh->next;
            }
            else
            {
                if (findfileshigh && findfileshigh->next)
                    findfileshigh = findfileshigh->next;
            }
        }
        else if ((ch == 13) && (currentlist == 0))
        {
            if (finddirshigh->type == CACHE1D_FIND_DRIVE)
                Bstrcpy(selectedboardfilename, finddirshigh->name);
            else
                Bstrcat(selectedboardfilename, finddirshigh->name);

            Bstrcat(selectedboardfilename, "/");

            if (pathsearchmode)
                Bcanonicalisefilename(selectedboardfilename, 1);
            else
                Bcorrectfilename(selectedboardfilename, 1);

            Bstrcpy(oldpath,selectedboardfilename);
            //printf("Changing directories to: %s\n", selectedboardfilename);

            getfilenames(selectedboardfilename, "*.map");
            ch = 0;

            begindrawing();
            CLEARLINES2D(0, ydim16, 0);
            enddrawing();
            showframe(1);
        }

        if (ch == 13 && !findfileshigh) ch = 0;
    }
    while ((ch != 13) && (ch != 27));

    if (ch == 13)
    {
        Bstrcat(selectedboardfilename, findfileshigh->name);
        //printf("Selected file: %s\n", selectedboardfilename);

        return(0);
    }

    pathsearchmode = bakpathsearchmode;

    return(-1);
}

static inline int32_t imod(int32_t a, int32_t b)
{
    if (a >= 0) return(a%b);
    return(((a+1)%b)+b-1);
}

int32_t fillsector(int16_t sectnum, int32_t fillcolor)
{
    int32_t x1, x2, y1, y2, sy, y, daminy;
    int32_t lborder, rborder, uborder, dborder, miny, maxy, dax;
    int16_t z, zz, startwall, endwall, fillcnt;

    char col;

    if (fillcolor < 0)
        col = 159-klabs(sintable[((totalclock<<3)&2047)]>>11);
    else
        col = fillcolor;

    lborder = 0; rborder = xdim;
    y = OSD_GetRowsCur();
    uborder = (y>=0)?(y+1)*8:0; dborder = ydim16-STATUS2DSIZ2;

    if (sectnum == -1)
        return 0;

    miny = dborder-1;
    maxy = uborder;

    startwall = sector[sectnum].wallptr;
    endwall = startwall + sector[sectnum].wallnum - 1;
    for (z=startwall; z<=endwall; z++)
    {
        screencoords(&x1,&y1, wall[z].x-pos.x,wall[z].y-pos.y, zoom);
        if (m32_sideview)
            y1 += getscreenvdisp(getflorzofslope(sectnum,wall[z].x,wall[z].y)-pos.z, zoom);

        x1 += halfxdim16;
        y1 += midydim16;

        if (m32_sideview)
        {
            tempxyar[z][0] = x1;
            tempxyar[z][1] = y1;
        }

        miny = min(miny, y1);
        maxy = max(maxy, y1);
    }

    if (miny < uborder) miny = uborder;
    if (maxy >= dborder) maxy = dborder-1;

    daminy = miny+2 - imod(miny+2,3);
    if (sector[sectnum].floorz > minhlsectorfloorz)
        daminy++;

//+((totalclock>>2)&3)
    for (sy=daminy; sy<=maxy; sy+=3)	// JBF 20040116: numframes%3 -> (totalclock>>2)&3
    {
        y = pos.y + ((sy-midydim16)<<14)/zoom;

        fillist[0] = lborder; fillcnt = 1;
        for (z=startwall; z<=endwall; z++)
        {
            if (m32_sideview)
            {
                x1 = tempxyar[z][0];
                y1 = tempxyar[z][1];
                x2 = tempxyar[wall[z].point2][0];
                y2 = tempxyar[wall[z].point2][1];

                if (y1 > y2)
                {
                    swaplong(&x1, &x2);
                    swaplong(&y1, &y2);
                }

                if (y1 <= sy && sy < y2)
                {
                    if (fillcnt == sizeof(fillist)/sizeof(fillist[0]))
                        break;

                    x1 += scale(sy-y1, x2-x1, y2-y1);
                    fillist[fillcnt++] = x1;
                }
            }
            else
            {
                x1 = wall[z].x; x2 = POINT2(z).x;
                y1 = wall[z].y; y2 = POINT2(z).y;

                if (y1 > y2)
                {
                    swaplong(&x1, &x2);
                    swaplong(&y1, &y2);
                }

                if (y1 <= y && y < y2)
                    //if (x1*(y-y2) + x2*(y1-y) <= 0)
                {
                    dax = x1 + scale(y-y1, x2-x1, y2-y1);
                    dax = halfxdim16 + (((dax-pos.x)*zoom)>>14);
                    if (dax >= lborder)
                    {
                        if (fillcnt == sizeof(fillist)/sizeof(fillist[0]))
                            break;

                        fillist[fillcnt++] = dax;
                    }
                }
            }
        }

        if (fillcnt > 0)
        {
            for (z=1; z<fillcnt; z++)
                for (zz=0; zz<z; zz++)
                    if (fillist[z] < fillist[zz])
                        swaplong(&fillist[z], &fillist[zz]);

            for (z=(fillcnt&1); z<fillcnt-1; z+=2)
            {
                if (fillist[z] > rborder)
                    break;
                if (fillist[z+1] > rborder)
                    fillist[z+1] = rborder;

                drawline16(fillist[z]+1,sy, fillist[z+1]-1,sy, col);  //editorcolors[fillcolor]
            }
        }
    }

    return(0);
}

static int16_t whitelinescan(int16_t sucksect, int16_t dalinehighlight)
{
    int32_t i, j, k;
    int16_t tnewnumwalls;

    if (numsectors >= MAXSECTORS)
        return MAXWALLS+1;

    Bmemcpy(&sector[numsectors], &sector[sucksect], sizeof(sectortype));
    sector[numsectors].wallptr = numwalls;
    sector[numsectors].wallnum = 0;

    i = dalinehighlight;
    tnewnumwalls = numwalls;
    do
    {
        if (tnewnumwalls >= MAXWALLS)
            return MAXWALLS+1;

        j = lastwall(i);
        if (wall[j].nextwall >= 0)
        {
            j = wall[j].point2;
            for (k=0; k<numwalls; k++)
            {
                if (POINT2(k).x == wall[j].x && POINT2(k).y == wall[j].y)
                    if (wall[k].nextwall == -1)
                    {
                        j = k;
                        break;
                    }
            }
        }

        Bmemcpy(&wall[tnewnumwalls], &wall[i], sizeof(walltype));

        wall[tnewnumwalls].nextwall = j;
        wall[tnewnumwalls].nextsector = sectorofwall(j);

        tnewnumwalls++;
        sector[numsectors].wallnum++;

        i = j;
    }
    while (i != dalinehighlight);

    for (i=numwalls; i<tnewnumwalls-1; i++)
        wall[i].point2 = i+1;
    wall[tnewnumwalls-1].point2 = numwalls;

    if (clockdir(numwalls) == CLOCKDIR_CCW)
        return(-1);
    else
        return(tnewnumwalls);
}

int32_t loadnames(const char *namesfile, int8_t root)
{
    char buffer[1024], *p, *name, *number, *endptr;
    static int32_t syms=0;
    int32_t num, line=0, a, comment=0;
    int8_t quotes=0, anglebrackets=0;
    BFILE *fp;

    Bstrncpyz(buffer, namesfile, sizeof(buffer));

    fp = fopenfrompath(buffer,"r");
    if (!fp)
    {
        p = buffer;
        while (*p)
        {
            *p = Btolower(*p);
            p++;
        }

        if ((fp = fopenfrompath(buffer,"r")) == NULL)
        {
            initprintf("Failed to open %s\n", buffer);
            return -1;
        }
    }

    if (root)
        //clearbufbyte(names, sizeof(names), 0);
        Bmemset(names,0,sizeof(names));

    initprintf("Loading %s\n", buffer);

    while (Bfgets(buffer, 1024, fp))
    {
        a = Bstrlen(buffer);
        if (a >= 1)
        {
            if (a > 1)
                if (buffer[a-2] == '\r') buffer[a-2] = 0;
            if (buffer[a-1] == '\n') buffer[a-1] = 0;
        }

        p = buffer;
        line++;
        while (*p == 32) p++; // 32 == 0x20 == space
        if (*p == 0) continue; // blank line

        if (*p == '#') // make '#' optional for compatibility
            p++;

        if (*p == '/')
        {
            if (*(p+1) == '/') continue; // comment
            if (*(p+1) == '*') {comment++; continue;} /* comment */
        }
        else if (*p == '*' && p[1] == '/')
        {
            comment--;
            continue;
        }
        else if (comment)
            continue;
        else if (!comment)
        {
            while (*p == 32) p++;
            if (*p == 0) continue; // null directive

            if (!Bstrncmp(p, "define ", 7))
            {
                // #define_...
                p += 7;
                while (*p == 32) p++;
                if (*p == 0)
                {
                    initprintf("Error: Malformed #define at line %d\n", line-1);
                    continue;
                }

                name = p;
                while (*p != 32 && *p != 0) p++;
                if (*p == 32)
                {
                    *(p++) = 0;
                    while (*p == 32) p++;
                    if (*p == 0)  	// #define_NAME with no number
                    {
                        initprintf("Error: No number given for name \"%s\" (line %d)\n", name, line-1);
                        continue;
                    }

                    number = p;
                    while (*p != 0 && *p != 32) p++;
                    *p = 0;

                    // add to list
                    num = Bstrtol(number, &endptr, 10);
                    if (*endptr != 0)
                    {
                        p = endptr;
                        goto badline;
                    }
                    //printf("Grokked \"%s\" -> \"%d\"\n", name, num);
                    if (num < 0 || num >= MAXTILES)
                    {
                        initprintf("Error: Constant %d for name \"%s\" out of range (line %d)\n", num, name, line-1);
                        continue;
                    }

                    if (Bstrlen(name) > 24)
                        initprintf("Warning: Name \"%s\" longer than 24 characters (line %d). Truncating.\n", name, line-1);

                    Bstrncpyz(names[num], name, 25);

                    syms++;

                    continue;
                }
                else  	// #define_NAME with no number
                {
                    initprintf("Error: No number given for name \"%s\" (line %d)\n", name, line-1);
                    continue;
                }
            }
            else if (!Bstrncmp(p, "include ", 8))
            {
                // #include_...
                p += 8;
                while (*p == 32) p++;
                if (*p == 0)
                {
                    initprintf("Error: Malformed #include at line %d\n", line-1);
                    continue;
                }

                if (*p == '\"')
                    {
                    quotes = 1;
                    p++;
                    }
                else if (*p == '<')
                    {
                    anglebrackets = 1;
                    p++;
                    }

                name = p;
                if (quotes == 1)
                    {
                        while (*p != '\"' && *p != 0) p++;
                        quotes = 0;
                        if (*p == 0)
                        {
                            initprintf("Error: Missing \'\"\' in #include at line %d\n", line-1);
                            continue;
                        }
                        *p = 0;
                    }
                else if (anglebrackets == 1)
                    {
                        while (*p != '>' && *p != 0) p++;
                        anglebrackets = 0;
                        if (*p == 0)
                        {
                            initprintf("Error: Missing \'>\' in #include at line %d\n", line-1);
                            continue;
                        }
                        *p = 0;
                    }
                else
                    {
                    while (*p != 32 && *p != 0) p++;
                    *p = 0;
                    }

                loadnames(name, 0);

                continue;
            }
        }
badline:
        initprintf("Error: Invalid statement found at character %d on line %d\n", (int32_t)(p-buffer), line-1);
    }
    if (root)
        initprintf("Loaded %d names.\n", syms);

    Bfclose(fp);
    return 0;
}

void printcoords16(int32_t posxe, int32_t posye, int16_t ange)
{
    char snotbuf[80];
    int32_t i, m;
    int32_t v8 = (numsectors > MAXSECTORSV7 || numwalls > MAXWALLSV7 ||
                  Numsprites > MAXSPRITESV7 || numyaxbunches > 0);
#if M32_UNDO
    Bsprintf(snotbuf,"x:%d y:%d ang:%d r%d",posxe,posye,ange,map_revision-1);
#else
    Bsprintf(snotbuf,"x:%d y:%d ang:%d",posxe,posye,ange);
#endif
    i = 0;
    while ((snotbuf[i] != 0) && (i < 33))
        i++;
    while (i < 33)
    {
        snotbuf[i] = 32;
        i++;
    }
    snotbuf[33] = 0;

    clearministatbar16();

    printext16(8, ydim-STATUS2DSIZ+128, whitecol, -1, snotbuf,0);

    if (highlightcnt<=0 && highlightsectorcnt<=0)
    {
        m = Bsprintf(snotbuf,"%d/%d %s. %d",
                     numsectors, v8?MAXSECTORSV8:MAXSECTORSV7,
                     numyaxbunches>0 ? "SEC":"sec", numwalls);
        if (numyaxbunches > 0)
        {
            if (xdim >= 800)
                Bsprintf(&snotbuf[m], "/%d wal. %d/16k spr. %d/256 bn.",
                         MAXWALLSV8, Numsprites, numyaxbunches);
            else
                Bsprintf(&snotbuf[m], " wal. %d spr. %d/256 bn.",
                         Numsprites, numyaxbunches);
        }
        else
        {
            if (xdim >= 800)
                Bsprintf(&snotbuf[m], "/%d wal. %d/%d spr.",
                         v8?MAXWALLSV8:MAXWALLSV7, Numsprites,
                         v8?MAXSPRITESV8:MAXSPRITESV7);
            else
                Bsprintf(&snotbuf[m], "/%dk wal. %d/%dk spr.",
                         (v8?MAXWALLSV8:MAXWALLSV7)/1000, Numsprites,
                         (v8?MAXSPRITESV8:MAXSPRITESV7)/1000);
        }
    }
    else
    {
        if (highlightcnt>0)
        {
            m = 0;
            for (i=0; i<highlightcnt; i++)
                m += !!(highlight[i]&16384);
            Bsprintf(snotbuf, "%d sprites, %d walls selected", m, highlightcnt-m);
        }
        else if (highlightsectorcnt>0)
            Bsprintf(snotbuf, "%d sectors with a total of %d walls selected", highlightsectorcnt, numhlsecwalls);
        else
            snotbuf[0] = 0;

        v8 = 1;  // yellow color
    }

    m = xdim/8 - 264/8;
    m = clamp(m, 1, (signed)sizeof(snotbuf)-1);

    i = 0;
    while (snotbuf[i] && i < m)
        i++;
    while (i < m)
    {
        snotbuf[i] = 32;
        i++;
    }
    snotbuf[m] = 0;

    printext16(264, ydim-STATUS2DSIZ+128, v8?editorcolors[10]:whitecol, -1, snotbuf,0);
}

#define DOPRINT(Yofs, fmt, ...) \
    Bsprintf(snotbuf, fmt, ## __VA_ARGS__); \
    printext16(8+col*200, ydim/*-(row*96)*/-STATUS2DSIZ+Yofs, color, -1, snotbuf, 0);

void showsectordata(int16_t sectnum, int16_t small)
{
    sectortype *sec;
    char snotbuf[80];
    int32_t col=0;  //,row = 0;
    int32_t color = small ? whitecol : editorcolors[11];

    sec = &sector[sectnum];

    if (small)
    {
        _printmessage16("^10Sector %d %s ^O(F7 to edit)", sectnum, ExtGetSectorCaption(sectnum));
        return;
    }

    DOPRINT(32, "^10Sector %d", sectnum);
    DOPRINT(48, "Firstwall: %d", sec->wallptr);
    DOPRINT(56, "Numberofwalls: %d", sec->wallnum);
    DOPRINT(64, "Firstsprite: %d", headspritesect[sectnum]);
    DOPRINT(72, "Tags: %d, %d", sec->hitag, sec->lotag);
    DOPRINT(80, "     (0x%x), (0x%x)", sec->hitag, sec->lotag);
    DOPRINT(88, "Extra: %d", sec->extra);
    DOPRINT(96, "Visibility: %d", sec->visibility);
    DOPRINT(104, "Pixel height: %d", (sec->floorz-sec->ceilingz)>>8);

    col++;

    DOPRINT(32, "^10CEILING:^O");
    DOPRINT(48, "Flags (hex): %x", sec->ceilingstat);
    {
        int32_t xp=sec->ceilingxpanning, yp=sec->ceilingypanning;
#ifdef YAX_ENABLE
        if (yax_getbunch(searchsector, YAX_CEILING) >= 0)
            xp = yp = 0;
#endif
        DOPRINT(56, "(X,Y)pan: %d, %d", xp, yp);
    }
    DOPRINT(64, "Shade byte: %d", sec->ceilingshade);
    DOPRINT(72, "Z-coordinate: %d", sec->ceilingz);
    DOPRINT(80, "Tile number: %d", sec->ceilingpicnum);
    DOPRINT(88, "Ceiling heinum: %d", sec->ceilingheinum);
    DOPRINT(96, "Palookup number: %d", sec->ceilingpal);
#ifdef YAX_ENABLE
    DOPRINT(104, "Bunch number: %d", yax_getbunch(sectnum, YAX_CEILING));
#endif

    col++;

    DOPRINT(32, "^10FLOOR:^O");
    DOPRINT(48, "Flags (hex): %x", sec->floorstat);
    {
        int32_t xp=sec->floorxpanning, yp=sec->floorypanning;
#ifdef YAX_ENABLE
        if (yax_getbunch(searchsector, YAX_FLOOR) >= 0)
            xp = yp = 0;
#endif
        DOPRINT(56, "(X,Y)pan: %d, %d", xp, yp);
    }
    DOPRINT(64, "Shade byte: %d", sec->floorshade);
    DOPRINT(72, "Z-coordinate: %d", sec->floorz);
    DOPRINT(80, "Tile number: %d", sec->floorpicnum);
    DOPRINT(88, "Floor heinum: %d", sec->floorheinum);
    DOPRINT(96, "Palookup number: %d", sec->floorpal);
#ifdef YAX_ENABLE
    DOPRINT(104, "Bunch number: %d", yax_getbunch(sectnum, YAX_FLOOR));
#endif
}

void showwalldata(int16_t wallnum, int16_t small)
{
    walltype *wal;
    int32_t sec;
    char snotbuf[80];
    int32_t col=0; //, row = 0;
    int32_t color = small ? whitecol : editorcolors[11];

    wal = &wall[wallnum];

    if (small)
    {
        _printmessage16("^10Wall %d %s ^O(F8 to edit)", wallnum,
                        ExtGetWallCaption(wallnum));
        return;
    }

    DOPRINT(32, "^10Wall %d", wallnum);
    DOPRINT(48, "X-coordinate: %d", wal->x);
    DOPRINT(56, "Y-coordinate: %d", wal->y);
    DOPRINT(64, "Point2: %d", wal->point2);
    DOPRINT(72, "Sector: ^010%d", sectorofwall(wallnum));

    DOPRINT(88, "Tags: %d,  %d", wal->hitag, wal->lotag);
    DOPRINT(96, "     (0x%x),  (0x%x)", wal->hitag, wal->lotag);

    col++;

    DOPRINT(32, "^10%s^O", (wal->picnum>=0 && wal->picnum<MAXTILES) ? names[wal->picnum] : "!INVALID!");
    DOPRINT(48, "Flags (hex): %x", wal->cstat);
    DOPRINT(56, "Shade: %d", wal->shade);
    DOPRINT(64, "Pal: %d", wal->pal);
    DOPRINT(72, "(X,Y)repeat: %d, %d", wal->xrepeat, wal->yrepeat);
    DOPRINT(80, "(X,Y)pan: %d, %d", wal->xpanning, wal->ypanning);
    DOPRINT(88, "Tile number: %d", wal->picnum);
    DOPRINT(96, "OverTile number: %d", wal->overpicnum);

    col++;

    DOPRINT(48-(small?16:0), "nextsector: %d", wal->nextsector);
    DOPRINT(56-(small?16:0), "nextwall: %d", wal->nextwall);

    DOPRINT(72-(small?16:0), "Extra: %d", wal->extra);

    // TX 20050102 I'm not sure what unit dist<<4 is supposed to be, but dist itself is correct in terms of game coordinates as one would expect
    DOPRINT(96-(small?16:0),  "Wall length: %d",  wallength(wallnum));

    sec = sectorofwall(wallnum);
    DOPRINT(104-(small?16:0), "Pixel height: %d", (sector[sec].floorz-sector[sec].ceilingz)>>8);
}

void showspritedata(int16_t spritenum, int16_t small)
{
    spritetype *spr;
    char snotbuf[80];
    int32_t col=0; //, row = 0;
    int32_t color = small ? whitecol : editorcolors[11];

    spr = &sprite[spritenum];

    if (small)
    {
        _printmessage16("^10Sprite %d %s ^O(F8 to edit)",spritenum, ExtGetSpriteCaption(spritenum));
        return;
    }

    DOPRINT(32, "^10Sprite %d", spritenum);
    DOPRINT(48, "X-coordinate: %d", spr->x);
    DOPRINT(56, "Y-coordinate: %d", spr->y);
    DOPRINT(64, "Z-coordinate: %d", spr->z);

    DOPRINT(72, "Sectnum: ^010%d", spr->sectnum);
    DOPRINT(80, "Statnum: %d", spr->statnum);

    DOPRINT(96, "Tags: %d,  %d", spr->hitag, spr->lotag);
    DOPRINT(104, "     (0x%x),  (0x%x)", spr->hitag, spr->lotag);

    col++;

    DOPRINT(32, "^10,0                        ^O");  // 24 blanks
    DOPRINT(32, "^10%s^O", (spr->picnum>=0 && spr->picnum<MAXTILES) ? names[spr->picnum] : "!INVALID!");
    DOPRINT(48, "Flags (hex): %x", spr->cstat);
    DOPRINT(56, "Shade: %d", spr->shade);
    DOPRINT(64, "Pal: %d", spr->pal);
    DOPRINT(72, "(X,Y)repeat: %d, %d", spr->xrepeat, spr->yrepeat);
    DOPRINT(80, "(X,Y)offset: %d, %d", spr->xoffset, spr->yoffset);
    DOPRINT(88, "Tile number: %d", spr->picnum);

    col++;

    DOPRINT(48, "Angle (2048 degrees): %d", spr->ang);
    DOPRINT(56, "X-Velocity: %d", spr->xvel);
    DOPRINT(64, "Y-Velocity: %d", spr->yvel);
    DOPRINT(72, "Z-Velocity: %d", spr->zvel);
    DOPRINT(80, "Owner: %d", spr->owner);
    DOPRINT(88, "Clipdist: %d", spr->clipdist);
    DOPRINT(96, "Extra: %d", spr->extra);
}

#undef DOPRINT

// gets called once per totalclock increment since last call
void keytimerstuff(void)
{
    if (DOWN_BK(STRAFE) == 0)
    {
        if (DOWN_BK(TURNLEFT)) angvel = max(angvel-pk_turnaccel, -128);
        if (DOWN_BK(TURNRIGHT)) angvel = min(angvel+pk_turnaccel, 127);
    }
    else
    {
        if (DOWN_BK(TURNLEFT)) svel = min(svel+16, 255); // svel and vel aren't even chars...
        if (DOWN_BK(TURNRIGHT)) svel = max(svel-16, -256);
    }
    if (DOWN_BK(MOVEFORWARD))  vel = min(vel+16, 255);
    if (DOWN_BK(MOVEBACKWARD)) vel = max(vel-16, -256);
    /*  if (DOWN_BK(STRAFELEFT))  svel = min(svel+8, 127);
    	if (DOWN_BK(STRAFERIGHT)) svel = max(svel-8, -128); */

    if (angvel < 0) angvel = min(angvel+pk_turndecel, 0);
    if (angvel > 0) angvel = max(angvel-pk_turndecel, 0);
    if (svel < 0) svel = min(svel+6, 0);
    if (svel > 0) svel = max(svel-6, 0);
    if (vel < 0) vel = min(vel+6, 0);
    if (vel > 0) vel = max(vel-6, 0);
    /*    if(mlook) pos.z -= (horiz-101)*(vel/40); */
}

#if 0
int32_t snfillprintf(char *outbuf, size_t bufsiz, int32_t fill, const char *fmt, ...)
{
    char tmpstr[256];
    int32_t nwritten, ofs;
    va_list va;

    va_start(va, fmt);
    nwritten = Bvsnprintf(tmpstr, bufsiz, fmt, va);
    va_end(va);

    ofs = min(nwritten, (signed)bufsiz-1);
    Bmemset(outbuf, fill, bufsiz-ofs);

    return ofs;
}
#endif

void _printmessage16(const char *fmt, ...)
{
    int32_t i, ybase;
    char snotbuf[156];
    char tmpstr[160];
    va_list va;

    va_start(va, fmt);
    Bvsnprintf(tmpstr, sizeof(tmpstr), fmt, va);
    va_end(va);

    i = 0;
    while (tmpstr[i] && i < 146)
    {
        snotbuf[i] = tmpstr[i];
        i++;
    }
    snotbuf[i] = 0;
    if (lastpm16time == totalclock)
        Bstrcpy(lastpm16buf, snotbuf);

    clearministatbar16();

    ybase = ydim-STATUS2DSIZ+128-8;

    printext16(8, ybase+8, whitecol, -1, snotbuf, 0);
}

void printmessage256(int32_t x, int32_t y, const char *name)
{
    char snotbuf[80];
    int32_t i;

    i = 0;
    while (name[i] && i < 62)
    {
        snotbuf[i] = name[i];
        i++;
    }
    while (i < 62)
    {
        snotbuf[i] = 32;
        i++;
    }
    snotbuf[62] = 0;
    printext256(x+2,y+2,0,-1,snotbuf,0);
    printext256(x,y,whitecol,-1,snotbuf,0);
}

//Find closest point (*dax, *day) on wall (dawall) to (x, y)
static void getclosestpointonwall(int32_t x, int32_t y, int32_t dawall, int32_t *nx, int32_t *ny,
                                  int32_t maybe_screen_coord_p)
{
    int64_t i, j, wx,wy, wx2,wy2, dx, dy;

    if (m32_sideview && maybe_screen_coord_p)
    {
        wx = m32_wallscreenxy[dawall][0];
        wy = m32_wallscreenxy[dawall][1];
        wx2 = m32_wallscreenxy[wall[dawall].point2][0];
        wy2 = m32_wallscreenxy[wall[dawall].point2][1];
    }
    else
    {
        wx = wall[dawall].x;
        wy = wall[dawall].y;
        wx2 = POINT2(dawall).x;
        wy2 = POINT2(dawall).y;
    }

    dx = wx2 - wx;
    dy = wy2 - wy;
    i = dx*(x-wx) + dy*(y-wy);
    if (i <= 0) { *nx = wx; *ny = wy; return; }
    j = dx*dx + dy*dy;
    if (i >= j) { *nx = wx2; *ny = wy2; return; }
    i=((i<<15)/j)<<15;
    *nx = wx + ((dx*i)>>30);
    *ny = wy + ((dy*i)>>30);
}

static void initcrc(void)
{
    int32_t i, j, k, a;

    for (j=0; j<256; j++)   //Calculate CRC table
    {
        k = (j<<8); a = 0;
        for (i=7; i>=0; i--)
        {
            if (((k^a)&0x8000) > 0)
                a = ((a<<1)&65535) ^ 0x1021;   //0x1021 = genpoly
            else
                a = ((a<<1)&65535);
            k = ((k<<1)&65535);
        }
        crctable[j] = (a&65535);
    }
}

static int32_t GetWallBaseZ(int32_t wallnum)
{
    int32_t z=0, sectnum, nextsec;

    sectnum = sectorofwall(wallnum);
    nextsec = wall[wallnum].nextsector;

    if (nextsec == -1)  //1-sided wall
    {
        if (wall[wallnum].cstat&4)  // floor-aligned
            z = sector[sectnum].floorz;
        else
            z = sector[sectnum].ceilingz;
    }
    else  //2-sided wall
    {
        if (wall[wallnum].cstat&4)
            z = sector[sectnum].ceilingz;
        else
        {
            if (sector[nextsec].ceilingz > sector[sectnum].ceilingz)
                z = sector[nextsec].ceilingz;   //top step
            if (sector[nextsec].floorz < sector[sectnum].floorz)
                z = sector[nextsec].floorz;   //bottom step
        }
    }
    return(z);
}

static void AlignWalls(int32_t w0, int32_t z0, int32_t w1, int32_t z1, int32_t doxpanning)
{
    int32_t n;
    int32_t tilenum = wall[w0].picnum;

    if (tilesizx[tilenum]==0 || tilesizy[tilenum]==0)
        return;

    //do the x alignment
    if (doxpanning)
        wall[w1].xpanning = (uint8_t)((wall[w0].xpanning + (wall[w0].xrepeat<<3))%tilesizx[tilenum]);

    for (n=picsiz[tilenum]>>4; (1<<n)<tilesizy[tilenum]; n++);

    wall[w1].yrepeat = wall[w0].yrepeat;
    wall[w1].ypanning = (uint8_t)(wall[w0].ypanning + (((z1-z0)*wall[w0].yrepeat)>>(n+3)));
}

void AlignWallPoint2(int32_t w0)
{
    int32_t w1 = wall[w0].point2;
    AlignWalls(w0,GetWallBaseZ(w0), w1,GetWallBaseZ(w1), 1);
}

#define ALIGN_WALLS_CSTAT_MASK (4+8+256)

// flags:
//  1: recurse nextwalls
//  2: iterate point2's
//  4: carry pixel width from first wall over to the rest
//  8: align TROR nextwalls
int32_t AutoAlignWalls(int32_t w0, uint32_t flags, int32_t nrecurs)
{
    int32_t z0, z1, tilenum, w1, visible, nextsec, sectnum;
    static int32_t numaligned, wall0, cstat0;
    static uint32_t lenrepquot;

    tilenum = wall[w0].picnum;

    if (nrecurs == 0)
    {
        //clear visited bits
        Bmemset(visited, 0, sizeof(visited));
        visited[w0>>3] |= (1<<(w0&7));
        numaligned = 0;
        lenrepquot = getlenbyrep(wallength(w0), wall[w0].xrepeat);
        wall0 = w0;
        cstat0 = wall[w0].cstat & ALIGN_WALLS_CSTAT_MASK;  // top/bottom orientation; x/y-flip
    }

    z0 = GetWallBaseZ(w0);

    w1 = wall[w0].point2;

    //loop through walls at this vertex in point2 order
    while (1)
    {
        //break if this wall would connect us in a loop
        if (visited[w1>>3]&(1<<(w1&7)))
            break;

        visited[w1>>3] |= (1<<(w1&7));

#ifdef YAX_ENABLE
        if (flags&8)
        {
            int32_t cf, ynw;

            for (cf=0; cf<2; cf++)
            {
                ynw = yax_getnextwall(w0, cf);

                if (ynw >= 0 && wall[ynw].picnum==tilenum && (visited[ynw>>3]&(1<<(ynw&7)))==0)
                {
                    wall[ynw].xrepeat = wall[w0].xrepeat;
                    wall[ynw].xpanning = wall[w0].xpanning;
                    AlignWalls(w0,z0, ynw,GetWallBaseZ(ynw), 0);  // initial vertical alignment

//                    AutoAlignWalls(ynw, flags&~8, nrecurs+1);  // recurse once
                }
            }
        }
#endif
        //break if reached back of left wall
        if (wall[w1].nextwall == w0)
            break;

        if (wall[w1].picnum == tilenum)
        {
            z1 = GetWallBaseZ(w1);
            visible = 0;

            nextsec = wall[w1].nextsector;
            if (nextsec < 0)
                visible = 1;
            else
            {
                int32_t cz,fz, czn,fzn;

                //ignore two sided walls that have no visible face
                sectnum = NEXTWALL(w1).nextsector;
                getzsofslope(sectnum, wall[w1].x,wall[w1].y, &cz, &fz);
                getzsofslope(nextsec, wall[w1].x,wall[w1].y, &czn, &fzn);

                if (cz < czn || fz > fzn)
                    visible = 1;
            }

            if (visible)
            {
                if ((flags&4) && w0!=wall0)
                    fixxrepeat(w0, lenrepquot);
                AlignWalls(w0,z0, w1,z1, 1);
                wall[w1].cstat &= ~ALIGN_WALLS_CSTAT_MASK;
                wall[w1].cstat |= cstat0;
                numaligned++;

                //if wall was 1-sided, no need to recurse
                if (wall[w1].nextwall < 0)
                {
                    if (!(flags&2))
                        break;
                    w0 = w1;
                    z0 = GetWallBaseZ(w0);
                    w1 = wall[w0].point2;
                    continue;
                }
                else if (flags&1)
                    AutoAlignWalls(w1, flags, nrecurs+1);
            }
        }

        if (wall[w1].nextwall < 0 || !(flags&2))
            break;
        w1 = NEXTWALL(w1).point2;
    }

    return numaligned;
}

#define PLAYTEST_MAPNAME "autosave_playtest.map"

void test_map(int32_t mode)
{
    if (!mode)
        updatesector(pos.x, pos.y, &cursectnum);
    else
        updatesector(startposx, startposy, &startsectnum);

#ifdef _WIN32
    if (fullscreen)
    {
        printmessage16("Must be in windowed mode to test map!");
        return;
    }
#endif

    if ((!mode && cursectnum >= 0) || (mode && startsectnum >= 0))
    {
        char *param = " -map " PLAYTEST_MAPNAME " -noinstancechecking";
        char *fullparam;
        char current_cwd[BMAX_PATH];
        int32_t slen = 0;
        BFILE *fp;

        if ((program_origcwd[0] == '\0') || !getcwd(current_cwd, BMAX_PATH))
            current_cwd[0] = '\0';
        else // Before we check if file exists, for the case there's no absolute path.
            chdir(program_origcwd);

        fp = fopen(game_executable, "rb"); // File exists?
        if (fp != NULL)
            fclose(fp);
        else
        {
#ifdef _WIN32
            fullparam = Bstrrchr(mapster32_fullpath, '\\');
#else
            fullparam = Bstrrchr(mapster32_fullpath, '/');
#endif
            if (fullparam)
            {
                slen = fullparam-mapster32_fullpath+1;
                Bstrncpy(game_executable, mapster32_fullpath, slen);
                // game_executable is now expected to not be NULL-terminated!
                Bstrcpy(game_executable+slen, DEFAULT_GAME_EXEC);
            }
            else
                Bstrcpy(game_executable, DEFAULT_GAME_LOCAL_EXEC);
        }

        if (current_cwd[0] != '\0') // Temporarily changing back,
            chdir(current_cwd);     // after checking if file exists.

        if (testplay_addparam)
            slen = Bstrlen(testplay_addparam);

        // Considering the NULL character, quatation marks
        // and a possible extra space not in testplay_addparam,
        // the length should be Bstrlen(game_executable)+Bstrlen(param)+(slen+1)+2+1.

        fullparam = Bmalloc(Bstrlen(game_executable)+Bstrlen(param)+slen+4);
        Bsprintf(fullparam,"\"%s\"",game_executable);

        if (testplay_addparam)
        {
            Bstrcat(fullparam, " ");
            Bstrcat(fullparam, testplay_addparam);
        }
        Bstrcat(fullparam, param);

        ExtPreSaveMap();
        if (mode)
            saveboard(PLAYTEST_MAPNAME,&startposx,&startposy,&startposz,&startang,&startsectnum);
        else
            saveboard(PLAYTEST_MAPNAME,&pos.x,&pos.y,&pos.z,&ang,&cursectnum);

        message("Board saved to " PLAYTEST_MAPNAME ". Starting the game...");
        OSD_Printf("...as `%s'\n", fullparam);

        showframe(1);
        uninitmouse();
#ifdef _WIN32
        {
            STARTUPINFO si;
            PROCESS_INFORMATION pi;

            ZeroMemory(&si,sizeof(si));
            ZeroMemory(&pi,sizeof(pi));
            si.cb = sizeof(si);

            if (!CreateProcess(NULL,fullparam,NULL,NULL,0,0,NULL,NULL,&si,&pi))
                message("Error launching the game!");
            else WaitForSingleObject(pi.hProcess,INFINITE);
        }
#else
        if (current_cwd[0] != '\0')
        {
            chdir(program_origcwd);
            if (system(fullparam))
                message("Error launching the game!");
            chdir(current_cwd);
        }
        else system(fullparam);
#endif
        printmessage16("Game process exited");
        initmouse();
        clearkeys();

        Bfree(fullparam);
    }
    else
        printmessage16("Position must be in valid player space to test map!");
}
