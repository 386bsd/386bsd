/* $XFree86: mit/server/ddx/x386/etc/ati.test.c,v 1.2 1993/03/27 09:31:24 dawes Exp $ */
/* ati.test.c -- Gather information about ATI VGA WONDER cards
 * Created: Sun Aug  9 10:15:01 1992
 * Revised: Fri Aug 21 14:04:29 1992 by root
 * Author: Rickard E. Faith, faith@cs.unc.edu
 * Copyright 1992 Rickard E. Faith; All Rights Reserved.
 * May be distributed freely for any purpose as long as the
 * copyright notice and the warranty disclaimer are kept.
 * This program comes with ABSOLUTELY NO WARRANTY.
 *
 * Log
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>

/* These are from the x386 compile.h for Linux */
static void inline outb(unsigned short port, char value)
{
   __asm__ volatile ("outb %0,%1"
		     ::"a" ((char) value),"d" ((unsigned short) port));
}
static void inline outw(unsigned short port, short value)
{
   __asm__ volatile ("outw %0,%1"
		     ::"a" ((short) value),"d" ((unsigned short) port));
}

static unsigned char inline inb(unsigned short port)
{
   unsigned char _v;
   __asm__ volatile ("inb %1,%0"
		     :"=a" (_v):"d" ((unsigned short) port));
   return _v;
}
static unsigned short inline inw(unsigned short port)
{
   unsigned short _v;
   __asm__ volatile ("inw %1,%0"
		     :"=a" (_v):"d" ((unsigned short) port));
   return _v;
}

#define FALSE          -1
#define TRUE            0
#define ATI_BOARD_V3	0	/* which ATI board does the user have? */
#define ATI_BOARD_V4	1	/* keep these chronologically increasing */
#define ATI_BOARD_V5	2
#define ATI_BOARD_PLUS	3
#define ATI_BOARD_XL	4

int ATIExtReg, ATIBoard;

