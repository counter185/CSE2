// Released under the MIT licence.
// See LICENCE.txt for details.

#include "../Rendering.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <map>

#ifdef __PS2__
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "../../WindowsWrapper.h"

#include "../Misc.h"
#include "../Shared/SDL.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef struct RenderBackend_Surface
{
	SDL_Texture *texture;
	size_t width;
	size_t height;
	bool render_target;
	bool lost;

	struct RenderBackend_Surface *next;
	struct RenderBackend_Surface *prev;
} RenderBackend_Surface;

typedef struct RenderBackend_GlyphAtlas
{
	std::map<uint64_t, SDL_Texture*> glyph_textures;
	uint8_t colorR = 255, colorG = 255, colorB = 255;
} RenderBackend_GlyphAtlas;
static uint64_t encodeXY(int x, int y) {
	return ((uint64_t)x << 32) | (uint64_t)y;
}

SDL_Window *window;

static SDL_Renderer *renderer;

static RenderBackend_Surface framebuffer;
static RenderBackend_Surface upscaled_framebuffer;

static SDL_Rect window_rect;

static RenderBackend_Surface *surface_list_head;

static RenderBackend_GlyphAtlas *glyph_atlas;

static void RectToSDLRect(const RenderBackend_Rect *rect, SDL_Rect *sdl_rect)
{
	sdl_rect->x = (int)rect->left;
	sdl_rect->y = (int)rect->top;
	sdl_rect->w = (int)(rect->right - rect->left);
	sdl_rect->h = (int)(rect->bottom - rect->top);

	if (sdl_rect->w < 0)
		sdl_rect->w = 0;

	if (sdl_rect->h < 0)
		sdl_rect->h = 0;
}

unsigned int texMem = 0;

RenderBackend_Surface* RenderBackend_Init(const char *window_title, size_t screen_width, size_t screen_height, bool fullscreen)
{
#ifdef __PS2__
	SDL_SetHint(SDL_HINT_PS2_DYNAMIC_VSYNC, "1");
	SDL_SetHint(SDL_HINT_PS2_GS_WIDTH, "320");
	SDL_SetHint(SDL_HINT_PS2_GS_HEIGHT, "240");
	//SDL_SetHint(SDL_HINT_PS2_GS_PROGRESSIVE, "1"); //do not use progressive scan it makes the screen constantly scroll vertically
	SDL_SetHint(SDL_HINT_PS2_GS_MODE, "PAL");
#endif

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

	Backend_PrintInfo("Available SDL render drivers:");

	for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i)
	{
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(i, &info) < 0)
			Backend_PrintError("Couldn't get render driver information: %s", SDL_GetError());
		else
			Backend_PrintInfo("%s", info.name);
	}

	window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, SDL_WINDOW_RESIZABLE);

	if (window != NULL)
	{
		if (fullscreen)
			if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) < 0)
				Backend_PrintError("Couldn't set window to fullscreen: %s", SDL_GetError());

	#if SDL_VERSION_ATLEAST(2,0,10)
		SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");	// We never interfere with the renderer, so don't let SDL implicitly disable batching
	#endif

		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

		if (renderer != NULL)
		{
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			SDL_RendererInfo info;
			if (SDL_GetRendererInfo(renderer, &info) < 0)
				Backend_PrintError("Couldn't get selected render driver information: %s", SDL_GetError());
			else
				Backend_PrintInfo("Selected SDL render driver: %s", info.name);

			framebuffer.texture = NULL;// SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);

			/*if (framebuffer.texture != NULL)
			{
				SDL_SetTextureBlendMode(framebuffer.texture, SDL_BLENDMODE_NONE);

				framebuffer.width = screen_width;
				framebuffer.height = screen_height;

				RenderBackend_HandleWindowResize(screen_width, screen_height);

				Backend_PostWindowCreation();

				return &framebuffer;
			}
			else
			{
				std::string error_message = std::string("Could not create framebuffer: ") + SDL_GetError();
				Backend_ShowMessageBox("Fatal error (SDLTexture rendering backend)", error_message.c_str());
			}*/
			framebuffer.width = screen_width;
			framebuffer.height = screen_height;
			RenderBackend_HandleWindowResize(screen_width, screen_height);

			Backend_PostWindowCreation();
			return &framebuffer;

			SDL_DestroyRenderer(renderer);
		}
		else
		{
			std::string error_message = std::string("Could not create renderer: ") + SDL_GetError();
			Backend_ShowMessageBox("Fatal error (SDLTexture rendering backend)", error_message.c_str());
		}


		SDL_DestroyWindow(window);
	}
	else
	{
		std::string error_message = std::string("Could not create window: ") + SDL_GetError();
		Backend_ShowMessageBox("Fatal error (SDLTexture rendering backend)", error_message.c_str());
	}

	return NULL;
}

