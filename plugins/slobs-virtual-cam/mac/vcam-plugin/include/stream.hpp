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

#include "vcam-ipc-server.hpp"
#include "object.hpp"

class Stream;
typedef std::shared_ptr<Stream> StreamPtr;

class Stream: public Object
{
    public:
        Stream(bool registerObject=false, Object *m_parent=nullptr);
        Stream(const Stream &other) = delete;
        ~Stream();

        OSStatus createObject();
        OSStatus registerObject(bool regist=true);
        void setFrameInfo(const FrameInfo &fi);
        void setFPS(const double &fps);
        bool start();
        void stop();
        bool running();

        void frameReady(const uint8_t *data);
        void setMirror(bool horizontalMirror, bool verticalMirror);

        // Stream Interface
        OSStatus copyBufferQueue(CMIODeviceStreamQueueAlteredProc queueAlteredProc,
                                    void *queueAlteredRefCon,
                                    CMSimpleQueueRef *queue);
        OSStatus deckPlay();
        OSStatus deckStop();
        OSStatus deckJog(SInt32 speed);
        OSStatus deckCueTo(Float64 frameNumber, Boolean playOnCue);

    private:
        ClockPtr m_clock;
        UInt64 m_sequence;
        CMTime m_pts;
        CMSimpleQueueRef m_queue;
        CMIODeviceStreamQueueAlteredProc m_queueAltered {nullptr};
        const uint8_t * m_currentData;
        void *m_queueAlteredRefCon {nullptr};
        CFRunLoopTimerRef m_timer {nullptr};
        std::mutex m_mutex;
        bool m_running {false};
        bool m_horizontalMirror {false};
        bool m_verticalMirror {false};

        bool startTimer();
        void stopTimer();
        static void streamLoop(CFRunLoopTimerRef timer, void *info);
        void sendFrame(const uint8_t *data);
};
