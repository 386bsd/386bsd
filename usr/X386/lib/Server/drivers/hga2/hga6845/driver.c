/*
 * Author:  Davor Matic, dmatic@athena.mit.edu
 */

#include "X.h"
#include "input.h"
#include "screenint.h"

#include "compiler.h"

#include "x386.h"
#include "x386Priv.h"
#include "xf86_OSlib.h"
#include "hga.h"

/*
 * Since the table of 6845 registers is write only, we need to keep
 * a local copy of the state here.  The initial state is assumed to 
 * be 80x25 text mode.
 */
static unsigned char 
  static_tbl[] = {0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19,
		  0x02, 0x0D, 0x0B, 0x0C, 0x00, 0x00, 0x00, 0x00};

typedef struct {
  hgaHWRec std;               
  unsigned char tbl[16];
  } hga6845Rec, *hga6845Ptr;

static Bool     HGA6845Probe();
static char *   HGA6845Ident();
static void     HGA6845EnterLeave();
static Bool     HGA6845Init();
static void *   HGA6845Save();
static void     HGA6845Restore();

hgaVideoChipRec HGA6845 = {
  HGA6845Probe,
  HGA6845Ident,
  HGA6845EnterLeave,
  HGA6845Init,
  HGA6845Save,
  HGA6845Restore,
};

#define new ((hga6845Ptr)hgaNewVideoState)

/*
 * HGA6845Ident
 */

char *
HGA6845Ident()

{
  return("hga6845");
}

/*
 * HGA6845Probe --
 *      check whether an HGA6845 based board is installed
 */

static Bool
HGA6845Probe()
{

  /*
   * Set up I/O ports to be used by this card
   */
  xf86ClearIOPortList(hga2InfoRec.scrnIndex);
  xf86AddIOPorts(hga2InfoRec.scrnIndex, Num_HGA_IOPorts, HGA_IOPorts);

  if (hga2InfoRec.chipset)
    {
      if (strcmp(hga2InfoRec.chipset, HGA6845Ident()))
	return (FALSE);
      else
	HGA6845EnterLeave(ENTER);
    }
  else
    {
#define DSP_VSYNC_MASK  0x80
#define DSP_ID_MASK  0x70
      unsigned char dsp, dsp_old;
      int i, cnt;

      HGA6845EnterLeave(ENTER);
      /*
       * Checks if there is a HGA 6845 based bard in the system.
       * The following loop tries to see if the Hercules display 
       * status port register is counting vertical syncs (50Hz). 
       */
      cnt = 0;
      dsp_old = inb(0x3BA) & DSP_VSYNC_MASK;
      for (i = 0; i < 0x10000; i++) {
        dsp = inb(0x3BA) & DSP_VSYNC_MASK;
        if (dsp != dsp_old) cnt++;
        dsp_old = dsp;
      }

      /* If there are active sync changes, we found a Hercules board. */
      if (cnt) {
        hga2InfoRec.videoRam = 64;

        /* The Plus and InColor versions have an ID code as well. */
        dsp = inb(0x3BA) & DSP_ID_MASK;
        switch(dsp) {
        case 0x10:        /* Plus */
          hga2InfoRec.videoRam = 64;
          break;
        case 0x50:        /* InColor */
          hga2InfoRec.videoRam = 256;
          break;
        }
      }
      else /* there is no hga card */
      {
	HGA6845EnterLeave(LEAVE);
	return(FALSE);
      }
      
      hga2InfoRec.chipset = HGA6845Ident();
    }

  return(TRUE);
}

/*
 * HGA6845EnterLeave --
 *      enable/disable io-mapping
 */

static void 
HGA6845EnterLeave(enter)
     Bool enter;
{
  unsigned char temp;

  if (enter)
    {
      xf86EnableIOPorts(hga2InfoRec.scrnIndex);
    }
  else
    {
      xf86DisableIOPorts(hga2InfoRec.scrnIndex);
    }
}

/*
 * HGA6845Restore --
 *      restore a video mode
 */

static void
HGA6845Restore(restore)
     hga6845Ptr restore;
{
  unsigned char i;

  hgaHWRestore(restore);

  for (i = 0; i < 16; i++) {
    outb(0x3B4, i);
    outb(0x3B5, static_tbl[i] = restore->tbl[i]);
  }
}

/*
 * HGA6845Save --
 *      save the current video mode
 */

static void *
HGA6845Save(save)
     hga6845Ptr save;
{
  unsigned char             i;

  save = (hga6845Ptr)hgaHWSave(save, sizeof(hga6845Rec));
 
  for (i = 0; i < 16; i++)
    save->tbl[i] = static_tbl[i];
  
  return ((void *) save);
}

/*
 * HGA6845Init --
 *      Handle the initialization of the HGAs registers
 */

static Bool
HGA6845Init(mode)
     DisplayModePtr mode;
{
  unsigned char i;
  unsigned char       /* 720x348 graphics mode parameters */
    init_tbl[] = {0x35, 0x2D, 0x2E, 0x07, 0x5B, 0x02, 0x57, 0x57,
		  0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  
  if (!hgaHWInit(mode, sizeof(hga6845Rec)))
    return(FALSE);
  
  for (i = 0; i < 16; i++) 
    new->tbl[i] = init_tbl[i];
  
  return(TRUE);
}
