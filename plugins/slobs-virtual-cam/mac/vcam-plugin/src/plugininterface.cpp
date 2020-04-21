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

#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <IOKit/audio/IOAudioTypes.h>

#include "plugininterface.hpp"
#include "util.h"
#include "logger.h"
#include "vcam-ipc-constants.h"

#include <vector>

struct PluginInterfacePrivate
{
    public:
        CMIOHardwarePlugInInterface *pluginInterface;
        PluginInterface *self;
        ULONG m_ref;
        ULONG m_reserved;
        VCAM_IPC_Server ipc_server;

        void updateDevices();
        static HRESULT QueryInterface(void *self,
                                        REFIID uuid,
                                        LPVOID *interface);
        static ULONG AddRef(void *self);
        static ULONG Release(void *self);
        static OSStatus Initialize(CMIOHardwarePlugInRef self);
        static OSStatus InitializeWithObjectID(CMIOHardwarePlugInRef self,
                                                CMIOObjectID objectID);
        static OSStatus Teardown(CMIOHardwarePlugInRef self);
        static void ObjectShow(CMIOHardwarePlugInRef self,
                                CMIOObjectID objectID);
        static Boolean ObjectHasProperty(CMIOHardwarePlugInRef self,
                                            CMIOObjectID objectID,
                                            const CMIOObjectPropertyAddress *address);
        static OSStatus ObjectIsPropertySettable(CMIOHardwarePlugInRef self,
                                                    CMIOObjectID objectID,
                                                    const CMIOObjectPropertyAddress *address,
                                                    Boolean *isSettable);
        static OSStatus ObjectGetPropertyDataSize(CMIOHardwarePlugInRef self,
                                                    CMIOObjectID objectID,
                                                    const CMIOObjectPropertyAddress *address,
                                                    UInt32 qualifierDataSize,
                                                    const void *qualifierData,
                                                    UInt32 *dataSize);
        static OSStatus ObjectGetPropertyData(CMIOHardwarePlugInRef self,
                                                CMIOObjectID objectID,
                                                const CMIOObjectPropertyAddress *address,
                                                UInt32 qualifierDataSize,
                                                const void *qualifierData,
                                                UInt32 dataSize,
                                                UInt32 *dataUsed,
                                                void *data);
        static OSStatus ObjectSetPropertyData(CMIOHardwarePlugInRef self,
                                                CMIOObjectID objectID,
                                                const CMIOObjectPropertyAddress *address,
                                                UInt32 qualifierDataSize,
                                                const void *qualifierData,
                                                UInt32 dataSize,
                                                const void *data);
        static OSStatus DeviceSuspend(CMIOHardwarePlugInRef self,
                                        CMIODeviceID device);
        static OSStatus DeviceResume(CMIOHardwarePlugInRef self,
                                        CMIODeviceID device);
        static OSStatus DeviceStartStream(CMIOHardwarePlugInRef self,
                                            CMIODeviceID device,
                                            CMIOStreamID stream);
        static OSStatus DeviceStopStream(CMIOHardwarePlugInRef self,
                                            CMIODeviceID device,
                                            CMIOStreamID stream);
        static OSStatus DeviceProcessAVCCommand(CMIOHardwarePlugInRef self,
                                                CMIODeviceID device,
                                                CMIODeviceAVCCommand *ioAVCCommand);
        static OSStatus DeviceProcessRS422Command(CMIOHardwarePlugInRef self,
                                                    CMIODeviceID device,
                                                    CMIODeviceRS422Command *ioRS422Command);
        static OSStatus StreamCopyBufferQueue(CMIOHardwarePlugInRef self,
                                                CMIOStreamID stream,
                                                CMIODeviceStreamQueueAlteredProc queueAlteredProc,
                                                void *queueAlteredRefCon,
                                                CMSimpleQueueRef *queue);
        static OSStatus StreamDeckPlay(CMIOHardwarePlugInRef self,
                                        CMIOStreamID stream);
        static OSStatus StreamDeckStop(CMIOHardwarePlugInRef self,
                                        CMIOStreamID stream);
        static OSStatus StreamDeckJog(CMIOHardwarePlugInRef self,
                                        CMIOStreamID stream,
                                        SInt32 speed);
        static OSStatus StreamDeckCueTo(CMIOHardwarePlugInRef self,
                                        CMIOStreamID stream,
                                        Float64 frameNumber,
                                        Boolean playOnCue);
};