void RenderBackend_Deinit(void)
{
	if (upscaled_framebuffer.texture != NULL)
		SDL_DestroyTexture(upscaled_framebuffer.texture);

	//SDL_DestroyTexture(framebuffer.texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}

void RenderBackend_DrawScreen(void)
{
	/*if (upscaled_framebuffer.texture != NULL)
	{
		if (SDL_SetRenderTarget(renderer, upscaled_framebuffer.texture) < 0)
			Backend_PrintError("Couldn't set upscaled framebuffer as the current rendering target: %s", SDL_GetError());

		if (SDL_RenderCopy(renderer, framebuffer.texture, NULL, NULL) < 0)
			Backend_PrintError("Failed to copy framebuffer texture to upscaled framebuffer: %s", SDL_GetError());
	}*/

	if (SDL_SetRenderTarget(renderer, NULL) < 0)
		Backend_PrintError("Couldn't set default render target as the current rendering target: %s", SDL_GetError());

	if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF) < 0)
		Backend_PrintError("Couldn't set color for drawing operations: %s", SDL_GetError());

	//SDL_RenderClear(renderer);

	/*if (SDL_RenderCopy(renderer, upscaled_framebuffer.texture != NULL ? upscaled_framebuffer.texture : framebuffer.texture, NULL, &window_rect) < 0)
		Backend_PrintError("Failed to copy upscaled framebuffer texture to default render target: %s", SDL_GetError());*/

	SDL_RenderPresent(renderer);
}

RenderBackend_Surface* RenderBackend_CreateSurface(size_t width, size_t height, bool render_target)
{
	RenderBackend_Surface *surface = (RenderBackend_Surface*)malloc(sizeof(RenderBackend_Surface));

	if (surface == NULL)
		return NULL;

	surface->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, render_target ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STATIC, width, height);
	texMem += width * height * 4;

	SDL_SetTextureBlendMode(surface->texture, SDL_BLENDMODE_BLEND);

	if (surface->texture == NULL)
	{
		free(surface);
		return NULL;
	}

	surface->width = width;
	surface->height = height;
	surface->render_target = render_target;
	surface->lost = false;

	// Add to linked-list
	surface->prev = NULL;
	surface->next = surface_list_head;
	surface_list_head = surface;

	if (surface->next != NULL)
		surface->next->prev = surface;

	return surface;
}

void RenderBackend_FreeSurface(RenderBackend_Surface *surface)
{
	texMem -= surface->width * surface->height * 4;
	// Remove from linked list
	if (surface->next != NULL)
		surface->next->prev = surface->prev;
	if (surface->prev != NULL)
		surface->prev->next = surface->next;

	if (surface->prev == NULL)
		surface_list_head = surface->next;

	SDL_DestroyTexture(surface->texture);
	free(surface);
}

bool RenderBackend_IsSurfaceLost(RenderBackend_Surface *surface)
{
	return surface->lost;
}

void RenderBackend_RestoreSurface(RenderBackend_Surface *surface)
{
	surface->lost = false;
}

void RenderBackend_UploadSurface(RenderBackend_Surface *surface, const unsigned char *pixels, size_t width, size_t height)
{
	unsigned char *buffer = (unsigned char*)malloc(width * height * 4);

	if (buffer == NULL)
	{
		Backend_PrintError("Couldn't allocate memory for surface buffer");
		return;
	}

	const unsigned char *src_pixel = pixels;
	unsigned char *buffer_pointer = buffer;

	// Convert the colour-keyed pixels to RGBA32
	for (size_t y = 0; y < height; ++y)
	{
		for (size_t x = 0; x < width; ++x)
		{
			unsigned char srcR = src_pixel[0];
			unsigned char srcG = src_pixel[1];
			unsigned char srcB = src_pixel[2];

			*buffer_pointer++ = srcR;
			*buffer_pointer++ = srcG;
			*buffer_pointer++ = srcB;

			if (src_pixel[0] == 0 && src_pixel[1] == 0 && src_pixel[2] == 0)	// Assumes the colour key will always be #000000 (black)
				*buffer_pointer++ = 0;
			else
				*buffer_pointer++ = 0xFF;

			src_pixel += 3;
		}
	}

	SDL_Rect rect = {0, 0, (int)width, (int)height};

	if (SDL_UpdateTexture(surface->texture, &rect, buffer, width * 4) < 0)
		Backend_PrintError("Couldn't update part of texture: %s", SDL_GetError());

	free(buffer);
}

