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

#ifndef VIRTUALCAM
#define VIRTUALCAM

#include "vcam-ipc-client.hpp"

#include <string>
#include <vector>

class VirtualCam
{
    private:
        VCAM_IPC_Client ipc_client;

    public:
        std::string name     = "";
        uint32_t width       = 0;
        uint32_t height      = 0;
        double   fps         = 0.0;
	    uint32_t  surfaceID  = 0;
	    std::string deviceID = "";
    public:
        VirtualCam();
        ~VirtualCam();

        bool uploadFrame(const uint8_t *frame);

        bool     createWebcam(const std::string name,
                              int width,
                              int height,
                              double fps);
        uint32_t createSharedMemory();
        bool     removeWebcam();
        bool     removeAllWebcams();

        bool     startDaemon();
        bool     removeDaemon();
};

#endif