PluginInterface::PluginInterface():
    ObjectInterface(),
    m_objectID(0)
{
    this->m_className = "PluginInterface";
    this->d = new PluginInterfacePrivate;
    this->d->self = this;
    this->d->pluginInterface = new CMIOHardwarePlugInInterface {
        //	Padding for COM
        NULL,

        // IUnknown Routines
        PluginInterfacePrivate::QueryInterface,
        PluginInterfacePrivate::AddRef,
        PluginInterfacePrivate::Release,

        // DAL Plug-In Routines
        PluginInterfacePrivate::Initialize,
        PluginInterfacePrivate::InitializeWithObjectID,
        PluginInterfacePrivate::Teardown,
        PluginInterfacePrivate::ObjectShow,
        PluginInterfacePrivate::ObjectHasProperty,
        PluginInterfacePrivate::ObjectIsPropertySettable,
        PluginInterfacePrivate::ObjectGetPropertyDataSize,
        PluginInterfacePrivate::ObjectGetPropertyData,
        PluginInterfacePrivate::ObjectSetPropertyData,
        PluginInterfacePrivate::DeviceSuspend,
        PluginInterfacePrivate::DeviceResume,
        PluginInterfacePrivate::DeviceStartStream,
        PluginInterfacePrivate::DeviceStopStream,
        PluginInterfacePrivate::DeviceProcessAVCCommand,
        PluginInterfacePrivate::DeviceProcessRS422Command,
        PluginInterfacePrivate::StreamCopyBufferQueue,
        PluginInterfacePrivate::StreamDeckPlay,
        PluginInterfacePrivate::StreamDeckStop,
        PluginInterfacePrivate::StreamDeckJog,
        PluginInterfacePrivate::StreamDeckCueTo
    };
    this->d->m_ref = 0;
    this->d->m_reserved = 0;

    auto homePath = std::string("/Users/") + getenv("USER");

    std::stringstream ss;
    ss << vcam_agent_path << "/" << vcam_agent << ".plist";
    auto daemon = ss.str();

    if (daemon[0] == '~')
        daemon.replace(0, 1, homePath);

    struct stat fileInfo;

    if (stat(daemon.c_str(), &fileInfo) == 0)
        this->d->ipc_server.connect(true);

    this->d->ipc_server.connectDeviceAdded(this, &PluginInterface::deviceAdded);
    this->d->ipc_server.connectDeviceRemoved(this, &PluginInterface::deviceRemoved);
    this->d->ipc_server.connectFrameReady(this, &PluginInterface::frameReady);
    this->d->ipc_server.connectMirrorChanged(this, &PluginInterface::setMirror);
}

PluginInterface::~PluginInterface()
{
    PrintFunction();
    this->d->ipc_server.disconnect();
    delete this->d->pluginInterface;
    delete this->d;
}

CMIOObjectID PluginInterface::objectID() const
{
    PrintFunction();
    return this->m_objectID;
}

CMIOHardwarePlugInRef PluginInterface::create()
{
    PrintFunction();
    auto pluginInterface = new PluginInterface();
    pluginInterface->d->AddRef(pluginInterface->d);

    return reinterpret_cast<CMIOHardwarePlugInRef>(pluginInterface->d);
}

Object *PluginInterface::findObject(CMIOObjectID objectID)
{
    PrintFunction();
    for (auto device: this->m_devices)
        if (auto object = device->findObject(objectID))
            return object;

    return nullptr;
}

HRESULT PluginInterface::QueryInterface(REFIID uuid, LPVOID *interface)
{
    PrintFunction();
    if (!interface)
        return E_POINTER;

    if (uuidEqual(uuid, kCMIOHardwarePlugInInterfaceID)
        || uuidEqual(uuid, IUnknownUUID)) {
        this->d->AddRef(this->d);
        *interface = this->d;

        return S_OK;
    }

    return E_NOINTERFACE;
}

OSStatus PluginInterface::Initialize()
{
    PrintFunction();

    return this->InitializeWithObjectID(kCMIOObjectUnknown);
}

OSStatus PluginInterface::InitializeWithObjectID(CMIOObjectID objectID)
{
    PrintFunction();

    this->m_objectID = objectID;
    auto device = this->d->ipc_server.getDevice();
    if(device && !device->deviceID.empty())
        this->deviceAdded(this,
            device->deviceID,
            device->name,
            device->width,
            device->height,
            device->fps
        );

    return kCMIOHardwareNoError;
}

OSStatus PluginInterface::Teardown()
{
    PrintFunction();
    return kCMIOHardwareNoError;
}

