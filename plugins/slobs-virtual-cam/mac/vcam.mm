/* Webcamoid, webcam capture application.
 * Copyright (C) 2017  Gonzalo Exequiel Pedone
 *
 * Webcamoid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Webcamoid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Webcamoid. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

/******************************************************************************
Modifications Copyright (C) 2020 by Streamlabs (General Workings Inc)

04/20/2020: Rewrite partial and sometimes complete to fits the application needs

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

#include "vcam.hpp"


#include <IOSurface/IOSurface.h>
#include <locale>
#include <codecvt>
#include <iostream>

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

VirtualCam::VirtualCam()
{
    this->ipc_client.connect(false);
}

VirtualCam::~VirtualCam()
{
    this->ipc_client.disconnect();
}

bool VirtualCam::createWebcam(const std::string name, int width, int height, double fps)
{
    this->name   = name;
    this->width  = width;
    this->height = height;
    this->fps    = fps;

    this->deviceID =
        this->ipc_client.deviceCreate(this->name, this->width, this->height, this->fps);
    if (this->deviceID.empty())
        return false;

    this->surfaceID      = this->createSharedMemory();
    return true;
}

uint32_t VirtualCam::createSharedMemory()
{
	NSDictionary* surfaceAttributes = 
        [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], (NSString*)kIOSurfaceIsGlobal,
        [NSNumber numberWithUnsignedInteger:(NSUInteger)this->width], (NSString*)kIOSurfaceWidth,
        [NSNumber numberWithUnsignedInteger:(NSUInteger)this->height], (NSString*)kIOSurfaceHeight,
        [NSNumber numberWithUnsignedInteger:2U], (NSString*)kIOSurfaceBytesPerElement, nil];

	auto surface =  IOSurfaceCreate((CFDictionaryRef) surfaceAttributes);
	CFRelease(surfaceAttributes);

    if (surface)
        return IOSurfaceGetID(surface);
    else
        return 0;
}

bool VirtualCam::removeWebcam()
{
    return this->ipc_client.deviceDestroy(this->deviceID);
}

bool VirtualCam::removeAllWebcams()
{
    return this->ipc_client.destroyAllDevices();
}

bool VirtualCam::startDaemon()
{
    return this->ipc_client.startDaemon();
}

bool VirtualCam::removeDaemon()
{
    this->ipc_client.removeDaemon();
}

bool VirtualCam::uploadFrame(const uint8_t *frame)
{
    uint32_t size = this->width * this->height * BYTES_PER_PIXEL;
    return this->ipc_client.deviceUploadFrame(
        deviceID,
        this->surfaceID,
        frame,
        size);
}
