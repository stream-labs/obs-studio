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

#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include "vcam-ipc-server.hpp"
#include "device.hpp"

#include <iostream>

struct PluginInterfacePrivate;

class PluginInterface: public ObjectInterface
{
    public:
        PluginInterface();
        PluginInterface(const PluginInterface &other) = delete;
        ~PluginInterface();

        CMIOObjectID objectID() const;
        static CMIOHardwarePlugInRef create();
        Object *findObject(CMIOObjectID objectID);

        HRESULT QueryInterface(REFIID uuid, LPVOID *interface);
        OSStatus Initialize();
        OSStatus InitializeWithObjectID(CMIOObjectID objectID);
        OSStatus Teardown();

    private:
        PluginInterfacePrivate *d;
        CMIOObjectID m_objectID;
        std::vector<DevicePtr> m_devices;

        static void deviceAdded(void *userData,
                                const std::string &deviceId,
                                    const std::string &name,
                                    uint32_t width,
                                    uint32_t height,
                                    double fps);
        static void deviceRemoved(void *userData,
                                    const std::string &deviceId);
        static void frameReady(void *userData,
                                const std::string &deviceId,
                                const uint8_t *data);
        static void setMirror(void *userData,
                                const std::string &deviceId,
                                bool horizontalMirror,
                                bool verticalMirror);
        bool createDevice(const std::string &deviceId,
                                    const std::string &name,
                                    uint32_t width,
                                    uint32_t height,
                                    double fps);
        void destroyDevice(const std::string &deviceId);

    friend struct PluginInterfacePrivate;
};

#endif