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

#ifndef IPC_CONSTANTS_H
#define IPC_CONSTANTS_H

#include <xpc/xpc.h>
#include <string>
#include <functional>

static std::string vcam_assisant       = "vcam-assistant";
static std::string vcam_agent          = "org.streamlabs.vcam-assistant";
static std::string vcam_client         = "org.streamlabs.client";
static std::string vcam_server         = "org.streamlabs.server";

static std::string vcam_plugin_path    = "/Library/CoreMediaIO/Plug-Ins/DAL";
static std::string vcam_plugin         = "vcam-plugin";
static std::string vcam_agent_path     = "~/Library/LaunchAgents";
static std::string vcam_vendor         = "Streamlabs";
static std::string vcam_product        = "Streamlabs OBS";

static uint8_t BYTES_PER_PIXEL = 2;

enum VCAM_IPC_EVENT: uint8_t {
    FRAME,
    ADD_CONNECTION,
    REMOVE_CONNECTION,
    GET_DEVICE,
    CREATE_DEVICE,
    REMOVE_DEVICE,
    MIRRORING,
    SET_MIRRORING,
};

struct DeviceInfo {
    std::string deviceID;
    std::string name;
    uint32_t width;
    uint32_t height;
    double fps;
    bool horizontalMirror;
    bool verticalMirror;
};

struct FrameInfo {
    uint32_t width;
    uint32_t height;
    uint32_t pix_format;
};

#endif
