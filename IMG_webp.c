/*
    SDL_image:  An example image loading library for use with SDL
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/* This is a WEBP image file loading framework */

#include <stdlib.h>
#include <stdio.h>

#include "SDL_image.h"

#ifdef LOAD_WEBP

/*=============================================================================
        File: SDL_webp.c
     Purpose: A WEBP loader for the SDL library      
    Revision: 
  Created by: Michael Bonfils (Murlock) (26 November 2011)
              murlock42@gmail.com
 Modified by: 

 Copyright notice:
          This library is free software; you can redistribute it and/or
          modify it under the terms of the GNU Library General Public
          License as published by the Free Software Foundation; either
          version 2 of the License, or (at your option) any later version.
 
          This library is distributed in the hope that it will be useful,
          but WITHOUT ANY WARRANTY; without even the implied warranty of
          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
          Library General Public License for more details.
 
          You should have received a copy of the GNU Library General Public
          License along with this library; if not, write to the Free
          Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Comments:
            

  Changes:

=============================================================================*/

#include "SDL_endian.h"

#ifdef macintosh
#define MACOS
#endif
#include <webp/decode.h>

static struct {
	int loaded;
	void *handle;
	int/*VP8StatuCode*/ (*webp_get_features_internal) (const uint8_t *data, uint32_t data_size, WebPBitstreamFeatures* const features, int decoder_abi_version);
	uint8_t*	(*webp_decode_rgb_into) (const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int output_stride);
	uint8_t*	(*webp_decode_rgba_into) (const uint8_t* data, uint32_t data_size, uint8_t* output_buffer, int output_buffer_size, int output_stride);
} lib;

#ifdef LOAD_WEBP_DYNAMIC
int IMG_InitWEBP()
{
	if ( lib.loaded == 0 ) {
		lib.handle = SDL_LoadObject(LOAD_WEBP_DYNAMIC);
		if ( lib.handle == NULL ) {
			return -1;
		}
		lib.webp_get_features_internal = 
			( int (*) (const uint8_t *, uint32_t, WebPBitstreamFeatures* const, int) )
			SDL_LoadFunction(lib.handle, "WebPGetFeaturesInternal" );
		if ( lib.webp_get_features_internal == NULL ) {
			SDL_UnloadObject(lib.handle);
			return -1;
		}

		lib.webp_decode_rgb_into = 
			( uint8_t* (*) (const uint8_t*, uint32_t, uint8_t*, int, int ) )
			SDL_LoadFunction(lib.handle, "WebPDecodeRGBInto" );
		if ( lib.webp_decode_rgb_into == NULL ) {
			SDL_UnloadObject(lib.handle);
			return -1;
		}

		lib.webp_decode_rgba_into = 
			( uint8_t* (*) (const uint8_t*, uint32_t, uint8_t*, int, int ) )
			SDL_LoadFunction(lib.handle, "WebPDecodeRGBInto" );
		if ( lib.webp_decode_rgba_into == NULL ) {
			SDL_UnloadObject(lib.handle);
			return -1;
		}
	}
	++lib.loaded;

	return 0;
}
void IMG_QuitWEBP()
{
	if ( lib.loaded == 0 ) {
		return;
	}
	if ( lib.loaded == 1 ) {
		SDL_UnloadObject(lib.handle);
	}
	--lib.loaded;
}
#else
int IMG_InitWEBP()
{
	if ( lib.loaded == 0 ) {
		lib.webp_get_features_internal = WebPGetFeaturesInternal;
		lib.webp_decode_rgb_into = WebPDecodeRGBInto;
		lib.webp_decode_rgba_into = WebPDecodeRGBAInto;
	}
	++lib.loaded;

	return 0;
}
void IMG_QuitWEBP()
{
	if ( lib.loaded == 0 ) {
		return;
	}
	if ( lib.loaded == 1 ) {
	}
	--lib.loaded;
}
#endif /* LOAD_WEBP_DYNAMIC */

