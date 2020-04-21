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

#include <map>
#include <sstream>
#include <algorithm>

#include "util.h"

bool uuidEqual(const REFIID &uuid1, const CFUUIDRef uuid2)
{
    auto iid2 = CFUUIDGetUUIDBytes(uuid2);
    auto puuid1 = reinterpret_cast<const UInt8 *>(&uuid1);
    auto puuid2 = reinterpret_cast<const UInt8 *>(&iid2);

    for (int i = 0; i < 16; i++)
        if (puuid1[i] != puuid2[i])
            return false;

    return true;
}

std::string enumToString(UInt32 value)
{
    auto valueChr = reinterpret_cast<char *>(&value);
    std::stringstream ss;

    for (int i = 3; i >= 0; i--)
        if (valueChr[i] < 0)
            ss << std::hex << valueChr[i];
        else if (valueChr[i] < 32)
            ss << int(valueChr[i]);
        else
            ss << valueChr[i];

    return "'" + ss.str() + "'";
}