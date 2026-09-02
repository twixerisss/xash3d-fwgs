/*
in_ogc.c - Wii remote pointer aiming
Copyright (C) 2026 twixerisss

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

#if XASH_OGC
#include "common.h"
#include "client.h"
#include "input.h"
#include <wiiuse/wpad.h>

/*
The remote's IR sensor arrives from SDL as an absolute pointer, not as
relative motion, so the engine's usual mouse look does not work with it: push
the pointer to the edge of the screen and the deltas simply stop, leaving you
unable to keep turning.

What console shooters do instead is treat the screen as two regions. Near the
middle the pointer only moves the aim - the view stays put, which is what
makes fine aiming feel steady. Past that box the view starts turning, faster
the further out you point, so you swing the camera by pushing towards the
edge of the screen and stop by coming back to the middle.
*/

static CVAR_DEFINE_AUTO( wii_ir, "1", FCVAR_ARCHIVE, "aim with the Wii remote pointer" );
static CVAR_DEFINE_AUTO( wii_ir_deadzone, "0.35", FCVAR_ARCHIVE, "fraction of the screen where pointing does not turn the view" );
static CVAR_DEFINE_AUTO( wii_ir_yawspeed, "220", FCVAR_ARCHIVE, "degrees per second of turn at the screen edge" );
static CVAR_DEFINE_AUTO( wii_ir_pitchspeed, "160", FCVAR_ARCHIVE, "degrees per second of pitch at the screen edge" );
static CVAR_DEFINE_AUTO( wii_ir_gunsway, "7", FCVAR_ARCHIVE, "degrees the weapon leans towards the pointer" );
static CVAR_DEFINE_AUTO( wii_ir_cursor, "1", FCVAR_ARCHIVE, "show the pointer in game as the aiming reticle" );

// where the player is pointing, as -1..1 from the centre of the screen.
// kept here so the view code can lean the weapon towards it.
static vec2_t ogc_pointer;

static qboolean OGC_GetPointerAngles( float *dyaw, float *dpitch );

void OGC_InputInit( void )
{
	// The IR coordinates come back in whatever space we ask for, and SDL
	// reports them straight through as absolute mouse position, so this has
	// to match the screen the engine thinks it is drawing to.
	if( refState.width > 0 && refState.height > 0 )
		WPAD_SetVRes( WPAD_CHAN_ALL, refState.width, refState.height );

	Cvar_RegisterVariable( &wii_ir );
	Cvar_RegisterVariable( &wii_ir_deadzone );
	Cvar_RegisterVariable( &wii_ir_yawspeed );
	Cvar_RegisterVariable( &wii_ir_pitchspeed );
	Cvar_RegisterVariable( &wii_ir_gunsway );
	Cvar_RegisterVariable( &wii_ir_cursor );

#if XASH_OGC_AIMTEST
	// The aim maths cannot be exercised without a real pointer, so check it
	// against known positions at startup instead. At a 90 degree horizontal
	// field of view the screen edge is 45 degrees off centre.
	{
		static const float cases[][2] = { {0,0}, {1,0}, {-1,0}, {0,1}, {0.5f,-0.5f} };
		vec2_t saved;
		int i;

		Vector2Copy( ogc_pointer, saved );
		for( i = 0; i < 5; i++ )
		{
			float dy = 0, dp = 0;

			ogc_pointer[0] = cases[i][0];
			ogc_pointer[1] = cases[i][1];
			OGC_GetPointerAngles( &dy, &dp );
			printf( "[AIMTEST] pointer=%+.2f,%+.2f -> dyaw=%+.2f dpitch=%+.2f (fov=%.0f %dx%d)\n",
				cases[i][0], cases[i][1], dy, dp, cl.local.scr_fov, refState.width, refState.height );
		}
		Vector2Copy( saved, ogc_pointer );
	}
#endif
}

/*
============
OGC_GetPointer

Pointer position as -1..1 from screen centre, for the view model.
============
*/
void OGC_GetPointer( float *x, float *y )
{
	if( x ) *x = ogc_pointer[0];
	if( y ) *y = ogc_pointer[1];
}

/*
============
OGC_WantVisiblePointer

The engine hides the cursor in game. With pointer aiming that would leave the
player with no indication of where the shot is going, since it no longer
leaves from the centre of the screen.
============
*/
qboolean OGC_WantVisiblePointer( void )
{
	return wii_ir.value && wii_ir_cursor.value;
}

/*
============
OGC_GetPointerAngles

The angular offset from the centre of the screen to where the player is
pointing. Derived from the field of view rather than being a fixed number,
so a shot fired along these angles lands exactly under the pointer.
============
*/
static qboolean OGC_GetPointerAngles( float *dyaw, float *dpitch )
{
	float halffov, tanhalf, aspect;

	if( !wii_ir.value || refState.width <= 0 || refState.height <= 0 )
		return false;

	halffov = bound( 10.0f, cl.local.scr_fov, 150.0f ) * 0.5f;
	tanhalf = tan( DEG2RAD( halffov ));
	aspect  = (float)refState.height / (float)refState.width;

	// screen x grows right and yaw decreases right; screen y grows down and
	// pitch increases down
	*dyaw   = -RAD2DEG( atan( ogc_pointer[0] * tanhalf ));
	*dpitch =  RAD2DEG( atan( ogc_pointer[1] * tanhalf * aspect ));

	return true;
}

