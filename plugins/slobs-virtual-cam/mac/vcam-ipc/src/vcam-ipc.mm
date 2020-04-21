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

#include "vcam-ipc.hpp"

#include <Foundation/Foundation.h>
#include <fstream>

VCAM_IPC::VCAM_IPC() {
    m_messagePort = nullptr;
    m_serverMessagePort = nullptr;
    client = false;
}

VCAM_IPC::~VCAM_IPC() {
    this->unregisterPeer();
}

void VCAM_IPC::connect(bool client) {
    this->client = client;
    this->registerPeer(client);
}

void VCAM_IPC::disconnect() {
    this->unregisterPeer();
    this->client = false;
}

std::string VCAM_IPC::replace(std::string& str,
    const std::string& from, const std::string& to) const {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return "";
    str.replace(start_pos, from.length(), to);
    return str;
}

bool VCAM_IPC::registerPeer(bool client) {\
    if (!client) {
        std::string plistFile =
                vcam_agent_path + "/" + vcam_agent + ".plist";

        if (!this->fileExists(plistFile)) {
            return false;
        }
    }

    if (this->m_serverMessagePort)
        return true;

    xpc_object_t dictionary = nullptr;
    xpc_object_t reply = nullptr;
    xpc_connection_t messagePort = nullptr;
    xpc_type_t replyType;
    bool status = false;

    auto serverMessagePort =
            xpc_connection_create_mach_service(vcam_agent.c_str(),
                                               nullptr,
                                               0);

    if (!serverMessagePort)
        goto registerEndPoint_failed;

    xpc_connection_set_event_handler(serverMessagePort,
        ^(xpc_object_t event) {
    });
    xpc_connection_resume(serverMessagePort);

    messagePort = xpc_connection_create(nullptr, nullptr);

    if (!messagePort)
        goto registerEndPoint_failed;

    xpc_connection_set_event_handler(messagePort, ^(xpc_object_t event) {
        auto type = xpc_get_type(event);

        if (type == XPC_TYPE_ERROR)
            return;

        auto client = reinterpret_cast<xpc_connection_t>(event);

        xpc_connection_set_event_handler(client, ^(xpc_object_t event) {
            this->messageReceived(client, event);
        });

        xpc_connection_resume(client);
    });

    xpc_connection_resume(messagePort);

    dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", ADD_CONNECTION);
    xpc_dictionary_set_bool(dictionary, "client", client);
    xpc_dictionary_set_connection(dictionary, "connection", messagePort);
    reply = xpc_connection_send_message_with_reply_sync(serverMessagePort,
                                                        dictionary);
    xpc_release(dictionary);
    replyType = xpc_get_type(reply);

    if (replyType == XPC_TYPE_DICTIONARY) {
        this->m_portName = 
            xpc_dictionary_get_string(reply, "port");
        status = xpc_dictionary_get_bool(reply, "status");
    }

    xpc_release(reply);

    if (replyType != XPC_TYPE_DICTIONARY || !status)
        goto registerEndPoint_failed;

    this->m_messagePort = messagePort;
    this->m_serverMessagePort = serverMessagePort;

    return true;

registerEndPoint_failed:
    if (messagePort)
        xpc_release(messagePort);

    if (serverMessagePort)
        xpc_release(serverMessagePort);

    return false;
}

void VCAM_IPC::unregisterPeer() {
    if (this->m_messagePort) {
        xpc_release(this->m_messagePort);
        this->m_messagePort = nullptr;
    }

    if (this->m_serverMessagePort) {
        if (!this->m_portName.empty()) {
            auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
            xpc_dictionary_set_int64(dictionary, "message", REMOVE_CONNECTION);
            xpc_dictionary_set_string(dictionary, "port", this->m_portName.c_str());
            xpc_connection_send_message(this->m_serverMessagePort,
                                        dictionary);
            xpc_release(dictionary);
        }

        xpc_release(this->m_serverMessagePort);
        this->m_serverMessagePort = nullptr;
    }

    this->m_portName.clear();
}

void VCAM_IPC::messageReceived(xpc_connection_t client, xpc_object_t event) {
    auto type = xpc_get_type(event);

    if (type == XPC_TYPE_ERROR) {
        auto description = xpc_copy_description(event);
        free(description);
    } else if (type == XPC_TYPE_DICTIONARY) {
        auto message = xpc_dictionary_get_int64(event, "message");

        auto it = this->functions.find((VCAM_IPC_EVENT) message);
        if (it == this->functions.end())
            return;
        
        it->second(client, event);
    }
}

DeviceInfo *VCAM_IPC::getDevice() {
    PrintFunction();
    if (!this->m_serverMessagePort)
        return nullptr;

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", GET_DEVICE);
    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                                dictionary);
    xpc_release(dictionary);
    auto replyType = xpc_get_type(reply);

    if (replyType != XPC_TYPE_DICTIONARY) {
        xpc_release(reply);
        return nullptr;
    }

    DeviceInfo *device = new DeviceInfo();
    device->deviceID = xpc_dictionary_get_string(reply, "device");
    if (device->deviceID.empty()) {
        xpc_release(reply);
        return nullptr;
    }

    device->name = xpc_dictionary_get_string(reply, "name");
    device->width = xpc_dictionary_get_uint64(reply, "width");
    device->height = xpc_dictionary_get_uint64(reply, "height");
    device->fps = xpc_dictionary_get_double(reply, "fps");
    return device;
}