void PluginInterface::deviceAdded(void *userData,
        const std::string &deviceId,
        const std::string &name,
        uint32_t width,
        uint32_t height,
        double fps)
{
    PrintFunction();

    auto self = reinterpret_cast<PluginInterface *>(userData);
    self->createDevice(deviceId, name, width, height, fps);
}

void PluginInterface::deviceRemoved(void *userData,
                                            const std::string &deviceId)
{
    PrintFunction();

    auto self = reinterpret_cast<PluginInterface *>(userData);
    self->destroyDevice(deviceId);
}

void PluginInterface::frameReady(void *userData,
                                         const std::string &deviceId,
                                        const uint8_t *data)
{
    auto self = reinterpret_cast<PluginInterface *>(userData);

    for (auto device: self->m_devices)
        if (device->deviceId() == deviceId)
            device->frameReady(data);
}

void PluginInterface::setMirror(void *userData,
                                        const std::string &deviceId,
                                        bool horizontalMirror,
                                        bool verticalMirror)
{
    PrintFunction();
    auto self = reinterpret_cast<PluginInterface *>(userData);

    for (auto device: self->m_devices)
        if (device->deviceId() == deviceId)
            device->setMirror(horizontalMirror, verticalMirror);
}

bool PluginInterface::createDevice(const std::string &deviceId,
                                    const std::string &name,
                                    uint32_t width,
                                    uint32_t height,
                                    double fps)
{
    PrintFunction();
    StreamPtr stream;
    FrameInfo fi;

    // Create one device.
    auto pluginRef = reinterpret_cast<CMIOHardwarePlugInRef>(this->d);
    auto device = std::make_shared<Device>(pluginRef);
    device->setDeviceId(deviceId);
    this->m_devices.push_back(device);

    // Define device properties.
    device->properties().setProperty(kCMIOObjectPropertyName,
                                     name.c_str());
    device->properties().setProperty(kCMIOObjectPropertyManufacturer,
                                     vcam_vendor.c_str());
    device->properties().setProperty(kCMIODevicePropertyModelUID,
                                     vcam_product.c_str());
    device->properties().setProperty(kCMIODevicePropertyLinkedCoreAudioDeviceUID,
                                     "");
    device->properties().setProperty(kCMIODevicePropertyLinkedAndSyncedCoreAudioDeviceUID,
                                     "");
    device->properties().setProperty(kCMIODevicePropertySuspendedByUser,
                                     UInt32(0));
    device->properties().setProperty(kCMIODevicePropertyHogMode,
                                     pid_t(-1),
                                     false);
    device->properties().setProperty(kCMIODevicePropertyDeviceMaster,
                                     pid_t(-1));
    device->properties().setProperty(kCMIODevicePropertyExcludeNonDALAccess,
                                     UInt32(0));
    device->properties().setProperty(kCMIODevicePropertyDeviceIsAlive,
                                     UInt32(1));
    device->properties().setProperty(kCMIODevicePropertyDeviceUID,
                                     deviceId.c_str());
    device->properties().setProperty(kCMIODevicePropertyTransportType,
                                     UInt32(kIOAudioDeviceTransportTypePCI));
    device->properties().setProperty(kCMIODevicePropertyDeviceIsRunningSomewhere,
                                     UInt32(0));

    if (device->createObject() != kCMIOHardwareNoError)
        goto createDevice_failed;

    stream = device->addStream();

    // Register one stream for this device.
    if (!stream)
        goto createDevice_failed;


    fi.width = width;
    fi.height = height;
    fi.pix_format = kCMPixelFormat_422YpCbCr8;
    stream->setFrameInfo(fi);
    stream->setFPS(fps);

    stream->properties().setProperty(kCMIOStreamPropertyDirection, UInt32(0));

    if (device->registerStreams() != kCMIOHardwareNoError) {
        device->registerStreams(false);

        goto createDevice_failed;
    }

    // Register the device.
    if (device->registerObject() != kCMIOHardwareNoError) {
        device->registerObject(false);
        device->registerStreams(false);

        goto createDevice_failed;
    }

    device->setMirror(this->d->ipc_server.isHorizontalMirrored(deviceId),
                      this->d->ipc_server.isVerticalMirrored(deviceId));
    return true;

createDevice_failed:
    this->m_devices.erase(std::prev(this->m_devices.end()));
    return false;
}

void PluginInterface::destroyDevice(const std::string &deviceId)
{
    PrintFunction();

    for (auto it = this->m_devices.begin(); it != this->m_devices.end(); it++) {
        auto device = *it;

        std::string curDeviceId;
        device->properties().getProperty(kCMIODevicePropertyDeviceUID,
                                         &curDeviceId);

        if (curDeviceId == deviceId) {
            device->stopStreams();
            device->registerObject(false);
            device->registerStreams(false);
            this->m_devices.erase(it);

            break;
        }
    }
}

