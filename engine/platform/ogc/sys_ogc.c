/*
sys_wii.c - misc wii stubs
Copyright (C) 2026 mintferret

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "platform/platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <fat.h>
#include <SDL.h>
#include <gccore.h>
#include <ogcsys.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// libogc's sbrk honours this weak symbol. Left at 0 the heap comes out of
// MEM1, which the engine shares with the whole executable - a GL build leaves
// barely 6MB of it, and loading a map runs the engine out of memory partway
// through, in a different place on every build. MEM2 has 64MB doing nothing.
u32 MALLOC_MEM2 = 1;

void Platform_ShellExecute( const char *path, const char *parms )
{
	Con_Reportf( S_WARN "Tried to shell execute ;%s; -- not supported\n", path );
}

#if XASH_OGC_GECKO
#include <sys/iosupport.h>

#ifndef XASH_OGC_GECKO_CHANNEL
#define XASH_OGC_GECKO_CHANNEL 1 // EXI channel 1 == memory card slot B
#endif

static ssize_t OGC_GeckoWrite( struct _reent *r, void *fd, const char *ptr, size_t len )
{
	usb_sendbuffer_safe( XASH_OGC_GECKO_CHANNEL, ptr, len );
	return len;
}

static const devoptab_t ogc_gecko_out =
{
	.name = "stdout",
	.write_r = OGC_GeckoWrite,
};
#endif // XASH_OGC_GECKO

/*
================
OGC_EarlyInit

Called from main() before anything else, so that a crash during engine
startup still produces a log. Routes stdout (printf, Con_Printf, Sys_Error)
to the USB Gecko in EXI slot B, which both real hardware and Dolphin expose.
================
*/
void OGC_EarlyInit( void )
{
#if XASH_OGC_GECKO
	// brings the gecko up: without this usb_isgeckoalive() stays false
	CON_EnableGecko( XASH_OGC_GECKO_CHANNEL, false );

#if XASH_OGC_GECKO_WAIT
	// Under Dolphin the capture tool races the emulated boot, and anything
	// printed before it attaches is gone for good. Give it a bounded window
	// to show up. Never enable this for a hardware build: with no gecko
	// plugged in it just burns the whole timeout on every launch.
	{
		int tries;

		for( tries = 0; tries < XASH_OGC_GECKO_WAIT * 100; tries++ )
		{
			if( usb_isgeckoalive( XASH_OGC_GECKO_CHANNEL ))
				break;
			usleep( 10000 );
		}
	}
#endif

	// libogc 3.1 no longer points stdout at the gecko console, so hook
	// newlib's device table ourselves. Everything the engine prints
	// (Con_Printf, Sys_Error, plain printf) goes out the wire from here on.
	devoptab_list[STD_OUT] = &ogc_gecko_out;
	devoptab_list[STD_ERR] = &ogc_gecko_out;
	setvbuf( stdout, NULL, _IONBF, 0 );
	setvbuf( stderr, NULL, _IONBF, 0 );
#endif
}

void OGC_Init( void )
{
	// NOTE: no SYS_STDIO_Report( true ) here. It looks harmless but it
	// replaces devoptab_list[STD_OUT] with libogc's UART device, which talks
	// to EXI channel 0 device 1 - a debug port that doesn't exist on retail
	// hardware and that hangs outright under Dolphin. It would also undo the
	// gecko hook installed in OGC_EarlyInit.

	// each step is announced: all of these poke at hardware that can hang,
	// and this is the only way to see which one did
	printf( "OGC_Init: WPAD\n" );
	WPAD_Init();

	printf( "OGC_Init: USB keyboard\n" );
	KEYBOARD_Init( NULL );

	printf( "OGC_Init: USB mouse\n" );
	MOUSE_Init( NULL );

	printf( "OGC_Init: done\n" );
}

void OGC_Shutdown( void )
{
	printf( "%s\n", __func__ );
}

//const struct in6_addr in6addr_any = {{ 0 }};
