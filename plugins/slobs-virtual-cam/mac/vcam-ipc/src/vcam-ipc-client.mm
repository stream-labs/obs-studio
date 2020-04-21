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

#include "vcam-ipc-client.hpp"

#include <Foundation/Foundation.h>
#include <IOSurface/IOSurface.h>
#include <CoreMedia/CMFormatDescription.h>

VCAM_IPC_Client::VCAM_IPC_Client() {

}

VCAM_IPC_Client::~VCAM_IPC_Client() {

}

std::string VCAM_IPC_Client::deviceCreate(std::string name, 
    uint32_t width, uint32_t height, double fps) {
    PrintFunction();

    this->registerPeer(false);

    if (!this->m_serverMessagePort || !this->m_messagePort)
        return {};

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", CREATE_DEVICE);
    xpc_dictionary_set_string(dictionary, "port", this->m_portName.c_str());
    xpc_dictionary_set_string(dictionary, "name", name.c_str());
    xpc_dictionary_set_uint64(dictionary, "width", width);
    xpc_dictionary_set_uint64(dictionary, "height", height);
    xpc_dictionary_set_double(dictionary, "fps", fps);

    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                             dictionary);
    xpc_release(dictionary);
    auto replyType = xpc_get_type(reply);

    if (replyType != XPC_TYPE_DICTIONARY) {
        xpc_release(reply);
        return {};
    }

    std::string deviceId(xpc_dictionary_get_string(reply, "device"));
    xpc_release(reply);

    return deviceId;
}

bool VCAM_IPC_Client::deviceDestroy(const std::string &deviceId) {
    PrintFunction();

    if (!this->m_serverMessagePort)
        return false;

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", REMOVE_DEVICE);
    xpc_dictionary_set_string(dictionary, "device", deviceId.c_str());
    xpc_connection_send_message(this->m_serverMessagePort,
                                dictionary);
    xpc_release(dictionary);

    return true;
}

bool VCAM_IPC_Client::destroyAllDevices() {
    PrintFunction();

    return true;
}

bool VCAM_IPC_Client::deviceUploadFrame(const std::string deviceID,
                    uint32_t surfaceID,
                    const uint8_t *frame,
                    uint32_t size) {
    // PrintFunction();

    if (!this->m_serverMessagePort)
        return false;

    auto surface = IOSurfaceLookup((IOSurfaceID) surfaceID);
    if (!surface)
        return false;

    uint32_t surfaceSeed = 0;
    IOSurfaceLock(surface, 0, &surfaceSeed);
    
    auto data = IOSurfaceGetBaseAddress(surface);
    if (!frame)
        return false;
    memcpy(data, frame, size);

    IOSurfaceUnlock(surface, 0, &surfaceSeed);
    auto surfaceObj = IOSurfaceCreateXPCObject(surface);

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", FRAME);
    xpc_dictionary_set_string(dictionary, "device", deviceID.c_str());
    xpc_dictionary_set_value(dictionary, "frame", surfaceObj);
    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                             dictionary);
    bool res = xpc_dictionary_get_bool(reply, "status"); 
    xpc_release(dictionary);
    xpc_release(reply);
    xpc_release(surfaceObj);

    return res;
}
void VCAM_IPC_Client::setMirroring(const std::string &deviceId,
                    bool horizontalMirrored,
                    bool verticalMirrored) {
    PrintFunction();

    if (!this->m_serverMessagePort)
        return;

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", SET_MIRRORING);
    xpc_dictionary_set_string(dictionary, "device", deviceId.c_str());
    xpc_dictionary_set_bool(dictionary, "hmirror", horizontalMirrored);
    xpc_dictionary_set_bool(dictionary, "vmirror", verticalMirrored);
    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                             dictionary);
    xpc_release(dictionary);
    xpc_release(reply);
}

bool VCAM_IPC_Client::startDaemon() {
    PrintFunction();

    return this->loadDaemon();
}

void VCAM_IPC_Client::removeDaemon() {
    PrintFunction();

    this->unloadDaemon();

    auto daemonsPath =
        replace(vcam_agent_path, "~", this->homePath());

    std::string daemon = daemonsPath + "/" + vcam_agent + ".plist";
    ::remove(daemon.c_str());
}