void PluginInterfacePrivate::updateDevices()
{
    PrintFunction();
    for (auto &device: this->self->m_devices) {
        device->setMirror(this->ipc_server.isHorizontalMirrored(device->deviceId()),
                          this->ipc_server.isVerticalMirrored(device->deviceId()));
    }
}

HRESULT PluginInterfacePrivate::QueryInterface(void *self,
                                                       REFIID uuid,
                                                       LPVOID *interface)
{
    PrintFunction();

    if (!self)
        return E_FAIL;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    return _self->self->QueryInterface(uuid, interface);
}

ULONG PluginInterfacePrivate::AddRef(void *self)
{
    PrintFunction();

    if (!self)
        return 0;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    _self->m_ref++;

    return _self->m_ref;
}

ULONG PluginInterfacePrivate::Release(void *self)
{
    PrintFunction();

    if (!self)
        return 0;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->m_ref > 0) {
        _self->m_ref--;

        if (_self->m_ref < 1) {
            delete _self->self;

            return 0UL;
        }
    }

    return _self->m_ref;
}

OSStatus PluginInterfacePrivate::Initialize(CMIOHardwarePlugInRef self)
{
    PrintFunction();

    if (!self)
        return kCMIOHardwareUnspecifiedError;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    return _self->self->Initialize();
}

OSStatus PluginInterfacePrivate::InitializeWithObjectID(CMIOHardwarePlugInRef self,
                                                                CMIOObjectID objectID)
{
    PrintFunction();

    if (!self)
        return kCMIOHardwareUnspecifiedError;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    return _self->self->InitializeWithObjectID(objectID);
}

OSStatus PluginInterfacePrivate::Teardown(CMIOHardwarePlugInRef self)
{
    PrintFunction();

    if (!self)
        return kCMIOHardwareUnspecifiedError;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    return _self->self->Teardown();
}

void PluginInterfacePrivate::ObjectShow(CMIOHardwarePlugInRef self,
                                                CMIOObjectID objectID)
{
    // PrintFunctionID(objectID);
    PrintFunction();

    if (!self)
        return;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        _self->self->show();
    else if (auto object = _self->self->findObject(objectID))
        object->show();
}

Boolean PluginInterfacePrivate::ObjectHasProperty(CMIOHardwarePlugInRef self,
                                                          CMIOObjectID objectID,
                                                          const CMIOObjectPropertyAddress *address)
{
    // PrintFunctionID(objectID);
    PrintFunction();
    Boolean result = false;

    if (!self)
        return result;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        result = _self->self->hasProperty(address);
    else if (auto object = _self->self->findObject(objectID))
        result = object->hasProperty(address);

    return result;
}

OSStatus PluginInterfacePrivate::ObjectIsPropertySettable(CMIOHardwarePlugInRef self,
                                                                  CMIOObjectID objectID,
                                                                  const CMIOObjectPropertyAddress *address,
                                                                  Boolean *isSettable)
{
    // PrintFunctionID(objectID);
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        status = _self->self->isPropertySettable(address,
                                                 isSettable);
    else if (auto object = _self->self->findObject(objectID))
        status = object->isPropertySettable(address,
                                            isSettable);

    return status;
}

OSStatus PluginInterfacePrivate::ObjectGetPropertyDataSize(CMIOHardwarePlugInRef self,
                                                                   CMIOObjectID objectID,
                                                                   const CMIOObjectPropertyAddress *address,
                                                                   UInt32 qualifierDataSize,
                                                                   const void *qualifierData,
                                                                   UInt32 *dataSize)
{
    // PrintFunctionID(objectID);
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        status = _self->self->getPropertyDataSize(address,
                                                  qualifierDataSize,
                                                  qualifierData,
                                                  dataSize);
    else if (auto object = _self->self->findObject(objectID))
        status = object->getPropertyDataSize(address,
                                             qualifierDataSize,
                                             qualifierData,
                                             dataSize);

    return status;
}

OSStatus PluginInterfacePrivate::ObjectGetPropertyData(CMIOHardwarePlugInRef self,
                                                               CMIOObjectID objectID,
                                                               const CMIOObjectPropertyAddress *address,
                                                               UInt32 qualifierDataSize,
                                                               const void *qualifierData,
                                                               UInt32 dataSize,
                                                               UInt32 *dataUsed,
                                                               void *data)
{
    // PrintFunctionID(objectID);
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        status = _self->self->getPropertyData(address,
                                              qualifierDataSize,
                                              qualifierData,
                                              dataSize,
                                              dataUsed,
                                              data);
    else if (auto object = _self->self->findObject(objectID))
        status = object->getPropertyData(address,
                                         qualifierDataSize,
                                         qualifierData,
                                         dataSize,
                                         dataUsed,
                                         data);

    return status;
}

