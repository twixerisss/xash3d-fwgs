/*
vid_ogc.c - video backend for Wii OGC (software and hardware paths)
Copyright (C) 2010-2025 Xash3D FWGS team, ported to Wii GX by Gerardo

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "platform/platform.h"

#if XASH_VIDEO == VIDEO_OGC

#include "common.h"
#include "client.h"
#include "ref_common.h"
#include "filesystem.h"
#include "vid_common.h"

#include <gccore.h>
#include <ogc/gx.h>
#include <ogc/cache.h>
#include <ogc/video.h>
#include <ogc/system.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>

#include "gx_local.h"

#define FIFO_SIZE   (256 * 1024)
#define SW_BPP      16
#define FB_WIDTH    640
#define FB_HEIGHT   480

static void *gp_fifo = NULL;
static void *sw_framebuffer = NULL;
static void *tex_framebuffer = NULL;
static void *xfb[2] = { NULL, NULL };
static int current_xfb = 0;

static int sw_width = FB_WIDTH;
static int sw_height = FB_HEIGHT;
static uint sw_stride = FB_WIDTH * 2;
static uint sw_bpp = SW_BPP;

static qboolean sw_initialized = false;
static qboolean video_initialized = false;
static qboolean gx_hw_renderer = false;
static GXRModeObj *rmode = NULL;
static uint32_t xfb_size = 0;
static uint32_t tex_size = 0;

static GXTexObj texObj;
static Mtx44 ortho;
static Mtx modelview;

static void MakeTexture565( const uint16_t *src, void *dst, int width, int height )
{
	uint16_t *out = (uint16_t *)dst;
	int x, y, ix, iy;

	for( y = 0; y < height; y += 4 )
	{
		for( x = 0; x < width; x += 4 )
		{
			for( iy = 0; iy < 4; iy++ )
			{
				const uint16_t *row = src + (y + iy) * width + x;
				*out++ = row[0];
				*out++ = row[1];
				*out++ = row[2];
				*out++ = row[3];
			}
		}
	}
}

static qboolean VID_InitGX( void )
{
	f32 yscale;
	u32 xfbHeight;
	GXColor background = { 0, 0, 0, 0xff };

	if( video_initialized )
		return true;

	VIDEO_Init();
	rmode = &TVNtsc480IntDf;
	VIDEO_Configure( rmode );

	xfb_size = rmode->fbWidth * rmode->xfbHeight * VI_DISPLAY_PIX_SZ;
	xfb[0] = MEM_K0_TO_K1( SYS_AllocateFramebuffer( rmode ) );
	xfb[1] = MEM_K0_TO_K1( SYS_AllocateFramebuffer( rmode ) );
	if( !xfb[0] || !xfb[1] )
	{
		Con_Reportf( "VID_InitGX: no se pudo asignar XFB\n" );
		return false;
	}

	VIDEO_ClearFrameBuffer( rmode, xfb[0], COLOR_BLACK );
	VIDEO_ClearFrameBuffer( rmode, xfb[1], COLOR_BLACK );
	VIDEO_SetNextFramebuffer( xfb[0] );
	VIDEO_SetBlack( FALSE );
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if( rmode->viTVMode & VI_NON_INTERLACE )
		VIDEO_WaitVSync();

	gp_fifo = memalign( 32, FIFO_SIZE );
	if( !gp_fifo )
	{
		Con_Reportf( "VID_InitGX: no se pudo asignar FIFO\n" );
		return false;
	}
	memset( gp_fifo, 0, FIFO_SIZE );
	GX_Init( gp_fifo, FIFO_SIZE );

	GX_SetCopyClear( background, 0x00ffffff );

	GX_SetViewport( 0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1 );
	yscale = GX_GetYScaleFactor( rmode->efbHeight, rmode->xfbHeight );
	xfbHeight = GX_SetDispCopyYScale( yscale );
	GX_SetScissor( 0, 0, rmode->fbWidth, rmode->efbHeight );
	GX_SetDispCopySrc( 0, 0, rmode->fbWidth, rmode->efbHeight );
	GX_SetDispCopyDst( rmode->fbWidth, xfbHeight );
	GX_SetCopyFilter( rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter );
	GX_SetFieldMode( rmode->field_rendering,
		( ( rmode->viHeight == 2 * rmode->xfbHeight ) ? GX_ENABLE : GX_DISABLE ) );

	if( rmode->aa )
		GX_SetPixelFmt( GX_PF_RGB565_Z16, GX_ZC_LINEAR );
	else
		GX_SetPixelFmt( GX_PF_RGB8_Z24, GX_ZC_LINEAR );

	GX_SetCullMode( GX_CULL_NONE );
	GX_CopyDisp( xfb[0], GX_TRUE );
	GX_SetDispCopyGamma( GX_GM_1_0 );

	guOrtho( ortho, 0, rmode->efbHeight, 0, rmode->fbWidth, 0, 300 );
	GX_LoadProjectionMtx( ortho, GX_ORTHOGRAPHIC );

	guMtxIdentity( modelview );
	GX_LoadPosMtxImm( modelview, GX_PNMTX0 );

	GX_ClearVtxDesc();
	GX_SetVtxDesc( GX_VA_POS, GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0 );

	GX_SetNumChans( 0 );
	GX_SetNumTexGens( 1 );
	GX_SetTexCoordGen( GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY );

	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL );
	GX_SetTevOp( GX_TEVSTAGE0, GX_REPLACE );

	GX_SetZMode( GX_FALSE, GX_ALWAYS, GX_FALSE );
	GX_SetBlendMode( GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR );
	GX_SetColorUpdate( GX_TRUE );
	GX_SetAlphaUpdate( GX_FALSE );

	video_initialized = true;
	Con_Reportf( "VID_InitGX: GX hardware listo %dx%d (software path)\n", rmode->fbWidth, rmode->efbHeight );
	return true;
}

qboolean SW_CreateBuffer( int width, int height, uint *stride, uint *bpp, uint *r, uint *g, uint *b )
{
	if( gx_hw_renderer )
		return false;

	if( !video_initialized && !VID_InitGX() )
		return false;

	sw_width = FB_WIDTH;
	sw_height = FB_HEIGHT;
	sw_bpp = SW_BPP;
	sw_stride = sw_width * ( sw_bpp / 8 );

	if( sw_framebuffer )
		free( sw_framebuffer );

	sw_framebuffer = memalign( 32, sw_height * sw_stride );
	if( !sw_framebuffer )
	{
		Con_Reportf( "SW_CreateBuffer: Out of memory\n" );
		return false;
	}
	memset( sw_framebuffer, 0, sw_height * sw_stride );

	tex_size = GX_GetTexBufferSize( FB_WIDTH, FB_HEIGHT, GX_TF_RGB565, GX_FALSE, 0 );
	if( tex_framebuffer )
		free( tex_framebuffer );
	tex_framebuffer = memalign( 32, tex_size );
	if( !tex_framebuffer )
	{
		Con_Reportf( "SW_CreateBuffer: no se pudo asignar textura\n" );
		free( sw_framebuffer );
		sw_framebuffer = NULL;
		return false;
	}
	memset( tex_framebuffer, 0, tex_size );

	GX_InitTexObj( &texObj, tex_framebuffer, FB_WIDTH, FB_HEIGHT,
		GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE );
	GX_InitTexObjFilterMode( &texObj, GX_NEAR, GX_NEAR );

	*stride = sw_stride;
	*bpp = sw_bpp;
	*r = 0xF800;
	*g = 0x07E0;
	*b = 0x001F;

	sw_initialized = true;
	Con_Reportf( "SW_CreateBuffer: %dx%d RGB565 (camino software)\n", sw_width, sw_height );
	return true;
}

void *SW_LockBuffer( void ) { return sw_framebuffer; }
void SW_UnlockBuffer( void ) {}

void VID_SwapBuffers( void )
{
	if( !video_initialized && !gx_hw_renderer )
		return;

	if( gx_hw_renderer )
	{
		GX_Present();
		return;
	}

	if( !sw_initialized || !sw_framebuffer || !tex_framebuffer )
		return;

	MakeTexture565( (const uint16_t *)sw_framebuffer, tex_framebuffer, sw_width, sw_height );
	DCFlushRange( tex_framebuffer, tex_size );
	GX_InvalidateTexAll();

	GX_LoadTexObj( &texObj, GX_TEXMAP0 );

	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
		GX_Position2f32( 0.0f, 0.0f );
		GX_TexCoord2f32( 0.0f, 0.0f );

		GX_Position2f32( (f32)sw_width, 0.0f );
		GX_TexCoord2f32( 1.0f, 0.0f );

		GX_Position2f32( (f32)sw_width, (f32)sw_height );
		GX_TexCoord2f32( 1.0f, 1.0f );

		GX_Position2f32( 0.0f, (f32)sw_height );
		GX_TexCoord2f32( 0.0f, 1.0f );
	GX_End();

	GX_DrawDone();
	GX_CopyDisp( xfb[current_xfb], GX_TRUE );

	VIDEO_SetNextFramebuffer( xfb[current_xfb] );
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if( rmode->viTVMode & VI_NON_INTERLACE )
		VIDEO_WaitVSync();

	current_xfb ^= 1;
}

qboolean VID_SetMode( void )
{
	return R_ChangeDisplaySettings( FB_WIDTH, FB_HEIGHT, 0 ) == rserr_ok;
}

qboolean R_Init_Video( ref_graphic_apis_t type )
{
	if( type != REF_SOFTWARE && type != REF_GL )
	{
		Con_Reportf( "R_Init_Video: solo soporta REF_SOFTWARE o REF_GL (GX)\n" );
		return false;
	}

	gx_hw_renderer = ( type == REF_GL );

	refState.desktopBitsPixel = gx_hw_renderer ? 24 : 16;

	if( !gx_hw_renderer )
	{
		if( !VID_InitGX() )
			return false;
	}

	if( !VID_SetMode() )
		return false;

	host.renderinfo_changed = false;

	if( gx_hw_renderer )
		Con_Reportf( "R_Init_Video: GX hardware renderer (gx_rmain) activo\n" );
	else
		Con_Reportf( "R_Init_Video: software + GX present listo\n" );

	return true;
}

rserr_t R_ChangeDisplaySettings( int width, int height, window_mode_t window_mode )
{
	width = FB_WIDTH;
	height = FB_HEIGHT;
	Con_Reportf( "%s: forzado %dx%d\n", __func__, width, height );
	R_SaveVideoMode( width, height, width, height, false );

	if( sw_initialized )
	{
		sw_initialized = false;
		if( sw_framebuffer )
		{
			free( sw_framebuffer );
			sw_framebuffer = NULL;
		}
		if( tex_framebuffer )
		{
			free( tex_framebuffer );
			tex_framebuffer = NULL;
		}
	}
	return rserr_ok;
}

int R_MaxVideoModes( void ) { return 1; }
vidmode_t *R_GetVideoMode( int num ) { return NULL; }

void R_Free_Video( void )
{
	if( sw_framebuffer ) { free( sw_framebuffer ); sw_framebuffer = NULL; }
	if( tex_framebuffer ) { free( tex_framebuffer ); tex_framebuffer = NULL; }
	if( gp_fifo ) { free( gp_fifo ); gp_fifo = NULL; }

	sw_initialized = false;
	video_initialized = false;
	gx_hw_renderer = false;
	Con_Reportf( "R_Free_Video: recursos liberados\n" );
}

void VID_RestoreScreenResolution( void ) {}
void VID_Info_f( void )
{
	if( gx_hw_renderer )
		Con_Printf( "Video: GX hardware renderer (nativo)\n" );
	else
		Con_Printf( "Video: Software RGB565 + GX present\n" );
	Con_Printf( "Resolution: %dx%d\n", FB_WIDTH, FB_HEIGHT );
}

#endif