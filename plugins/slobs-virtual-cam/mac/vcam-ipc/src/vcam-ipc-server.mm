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

#include "vcam-ipc-server.hpp"

#include <Foundation/Foundation.h>
#include <IOSurface/IOSurface.h>
#include <CoreMedia/CMFormatDescription.h>

VCAM_IPC_Server::VCAM_IPC_Server() {
    this->registerFunction(FRAME,
        std::bind(&VCAM_IPC_Server::frameReady,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(CREATE_DEVICE,
        std::bind(&VCAM_IPC_Server::deviceCreate,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(REMOVE_DEVICE,
        std::bind(&VCAM_IPC_Server::deviceDestroy,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(SET_MIRRORING,
        std::bind(&VCAM_IPC_Server::setMirror,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

VCAM_IPC_Server::~VCAM_IPC_Server() {
    this->unregisterFunction(FRAME);
    this->unregisterFunction(CREATE_DEVICE);
    this->unregisterFunction(REMOVE_DEVICE);
    this->unregisterFunction(SET_MIRRORING);
}

void VCAM_IPC_Server::deviceCreate(xpc_connection_t client, xpc_object_t event) {
    PrintFunction();

    std::string device = xpc_dictionary_get_string(event, "device");
    std::string name = xpc_dictionary_get_string(event, "name");
    uint32_t width = (uint32_t) xpc_dictionary_get_uint64(event, "width");
    uint32_t height = (uint32_t) xpc_dictionary_get_uint64(event, "height");
    double fps = xpc_dictionary_get_double(event, "fps");
    
    EMIT(this, DeviceAdded, device, name, width, height, fps)
}

void VCAM_IPC_Server::deviceDestroy(xpc_connection_t client, xpc_object_t event) {
    PrintFunction();

    std::string device = xpc_dictionary_get_string(event, "device");

    EMIT(this, DeviceRemoved, device)
}

void VCAM_IPC_Server::frameReady(xpc_connection_t client, xpc_object_t event) {
    std::string deviceId =
            xpc_dictionary_get_string(event, "device");
    auto frame = xpc_dictionary_get_value(event, "frame");
    auto surface = IOSurfaceLookupFromXPCObject(frame);

    if (surface) {
        uint32_t surfaceSeed = 0;
        IOSurfaceLock(surface, kIOSurfaceLockReadOnly, &surfaceSeed);
        const uint8_t *data = reinterpret_cast<uint8_t *>(IOSurfaceGetBaseAddress(surface));
        IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, &surfaceSeed);

        EMIT(this, FrameReady, deviceId, data)
    }

    auto reply = xpc_dictionary_create_reply(event);
    xpc_dictionary_set_bool(reply, "status", surface? true: false);
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void VCAM_IPC_Server::setMirror(xpc_connection_t client, xpc_object_t event) {
    PrintFunction();
    std::string deviceId =
            xpc_dictionary_get_string(event, "device");
    bool horizontalMirror =
            xpc_dictionary_get_bool(event, "hmirror");
    bool verticalMirror =
            xpc_dictionary_get_bool(event, "vmirror");

    EMIT(this,
            MirrorChanged,
            deviceId,
            horizontalMirror,
            verticalMirror)
}