OSStatus PluginInterfacePrivate::ObjectSetPropertyData(CMIOHardwarePlugInRef self,
                                                               CMIOObjectID objectID,
                                                               const CMIOObjectPropertyAddress *address,
                                                               UInt32 qualifierDataSize,
                                                               const void *qualifierData,
                                                               UInt32 dataSize,
                                                               const void *data)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);

    if (_self->self->objectID() == objectID)
        status = _self->self->setPropertyData(address,
                                              qualifierDataSize,
                                              qualifierData,
                                              dataSize,
                                              data);
    else if (auto object = _self->self->findObject(objectID))
        status = object->setPropertyData(address,
                                         qualifierDataSize,
                                         qualifierData,
                                         dataSize,
                                         data);

    return status;
}

OSStatus PluginInterfacePrivate::DeviceSuspend(CMIOHardwarePlugInRef self,
                                                       CMIODeviceID device)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->suspend();

    return status;
}

OSStatus PluginInterfacePrivate::DeviceResume(CMIOHardwarePlugInRef self,
                                                      CMIODeviceID device)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->resume();

    return status;
}

OSStatus PluginInterfacePrivate::DeviceStartStream(CMIOHardwarePlugInRef self,
                                                           CMIODeviceID device,
                                                           CMIOStreamID stream)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->startStream(stream);

    return status;
}

OSStatus PluginInterfacePrivate::DeviceStopStream(CMIOHardwarePlugInRef self,
                                                          CMIODeviceID device,
                                                          CMIOStreamID stream)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->stopStream(stream);

    return status;
}

OSStatus PluginInterfacePrivate::DeviceProcessAVCCommand(CMIOHardwarePlugInRef self,
                                                                 CMIODeviceID device,
                                                                 CMIODeviceAVCCommand *ioAVCCommand)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->processAVCCommand(ioAVCCommand);

    return status;
}

OSStatus PluginInterfacePrivate::DeviceProcessRS422Command(CMIOHardwarePlugInRef self,
                                                                   CMIODeviceID device,
                                                                   CMIODeviceRS422Command *ioRS422Command)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Device *>(_self->self->findObject(device));

    if (object)
        status = object->processRS422Command(ioRS422Command);

    return status;
}

OSStatus PluginInterfacePrivate::StreamCopyBufferQueue(CMIOHardwarePlugInRef self,
                                                               CMIOStreamID stream,
                                                               CMIODeviceStreamQueueAlteredProc queueAlteredProc,
                                                               void *queueAlteredRefCon,
                                                               CMSimpleQueueRef *queue)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Stream *>(_self->self->findObject(stream));

    if (object)
        status = object->copyBufferQueue(queueAlteredProc,
                                         queueAlteredRefCon,
                                         queue);

    return status;
}

OSStatus PluginInterfacePrivate::StreamDeckPlay(CMIOHardwarePlugInRef self,
                                                        CMIOStreamID stream)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Stream *>(_self->self->findObject(stream));

    if (object)
        status = object->deckPlay();

    return status;
}

OSStatus PluginInterfacePrivate::StreamDeckStop(CMIOHardwarePlugInRef self,
                                                        CMIOStreamID stream)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Stream *>(_self->self->findObject(stream));

    if (object)
        status = object->deckStop();

    return status;
}

OSStatus PluginInterfacePrivate::StreamDeckJog(CMIOHardwarePlugInRef self,
                                                       CMIOStreamID stream,
                                                       SInt32 speed)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Stream *>(_self->self->findObject(stream));

    if (object)
        status = object->deckJog(speed);

    return status;
}

OSStatus PluginInterfacePrivate::StreamDeckCueTo(CMIOHardwarePlugInRef self,
                                                         CMIOStreamID stream,
                                                         Float64 frameNumber,
                                                         Boolean playOnCue)
{
    PrintFunction();
    OSStatus status = kCMIOHardwareUnspecifiedError;

    if (!self)
        return status;

    auto _self = reinterpret_cast<PluginInterfacePrivate *>(self);
    auto object = reinterpret_cast<Stream *>(_self->self->findObject(stream));

    if (object)
        status = object->deckCueTo(frameNumber, playOnCue);

    return status;
}