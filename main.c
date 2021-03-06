#include <proto/exec.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>

/********************************************
 * Here you can activate various features:  *
 *                                          *
 * MODPLAY for protracker modules replay    *
 * via ptreplay.library                     *
 *                                          *
 * VBL_SYS_INT for system friendly VBL      *
 * interrupts                               *
 * *****************************************/
 
#define MODPLAY
#define VBL_SYS_INT
#define MODULE "assets/red.mod"

// Vertical blank (system) interrupt
#ifdef VBL_SYS_INT
#include <hardware/intbits.h>
struct Interrupt *vbint;
void __amigainterrupt interruptHandler();
#endif

// Variables declarations
volatile UBYTE *ciaa = (volatile UBYTE *) 0xbfe001;
UBYTE *bitplan1;            // pointer to bitplan data
UWORD SystemDMA;            // backup of initial DMA
UWORD __chip clist[];       // Copperlist

extern struct Custom custom;
extern struct GfxBase *GfxBase;
struct copinit *oldcop;     // Initial copperlist

// Functions declarations
void waitLMB() {
    while ((*ciaa & 64) != 0);
}

int startup() {
    // Allocating memory (chipram) for bitplans
    if ((bitplan1 = AllocMem(0x2800, MEMF_CHIP|MEMF_CLEAR)) == NULL) return 1;
    // Updating copperlist with bitplan address
    ULONG bpl1addr;
    bpl1addr = (ULONG)bitplan1;
    clist[1] = (UWORD)(bpl1addr>>16);       // BPL1PTH
    clist[3] = (UWORD)bpl1addr;             // BPL1PTL

    // Saving initial copperlist
    GfxBase = (struct GfxBase*)OpenLibrary("graphics.library", 0);
    oldcop = GfxBase->copinit;
    SystemDMA = custom.dmaconr|0x8000;      // Saving initial DMA with the SET/CLR flag set

    WaitTOF();                                                                          // Waiting for both copperlists to finish
    WaitTOF();

    custom.dmacon = 0x7FFF;                                                             // Clear all DMA channels
    custom.bplcon0 = 0x1000;                                                            // 1 bitplan in low resolution
    custom.bplcon1 = 0x0000;
    custom.ddfstrt = 0X0038;
    custom.ddfstop = 0X00D0;
    custom.diwstrt = 0x2C81;
    custom.diwstop = 0x2CC1;
    custom.bpl1mod = 0x0000;
    custom.cop1lc = (ULONG)clist;                                                       // copperlist address
    custom.dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_COPPER | DMAF_RASTER;              // playfield and copper DMA enabled
    custom.copjmp1 = 0x0000;                                                            // copper start

    #ifdef VBL_SYS_INT
    if (vbint = AllocMem(sizeof(struct Interrupt),      // Allocate memory for
                         MEMF_PUBLIC|MEMF_CLEAR))       // interrupt node
    {
        vbint->is_Node.ln_Type = NT_INTERRUPT;          // Initialize the node
        vbint->is_Node.ln_Pri = -60;
        vbint->is_Node.ln_Name = "VertB-Interrupt";
        vbint->is_Code = interruptHandler;
        AddIntServer(INTB_VERTB, vbint);                // Launching interrupt server
    }
    else {
        CloseLibrary((struct Library *)GfxBase);
        return 1;
    }
    #endif

    return 0;
}

void restore() {
    custom.dmacon = SystemDMA;                  // Restores initial DMA
    custom.cop1lc = (ULONG)oldcop;              // Restores initial copperlist
    CloseLibrary((struct Library *)GfxBase);
    #ifdef VBL_SYS_INT
    RemIntServer(INTB_VERTB, vbint);            // Removes interrupt server
    FreeMem(vbint, sizeof(struct Interrupt));   // Frees interrupt server memory
    #endif
    if (bitplan1) FreeMem(bitplan1, 0x2800);    // Frees bitplane memory
}

#ifdef MODPLAY
#include <proto/exec.h>
#include "include/ptreplay.h"

struct Library *PTReplayBase;
struct Module *Mod = NULL;
BYTE SigBit;

int mod_play() {
    if((PTReplayBase = OpenLibrary("ptreplay.library",0L)) && (SigBit=AllocSignal(-1)!=-1)) {
        if(Mod = PTLoadModule(MODULE)) {
            PTInstallBits(Mod, SigBit, -1, -1, -1);
            PTPlay(Mod);
            return 0;
        }
    return 1;
    }
}

void mod_stop() {
    PTStop(Mod);
    FreeSignal(SigBit);
    PTUnloadModule(Mod);
    CloseLibrary(PTReplayBase);
}
#endif

/****************************************************************
* Here you can place code to run during vertical blank interval *
****************************************************************/

#ifdef VBL_SYS_INT
__amigainterrupt void interruptHandler() {
}
#endif

/*************************
 * Here starts your code *
 * **********************/

// Basic copperlist : just resets bitplan pointers and color registers at each frame
UWORD __chip clist[] = {
    0x00E0, 0x0000,
    0x00E2, 0x0000,
    0x0180, 0x0000,
    0x0182, 0x0FFF,
    0xFFFF, 0xFFFE
};

int main() {
    if (startup()) return 10;
    #ifdef MODPLAY
    mod_play();
    #endif
    waitLMB();
    #ifdef MODPLAY
    mod_stop();
    #endif
    restore();
    return 0;
}