void RenderBackend_Blit(RenderBackend_Surface *source_surface, const RenderBackend_Rect *rect, RenderBackend_Surface *destination_surface, long x, long y, bool colour_key)
{
	SDL_Rect source_rect;
	RectToSDLRect(rect, &source_rect);

	SDL_Rect destination_rect = {(int)x, (int)y, source_rect.w, source_rect.h};

	// Blit the texture
	if (source_surface != &framebuffer) {
		if (SDL_SetTextureBlendMode(source_surface->texture, SDL_BLENDMODE_BLEND /*colour_key ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE*/) < 0)
			Backend_PrintError("Couldn't set texture blend mode: %s", SDL_GetError());

		if (SDL_SetRenderTarget(renderer, destination_surface == &framebuffer ? NULL : destination_surface->texture) < 0)
			Backend_PrintError("Couldn't set current rendering target: %s", SDL_GetError());

		if (SDL_RenderCopy(renderer, source_surface->texture, &source_rect, &destination_rect) < 0)
			Backend_PrintError("Couldn't copy part of texture to rendering target: %s", SDL_GetError());
	}
}

void RenderBackend_ColourFill(RenderBackend_Surface *surface, const RenderBackend_Rect *rect, unsigned char red, unsigned char green, unsigned char blue)
{
	SDL_Rect sdl_rect;
	RectToSDLRect(rect, &sdl_rect);

	Uint8 alpha = SDL_ALPHA_OPAQUE;

	// Check colour-key
	if (red == 0 && green == 0 && blue == 0)
		alpha = 0;

	if (SDL_SetRenderDrawColor(renderer, red, green, blue, alpha) < 0)
		Backend_PrintError("Couldn't set color for drawing operations: %s", SDL_GetError());

	// Draw colour
	if (SDL_SetRenderTarget(renderer, surface->texture) < 0)
		Backend_PrintError("Couldn't set texture current rendering target: %s", SDL_GetError());

	if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE) < 0)
		Backend_PrintError("Couldn't disable blending for drawing operations: %s", SDL_GetError());

	if (SDL_RenderFillRect(renderer, &sdl_rect) < 0)
		Backend_PrintError("Couldn't fill rectangle on current rendering target: %s", SDL_GetError());

	if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0)
		Backend_PrintError("Couldn't enable alpha blending for drawing operations: %s", SDL_GetError());
}

RenderBackend_GlyphAtlas* RenderBackend_CreateGlyphAtlas(size_t width, size_t height)
{
	RenderBackend_GlyphAtlas* atlas = new RenderBackend_GlyphAtlas;//(RenderBackend_GlyphAtlas*)malloc(sizeof(RenderBackend_GlyphAtlas));

	if (atlas != NULL)
	{
		atlas->glyph_textures = {};
		/*atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
		texMem += width * height * 4;

		if (atlas->texture != NULL)
		{
			return atlas;
		}
		else
		{
			Backend_PrintError("Couldn't create texture for renderer: %s", SDL_GetError());
		}

		free(atlas);*/
		return atlas;
	}

	return NULL;
}

void RenderBackend_DestroyGlyphAtlas(RenderBackend_GlyphAtlas *atlas)
{
	for (auto& tex : atlas->glyph_textures) {
		SDL_DestroyTexture(tex.second);
	}
	delete atlas;
}

void RenderBackend_UploadGlyph(RenderBackend_GlyphAtlas *atlas, size_t x, size_t y, const unsigned char *pixels, size_t width, size_t height, size_t pitch)
{
	unsigned char* buffer = (unsigned char*)malloc(width * height * 4);

	if (buffer != NULL)
	{
		unsigned char *destination_pointer = buffer;

		for (size_t iy = 0; iy < height; ++iy)
		{
			const unsigned char *source_pointer = &pixels[iy * pitch];

			for (size_t ix = 0; ix < width; ++ix)
			{
				*destination_pointer++ = 0xFF;
				*destination_pointer++ = 0xFF;
				*destination_pointer++ = 0xFF;
				*destination_pointer++ = *source_pointer++;
			}
		}

		uint64_t encxy = encodeXY(x, y);
		SDL_Texture* glyph_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 
			width > 16 ? width : 16,
			height > 16 ? height : 16);
		SDL_SetTextureBlendMode(glyph_texture, SDL_BLENDMODE_BLEND);
		SDL_UpdateTexture(glyph_texture, NULL, buffer, width * 4);
		atlas->glyph_textures[encxy] = glyph_texture;

		free(buffer);
	}
}

