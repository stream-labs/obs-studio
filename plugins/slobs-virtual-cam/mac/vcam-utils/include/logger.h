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

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>

#define VCAM_CALLBACK(CallbackName, ...) \
    using CallbackName##CallbackT = void (*)(void *userData, __VA_ARGS__); \
    using CallbackName##Callback = std::pair<void *, CallbackName##CallbackT>;

#define VCAM_CALLBACK_NOARGS(CallbackName) \
    using CallbackName##CallbackT = void (*)(void *userData); \
    using CallbackName##Callback = std::pair<void *, CallbackName##CallbackT>;

#define StartLogging(...) Logger::start(__VA_ARGS__)
#define Print(...) Logger::log(__VA_ARGS__)
#define StopLogging() Logger::stop()

namespace Logger
{
    VCAM_CALLBACK(Log, const char *data, size_t size)

    void start(const std::string &fileName=std::string(),
                const std::string &extension=std::string());
    void start(LogCallback callback);
    std::string header();
    std::ostream &out();
    void log();
    void tlog();
    void stop();

    template<typename First, typename... Next>
    void tlog(const First &first, const Next &... next)
    {
        out() << first;
        tlog(next...);
    }

    template<typename... Param>
    void log(const Param &... param)
    {
        tlog(header(), " ", param...);
    }
}

#endif