/******************************************************************************
Copyright (C) 2020 by Streamlabs (General Workings Inc)
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

#include <obs-module.h>
#include <util/bmem.h>
#include <stdio.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include "libavutil/frame.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
}
#include "vcam.hpp"
#include <iostream>

#include <thread>
#include <mutex>

struct virtual_output {
	obs_output_t *output;
	VirtualCam* VCAM = nullptr;
	struct SwsContext *scaler;
};

static const char *virtual_output_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("VirtualWebcam");
}

static void *virtual_output_create(obs_data_t *settings, obs_output_t *output)
{
	struct virtual_output *data = (virtual_output *)bzalloc(sizeof(struct virtual_output));
	data->output = output;

	const char *name = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;
	double fps = 0.0;
	bool res = false;

	name = obs_data_get_string(settings, "name");
	if (!name)
		goto fail;

	width = (uint32_t)obs_data_get_int(settings, "width");
	if (!width)
		goto fail;

	height = (uint32_t)obs_data_get_int(settings, "height");
	if (!height)
		goto fail;

	fps = obs_data_get_double(settings, "fps");
	if (!fps)
		goto fail;

	data->VCAM = new VirtualCam();
	if (!data->VCAM->startDaemon())
		goto fail;

	data->VCAM->removeAllWebcams();
	blog(LOG_INFO, "Creating Virtual webcam: %s, %d, %d, %d",
		name,
		width,
		height,
		(int) fps);
	res = data->VCAM->createWebcam(
		name,
		width,
		height,
		fps
	);
	if (!res)
		goto fail;

	data->scaler = 
		sws_getContext(width,
					height,
					AV_PIX_FMT_NV12,
					width,
					height,
					AV_PIX_FMT_UYVY422,
					SWS_BICUBIC, NULL, NULL, NULL
	);
	if (!data->scaler)
		goto fail;

	blog(LOG_INFO, "Virtual webcam created successfully");
	return data;

fail:
	blog(LOG_ERROR, "Unable to create the virtual webcam");
	bfree(data);
	return NULL;
}

static void virtual_output_destroy(void *data)
{
	struct virtual_output *output = (virtual_output *)data;
	if (!output->VCAM->removeWebcam())
		blog(LOG_ERROR, "Error when trying to remove the virtual webcam");

	output->VCAM->removeDaemon();

	bfree(output);
}

static bool virtual_output_start(void *data)
{
	struct virtual_output *output = (virtual_output *)data;
	bool res = obs_output_begin_data_capture(output->output, 0);

	blog(LOG_INFO, "Virtual webcam, output started result: %d", res);

	return res;
}

static void virtual_output_stop(void *data, uint64_t ts)
{
	struct virtual_output *output = (virtual_output *)data;
	obs_output_end_data_capture(output->output);
}

static void receive_raw_video(void *param, struct video_data *frame)
{
	struct virtual_output *output = (virtual_output *)param;

	uint8_t *converted_frame[MAX_AV_PLANES];
	uint32_t size = output->VCAM->width * output->VCAM->height * 2;
	converted_frame[0] = (uint8_t *)bmalloc(size);

	uint32_t dest_linesize[MAX_AV_PLANES];
	dest_linesize[0] = output->VCAM->width * 2;

	int hightOutput = sws_scale(
		output->scaler,
		frame->data,
		(const int *)frame->linesize,
		0,
		output->VCAM->height,
		converted_frame,
		(const int *)dest_linesize
	);

	output->VCAM->uploadFrame((const uint8_t *)converted_frame[0]);
}

struct obs_output_info virtual_output = {
	.id = "virtual_output",
	.flags = OBS_OUTPUT_VIDEO,
	.get_name = virtual_output_getname,
	.create = virtual_output_create,
	.destroy = virtual_output_destroy,
	.start = virtual_output_start,
	.stop = virtual_output_stop,
	.raw_video = receive_raw_video
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-virtual-output", "en-US")

bool obs_module_load(void)
{
	obs_register_output(&virtual_output);

	return true;
}
