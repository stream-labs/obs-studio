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

#include "objectinterface.hpp"
#include "logger.h"
#include "util.h"

ObjectInterface::ObjectInterface():
    m_objectID(0),
    m_classID(0)
{

}

ObjectInterface::~ObjectInterface()
{

}

ObjectProperties ObjectInterface::properties() const
{
    return this->m_properties;
}

ObjectProperties &ObjectInterface::properties()
{
    return this->m_properties;
}

void ObjectInterface::setProperties(const ObjectProperties &properties)
{
    this->m_properties = properties;
}

void ObjectInterface::updateProperties(const ObjectProperties &properties)
{
    this->m_properties.update(properties);
}

CMIOObjectPropertyAddress ObjectInterface::address(CMIOObjectPropertySelector selector,
                                                           CMIOObjectPropertyScope scope,
                                                           CMIOObjectPropertyElement element)
{
    return CMIOObjectPropertyAddress {selector, scope, element};
}

void ObjectInterface::show()
{
    PrintFunction();
}

Boolean ObjectInterface::hasProperty(const CMIOObjectPropertyAddress *address)
{
    PrintFunction();

    if (!this->m_properties.getProperty(address->mSelector)) {
        return false;
    }

    return true;
}

OSStatus ObjectInterface::isPropertySettable(const CMIOObjectPropertyAddress *address,
                                                     Boolean *isSettable)
{
    PrintFunction();

    if (!this->m_properties.getProperty(address->mSelector)) {
        return kCMIOHardwareUnknownPropertyError;
    }

    bool settable = this->m_properties.isSettable(address->mSelector);

    if (isSettable)
        *isSettable = settable;

    return kCMIOHardwareNoError;
}

OSStatus ObjectInterface::getPropertyDataSize(const CMIOObjectPropertyAddress *address,
                                                      UInt32 qualifierDataSize,
                                                      const void *qualifierData,
                                                      UInt32 *dataSize)
{
    PrintFunction();

    if (!this->m_properties.getProperty(address->mSelector,
                                        qualifierDataSize,
                                        qualifierData,
                                        0,
                                        dataSize)) {
        return kCMIOHardwareUnknownPropertyError;
    }

    return kCMIOHardwareNoError;
}

OSStatus ObjectInterface::getPropertyData(const CMIOObjectPropertyAddress *address,
                                                  UInt32 qualifierDataSize,
                                                  const void *qualifierData,
                                                  UInt32 dataSize,
                                                  UInt32 *dataUsed,
                                                  void *data)
{
    PrintFunction();

    if (!this->m_properties.getProperty(address->mSelector,
                                        qualifierDataSize,
                                        qualifierData,
                                        dataSize,
                                        dataUsed,
                                        data)) {
        return kCMIOHardwareUnknownPropertyError;
    }

    return kCMIOHardwareNoError;
}

OSStatus ObjectInterface::setPropertyData(const CMIOObjectPropertyAddress *address,
                                                  UInt32 qualifierDataSize,
                                                  const void *qualifierData,
                                                  UInt32 dataSize,
                                                  const void *data)
{
    PrintFunction();

    if (!this->m_properties.setProperty(address->mSelector,
                                        dataSize,
                                        data)) {
        return kCMIOHardwareUnknownPropertyError;
    }

    return kCMIOHardwareNoError;
}
