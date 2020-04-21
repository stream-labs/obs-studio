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

#ifndef VCAM_IPC_SERVER
#define VCAM_IPC_SERVER

#include "vcam-ipc.hpp"

class VCAM_IPC_Server: public VCAM_IPC {
public:
    SIGNAL(FrameReady,
                    const std::string &deviceId,
                    const uint8_t *data)
    SIGNAL(DeviceAdded,
                    const std::string &deviceId,
                    const std::string &name,
                    uint32_t width,
                    uint32_t height,
                    double fps)
    SIGNAL(DeviceRemoved,
                    const std::string &deviceId)
    SIGNAL(MirrorChanged,
                    const std::string &deviceId,
                    bool horizontalMirror,
                    bool verticalMirror)
public:
    VCAM_IPC_Server();
    ~VCAM_IPC_Server();

    void deviceCreate(xpc_connection_t client, xpc_object_t event);
    void deviceDestroy(xpc_connection_t client, xpc_object_t event);
    void frameReady(xpc_connection_t client, xpc_object_t event);
    void setMirror(xpc_connection_t client, xpc_object_t event);
};

#endif