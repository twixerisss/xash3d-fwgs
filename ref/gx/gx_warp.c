/*
gx_warp.c - sky and water polygons (Wii GX native port)
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
#include "wadfile.h"

#define SKYCLOUDS_QUALITY	12
#define MAX_CLIP_VERTS	128

static const int r_skyTexOrder[SKYBOX_MAX_SIDES] = { 0, 2, 1, 3, 4, 5 };

static const vec3_t skyclip[SKYBOX_MAX_SIDES] =
{
{  1,  1,  0 },
{  1, -1,  0 },
{  0, -1,  1 },
{  0,  1,  1 },
{  1,  0,  1 },
{ -1,  0,  1 }
};

static const int st_to_vec[SKYBOX_MAX_SIDES][3] =
{
{  3, -1,  2 },
{ -3,  1,  2 },
{  1,  3,  2 },
{ -1, -3,  2 },
{ -2, -1,  3 },
{  2, -1, -3 }
};

static const int vec_to_st[SKYBOX_MAX_SIDES][3] =
{
{ -2,  3,  1 },
{  2,  3, -1 },
{  1,  3,  2 },
{ -1,  3, -2 },
{ -2, -1,  3 },
{ -2,  1, -3 }
};

#define RIPPLES_CACHEWIDTH_BITS 7
#define RIPPLES_CACHEWIDTH ( 1 << RIPPLES_CACHEWIDTH_BITS )
#define RIPPLES_CACHEWIDTH_MASK (( RIPPLES_CACHEWIDTH ) - 1 )
#define RIPPLES_TEXSIZE ( RIPPLES_CACHEWIDTH * RIPPLES_CACHEWIDTH )
#define RIPPLES_TEXSIZE_MASK ( RIPPLES_TEXSIZE - 1 )

STATIC_ASSERT( RIPPLES_TEXSIZE == 0x4000, "fix the algorithm to work with custom resolution" );

static struct
{
	double time;
	double oldtime;

	short *curbuf, *oldbuf;
	short buf[2][RIPPLES_TEXSIZE];
	qboolean update;

	uint32_t texture[RIPPLES_TEXSIZE];
} g_ripple;

static void GX_SetupVtxFormat( void )
{
	GX_ClearVtxDesc();
	GX_SetVtxDesc( GX_VA_POS,  GX_DIRECT );
	GX_SetVtxDesc( GX_VA_TEX0, GX_DIRECT );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_POS,  GX_POS_XYZ, GX_F32, 0 );
	GX_SetVtxAttrFmt( GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0 );
}

static float gx_fog_density_backup = -1.0f;

static void GX_PushFogDensity( float scale )
{
	if( !glState.isFogEnabled )
		return;
	gx_fog_density_backup = RI.fogDensity;
	RI.fogDensity *= scale;
	R_DrawFog();
}

static void GX_PopFogDensity( void )
{
	if( gx_fog_density_backup < 0.0f )
		return;
	RI.fogDensity = gx_fog_density_backup;
	gx_fog_density_backup = -1.0f;
	R_DrawFog();
}

static void DrawSkyPolygon( int nump, vec3_t vecs )
{
	vec3_t v;
	VectorClear( v );

	float *vp = vecs;
	for( int i = 0; i < nump; i++, vp += 3 )
		VectorAdd( vp, v, v );

	vec3_t av;
	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );

	int axis;
	if( av[0] > av[1] && av[0] > av[2] )
		axis = (v[0] < 0) ? 1 : 0;
	else if( av[1] > av[2] && av[1] > av[0] )
		axis = (v[1] < 0) ? 3 : 2;
	else axis = (v[2] < 0) ? 5 : 4;

	for( int i = 0; i < nump; i++, vecs += 3 )
	{
		int j = vec_to_st[axis][2];
		float dv = (j > 0) ? vecs[j-1] : -vecs[-j-1];

		if( dv == 0.0f ) continue;

		j = vec_to_st[axis][0];
		float s = (j < 0) ? -vecs[-j-1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		float t = (j < 0) ? -vecs[-j-1] / dv : vecs[j-1] / dv;

		if( s < RI.skyMins[0][axis] ) RI.skyMins[0][axis] = s;
		if( t < RI.skyMins[1][axis] ) RI.skyMins[1][axis] = t;
		if( s > RI.skyMaxs[0][axis] ) RI.skyMaxs[0][axis] = s;
		if( t > RI.skyMaxs[1][axis] ) RI.skyMaxs[1][axis] = t;
	}
}

static void ClipSkyPolygon( int nump, vec3_t vecs, int stage )
{
	const float	*norm;
	float		*v, d, e;
	qboolean		front, back;
	float		dists[MAX_CLIP_VERTS + 1];
	int		sides[MAX_CLIP_VERTS + 1];
	vec3_t		newv[2][MAX_CLIP_VERTS + 1];
	int		newc[2];
	int		i, j;

	if( nump > MAX_CLIP_VERTS )
		gEngfuncs.Host_Error( "%s: MAX_CLIP_VERTS\n", __func__ );
loc1:
	if( stage == 6 )
	{
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if( d < -ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if( !front || !back )
	{
		stage++;
		goto loc1;
	}

	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, ( vecs + ( i * 3 )));
	newc[0] = newc[1] = 0;

	for( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		switch( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i+1] );
		for( j = 0; j < 3; j++ )
		{
			e = v[j] + d * ( v[j+3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

static void MakeSkyVec( float s, float t, int axis )
{
	int farclip = RI.farClip;

	vec3_t b;
	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	vec3_t v;
	for( int j = 0; j < 3; j++ )
	{
		int k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += RI.cullorigin[j];
	}

	s = (s + 1.0f) * 0.5f;
	t = (t + 1.0f) * 0.5f;

	s = bound( 0.0f, s, 1.0f );
	t = bound( 0.0f, t, 1.0f );

	t = 1.0f - t;

	GX_Position3f32( v[0], v[1], v[2] );
	GX_TexCoord2f32( s, t );
}

void R_ClearSkyBox( void )
{
	for( int i = 0; i < SKYBOX_MAX_SIDES; i++ )
	{
		RI.skyMins[0][i] = RI.skyMins[1][i] = 9999999.0f;
		RI.skyMaxs[0][i] = RI.skyMaxs[1][i] = -9999999.0f;
	}
}

void R_AddSkyBoxSurface( msurface_t *fa )
{
	if( FBitSet( tr.world->flags, FWORLD_SKYSPHERE ) && fa->polys && !FBitSet( tr.world->flags, FWORLD_CUSTOM_SKYBOX ))
	{
		glpoly2_t *p = fa->polys;

		GX_SetupVtxFormat();
		GX_Begin( GX_TRIANGLE_FAN, GX_VTXFMT0, p->numverts );
		float *v = p->verts[0];
		for( int i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			GX_Position3f32( v[0], v[1], v[2] );
			GX_TexCoord2f32( v[3], v[4] );
		}
		GX_End();
	}

	vec3_t verts[MAX_CLIP_VERTS];
	for( glpoly2_t *p = fa->polys; p; p = p->next )
	{
		for( int i = 0; i < p->numverts; i++ )
			VectorSubtract( p->verts[i], RI.cullorigin, verts[i] );
		ClipSkyPolygon( p->numverts, verts[0], 0 );
	}
}

void R_UnloadSkybox( void )
{
	for( int i = 0; i < SKYBOX_MAX_SIDES; i++ )
	{
		if( !tr.skyboxTextures[i] ) continue;
		GX_FreeTexture( tr.skyboxTextures[i] );
	}

	tr.skyboxbasenum = SKYBOX_BASE_NUM;

	memset( tr.skyboxTextures, 0, sizeof( tr.skyboxTextures ));
	ClearBits( tr.world->flags, FWORLD_CUSTOM_SKYBOX );
}

void R_DrawSkyBox( void )
{
	if( !RI.fogSkybox ) R_AllowFog( false );

	if( RI.fogEnabled )
		GX_PushFogDensity( 0.5f );

	GX_SetBlendMode( GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR );
	GX_SetAlphaCompare( GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0 );
	GX_SetTevOp( GX_TEVSTAGE0, GX_REPLACE );
	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0 );

	GX_SetupVtxFormat();

	for( int i = 0; i < SKYBOX_MAX_SIDES; i++ )
	{
		if( RI.skyMins[0][i] >= RI.skyMaxs[0][i] || RI.skyMins[1][i] >= RI.skyMaxs[1][i] )
			continue;

		if( tr.skyboxTextures[r_skyTexOrder[i]] )
			GX_Bind( XASH_TEXTURE0, tr.skyboxTextures[r_skyTexOrder[i]] );
		else GX_Bind( XASH_TEXTURE0, tr.grayTexture );

		GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
		MakeSkyVec( RI.skyMins[0][i], RI.skyMins[1][i], i );
		MakeSkyVec( RI.skyMins[0][i], RI.skyMaxs[1][i], i );
		MakeSkyVec( RI.skyMaxs[0][i], RI.skyMaxs[1][i], i );
		MakeSkyVec( RI.skyMaxs[0][i], RI.skyMins[1][i], i );
		GX_End();
	}

	GX_SetTevOp( GX_TEVSTAGE0, GX_MODULATE );
	GX_SetTevOrder( GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0 );

	if( RI.fogEnabled )
		GX_PopFogDensity();

	if( !RI.fogSkybox )
		R_AllowFog( true );

	R_LoadIdentity();
}

static void R_CloudVertex( float s, float t, int axis, vec3_t v )
{
	int farclip = RI.farClip;

	vec3_t b;
	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for( int j = 0; j < 3; j++ )
	{
		int k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k-1] : b[k-1];
		v[j] += RI.cullorigin[j];
	}
}

static void R_CloudTexCoord( const vec3_t v, float speed, float *s, float *t )
{
	float speedscale = gp_cl->time * speed;
	speedscale -= (int)speedscale & ~127;

	vec3_t dir;
	VectorSubtract( v, RI.rvp.vieworigin, dir );
	dir[2] *= 3.0f;

	float length = VectorLength( dir );
	length = 6.0f * 63.0f / length;

	*s = ( speedscale + dir[0] * length ) * (1.0f / 128.0f);
	*t = ( speedscale + dir[1] * length ) * (1.0f / 128.0f);
}

static void R_CloudDrawPoly( const float *verts )
{
	GX_SetRenderMode( kRenderNormal );
	GX_Bind( XASH_TEXTURE0, tr.solidskyTexture );

	GX_SetupVtxFormat();
	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	const float *v = verts;
	for( int i = 0; i < 4; i++, v += VERTEXSIZE )
	{
		float s, t;
		R_CloudTexCoord( v, 8.0f, &s, &t );
		GX_Position3f32( v[0], v[1], v[2] );
		GX_TexCoord2f32( s, t );
	}
	GX_End();

	GX_SetRenderMode( kRenderTransTexture );
	GX_Bind( XASH_TEXTURE0, tr.alphaskyTexture );

	GX_Begin( GX_QUADS, GX_VTXFMT0, 4 );
	v = verts;
	for( int i = 0; i < 4; i++, v += VERTEXSIZE )
	{
		float s, t;
		R_CloudTexCoord( v, 16.0f, &s, &t );
		GX_Position3f32( v[0], v[1], v[2] );
		GX_TexCoord2f32( s, t );
	}
	GX_End();

	GX_SetBlendMode( GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR );
}

static void R_CloudRenderSide( int axis )
{
	vec3_t verts[4];
	R_CloudVertex( -1.0f, -1.0f, axis, verts[0] );
	R_CloudVertex( -1.0f,  1.0f, axis, verts[1] );
	R_CloudVertex(  1.0f,  1.0f, axis, verts[2] );
	R_CloudVertex(  1.0f, -1.0f, axis, verts[3] );

	vec3_t vup, vright;
	VectorSubtract( verts[2], verts[3], vup );
	VectorSubtract( verts[2], verts[1], vright );

	float di = SKYCLOUDS_QUALITY;
	float qi = 1.0f / di;
	float dj = (axis < 4) ? di * 2 : di;
	float qj = 1.0f / dj;

	for( int i = 0; i < di; i++ )
	{
		for( int j = 0; j < dj; j++ )
		{
			if( i * qi < RI.skyMins[0][axis] / 2 + 0.5f - qi
			 || i * qi > RI.skyMaxs[0][axis] / 2 + 0.5f
			 || j * qj < RI.skyMins[1][axis] / 2 + 0.5f - qj
			 || j * qj > RI.skyMaxs[1][axis] / 2 + 0.5f )
				continue;

			vec3_t temp, temp2;
			VectorScale( vright, qi * i, temp );
			VectorScale( vup, qj * j, temp2 );
			VectorAdd( temp, temp2, temp );
			float final_verts[4][VERTEXSIZE];
			VectorAdd( verts[0], temp, final_verts[0] );

			VectorScale( vup, qj, temp );
			VectorAdd( final_verts[0], temp, final_verts[1] );

			VectorScale( vright, qi, temp );
			VectorAdd( final_verts[1], temp, final_verts[2] );

			VectorAdd( final_verts[0], temp, final_verts[3] );

			R_CloudDrawPoly( final_verts[0] );
		}
	}
}

void R_DrawClouds( void )
{
	if( RI.fogEnabled )
		GX_PushFogDensity( 0.25f );

	GX_SetZMode( GX_TRUE, GX_GEQUAL, GX_FALSE );

	for( int i = 0; i < SKYBOX_MAX_SIDES; i++ )
	{
		if( RI.skyMins[0][i] >= RI.skyMaxs[0][i] || RI.skyMins[1][i] >= RI.skyMaxs[1][i] )
			continue;
		R_CloudRenderSide( i );
	}

	GX_SetZMode( GX_TRUE, GX_LEQUAL, GX_TRUE );

	if( RI.fogEnabled )
		GX_PopFogDensity();
}

void R_ResetRipples( void )
{
	g_ripple.curbuf = g_ripple.buf[0];
	g_ripple.oldbuf = g_ripple.buf[1];
	g_ripple.time = g_ripple.oldtime = gp_cl->time - 0.1;
	memset( g_ripple.buf, 0, sizeof( g_ripple.buf ));
}

static void R_SwapBufs( void )
{
	short *tempbufp = g_ripple.curbuf;
	g_ripple.curbuf = g_ripple.oldbuf;
	g_ripple.oldbuf = tempbufp;
}

static void R_SpawnNewRipple( int x, int y, short val )
{
#define PIXEL( x, y ) ((( x ) & RIPPLES_CACHEWIDTH_MASK ) + ((( y ) & RIPPLES_CACHEWIDTH_MASK) << 7 ))
	g_ripple.oldbuf[PIXEL( x, y )] += val;

	val >>= 2;
	g_ripple.oldbuf[PIXEL( x + 1, y )] += val;
	g_ripple.oldbuf[PIXEL( x - 1, y )] += val;
	g_ripple.oldbuf[PIXEL( x, y + 1 )] += val;
	g_ripple.oldbuf[PIXEL( x, y - 1 )] += val;
#undef PIXEL
}

static void R_RunRipplesAnimation( const short *oldbuf, short *pbuf )
{
	size_t i = 0;
	const int w = RIPPLES_CACHEWIDTH;
	const int m = RIPPLES_TEXSIZE_MASK;

	for( i = w; i < m + w; i++, pbuf++ )
	{
		*pbuf = (
			( (int)oldbuf[( i - ( w * 2 )) & m]
			+ (int)oldbuf[( i - ( w + 1 )) & m]
			+ (int)oldbuf[( i - ( w - 1 )) & m]
			+ (int)oldbuf[( i ) & m]) >> 1 ) - (int)*pbuf;

		*pbuf -= ( *pbuf >> 6 );
	}
}

void R_AnimateRipples( void )
{
	double frametime = gp_cl->time - g_ripple.time;

	g_ripple.update = r_ripple.value && frametime >= r_ripple_updatetime.value;

	if( !g_ripple.update )
		return;

	g_ripple.time = gp_cl->time;

	R_SwapBufs();

	if( g_ripple.time - g_ripple.oldtime > r_ripple_spawntime.value )
	{
		int x, y, val;

		g_ripple.oldtime = g_ripple.time;

		x = rand() & 0x7fff;
		y = rand() & 0x7fff;
		val = rand() & 0x3ff;

		R_SpawnNewRipple( x, y, val );
	}

	R_RunRipplesAnimation( g_ripple.oldbuf, g_ripple.curbuf );
}

static void R_GetRippleTextureSize( const texture_t *image, int *width, int *height )
{
	if( image->width > image->height )
	{
		*width = RIPPLES_CACHEWIDTH;
		*height = (float)image->height / image->width * RIPPLES_CACHEWIDTH;
	}
	else if( image->width < image->height )
	{
		*width = (float)image->width / image->height * RIPPLES_CACHEWIDTH;
		*height = RIPPLES_CACHEWIDTH;
	}
	else
	{
		*width = *height = RIPPLES_CACHEWIDTH;
	}
}

qboolean R_UploadRipples( texture_t *image )
{
	qboolean update = g_ripple.update;

	if( !r_ripple.value )
	{
		GX_Bind( XASH_TEXTURE0, image->gl_texturenum );
		return false;
	}

	const gl_texture_t *glt = R_GetTexture( image->gl_texturenum );
	if( !glt || !glt->original || !glt->original->buffer )
	{
		GX_Bind( XASH_TEXTURE0, image->gl_texturenum );
		return false;
	}

	int width, height;
	if( !image->fb_texturenum )
	{
		rgbdata_t pic = { 0 };
		string name;

		Q_snprintf( name, sizeof( name ), "*rippletex_%s", image->name );
		R_GetRippleTextureSize( image, &width, &height );

		pic.width = width;
		pic.height = height;
		pic.depth = 1;
		pic.flags = IMAGE_HAS_COLOR;
		pic.buffer = (byte *)g_ripple.texture;
		pic.type = PF_RGBA_32;
		pic.size = width * height * 4;
		pic.numMips = 1;
		memset( pic.buffer, 0, pic.size );

		image->fb_texturenum = GX_LoadTextureInternal( name, &pic, TF_NOMIPMAP | TF_ALLOW_NEAREST );

		update = true;
		image->dt_texturenum = ( tr.framecount - 1 ) & 0xFFFF;
	}

	GX_Bind( XASH_TEXTURE0, image->fb_texturenum );

	if( !update || image->dt_texturenum == ( tr.framecount & 0xFFFF ))
		return true;

	image->dt_texturenum = tr.framecount & 0xFFFF;

	R_GetRippleTextureSize( image, &width, &height );

	int size = r_ripple.value == 1.0f ? 64 : RIPPLES_CACHEWIDTH;
	const uint32_t *pixels = (const uint32_t *)glt->original->buffer;

	for( int y = 0; y < height; y++ )
	{
		int ry = (float)y / height * size;

		for( int x = 0; x < width; x++ )
		{
			int rx = (float)x / width * size;
			int val = g_ripple.curbuf[ry * RIPPLES_CACHEWIDTH + rx] / 16;

			int rpy = ( y - val ) % height;
			int rpx = ( x + val ) % width;

			int py = (float)rpy / height * image->height;
			int px = (float)rpx / width * image->width;

			if( py < 0 ) py = image->height + py;
			if( px < 0 ) px = image->width + px;

			g_ripple.texture[y * width + x] = pixels[py * image->width + px];
		}
	}

	GX_UpdateTexture( image->fb_texturenum, width, height, width, height, (byte *)g_ripple.texture, PF_RGBA_32 );

	return true;
}