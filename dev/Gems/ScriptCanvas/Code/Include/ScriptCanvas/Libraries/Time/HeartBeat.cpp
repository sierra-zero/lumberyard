/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <ScriptCanvas/Libraries/Time/HeartBeat.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Time
        {
            //////////////
            // HeartBeat
            //////////////

            void HeartBeat::OnInputSignal(const SlotId& slotId)
            {
                if (slotId == HeartBeatProperty::GetStartSlotId(this))
                {
                    StartTimer();
                }
                else if (slotId == HeartBeatProperty::GetStopSlotId(this))
                {
                    StopTimer();
                }
            }

            void HeartBeat::OnTimeElapsed()
            {
                size_t latentExecutionId = static_cast<size_t>(AZStd::GetTimeNowMicroSecond());
                if (m_latentStartTimerEvent && m_latentStartTimerEvent->HasHandlerConnected())
                {
                    m_latentStartTimerEvent->Signal(AZStd::move(latentExecutionId));
                }
                SignalOutput(HeartBeatProperty::GetPulseSlotId(this));
                if (m_latentStopTimerEvent && m_latentStopTimerEvent->HasHandlerConnected())
                {
                    m_latentStopTimerEvent->Signal(AZStd::move(latentExecutionId));
                }
            }
        }
    }
}


#include <Include/ScriptCanvas/Libraries/Time/HeartBeat.generated.cpp>
