/*
gx_draw.c - orthogonal drawing stuff (Wii GX port)
Copyright (C) 2010 Uncle Mike
Ported to Wii GX by Gerardo

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "gx_local.h"
#include <gccore.h>
#include <ogc/gx.h>

extern float gldepthmin, gldepthmax;

void R_GetTextureParms( int *w, int *h, int texnum )
{
	gl_texture_t *glt = R_GetTexture( texnum );

	if( w ) *w = glt->srcWidth;
	if( h ) *h = glt->srcHeight;
}

void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, int texnum )
{
	GX_Bind( XASH_TEXTURE0, texnum );

	GX_ClearVtxDesc();
	GX_SetVtxDesc( GX_VA_POS,  GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );

	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS,  GX_POS_XY, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0 );

	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );

		GX_Position2f32( x, y );
		GX_TexCoord2f32( s1, t1 );

		GX_Position2f32( x + w, y );
		GX_TexCoord2f32( s2, t1 );

		GX_Position2f32( x + w, y + h );
		GX_TexCoord2f32( s2, t2 );

		GX_Position2f32( x, y + h );
		GX_TexCoord2f32( s1, t2 );

	GX_End();
}

static void GX_ConvertToRGBA8( byte *dst, const byte *src, int width, int height, pixformat_t fmt )
{
	int bpp, rOff, gOff, bOff, aOff;
	qboolean hasAlpha = true;

	switch( fmt )
	{
	case PF_RGBA_32:
		bpp = 4; rOff = 0; gOff = 1; bOff = 2; aOff = 3;
		break;
	case PF_BGRA_32:
		bpp = 4; rOff = 2; gOff = 1; bOff = 0; aOff = 3;
		break;
	case PF_RGB_24:
		bpp = 3; rOff = 0; gOff = 1; bOff = 2; aOff = -1; hasAlpha = false;
		break;
	case PF_BGR_24:
		bpp = 3; rOff = 2; gOff = 1; bOff = 0; aOff = -1; hasAlpha = false;
		break;
	case PF_LUMINANCE:
		bpp = 1; rOff = gOff = bOff = 0; aOff = -1; hasAlpha = false;
		break;
	default:
		gEngfuncs.Con_DPrintf( S_ERROR "%s: unsupported pixel format %i\n", __func__, fmt );
		return;
	}

	for( int ty = 0; ty < height; ty += 4 )
	{
		for( int tx = 0; tx < width; tx += 4 )
		{
			byte *arBlock = dst;
			byte *gbBlock = dst + 32;

			for( int y = 0; y < 4; y++ )
			{
				int sy = ty + y;
				if( sy >= height ) sy = height - 1;

				for( int x = 0; x < 4; x++ )
				{
					int sx = tx + x;
					if( sx >= width ) sx = width - 1;

					const byte *texel = src + ( sy * width + sx ) * bpp;
					byte r = texel[rOff];
					byte g = texel[gOff];
					byte b = texel[bOff];
					byte a = hasAlpha ? texel[aOff] : 255;

					*arBlock++ = a;
					*arBlock++ = r;
					*gbBlock++ = g;
					*gbBlock++ = b;
				}
			}

			dst += 64;
		}
	}
}

void GX_UpdateTexture( int texnum, int cols, int rows, int width, int height, const byte *buffer, pixformat_t fmt )
{
	switch( fmt )
	{
	case PF_RGBA_32:
	case PF_BGRA_32:
	case PF_RGB_24:
	case PF_BGR_24:
	case PF_LUMINANCE:
		break;
	default:
		gEngfuncs.Con_DPrintf( S_ERROR "%s: unsupported pixel format %i\n", __func__, fmt );
		return;
	}

	width  = ( width  + 3 ) & ~3;
	height = ( height + 3 ) & ~3;

	byte *raw;
	if( cols != width || rows != height )
	{
		raw = GX_ResampleTexture( buffer, cols, rows, width, height, false );
		cols = width;
		rows = height;
	}
	else
		raw = (byte *)buffer;

	if( cols > glConfig.max_2d_texture_size )
		gEngfuncs.Host_Error( "%s: size %i exceeds hardware limits\n", __func__, cols );
	if( rows > glConfig.max_2d_texture_size )
		gEngfuncs.Host_Error( "%s: size %i exceeds hardware limits\n", __func__, rows );

	gl_texture_t *tex = R_GetTexture( texnum );

	size_t nativeSize = (size_t)( cols * rows ) * 4;

	if( cols == (int)tex->width && rows == (int)tex->height && tex->nativeData != NULL )
	{
		GX_ConvertToRGBA8( (byte *)tex->nativeData, raw, cols, rows, fmt );
		DCFlushRange( tex->nativeData, nativeSize );
		GX_InvalidateTexAll();
	}
	else
	{
		if( tex->nativeData != NULL )
		{
			free( tex->nativeData );
			tex->nativeData = NULL;
		}

		tex->nativeData = memalign( 32, nativeSize );
		tex->width  = cols;
		tex->height = rows;

		GX_ConvertToRGBA8( (byte *)tex->nativeData, raw, cols, rows, fmt );
		DCFlushRange( tex->nativeData, nativeSize );

		GX_InitTexObj( &tex->texObj, tex->nativeData, (u16)cols, (u16)rows,
			GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE );
	}

	GX_ApplyTextureParams( tex );
}

void GX_ReadPixelsRGBA( int x, int y, int w, int h, byte *out )
{
    (void)x; (void)y;

    int tw = ( w + 3 ) & ~3;
    int th = ( h + 3 ) & ~3;

    size_t tileSize = (size_t)tw * th * 4;

    void *tileBuf = memalign( 32, tileSize );
    if( !tileBuf )
    {
        gEngfuncs.Con_DPrintf( S_ERROR "%s: out of memory (%zu bytes)\n", __func__, tileSize );
        return;
    }

    GX_DrawDone();
    GX_CopyTex( tileBuf, GX_FALSE );
    GX_PixModeSync();
    GX_DrawDone();
    DCInvalidateRange( tileBuf, tileSize );

    const byte *src = (const byte *)tileBuf;

    for( int ty = 0; ty < th; ty += 4 )
    {
        for( int tx = 0; tx < tw; tx += 4 )
        {
            const byte *arBlock = src;
            const byte *gbBlock = src + 32;

            for( int row = 0; row < 4; row++ )
            {
                int py = ty + row;

                for( int col = 0; col < 4; col++ )
                {
                    int px = tx + col;
                    int i  = row * 4 + col;

                    byte a = arBlock[i * 2 + 0];
                    byte r = arBlock[i * 2 + 1];
                    byte g = gbBlock[i * 2 + 0];
                    byte b = gbBlock[i * 2 + 1];

                    if( px < w && py < h )
                    {
                        byte *dst = out + ( py * w + px ) * 4;
                        dst[0] = r;
                        dst[1] = g;
                        dst[2] = b;
                        dst[3] = a;
                    }
                }
            }

            src += 64;
        }
    }

    free( tileBuf );
}

void R_Set2DMode( qboolean enable )
{
	static u8 savedProjType = GX_PERSPECTIVE;

	if( enable )
	{
		if( glState.in2DMode )
			return;

		savedProjType = FBitSet( RI.rvp.flags, RF_DRAW_OVERVIEW ) ? GX_ORTHOGRAPHIC : GX_PERSPECTIVE;

		matrix4x4 projection_matrix;

		switch( tr.rotation )
		{
		case REF_ROTATE_CW:
			GX_SetViewport( 0, 0, gpGlobals->height, gpGlobals->width, 0.0f, 1.0f );
			Matrix4x4_CreateOrtho( projection_matrix, 0, gpGlobals->height, gpGlobals->width, 0, -99999, 99999 );
			Matrix4x4_ConcatRotate( projection_matrix, 90, 0, 0, 1 );
			Matrix4x4_ConcatTranslate( projection_matrix, 0, -gpGlobals->height, 0 );
			break;
		case REF_ROTATE_CCW:
			GX_SetViewport( 0, 0, gpGlobals->height, gpGlobals->width, 0.0f, 1.0f );
			Matrix4x4_CreateOrtho( projection_matrix, 0, gpGlobals->height, gpGlobals->width, 0, -99999, 99999 );
			Matrix4x4_ConcatRotate( projection_matrix, -90, 0, 0, 1 );
			Matrix4x4_ConcatTranslate( projection_matrix, -gpGlobals->width, 0, 0 );
			break;
		default:
			GX_SetViewport( 0, 0, gpGlobals->width, gpGlobals->height, 0.0f, 1.0f );
			Matrix4x4_CreateOrtho( projection_matrix, 0, gpGlobals->width, gpGlobals->height, 0, -99999, 99999 );
			break;
		}

		Mtx44 gxProj;
		Matrix4x4_ToMtx44( gxProj, projection_matrix );
		GX_LoadProjectionMtx( gxProj, GX_ORTHOGRAPHIC );
		GX_SaveProjectionMtx( gxProj, GX_ORTHOGRAPHIC );

		matrix4x4 worldview_matrix;
		Matrix4x4_LoadIdentity( worldview_matrix );

		Mtx gxMv;
		Matrix4x4_ToMtx( gxMv, worldview_matrix );
		GX_LoadPosMtxImm( gxMv, GX_PNMTX0 );

		GX_SetCullMode( GX_CULL_NONE );
		GX_SetZMode( GX_FALSE, GX_ALWAYS, GX_FALSE );
		GX_SetAlphaCompare( GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0 );

		GXColor white = { 255, 255, 255, 255 };
		GX_SetChanMatColor( GX_COLOR0A0, white );

		glState.in2DMode = true;
		RI.currententity = NULL;
		RI.currentmodel = NULL;
	}
	else
	{
		GX_SetZMode( GX_TRUE, GX_LEQUAL, GX_TRUE );
		glState.in2DMode = false;

		Mtx44 gxProj;
		Matrix4x4_ToMtx44( gxProj, RI.projectionMatrix );
		GX_LoadProjectionMtx( gxProj, savedProjType );
		GX_SaveProjectionMtx( gxProj, savedProjType );

		Mtx gxMv;
		Matrix4x4_ToMtx( gxMv, RI.worldviewMatrix );
		GX_LoadPosMtxImm( gxMv, GX_PNMTX0 );

		GX_SetCullMode( GX_CULL_FRONT );
	}
}