void RenderBackend_PrepareToDrawGlyphs(RenderBackend_GlyphAtlas *atlas, RenderBackend_Surface *destination_surface, unsigned char red, unsigned char green, unsigned char blue)
{
	glyph_atlas = atlas;

	if (SDL_SetRenderTarget(renderer, destination_surface->texture) < 0)
		Backend_PrintError("Couldn't set texture as current rendering target: %s", SDL_GetError());

	atlas->colorR = red;
	atlas->colorG = green;
	atlas->colorB = blue;

	// The SDL_Texture side of things uses alpha, not a colour-key, so the bug where the font is blended
	// with the colour key doesn't occur.
	/*if (SDL_SetTextureColorMod(atlas->texture, red, green, blue) < 0)
		Backend_PrintError("Couldn't set additional color value: %s", SDL_GetError());

	if (SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND) < 0)
		Backend_PrintError("Couldn't set texture blend mode: %s", SDL_GetError());*/

}

void RenderBackend_DrawGlyph(long x, long y, size_t glyph_x, size_t glyph_y, size_t glyph_width, size_t glyph_height)
{
	uint64_t key = encodeXY(glyph_x, glyph_y);
	SDL_Rect source_rect;
	source_rect.x = 0;
	source_rect.y = 0;
	source_rect.w = glyph_width;
	source_rect.h = glyph_height;

	SDL_Rect destination_rect;
	destination_rect.x = x;
	destination_rect.y = y;
	destination_rect.w = glyph_width;
	destination_rect.h = glyph_height;

	SDL_Texture* target = glyph_atlas->glyph_textures[key];
	SDL_SetTextureColorMod(target, glyph_atlas->colorR, glyph_atlas->colorG, glyph_atlas->colorB);
	if (SDL_RenderCopy(renderer, target, &source_rect, &destination_rect) < 0)
		Backend_PrintError("Couldn't copy glyph texture portion to renderer: %s", SDL_GetError());
}

void RenderBackend_HandleRenderTargetLoss(void)
{
	for (RenderBackend_Surface *surface = surface_list_head; surface != NULL; surface = surface->next)
		if (surface->render_target)
			surface->lost = true;
}

void RenderBackend_HandleWindowResize(size_t width, size_t height)
{
	size_t upscale_factor = MAX(1, MIN((width + framebuffer.width / 2) / framebuffer.width, (height + framebuffer.height / 2) / framebuffer.height));

	upscaled_framebuffer.width = framebuffer.width * upscale_factor;
	upscaled_framebuffer.height = framebuffer.height * upscale_factor;

	if (upscaled_framebuffer.texture != NULL)
	{
		SDL_DestroyTexture(upscaled_framebuffer.texture);
		upscaled_framebuffer.texture = NULL;
	}

	// Create rect that forces 4:3 no matter what size the window is
	if (width * upscaled_framebuffer.height >= upscaled_framebuffer.width * height) // Fancy way to do `if (width / height >= upscaled_framebuffer.width / upscaled_framebuffer.height)` without floats
	{
		window_rect.w = (height * upscaled_framebuffer.width) / upscaled_framebuffer.height;
		window_rect.h = height;
	}
	else
	{
		window_rect.w = width;
		window_rect.h = (width * upscaled_framebuffer.height) / upscaled_framebuffer.width;
	}

	window_rect.x = (width - window_rect.w) / 2;
	window_rect.y = (height - window_rect.h) / 2;

	if (window_rect.w % framebuffer.width != 0 || window_rect.h % framebuffer.height != 0)
	{
		/*SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		upscaled_framebuffer.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, upscaled_framebuffer.width, upscaled_framebuffer.height);
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

		if (upscaled_framebuffer.texture == NULL)
			Backend_PrintError("Couldn't regenerate upscaled framebuffer");

		SDL_SetTextureBlendMode(upscaled_framebuffer.texture, SDL_BLENDMODE_NONE);*/
	}
}

unsigned int RenderBackend_GetTextureMemoryUsage(void)
{
	return texMem;
}