/*
============
OGC_ApplyPointerToAim

Aims the shot where the player is pointing rather than where the camera is
looking. GoldSrc has no notion of an aim direction separate from the view:
the client puts its view angles in the usercmd and the server fires along
them. So the split is made here, after the client dll has filled the command
in - the command carries the pointer angles while cl.viewangles, which the
camera renders from, is left alone.
============
*/
void OGC_ApplyPointerToAim( vec3_t viewangles, float *forwardmove, float *sidemove )
{
	float dyaw, dpitch;

#ifdef XASH_OGC_AIMPROOF
	// force a fixed pointer offset so the two ends can be compared
	ogc_pointer[0] = 0.5f; ogc_pointer[1] = 0.0f;
#endif
	if( !OGC_GetPointerAngles( &dyaw, &dpitch ))
		return;

#ifdef XASH_OGC_AIMPROOF
	{
		static int n;
		if(( n % 4 ) == 0 && n < 160 )
			Con_Printf( "[AIMPROOF] cl_n=%d camera yaw=%.3f  sent yaw=%.3f\n",
				n, cl.viewangles[YAW], viewangles[YAW] + dyaw );
		n++;
	}
#endif
	viewangles[YAW]   += dyaw;
	viewangles[PITCH] += dpitch;
	viewangles[PITCH] = bound( -89.0f, viewangles[PITCH], 89.0f );

	// Movement is derived from these same angles, so turning the aim would
	// also turn "forward" - walk while pointing at the edge of the screen and
	// you would drift sideways. Counter-rotate the move vector by the same
	// angle so it stays relative to the camera.
	if( forwardmove && sidemove )
	{
		float rad = DEG2RAD( dyaw );
		float c = cos( rad ), sn = sin( rad );
		float fm = *forwardmove, sm = *sidemove;

		*forwardmove = fm * c - sm * sn;
		*sidemove    = fm * sn + sm * c;
	}
}

/*
============
OGC_ApplyPointerToViewModel

Leans the weapon towards where the player is pointing. Purely cosmetic - the
shot still leaves along the view axis - but without it the gun sits dead
centre while the pointer moves independently, which looks wrong.
============
*/
void OGC_ApplyPointerToViewModel( cl_entity_t *view )
{
	float sway;

	if( !view || !wii_ir.value )
		return;

	sway = wii_ir_gunsway.value;
	if( sway == 0.0f )
		return;

	// yaw follows the pointer left/right, pitch follows up/down. Screen y
	// grows downwards while pitch is positive downwards too, so both take
	// the pointer value as-is.
	view->angles[YAW]   -= ogc_pointer[0] * sway;
	view->angles[PITCH] += ogc_pointer[1] * sway;

	VectorCopy( view->angles, view->curstate.angles );
	VectorCopy( view->angles, view->latched.prevangles );
}

/*
============
OGC_PointerMove

Adds the pointer's contribution to the view angles.
============
*/
void OGC_PointerMove( float *pitch, float *yaw )
{
	int   px, py;
	float nx, ny, deadzone, scale;

	if( !wii_ir.value || refState.width <= 0 || refState.height <= 0 )
	{
		ogc_pointer[0] = ogc_pointer[1] = 0.0f;
		return;
	}

	Platform_GetMousePos( &px, &py );

	// -1..1 across the screen, 0 at the centre
	nx = ( px / (float)refState.width  ) * 2.0f - 1.0f;
	ny = ( py / (float)refState.height ) * 2.0f - 1.0f;

	nx = bound( -1.0f, nx, 1.0f );
	ny = bound( -1.0f, ny, 1.0f );

	ogc_pointer[0] = nx;
	ogc_pointer[1] = ny;

	deadzone = bound( 0.0f, wii_ir_deadzone.value, 0.95f );

	// how far past the box we are, renormalised so the turn ramps up from
	// nothing at the edge of the box to full speed at the edge of the screen
	scale = 1.0f - deadzone;
	if( scale <= 0.0f )
		return;

	if( fabs( nx ) > deadzone )
	{
		float over = ( fabs( nx ) - deadzone ) / scale;

		if( nx < 0 ) over = -over;
		*yaw -= over * wii_ir_yawspeed.value * (float)host.realframetime;
	}

	if( fabs( ny ) > deadzone )
	{
		float over = ( fabs( ny ) - deadzone ) / scale;

		if( ny < 0 ) over = -over;
		*pitch += over * wii_ir_pitchspeed.value * (float)host.realframetime;
	}
}
#endif // XASH_OGC