bool VCAM_IPC::isHorizontalMirrored(const std::string &deviceId) {
    PrintFunction();

    if (!this->m_serverMessagePort)
        return false;

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", MIRRORING);
    xpc_dictionary_set_string(dictionary, "device", deviceId.c_str());
    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                             dictionary);
    xpc_release(dictionary);
    auto replyType = xpc_get_type(reply);

    if (replyType != XPC_TYPE_DICTIONARY) {
        xpc_release(reply);

        return false;
    }

    bool horizontalMirror = xpc_dictionary_get_bool(reply, "hmirror");
    xpc_release(reply);

    return horizontalMirror;
}

bool VCAM_IPC::isVerticalMirrored(const std::string &deviceId) {
    PrintFunction();

    if (!this->m_serverMessagePort)
        return false;

    auto dictionary = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_int64(dictionary, "message", MIRRORING);
    xpc_dictionary_set_string(dictionary, "device", deviceId.c_str());
    auto reply = xpc_connection_send_message_with_reply_sync(this->m_serverMessagePort,
                                                             dictionary);
    xpc_release(dictionary);
    auto replyType = xpc_get_type(reply);

    if (replyType != XPC_TYPE_DICTIONARY) {
        xpc_release(reply);

        return false;
    }

    bool verticalMirror = xpc_dictionary_get_bool(reply, "vmirror");
    xpc_release(reply);

    return verticalMirror;
}

std::string VCAM_IPC::homePath() const
{
    auto homePath = NSHomeDirectory();

    if (!homePath)
        return {};

    return std::string(homePath.UTF8String);
}

bool VCAM_IPC::fileExists(const std::wstring &path) const
{
    return this->fileExists(std::string(path.begin(), path.end()));
}

bool VCAM_IPC::fileExists(const std::string &path) const
{
    struct stat stats;
    memset(&stats, 0, sizeof(struct stat));

    return stat(path.c_str(), &stats) == 0;
}

std::wstring VCAM_IPC::fileName(const std::wstring &path) const
{
    return path.substr(path.rfind(L'/') + 1);
}

bool VCAM_IPC::mkpath(const std::string &path) const
{
    if (path.empty())
        return false;

    if (this->fileExists(path))
        return true;

    // Create parent folders
    for (auto pos = path.find('/');
         pos != std::string::npos;
         pos = path.find('/', pos + 1)) {
        auto path_ = path.substr(0, pos);

        if (path_.empty() || this->fileExists(path_))
            continue;

        if (mkdir(path_.c_str(),
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
            return false;
    }

    return !mkdir(path.c_str(),
                  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

bool createDaemonPlist(const std::string &fileName)
{
    PrintFunction();
    std::fstream plistFile;
    plistFile.open(fileName, std::ios_base::out);

    if (!plistFile.is_open())
        return false;

    plistFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl
              << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
              << "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
              << std::endl
              << "<plist version=\"1.0\">" << std::endl
              << "    <dict>" << std::endl
              << "        <key>Label</key>" << std::endl
              << "        <string>" << vcam_agent
                                    << "</string>" << std::endl
              << "        <key>ProgramArguments</key>" << std::endl
              << "        <array>" << std::endl
              << "            <string>" << vcam_plugin_path
                                        << "/"
                                        << vcam_plugin
                                        << ".plugin/Contents/Resources/"
                                        << vcam_assisant
                                        << "</string>" << std::endl
              << "            <string>--timeout</string>" << std::endl
              << "            <string>300.0</string>" << std::endl
              << "        </array>" << std::endl
              << "        <key>MachServices</key>" << std::endl
              << "        <dict>" << std::endl
              << "            <key>" << vcam_agent
                                     << "</key>" << std::endl
              << "            <true/>" << std::endl
              << "        </dict>" << std::endl;
    plistFile << "    </dict>" << std::endl
              << "</plist>" << std::endl;

    std::cout << "success creating plist" << std::endl;
    return true;
}

bool VCAM_IPC::loadDaemon()
{
    auto daemonsPath = replace(vcam_agent_path, "~", this->homePath());
    auto dstDaemonsPath = daemonsPath + "/" + vcam_agent + ".plist";

    if (!this->fileExists(dstDaemonsPath)) {
        std::cout << "Daemon file plist doesn't exist" << std::endl;
        return createDaemonPlist(dstDaemonsPath);
    }

    auto launchctl = popen(("launchctl load -w '" + dstDaemonsPath + "'").c_str(),
                       "r");

    bool result = launchctl && !pclose(launchctl);
    return result;
}

void VCAM_IPC::unloadDaemon() const
{
    PrintFunction();
    std::string daemonPlist = vcam_agent + ".plist";
    auto daemonsPath = replace(vcam_agent_path, "~", this->homePath());
    auto dstDaemonsPath = daemonsPath + "/" + daemonPlist;

    if (!this->fileExists(dstDaemonsPath))
        return ;

    auto launchctl =
            popen(("launchctl unload -w '" + dstDaemonsPath + "'").c_str(),
                   "r");

    bool result = launchctl && !pclose(launchctl);
}
