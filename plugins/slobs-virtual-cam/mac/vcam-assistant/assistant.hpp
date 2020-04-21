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

#ifndef ASSISTANT_H
#define ASSISTANT_H

#include <string>
#include <map>
#include <xpc/xpc.h>
#include <vector>

#include "logger.h"
#include "../vcam-ipc/include/vcam-ipc-constants.h"

struct AssistantDevice
{
    std::string name;
    uint32_t width;
    uint32_t height;
    double fps;
    bool horizontalMirror {false};
    bool verticalMirror {false};
};

using AssistantPeers = std::map<std::string, xpc_connection_t>;
using DeviceConfigs = std::map<std::string, AssistantDevice>;

typedef std::function<void (xpc_connection_t, xpc_object_t)> Function;

class Assistant
{
    public:
        Assistant();
        ~Assistant();

        void setTimeout(double timeout);
        void messageReceived(xpc_connection_t client, xpc_object_t event);

    private:
        AssistantPeers m_servers;
        AssistantPeers m_clients;
        std::map<VCAM_IPC_EVENT, Function> functions;

        DeviceInfo *device = nullptr;

        DeviceConfigs m_deviceConfigs;
        CFRunLoopTimerRef m_timer {nullptr};
        double m_timeout {0.0};

        inline static uint64_t id();
        bool startTimer();
        void stopTimer();
        static void timerTimeout(CFRunLoopTimerRef timer, void *info);

        void addConnection(xpc_connection_t client, xpc_object_t event);
        void removeConnection(xpc_connection_t client, xpc_object_t event);
    
        void deviceCreate(xpc_connection_t client, xpc_object_t event);
        void deviceDestroyById(const std::string &deviceId);
        void deviceDestroy(xpc_connection_t client, xpc_object_t event);
        void setMirroring(xpc_connection_t client, xpc_object_t event);
        void frameReady(xpc_connection_t client, xpc_object_t event);
        void getDevice(xpc_connection_t client, xpc_object_t event);
        void mirroring(xpc_connection_t client, xpc_object_t event);

        void registerFunction(VCAM_IPC_EVENT event, Function func) {
            functions.emplace(std::make_pair(event, func)); 
        };

        void unregisterFunction(VCAM_IPC_EVENT event) {
            functions.erase(event);
        }

        void callFunction(VCAM_IPC_EVENT event, xpc_connection_t connection, xpc_object_t object) {
            auto it = functions.find(event);
            if (it == functions.end())
                return;

            it->second(connection, object);
        }
};

#endif