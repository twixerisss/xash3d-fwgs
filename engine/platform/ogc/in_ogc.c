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

// where the player is pointing, as -1..1 from the centre of the screen.
// kept here so the view code can lean the weapon towards it.
static vec2_t ogc_pointer;

void OGC_InputInit( void )
{
	Cvar_RegisterVariable( &wii_ir );
	Cvar_RegisterVariable( &wii_ir_deadzone );
	Cvar_RegisterVariable( &wii_ir_yawspeed );
	Cvar_RegisterVariable( &wii_ir_pitchspeed );
	Cvar_RegisterVariable( &wii_ir_gunsway );
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