static int webp_getinfo( SDL_RWops *src, int *datasize ) {
	int start;
	int is_WEBP;
	Uint8 magic[20];

	if ( !src )
		return 0;
	start = SDL_RWtell(src);
	is_WEBP = 0;
	if ( SDL_RWread(src, magic, 1, sizeof(magic)) == sizeof(magic) ) {
		if ( magic[ 0] == 'R' &&
                     magic[ 1] == 'I' &&
                     magic[ 2] == 'F' &&
                     magic[ 3] == 'F' &&
										 magic[ 8] == 'W' &&
										 magic[ 9] == 'E' &&
										 magic[10] == 'B' &&
										 magic[11] == 'P' &&
										 magic[12] == 'V' &&
										 magic[13] == 'P' &&
										 magic[14] == '8' &&
										 magic[15] == ' '  ) {
			is_WEBP = 1;
			int data = magic[16] | magic[17]<<8 | magic[18]<<16 | magic[19]<<24;
			if ( datasize )
				*datasize = data;
		}
	}
	SDL_RWseek(src, start, RW_SEEK_SET);
	return(is_WEBP);
}

/* See if an image is contained in a data source */
int IMG_isWEBP(SDL_RWops *src)
{
	return webp_getinfo( src, NULL );
}

SDL_Surface *IMG_LoadWEBP_RW(SDL_RWops *src)
{
	int start;
	const char *error = NULL;
	SDL_Surface *volatile surface = NULL;
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;

	if ( !src ) {
		/* The error message has been set in SDL_RWFromFile */
		return NULL;
	}

	start = SDL_RWtell(src);

	if ( !IMG_Init(IMG_INIT_WEBP) ) {
		goto error;
	}


	int raw_data_size = -1;
	if ( !webp_getinfo( src, &raw_data_size ) ) {
		error = "Invalid WEBP";
		goto error;
	}

	// skip header
	SDL_RWseek(src, start+20, RW_SEEK_SET );

	uint8_t *raw_data = (uint8_t*) malloc( raw_data_size );
	if ( raw_data == NULL ) {
		error = "Failed to allocate enought buffer for WEBP";
		goto error;
	}

	int r = SDL_RWread(src, raw_data, 1, raw_data_size );
	if ( r != raw_data_size ) {
		error = "Failed to read WEBP";
		goto error;
	}
	
#if 0
	// extract size of picture, not interesting since we don't know about alpha channel
	int width = -1, height = -1;
	if ( !WebPGetInfo( raw_data, raw_data_size, &width, &height ) ) {
		printf("WebPGetInfo has failed\n" );
		return NULL;
	}
#endif

	WebPBitstreamFeatures features;
  if ( lib.webp_get_features_internal( raw_data, raw_data_size, &features, WEBP_DECODER_ABI_VERSION ) != VP8_STATUS_OK ) {
		error = "WebPGetFeatures has failed";
		return NULL;
	}

	/* Check if it's ok !*/
	Rmask = 0x000000FF;
	Gmask = 0x0000FF00;
	Bmask = 0x00FF0000;
	Amask = features.has_alpha?0xFF000001:0;

	surface = SDL_AllocSurface(SDL_SWSURFACE, features.width, features.height,
			features.has_alpha?32:24, Rmask,Gmask,Bmask,Amask);

	if ( surface == NULL ) {
		error = "Failed to allocate SDL_Surface";
		goto error;
	}


	uint8_t *ret = NULL;
	if ( features.has_alpha ) {
		ret = lib.webp_decode_rgba_into( raw_data, raw_data_size, surface->pixels, surface->pitch * surface->h,  surface->pitch );
	} else {
		ret = lib.webp_decode_rgb_into( raw_data, raw_data_size, surface->pixels, surface->pitch * surface->h,  surface->pitch );
	}

	if ( !ret ) {
		error = "Failed to decode WEBP";
		goto error;
	}

	return surface;


error:

	if ( surface ) {
		SDL_FreeSurface( surface );
	}

	if ( raw_data ) {
		free( raw_data );
	}

	if ( error ) {
		IMG_SetError( error );
	}

	SDL_RWseek(src, start, RW_SEEK_SET);
	return(NULL);
}

#else

int IMG_InitWEBP()
{
	IMG_SetError("WEBP images are not supported");
	return(-1);
}

void IMG_QuitWEBP()
{
}

/* See if an image is contained in a data source */
int IMG_isWEBP(SDL_RWops *src)
{
	return(0);
}

/* Load a WEBP type image from an SDL datasource */
SDL_Surface *IMG_LoadWEBP_RW(SDL_RWops *src)
{
	return(NULL);
}

#endif /* LOAD_WEBP */
