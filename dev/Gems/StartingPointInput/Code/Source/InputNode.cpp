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
#include <StartingPointInput_precompiled.h>
#include <InputNode.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/RTTI/BehaviorContext.h>

// CryCommon
#include <PlayerProfileRequestBus.h>
#include <ScriptCanvas/Libraries/Libraries.h>

namespace InputNodes
{
    void InputNode::OnActivate()
    {
        ScriptCanvas::ExecutionTimingNotificationsBus::EventResult(m_latentStartTimerEvent, this->GetOwningScriptCanvasId(), &ScriptCanvas::ExecutionTimingNotifications::GetLatentStartTimerEvent);
        ScriptCanvas::ExecutionTimingNotificationsBus::EventResult(m_latentStopTimerEvent, this->GetOwningScriptCanvasId(), &ScriptCanvas::ExecutionTimingNotifications::GetLatentStopTimerEvent);
    }

    void InputNode::OnPostActivate()
    {
        if (GetExecutionType() == ScriptCanvas::ExecutionType::Runtime)
        {
            const ScriptCanvas::SlotId eventNameSlotId = InputNodeProperty::GetEventNameSlotId(this);

            m_eventName = (*(FindDatum(eventNameSlotId)->GetAs<ScriptCanvas::Data::StringType>()));
            AZ::InputEventNotificationBus::Handler::BusConnect(AZ::InputEventNotificationId(m_eventName.c_str()));
        }
    }

    void InputNode::OnDeactivate()
    {
        AZ::InputEventNotificationBus::Handler::BusDisconnect();
    }

    void InputNode::OnInputChanged(const ScriptCanvas::Datum& input, const ScriptCanvas::SlotId& slotId)
    {
        // we got a new event name, we need to drop our connection to the old and connect to the new event
        const ScriptCanvas::SlotId eventNameSlotId = InputNodeProperty::GetEventNameSlotId(this);

        if (slotId == eventNameSlotId)
        {
            AZ::InputEventNotificationBus::Handler::BusDisconnect();

            m_eventName = (*input.GetAs<ScriptCanvas::Data::StringType>());
            AZ::InputEventNotificationBus::Handler::BusConnect(AZ::InputEventNotificationId(m_eventName.c_str()));
        }
    }
    
    void InputNode::OnPressed(float value)
    {
        size_t latentExecutionId = static_cast<size_t>(AZStd::GetTimeNowMicroSecond());
        if (m_latentStartTimerEvent && m_latentStartTimerEvent->HasHandlerConnected())
        {
            m_latentStartTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
        m_value = value;
        const ScriptCanvas::Datum output = ScriptCanvas::Datum(m_value);
        const ScriptCanvas::SlotId pressedSlotId = InputNodeProperty::GetPressedSlotId(this);
        const ScriptCanvas::SlotId valueId = InputNodeProperty::GetValueSlotId(this);

        if (auto* slot = GetSlot(valueId))
        {
            PushOutput(output, *slot);
        }

        SignalOutput(pressedSlotId);
        if (m_latentStopTimerEvent && m_latentStopTimerEvent->HasHandlerConnected())
        {
            m_latentStopTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
    }

    void InputNode::OnHeld(float value)
    {
        size_t latentExecutionId = static_cast<size_t>(AZStd::GetTimeNowMicroSecond());
        if (m_latentStartTimerEvent && m_latentStartTimerEvent->HasHandlerConnected())
        {
            m_latentStartTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
        m_value = value;
        const ScriptCanvas::Datum output = ScriptCanvas::Datum(m_value);
        const ScriptCanvas::SlotId heldSlotId = InputNodeProperty::GetHeldSlotId(this);
        const ScriptCanvas::SlotId valueId = InputNodeProperty::GetValueSlotId(this);
        if (auto* slot = GetSlot(valueId))
        {
            PushOutput(output, *slot);
        }
        SignalOutput(heldSlotId);
        if (m_latentStopTimerEvent && m_latentStopTimerEvent->HasHandlerConnected())
        {
            m_latentStopTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
    }

    void InputNode::OnReleased(float value)
    {
        size_t latentExecutionId = static_cast<size_t>(AZStd::GetTimeNowMicroSecond());
        if (m_latentStartTimerEvent && m_latentStartTimerEvent->HasHandlerConnected())
        {
            m_latentStartTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
        m_value = value;
        const ScriptCanvas::Datum output = ScriptCanvas::Datum(m_value);
        const ScriptCanvas::SlotId releasedSlotId = InputNodeProperty::GetReleasedSlotId(this);
        const ScriptCanvas::SlotId valueId = InputNodeProperty::GetValueSlotId(this);

        if (auto* slot = GetSlot(valueId))
        {
            PushOutput(output, *slot);
        }
        SignalOutput(releasedSlotId);
        if (m_latentStopTimerEvent && m_latentStopTimerEvent->HasHandlerConnected())
        {
            m_latentStopTimerEvent->Signal(AZStd::move(latentExecutionId));
        }
    }
} // namespace InputNodes
#include <Source/InputNode.generated.cpp>
