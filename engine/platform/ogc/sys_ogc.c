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
	SYS_STDIO_Report(true);
	WPAD_Init();
	KEYBOARD_Init(NULL);
	MOUSE_Init(NULL);
	printf( "%s\n", __func__ );
}

void OGC_Shutdown( void )
{
	printf( "%s\n", __func__ );
}

//const struct in6_addr in6addr_any = {{ 0 }};
