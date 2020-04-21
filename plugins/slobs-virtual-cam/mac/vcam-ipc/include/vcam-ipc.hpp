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

#ifndef VCAMIPC
#define VCAMIPC

#include <vector>
#include <string>
#include <map>
#include <functional>

#include <sys/stat.h>
#include <dirent.h>

#include "logger.h"
#include "vcam-ipc-constants.h"
#include "signal.h"

#define PrintFunction() \
    Print("VCAM-IPC::", __FUNCTION__, "()")

typedef std::function<void (xpc_connection_t, xpc_object_t)> Function;

class VCAM_IPC {
protected:
    bool client;
    std::string m_portName;
    xpc_connection_t m_messagePort;
    xpc_connection_t m_serverMessagePort;

private:
    std::map<VCAM_IPC_EVENT, Function> functions;

public:
    VCAM_IPC();
    ~VCAM_IPC();

    void connect(bool client);
    void disconnect();
    bool registerPeer(bool client);
    void unregisterPeer();
    void messageReceived(xpc_connection_t client, xpc_object_t event);

    // Common
    DeviceInfo *getDevice();
    bool isHorizontalMirrored(const std::string &deviceId);
    bool isVerticalMirrored(const std::string &deviceId);

    void registerFunction(VCAM_IPC_EVENT event, Function func) {
        functions.emplace(std::make_pair(event, func)); 
    };

    void unregisterFunction(VCAM_IPC_EVENT event) {
        functions.erase(event);
    }

    // Utility methods
    std::string replace(std::string& str,
        const std::string& from, const std::string& to) const;
    std::string homePath() const;
    bool fileExists(const std::wstring &path) const;
    bool fileExists(const std::string &path) const;
    std::wstring fileName(const std::wstring &path) const;
    bool mkpath(const std::string &path) const;
    bool loadDaemon();
    void unloadDaemon() const;
};

#endif