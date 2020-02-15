/******************************************************************************
    Copyright (C) 2013 by Ruwen Hahn <palana@stunned.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "gl-subsystem.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

//#include "util/darray.h"

struct gl_windowinfo {
	NSView *view;
	NSOpenGLContext *context;
	gs_texture_t *texture;
	GLuint fbo;
};

struct gl_platform {
	NSOpenGLContext *context;
};

IOSurfaceRef surface;

static NSOpenGLContext *gl_context_create(NSOpenGLContext *share)
{
	unsigned attrib_count = 0;

#define ADD_ATTR(x)                                                           \
	{                                                                     \
		attributes[attrib_count++] = (NSOpenGLPixelFormatAttribute)x; \
	}
#define ADD_ATTR2(x, y)      \
	{                    \
		ADD_ATTR(x); \
		ADD_ATTR(y); \
	}

	NSOpenGLPixelFormatAttribute attributes[40];

	ADD_ATTR(NSOpenGLPFADoubleBuffer);
	ADD_ATTR2(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
	ADD_ATTR(0);

#undef ADD_ATTR2
#undef ADD_ATTR

	NSOpenGLPixelFormat *pf;
	pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
	if (!pf) {
		blog(LOG_ERROR, "Failed to create pixel format");
		return NULL;
	}

	NSOpenGLContext *context;
	context = [[NSOpenGLContext alloc] initWithFormat:pf
					     shareContext:share];
	[pf release];
	if (!context) {
		blog(LOG_ERROR, "Failed to create context");
		return NULL;
	}

	[context clearDrawable];

	return context;
}

struct gl_platform *gl_platform_create(gs_device_t *device, uint32_t adapter)
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(adapter);

	NSOpenGLContext *context = gl_context_create(nil);
	if (!context) {
		blog(LOG_ERROR, "gl_context_create failed");
		return NULL;
	}

	[context makeCurrentContext];
	GLint interval = 0;
	[context setValues:&interval forParameter:NSOpenGLCPSwapInterval];
	const bool success = gladLoadGL() != 0;
	[NSOpenGLContext clearCurrentContext];

	if (!success) {
		blog(LOG_ERROR, "gladLoadGL failed");
		[context release];
		return NULL;
	}

	struct gl_platform *plat = bzalloc(sizeof(struct gl_platform));
	plat->context = context;
	return plat;
}

void gl_platform_destroy(struct gl_platform *platform)
{
	if (!platform)
		return;

	[platform->context release];
	platform->context = nil;

	bfree(platform);
}

bool gl_platform_init_swapchain(struct gs_swap_chain *swap)
{
	blog(LOG_INFO, "gl_platform_init_swapchain");
	NSOpenGLContext *parent = swap->device->plat->context;
	NSOpenGLContext *context = gl_context_create(parent);
	bool success = context != nil;
	if (success) {
		CGLContextObj parent_obj = [parent CGLContextObj];
		CGLLockContext(parent_obj);

		[parent makeCurrentContext];
		struct gs_init_data *init_data = &swap->info;
		// swap->wi->texture = bzalloc(sizeof(struct gs_texture_2d));
		swap->wi->texture = swap->wi->texture = device_texture_create(
                            swap->device, init_data->cx, init_data->cy,
                            init_data->format, 1, NULL, GS_RENDER_TARGET);
		// glEnable(GL_FRAMEBUFFER);
		// glGenTextures(1, &swap->wi->texture->texture);
		// glBindTexture(GL_FRAMEBUFFER, swap->wi->texture->texture);
		CGLTexImageIOSurface2D(parent_obj, GL_FRAMEBUFFER, GL_RGBA,
					init_data->cx, init_data->cy, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
					surface, 0);
		// glDisable(GL_FRAMEBUFFER);
		glFlush();
		[NSOpenGLContext clearCurrentContext];

		CGLContextObj context_obj = [context CGLContextObj];
		CGLLockContext(context_obj);

		[context makeCurrentContext];
		[context setView:swap->wi->view];
		GLint interval = 0;
		[context setValues:&interval
			forParameter:NSOpenGLCPSwapInterval];
		gl_gen_framebuffers(1, &swap->wi->fbo);
		gl_bind_framebuffer(GL_FRAMEBUFFER, swap->wi->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D,
				       swap->wi->texture->texture, 0);
		GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE) {
			blog(LOG_INFO, "VALID FRAMEBUFFER");
		} else {
			blog(LOG_INFO, "INVALID FRAMEBUFFER");
		}
		gl_success("glFrameBufferTexture2D");
		glFlush();
		[NSOpenGLContext clearCurrentContext];

		CGLUnlockContext(context_obj);

		CGLUnlockContext(parent_obj);

		swap->wi->context = context;
		blog(LOG_INFO, "gl_platform_init_swapchain - end");
	}

	return success;
}

void gl_platform_cleanup_swapchain(struct gs_swap_chain *swap)
{
	NSOpenGLContext *parent = swap->device->plat->context;
	CGLContextObj parent_obj = [parent CGLContextObj];
	CGLLockContext(parent_obj);

	NSOpenGLContext *context = swap->wi->context;
	CGLContextObj context_obj = [context CGLContextObj];
	CGLLockContext(context_obj);

	[context makeCurrentContext];
	gl_delete_framebuffers(1, &swap->wi->fbo);
	glFlush();
	[NSOpenGLContext clearCurrentContext];

	CGLUnlockContext(context_obj);

	[parent makeCurrentContext];
	gs_texture_destroy(swap->wi->texture);
	glFlush();
	[NSOpenGLContext clearCurrentContext];
	swap->wi->context = nil;

	CGLUnlockContext(parent_obj);
}

struct gl_windowinfo *gl_windowinfo_create(const struct gs_init_data *info)
{
	if (!info)
		return NULL;

	if (!info->window.view)
		return NULL;

	struct gl_windowinfo *wi = bzalloc(sizeof(struct gl_windowinfo));

	wi->view = info->window.view;
	[info->window.view setWantsBestResolutionOpenGLSurface:YES];

	return wi;
}

void gl_windowinfo_destroy(struct gl_windowinfo *wi)
{
	if (!wi)
		return;

	wi->view = nil;
	bfree(wi);
}

void gl_update(gs_device_t *device)
{
	blog(LOG_INFO, "gl_update");
	gs_swapchain_t *swap = device->cur_swap;
	NSOpenGLContext *parent = device->plat->context;
	NSOpenGLContext *context = swap->wi->context;
	dispatch_async(dispatch_get_main_queue(), ^() {
		CGLContextObj parent_obj = [parent CGLContextObj];
		CGLLockContext(parent_obj);

		CGLContextObj context_obj = [context CGLContextObj];
		CGLLockContext(context_obj);

		[context makeCurrentContext];
		[context update];

		struct gs_init_data *info = &swap->info;
		gs_texture_t *previous = swap->wi->texture;
		swap->wi->texture = bzalloc(sizeof(struct gs_texture_2d));
		swap->wi->texture = swap->wi->texture = device_texture_create(
                            swap->device, info->cx, info->cy,
                            info->format, 1, NULL, GS_RENDER_TARGET);
		// glEnable(GL_TEXTURE_RECTANGLE_ARB);
		// glGenTextures(1, &swap->wi->texture->texture);
		// glBindTexture(GL_TEXTURE_RECTANGLE, swap->wi->texture->texture);
		blog(LOG_INFO, "update swapchain width: %d", info->cx);
		blog(LOG_INFO, "update swapchain height: %d", info->cy);
		CGLTexImageIOSurface2D(parent_obj, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA,
					info->cx, info->cy, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
					surface, 0);
		gl_success("CGLTexImageIOSurface2D");
		// glDisable(GL_TEXTURE_RECTANGLE_ARB);
		gl_gen_framebuffers(1, &swap->wi->fbo);
		gl_bind_framebuffer(GL_FRAMEBUFFER, swap->wi->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				       GL_TEXTURE_2D,
				       swap->wi->texture->texture, 0);
		GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(status != GL_FRAMEBUFFER_COMPLETE) {
			blog(LOG_INFO, "UPDATE VALID FRAMEBUFFER");
		} else {
			blog(LOG_INFO, "UPDATE INVALID FRAMEBUFFER");
		}
		gl_success("glFrameBufferTexture2D");
		gs_texture_destroy(previous);
		glFlush();
		[NSOpenGLContext clearCurrentContext];

		CGLUnlockContext(context_obj);

		CGLUnlockContext(parent_obj);
		blog(LOG_INFO, "gl_update - end");
	});
}

void gl_clear_context(gs_device_t *device)
{
	UNUSED_PARAMETER(device);
	[NSOpenGLContext clearCurrentContext];
}

void device_enter_context(gs_device_t *device)
{
	CGLLockContext([device->plat->context CGLContextObj]);

	[device->plat->context makeCurrentContext];
}

void device_leave_context(gs_device_t *device)
{
	glFlush();
	[NSOpenGLContext clearCurrentContext];
	device->cur_render_target = NULL;
	device->cur_zstencil_buffer = NULL;
	device->cur_swap = NULL;
	device->cur_fbo = NULL;

	CGLUnlockContext([device->plat->context CGLContextObj]);
}

void *device_get_device_obj(gs_device_t *device)
{
	return device->plat->context;
}

void device_load_swapchain(gs_device_t *device, gs_swapchain_t *swap)
{
	if (device->cur_swap == swap)
		return;

	device->cur_swap = swap;
	if (swap) {
		device_set_render_target(device, swap->wi->texture, NULL);
	}
}

void device_present(gs_device_t *device)
{
	glFlush();
	[NSOpenGLContext clearCurrentContext];

	CGLUnlockContext([device->plat->context CGLContextObj]);

	CGLLockContext([device->cur_swap->wi->context CGLContextObj]);

	IOSurfaceLock(surface, 0, nil);

	[device->cur_swap->wi->context makeCurrentContext];
	gl_bind_framebuffer(GL_READ_FRAMEBUFFER, device->cur_swap->wi->fbo);
	// gl_bind_framebuffer(GL_DRAW_FRAMEBUFFER, 0);
	const uint32_t width = device->cur_swap->info.cx;
	const uint32_t height = device->cur_swap->info.cy;
	glBlitFramebuffer(0, 0, width, height, 0, height, width, 0,
			  GL_COLOR_BUFFER_BIT, GL_NEAREST);
	[device->cur_swap->wi->context flushBuffer];

	IOSurfaceUnlock(surface, 0, 0);

	glFlush();
	[NSOpenGLContext clearCurrentContext];

	CGLUnlockContext([device->cur_swap->wi->context CGLContextObj]);

	CGLLockContext([device->plat->context CGLContextObj]);

	[device->plat->context makeCurrentContext];
}

void gl_getclientsize(const struct gs_swap_chain *swap, uint32_t *width,
		      uint32_t *height)
{
	if (width)
		*width = swap->info.cx;
	if (height)
		*height = swap->info.cy;
}

gs_texture_t *device_texture_create_from_iosurface(gs_device_t *device,
						   void *iosurf)
{
	IOSurfaceRef ref = (IOSurfaceRef)iosurf;
	struct gs_texture_2d *tex = bzalloc(sizeof(struct gs_texture_2d));

	OSType pf = IOSurfaceGetPixelFormat(ref);
	if (pf != 'BGRA')
		blog(LOG_ERROR, "Unexpected pixel format: %d (%c%c%c%c)", pf,
		     pf >> 24, pf >> 16, pf >> 8, pf);

	const enum gs_color_format color_format = GS_BGRA;

	tex->base.device = device;
	tex->base.type = GS_TEXTURE_2D;
	tex->base.format = GS_BGRA;
	tex->base.levels = 1;
	tex->base.gl_format = convert_gs_format(color_format);
	tex->base.gl_internal_format = convert_gs_internal_format(color_format);
	tex->base.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
	tex->base.gl_target = GL_TEXTURE_RECTANGLE;
	tex->base.is_dynamic = false;
	tex->base.is_render_target = false;
	tex->base.gen_mipmaps = false;
	tex->width = IOSurfaceGetWidth(ref);
	tex->height = IOSurfaceGetHeight(ref);

	if (!gl_gen_textures(1, &tex->base.texture))
		goto fail;

	if (!gl_bind_texture(tex->base.gl_target, tex->base.texture))
		goto fail;

	CGLError err = CGLTexImageIOSurface2D(
		[[NSOpenGLContext currentContext] CGLContextObj],
		tex->base.gl_target, tex->base.gl_internal_format, tex->width,
		tex->height, tex->base.gl_format, tex->base.gl_type, ref, 0);

	if (err != kCGLNoError) {
		blog(LOG_ERROR,
		     "CGLTexImageIOSurface2D: %u, %s"
		     " (device_texture_create_from_iosurface)",
		     err, CGLErrorString(err));

		gl_success("CGLTexImageIOSurface2D");
		goto fail;
	}

	if (!gl_tex_param_i(tex->base.gl_target, GL_TEXTURE_MAX_LEVEL, 0))
		goto fail;

	if (!gl_bind_texture(tex->base.gl_target, 0))
		goto fail;

	return (gs_texture_t *)tex;

fail:
	gs_texture_destroy((gs_texture_t *)tex);
	blog(LOG_ERROR, "device_texture_create_from_iosurface (GL) failed");
	return NULL;
}

bool gs_texture_rebind_iosurface(gs_texture_t *texture, void *iosurf)
{
	if (!texture)
		return false;

	if (!iosurf)
		return false;

	struct gs_texture_2d *tex = (struct gs_texture_2d *)texture;
	IOSurfaceRef ref = (IOSurfaceRef)iosurf;

	OSType pf = IOSurfaceGetPixelFormat(ref);
	if (pf != 'BGRA')
		blog(LOG_ERROR, "Unexpected pixel format: %d (%c%c%c%c)", pf,
		     pf >> 24, pf >> 16, pf >> 8, pf);

	if (tex->width != IOSurfaceGetWidth(ref) ||
	    tex->height != IOSurfaceGetHeight(ref))
		return false;

	if (!gl_bind_texture(tex->base.gl_target, tex->base.texture))
		return false;

	CGLError err = CGLTexImageIOSurface2D(
		[[NSOpenGLContext currentContext] CGLContextObj],
		tex->base.gl_target, tex->base.gl_internal_format, tex->width,
		tex->height, tex->base.gl_format, tex->base.gl_type, ref, 0);

	if (err != kCGLNoError) {
		blog(LOG_ERROR,
		     "CGLTexImageIOSurface2D: %u, %s"
		     " (gs_texture_rebind_iosurface)",
		     err, CGLErrorString(err));

		gl_success("CGLTexImageIOSurface2D");
		return false;
	}

	if (!gl_bind_texture(tex->base.gl_target, 0))
		return false;

	return true;
}

uint32_t create_iosurface(gs_device_t *device)
{
	// const uint32_t width = device->cur_viewport.cx;
	// const uint32_t height = device->cur_viewport.cy;
	const uint32_t width = 1532;
	const uint32_t height = 490;

	NSDictionary* surfaceAttributes = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], (NSString*)kIOSurfaceIsGlobal,
									   [NSNumber numberWithUnsignedInteger:(NSUInteger)width], (NSString*)kIOSurfaceWidth,
									   [NSNumber numberWithUnsignedInteger:(NSUInteger)height], (NSString*)kIOSurfaceHeight,
									   [NSNumber numberWithUnsignedInteger:4U], (NSString*)kIOSurfaceBytesPerElement, nil];

	IOSurfaceRef _surfaceRef =  IOSurfaceCreate((CFDictionaryRef) surfaceAttributes);

	if (_surfaceRef)
		surface = _surfaceRef;

	[surfaceAttributes release];

    return IOSurfaceGetID(_surfaceRef);
}