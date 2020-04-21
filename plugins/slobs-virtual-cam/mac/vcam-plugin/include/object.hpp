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

#ifndef OBJECT_H
#define OBJECT_H

#include "objectinterface.hpp"

#include <list>

class Object;
typedef std::shared_ptr<Object> ObjectPtr;

class Object: public ObjectInterface
{
    public:
        Object(CMIOHardwarePlugInRef m_pluginInterface,
                Object *m_parent=nullptr);
        Object(Object *m_parent=nullptr);
        virtual ~Object();

        CMIOObjectID objectID() const;
        UInt32 classID() const;
        virtual OSStatus createObject();
        virtual OSStatus registerObject(bool regist);
        Object *findObject(CMIOObjectID objectID);
        OSStatus propertyChanged(UInt32 numberAddresses,
                                    const CMIOObjectPropertyAddress *addresses);

        OSStatus setPropertyData(const CMIOObjectPropertyAddress *address,
                                    UInt32 qualifierDataSize,
                                    const void *qualifierData,
                                    UInt32 dataSize,
                                    const void *data);

    private:
        void childsUpdate();

    protected:
        CMIOHardwarePlugInRef m_pluginInterface;
        Object *m_parent;
        std::list<Object *> m_childs;
        bool m_isCreated;
};
#endif