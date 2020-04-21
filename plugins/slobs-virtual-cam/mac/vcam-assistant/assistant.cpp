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

#include <sstream>
#include <CoreFoundation/CoreFoundation.h>
#include <xpc/xpc.h>
#include <xpc/connection.h>

#include "assistant.hpp"

#include <iostream>

#define PrintFunction() \
    Print("VCAM-ASSISTANT::", __FUNCTION__, "()")

Assistant::Assistant()
{
    this->registerFunction(FRAME,
        std::bind(&Assistant::frameReady,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(ADD_CONNECTION,
        std::bind(&Assistant::addConnection,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(REMOVE_CONNECTION,
        std::bind(&Assistant::removeConnection,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(CREATE_DEVICE,
        std::bind(&Assistant::deviceCreate,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(REMOVE_DEVICE,
        std::bind(&Assistant::deviceDestroy,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(GET_DEVICE,
        std::bind(&Assistant::getDevice,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(MIRRORING,
        std::bind(&Assistant::mirroring,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->registerFunction(SET_MIRRORING,
        std::bind(&Assistant::setMirroring,
            this, 
            std::placeholders::_1,
            std::placeholders::_2
        )
    );

    this->startTimer();
}

Assistant::~Assistant()
{
    std::vector<AssistantPeers *> allPeers {
        &this->m_clients,
        &this->m_servers
    };

    for (auto &device: this->m_deviceConfigs) {
        auto notification = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_int64(notification, "message", REMOVE_DEVICE);
        xpc_dictionary_set_string(notification, "device", device.first.c_str());

        for (auto peers: allPeers)
            for (auto &peer: *peers)
                xpc_connection_send_message(peer.second, notification);

        xpc_release(notification);
    }
}

void Assistant::setTimeout(double timeout)
{
    this->m_timeout = timeout;
}

void Assistant::messageReceived(xpc_connection_t client,
                                        xpc_object_t event)
{
    auto type = xpc_get_type(event);

    if (type == XPC_TYPE_DICTIONARY) {
        int64_t message = xpc_dictionary_get_int64(event, "message");

        auto it = this->functions.find((VCAM_IPC_EVENT) message);
        if (it == this->functions.end())
            return;
        
        it->second(client, event);
    }
}

uint64_t Assistant::id()
{
    static uint64_t id = 0;

    return id++;
}

bool Assistant::startTimer()
{
    // AkAssistantPrivateLogMethod();

    if (this->m_timer || this->m_timeout <= 0.0)
        return false;

    // If no peer has been connected for 5 minutes shutdown the assistant.
    CFRunLoopTimerContext context {0, this, nullptr, nullptr, nullptr};
    this->m_timer =
            CFRunLoopTimerCreate(kCFAllocatorDefault,
                                 CFAbsoluteTimeGetCurrent() + this->m_timeout,
                                 0,
                                 0,
                                 0,
                                 Assistant::timerTimeout,
                                 &context);

    if (!this->m_timer)
        return false;

    CFRunLoopAddTimer(CFRunLoopGetMain(),
                      this->m_timer,
                      kCFRunLoopCommonModes);

    return true;
}

void Assistant::stopTimer()
{
    // AkAssistantPrivateLogMethod();

    if (!this->m_timer)
        return;

    CFRunLoopTimerInvalidate(this->m_timer);
    CFRunLoopRemoveTimer(CFRunLoopGetMain(),
                         this->m_timer,
                         kCFRunLoopCommonModes);
    CFRelease(this->m_timer);
    this->m_timer = nullptr;
}

void Assistant::timerTimeout(CFRunLoopTimerRef timer, void *info)
{
    CFRunLoopStop(CFRunLoopGetMain());
}

void Assistant::addConnection(xpc_connection_t client,
                                       xpc_object_t event)
{
    bool isClient = xpc_dictionary_get_bool(event, "client");
    std::string portName = isClient ?
                vcam_client.c_str():
                vcam_server.c_str();
    portName += std::to_string(this->id());
    auto endpoint = xpc_dictionary_get_value(event, "connection");
    auto connection = xpc_connection_create_from_endpoint(reinterpret_cast<xpc_endpoint_t>(endpoint));
    xpc_connection_set_event_handler(connection, ^(xpc_object_t) {});
    xpc_connection_resume(connection);
    bool ok = true;
    AssistantPeers *peers;

    if (portName.find(vcam_client.c_str()) != std::string::npos)
        peers = &this->m_clients;
    else
        peers = &this->m_servers;

    for (auto &peer: *peers)
        if (peer.first == portName) {
            ok = false;
            break;
        }

    if (ok) {
        (*peers)[portName] = connection;
        this->stopTimer();
    }

    auto reply = xpc_dictionary_create_reply(event);
    xpc_dictionary_set_string(reply, "port", portName.c_str());
    xpc_dictionary_set_bool(reply, "status", ok);
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void Assistant::removeConnection(xpc_connection_t client,
                                          xpc_object_t event)
{
    auto portName = xpc_dictionary_get_string(event, "port");

    std::vector<AssistantPeers *> allPeers {
        &this->m_clients,
        &this->m_servers
    };

    bool breakLoop = false;

    for (auto peers: allPeers) {
        for (auto &peer: *peers)
            if (peer.first == portName) {
                xpc_release(peer.second);
                peers->erase(portName);
                breakLoop = true;

                break;
            }

        if (breakLoop)
            break;
    }

    bool peersEmpty = this->m_servers.empty() && this->m_clients.empty();

    if (peersEmpty)
        this->startTimer();
}

void Assistant::deviceCreate(xpc_connection_t client,
                                            xpc_object_t event)
{
    PrintFunction();
    std::string portName = xpc_dictionary_get_string(event, "port");

    this->device = new DeviceInfo();

    this->device->deviceID = "/streamlabs_webcam";
    this->device->name = xpc_dictionary_get_string(event, "name");
    this->device->width = (uint32_t) xpc_dictionary_get_uint64(event, "width");
    this->device->height = (uint32_t) xpc_dictionary_get_uint64(event, "height");
    this->device->fps = xpc_dictionary_get_double(event, "fps");

    auto notification = xpc_copy(event);
    xpc_dictionary_set_string(notification, "device", this->device->deviceID.c_str());
    xpc_dictionary_set_string(notification, "name", this->device->name.c_str());
    xpc_dictionary_set_uint64(notification, "width", this->device->width);
    xpc_dictionary_set_uint64(notification, "height", this->device->height);
    xpc_dictionary_set_double(notification, "fps", this->device->fps);


    for (auto &client: this->m_clients)
        xpc_connection_send_message(client.second, notification);

    xpc_release(notification);

    auto reply = xpc_dictionary_create_reply(event);
    xpc_dictionary_set_string(reply, "device", this->device->deviceID.c_str());
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void Assistant::deviceDestroyById(const std::string &deviceId)
{
    PrintFunction();
    auto it = this->m_deviceConfigs.find(deviceId);

    if (it != this->m_deviceConfigs.end()) {
        this->m_deviceConfigs.erase(it);

        auto notification = xpc_dictionary_create(nullptr, nullptr, 0);
        xpc_dictionary_set_int64(notification, "message", REMOVE_DEVICE);
        xpc_dictionary_set_string(notification, "device", deviceId.c_str());

        for (auto &client: this->m_clients)
            xpc_connection_send_message(client.second, notification);

        xpc_release(notification);
    }
}

void Assistant::deviceDestroy(xpc_connection_t client,
                                             xpc_object_t event)
{
    PrintFunction();
    std::string deviceId = xpc_dictionary_get_string(event, "device");
    this->deviceDestroyById(deviceId);
}

void Assistant::setMirroring(xpc_connection_t client,
                                            xpc_object_t event)
{
    PrintFunction();
    std::string deviceId = xpc_dictionary_get_string(event, "device");
    bool horizontalMirror = xpc_dictionary_get_bool(event, "hmirror");
    bool verticalMirror = xpc_dictionary_get_bool(event, "vmirror");
    bool ok = false;

    if (this->m_deviceConfigs.count(deviceId) > 0)
        if (this->m_deviceConfigs[deviceId].horizontalMirror != horizontalMirror
            || this->m_deviceConfigs[deviceId].verticalMirror != verticalMirror) {
            this->m_deviceConfigs[deviceId].horizontalMirror = horizontalMirror;
            this->m_deviceConfigs[deviceId].verticalMirror = verticalMirror;
            auto notification = xpc_copy(event);

            for (auto &client: this->m_clients)
                xpc_connection_send_message(client.second, notification);

            xpc_release(notification);
            ok = true;
        }

    auto reply = xpc_dictionary_create_reply(event);
    xpc_dictionary_set_bool(reply, "status", ok);
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void Assistant::frameReady(xpc_connection_t client,
                                          xpc_object_t event)
{
    auto reply = xpc_dictionary_create_reply(event);
    bool ok = true;

    for (auto &client: this->m_clients) {
        auto reply = xpc_connection_send_message_with_reply_sync(client.second,
                                                                 event);
        auto replyType = xpc_get_type(reply);
        bool isOk = false;

        if (replyType == XPC_TYPE_DICTIONARY)
            isOk = xpc_dictionary_get_bool(reply, "status");

        ok &= isOk;
        xpc_release(reply);
    }

    xpc_dictionary_set_bool(reply, "status", ok);
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void Assistant::getDevice(xpc_connection_t client,
                                        xpc_object_t event)
{
    PrintFunction();
    auto reply = xpc_dictionary_create_reply(event);
    if (!this->device) {
        xpc_dictionary_set_string(reply, "device", "");
    } else {
        xpc_dictionary_set_string(reply, "device", this->device->deviceID.c_str());
        xpc_dictionary_set_string(reply, "name", this->device->name.c_str());
        xpc_dictionary_set_uint64(reply, "width", this->device->width);
        xpc_dictionary_set_uint64(reply, "height", this->device->height);
        xpc_dictionary_set_double(reply, "fps", this->device->fps);
    }

    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

void Assistant::mirroring(xpc_connection_t client,
                                         xpc_object_t event)
{
    PrintFunction();
    std::string deviceId = xpc_dictionary_get_string(event, "device");
    bool horizontalMirror = false;
    bool verticalMirror = false;

    if (this->m_deviceConfigs.count(deviceId) > 0) {
        horizontalMirror = this->m_deviceConfigs[deviceId].horizontalMirror;
        verticalMirror = this->m_deviceConfigs[deviceId].verticalMirror;
    }

    auto reply = xpc_dictionary_create_reply(event);
    xpc_dictionary_set_bool(reply, "hmirror", horizontalMirror);
    xpc_dictionary_set_bool(reply, "vmirror", verticalMirror);
    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}