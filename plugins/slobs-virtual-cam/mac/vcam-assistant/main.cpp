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

#include <CoreFoundation/CFRunLoop.h>
#include <xpc/xpc.h>
#include <xpc/connection.h>

#include "assistant.hpp"
#include "logger.h"

#include <iostream>

Assistant vcam_assistant;

int main(int argc, char **argv)
{
    std::string fileName = "/tmp/" + vcam_assisant;
    StartLogging(fileName, "log");

    auto server =
            xpc_connection_create_mach_service(vcam_agent.c_str(),
                                               NULL,
                                               XPC_CONNECTION_MACH_SERVICE_LISTENER);

    if (!server)
        return EXIT_FAILURE;

    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            auto timeout = strtod(argv[i + 1], nullptr);
            vcam_assistant.setTimeout(timeout);

            break;
        }

    xpc_connection_set_event_handler(server, ^(xpc_object_t event) {
        auto type = xpc_get_type(event);

        if (type == XPC_TYPE_ERROR) {
             auto description = xpc_copy_description(event);
             free(description);

             return;
        }
        auto client = reinterpret_cast<xpc_connection_t>(event);

        xpc_connection_set_event_handler(client, ^(xpc_object_t event) {
            vcam_assistant.messageReceived(client, event);
        });

        xpc_connection_resume(client);
    });

    xpc_connection_resume(server);
    CFRunLoopRun();
    xpc_release(server);

    StopLogging();

    return 0;
}