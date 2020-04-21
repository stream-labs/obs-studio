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

#ifndef OBJECTPROPERTIES_H
#define OBJECTPROPERTIES_H

#include <map>
#include <vector>
#include <string>
#include <CoreMediaIO/CMIOHardwareObject.h>
#include <CoreAudio/CoreAudioTypes.h>

#include "clock.hpp"
#include "vcam-ipc-constants.h"

class ObjectPropertiesPrivate;
class Object;
typedef std::shared_ptr<Object> ObjectPtr;

enum PropertyType
{
    PropertyTypeUInt32,
    PropertyTypeFloat64,
    PropertyTypePidT,
    PropertyTypeString,
    PropertyTypeWString,
    PropertyTypeObjectVector,
    PropertyTypeObjectPtrVector,
    PropertyTypeVideoFormat,
    PropertyTypeVideoFormatVector,
    PropertyTypeFloat64Vector,
    PropertyTypeAudioValueRangeVector,
    PropertyTypeClock,
    PropertyTypeAddress
};

struct PropertyValue
{
    PropertyType type;
    bool isSettable;

    union
    {
        UInt32 uint32;
        Float64 float64;
        pid_t pidT;
    } num;

    std::string str;
    std::wstring wstr;
    std::vector<Object *> objects;
    std::vector<ObjectPtr> objectsPtr;
    std::vector<FrameInfo> videoFormats;
    std::vector<Float64> float64Vector;
    std::vector<AudioValueRange> audioValueRangeVector;
    FrameInfo videoFormat;
    ClockPtr clock;
    CMIOObjectPropertyAddress address;
};

class ObjectProperties
{
    public:
        ObjectProperties();
        ObjectProperties(const ObjectProperties &other);
        ObjectProperties &operator =(const ObjectProperties &other);
        virtual ~ObjectProperties();

        std::vector<UInt32> properties() const;

        // Set properties
        bool setProperty(UInt32 property,
                            const std::string &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::wstring &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            UInt32 value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            Float64 value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            pid_t value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::vector<Object *> &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::vector<ObjectPtr> &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const FrameInfo &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::vector<FrameInfo> &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::vector<Float64> &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const std::vector<AudioValueRange> &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const ClockPtr &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            const CMIOObjectPropertyAddress &value,
                            bool isSettable=true);
        bool setProperty(UInt32 property,
                            UInt32 dataSize,
                            const void *data);

        // Get properties
        bool getProperty(UInt32 property,
                            UInt32 *value);
        bool getProperty(UInt32 property,
                            Float64 *value);
        bool getProperty(UInt32 property,
                            std::string *value);
        bool getProperty(UInt32 property,
                            FrameInfo *value);
        bool getProperty(UInt32 property,
                            UInt32 qualifierDataSize=0,
                            const void *qualifierData=nullptr,
                            UInt32 dataSize=0,
                            UInt32 *dataUsed=nullptr,
                            void *data=nullptr);

        void removeProperty(UInt32 property);
        void update(const ObjectProperties &other);
        bool isSettable(UInt32 property);

    private:
        std::map<UInt32, PropertyValue> m_properties;

    protected:
        virtual bool qualify(UInt32 property,
                                UInt32 qualifierDataSize,
                                const void *qualifierData,
                                const void *data);
};
#endif