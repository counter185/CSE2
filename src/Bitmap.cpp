// Released under the MIT licence.
// See LICENCE.txt for details.

#include "Bitmap.h"

#include <stddef.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_BMP
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_STDIO

#include "../external/stb_image.h"

#ifdef __PS2__
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "File.h"

unsigned char* DecodeBitmap(const unsigned char *in_buffer, size_t in_buffer_size, size_t *width, size_t *height, unsigned int bytes_per_pixel)
{
	SDL_Surface* srf = SDL_LoadBMP_RW(SDL_RWFromConstMem(in_buffer, (int)in_buffer_size), 1);
	
	if (srf != NULL) {
		
		SDL_Surface* conv = SDL_ConvertSurfaceFormat(srf, SDL_PIXELFORMAT_RGB24, 0);
		*width = conv->w;
		*height = conv->h;
		unsigned char* image_buffer = (unsigned char*)malloc(conv->w * conv->h * bytes_per_pixel);
		if (image_buffer != NULL) {
			memcpy(image_buffer, conv->pixels, conv->w * conv->h * bytes_per_pixel);
		}
		SDL_FreeSurface(srf);
		SDL_FreeSurface(conv);
		return image_buffer;

	}
	else {
		int int_width, int_height;
		unsigned char* image_buffer = stbi_load_from_memory(in_buffer, in_buffer_size, &int_width, &int_height, NULL, bytes_per_pixel);
		if (image_buffer != NULL) {
			*width = (size_t)int_width;
			*height = (size_t)int_height;
		}
		return image_buffer;
	}

	return NULL;
}

unsigned char* DecodeBitmapFromFile(const char *path, size_t *width, size_t *height, unsigned int bytes_per_pixel)
{
	size_t file_size;
	unsigned char *file_buffer = LoadFileToMemory(path, &file_size);

	if (file_buffer != NULL)
	{
		unsigned char *image_buffer = DecodeBitmap(file_buffer, file_size, width, height, bytes_per_pixel);

		free(file_buffer);

		return image_buffer;
	}

	return NULL;
}

void FreeBitmap(unsigned char *buffer)
{
	stbi_image_free(buffer);
}