int main( void )
{
   const char signature[]    = " 761295520";
   const int  bios_data_size = 0x50;
   char       bios_data[bios_data_size];
   char       bios_signature[10];
   int        fd;

   int videoRam;
	 
   if ((fd = open( "/dev/mem", O_RDONLY )) < 0
       || lseek( fd, 0xc0030, SEEK_SET) < 0
       || read( fd, bios_signature, 10 ) != 10) {

      if (fd >= 0) close( fd );
      printf( "Failed to read VGA BIOS.  Cannot detect ATI card.\n" );
      return FALSE;
   }
   if (strncmp( signature, bios_signature, 10 )) {
      printf( "Incorrect Signature for ATI BIOS\n" );
      return FALSE;
   }
   close( fd );

   /* Assume an ATI card.  Now look in the BIOS for magic numbers. */
   if ((fd = open( "/dev/mem", O_RDONLY )) < 0
       || lseek( fd, 0xc0000, SEEK_SET ) < 0
       || read( fd, bios_data, bios_data_size ) != bios_data_size) {

      if (fd >= 0) close( fd );
      printf( "Failed to read VGA BIOS.  Cannot set up ATI card.\n" );
      return FALSE;
   }
   close( fd );
   
   printf( "ATI BIOS Information Block:\n" );
   printf( "   Signature code:                %c%c = ",
	   bios_data[ 0x40 ], bios_data[ 0x41 ] );
   if (bios_data[ 0x40 ] != '3') printf( "Unknown\n" );
   else switch( bios_data[ 0x41 ] ) {
   case '1':
      printf( "VGA WONDER\n" );
      break;
   case '2':
      printf( "EGA WONDER800+\n" );
      break;
   case '3':
      printf( "VGA BASIC-16\n" );
      break;
   default:
      printf( "Unknown\n" );
      break;
   }
   printf( "   Chip version:                  %c =  ",
	   bios_data[ 0x43 ] );
   switch (bios_data[ 0x43 ] ) {
   case '1':
      printf( "ATI 18800\n" );
      break;
   case '2':
      printf( "ATI 18800-1\n" );
      break;
   case '3':
      printf( "ATI 28800-2\n" );
      break;
   case '4': case '5':
      printf( "ATI 28800-%c\n", bios_data[ 0x43 ] );
      break;
   default:
      printf( "Unknown\n" );
      break;
   }
   printf( "   BIOS version:                  %d.%d\n",
	   bios_data[ 0x4c ], bios_data[ 0x4d ] );
   
   printf( "\n   Byte at offset 0x42 =          0x%02x\n",
	   bios_data[ 0x42 ] );
   printf( "   8 and 16 bit ROM supported:    %s\n",
	   bios_data[ 0x42 ] & 0x01 ? "Yes" : "No" );
   printf( "   Mouse chip present:            %s\n",
	   bios_data[ 0x42 ] & 0x02 ? "Yes" : "No" );
   printf( "   Inport compatible mouse port : %s\n",
	   bios_data[ 0x42 ] & 0x04 ? "Yes" : "No" );
   printf( "   Micro Channel supported:       %s\n",
	   bios_data[ 0x42 ] & 0x08 ? "Yes" : "No" );
   printf( "   Clock chip present:            %s\n",
	   bios_data[ 0x42 ] & 0x10 ? "Yes" : "No" );
   printf( "   Uses c000:0 to d000:ffff:      %s\n",
	   bios_data[ 0x42 ] & 0x80 ? "Yes" : "No" );
   
   printf( "\n   Byte at offset 0x44 =          0x%02x\n",
	  bios_data[ 0x44 ] );
   printf( "   Supports 70Hz non-interlaced:  %s\n",
	   bios_data[ 0x44 ] & 0x01 ? "No" : "Yes" );
   printf( "   Supports Korean characters:    %s\n",
	   bios_data[ 0x44 ] & 0x02 ? "Yes" : "No" );
   printf( "   Uses 45Mhz memory clock:       %s\n",
	   bios_data[ 0x44 ] & 0x04 ? "Yes" : "No" );
   printf( "   Supports zero wait states:     %s\n",
	   bios_data[ 0x44 ] & 0x08 ? "Yes" : "No" );
   printf( "   Uses paged ROMS:               %s\n",
	   bios_data[ 0x44 ] & 0x10 ? "Yes" : "No" );
   printf( "   8514/A hardware on board:      %s\n",
	   bios_data[ 0x44 ] & 0x40 ? "No" : "Yes" );
   printf( "   32K color DAC on board:        %s\n",
	   bios_data[ 0x44 ] & 0x80 ? "Yes" : "No" );

   ATIExtReg = *((short int *)bios_data + 0x08);
   ioperm( ATIExtReg, 2, 1 );

   switch( bios_data[ 0x43 ] ) {
   case '1':
      ATIBoard = ATI_BOARD_V3;
      break;
   case '2':
      if (bios_data[ 0x42 ] & 0x10) ATIBoard = ATI_BOARD_V5;
      else ATIBoard = ATI_BOARD_V4;
      break;
   case '3':
   case '4':
      ATIBoard = ATI_BOARD_PLUS;
      break;
   case '5':
   default:
      if (bios_data[ 0x44 ] & 0x80) ATIBoard = ATI_BOARD_XL;
      else ATIBoard = ATI_BOARD_PLUS;
   }

   printf( "\nThis video adapter is a:          " );
   switch (ATIBoard) {
   case ATI_BOARD_V3:   printf( "VGA WONDER V3\n" ); break;
   case ATI_BOARD_V4:   printf( "VGA WONDER V4\n" ); break;
   case ATI_BOARD_V5:   printf( "VGA WONDER V5\n" ); break;
   case ATI_BOARD_PLUS: printf( "VGA WONDER PLUS (V6)\n" ); break;
   case ATI_BOARD_XL:   printf( "VGA WONDER XL (V7)\n" ); break;
   default:             printf( "Unknown\n" ); break;
   }
      
   videoRam = 0;
   if (bios_data[ 0x43 ] <= '2') {
      outb( ATIExtReg, 0xbb );
      videoRam = (inb( ATIExtReg + 1 ) & 0x20) ? 512 : 256;
   } else {
      outb( ATIExtReg, 0xb0 );
      switch (inb( ATIExtReg + 1 ) & 0x18) {
      case 0x00:
	 videoRam = 256;
	 break;
      case 0x10:
	 videoRam = 512;
	 break;
      case 0x08:
	 videoRam = 1024;
	 break;
      }
   }

   printf( "Amount of RAM on video adapter:   %dk\n", videoRam );
   printf( "\nThe extended registers (ATIExtREG) are at 0x%x\n", ATIExtReg );
   printf( "The x386 ATI driver will set ATIBoard =   %d\n", ATIBoard );

   return TRUE;
}
