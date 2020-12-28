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

#include <ScriptCanvas/Libraries/Core/Repeater.h>

#include <Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Utils/SerializationUtils.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Core
        {
            namespace
            {
                AZStd::string StringifyUnits(Internal::BaseTimerNode::TimeUnits timeUnits)
                {
                    switch (timeUnits)
                    {
                    case Internal::BaseTimerNode::TimeUnits::Seconds:
                        return "Seconds";
                    case Internal::BaseTimerNode::TimeUnits::Ticks:
                        return "Ticks";
                    default:
                        break;
                    }

                    return "???";
                }
            }

            /////////////
            // Repeater
            /////////////

            bool Repeater::VersionConverter(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode & classElement)
            {
                /*
                // Old units enum. Here for reference in version conversion
                enum DelayUnits
                {
                    Unknown = -1,
                    Seconds,
                    Ticks
                };
                */

                bool convertedElement = true;

                if (classElement.GetVersion() < 2)
                {
                    if (SerializationUtils::InsertNewBaseClass<Internal::BaseTimerNode>(context, classElement))
                    {
                        int delayUnits = 0;
                        classElement.GetChildData(AZ_CRC("m_delayUnits", 0x41accf72), delayUnits);

                        Internal::BaseTimerNode::TimeUnits timeUnit = Internal::BaseTimerNode::TimeUnits::Unknown;

                        if (delayUnits == 0)
                        {
                            timeUnit = Internal::BaseTimerNode::TimeUnits::Seconds;
                        }
                        else
                        {
                            timeUnit = Internal::BaseTimerNode::TimeUnits::Ticks;
                        }

                        AZ::SerializeContext::DataElementNode* baseTimerNode = classElement.FindSubElement(AZ_CRC("BaseClass1", 0xd4925735));

                        if (baseTimerNode)
                        {
                            baseTimerNode->AddElementWithData<int>(context, "m_timeUnits", static_cast<int>(timeUnit));
                        }

                        classElement.RemoveElementByName(AZ_CRC("m_delayUnits", 0x41accf72));
                    }
                    else
                    {
                        convertedElement = false;
                    }
                }

                return convertedElement;
            }

            void Repeater::OnInit()
            {
                Internal::BaseTimerNode::OnInit();

                AZStd::string slotName = GetTimeSlotName();

                Slot* slot = GetSlotByName(slotName);

                if (slot == nullptr)
                {
                    // Handle older versions and improperly updated names
                    for (auto testUnit : { Internal::BaseTimerNode::Seconds, Internal::BaseTimerNode::Ticks})
                    {
                        AZStd::string legacyName = StringifyUnits(testUnit);

                        slot = GetSlotByName(legacyName);

                        if (slot)
                        {
                            slot->Rename(slotName);
                            m_timeSlotId = slot->GetId();
                            break;
                        }
                    }
                }
            }

            void Repeater::OnInputSignal(const SlotId& slotId)
            {
                m_repetionCount = aznumeric_cast<int>(RepeaterProperty::GetRepetitions(this));

                if (m_repetionCount > 0)
                {
                    StartTimer();
                }
            }

            void Repeater::OnTimeElapsed()
            {
                size_t latentExecutionId = static_cast<size_t>(AZStd::GetTimeNowMicroSecond());
                if (m_latentStartTimerEvent && m_latentStartTimerEvent->HasHandlerConnected())
                {
                    m_latentStartTimerEvent->Signal(AZStd::move(latentExecutionId));
                }
                m_repetionCount--;
                SignalOutput(RepeaterProperty::GetActionSlotId(this), ScriptCanvas::ExecuteMode::UntilNodeIsFoundInStack);

                if (m_repetionCount == 0)
                {
                    StopTimer();
                    SignalOutput(RepeaterProperty::GetCompleteSlotId(this));
                    if (m_latentStopTimerEvent && m_latentStopTimerEvent->HasHandlerConnected())
                    {
                        m_latentStopTimerEvent->Signal(AZStd::move(latentExecutionId));
                    }
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Core/Repeater.generated.cpp>
