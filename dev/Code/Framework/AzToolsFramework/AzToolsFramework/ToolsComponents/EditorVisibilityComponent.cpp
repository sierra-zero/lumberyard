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
#include "StdAfx.h"
#include "EditorVisibilityComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>

namespace AzToolsFramework
{
    namespace Components
    {
        void EditorVisibilityComponent::Reflect(AZ::ReflectContext* context)
        {
            if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serializeContext->Class<EditorVisibilityComponent, EditorComponentBase>()
                    ->Field("VisibilityFlag", &EditorVisibilityComponent::m_visibilityFlag)
                    ;

                if (AZ::EditContext* editContext = serializeContext->GetEditContext())
                {
                    editContext->Class<EditorVisibilityComponent>("Visibility", "Edit-time entity visibility")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Hide)
                            ->Attribute(AZ::Edit::Attributes::SliceFlags, AZ::Edit::SliceFlags::NotPushable)
                            ->Attribute(AZ::Edit::Attributes::HideIcon, true);
                }
            }
        }

        void EditorVisibilityComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorVisibilityService", 0x90888caf));
        }

        void EditorVisibilityComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& services)
        {
            services.push_back(AZ_CRC("EditorVisibilityService", 0x90888caf));
        }

        EditorVisibilityComponent::~EditorVisibilityComponent()
        {
            // We disconnect from the bus here because we need to be able to respond even if the entity and component are not active
            // This is a special case for certain EditorComponents only!
            EditorVisibilityRequestBus::Handler::BusDisconnect();
        }

        void EditorVisibilityComponent::Init()
        {
            // We connect to the bus here because we need to be able to respond even if the entity and component are not active
            // This is a special case for certain EditorComponents only!
            EditorVisibilityRequestBus::Handler::BusConnect(GetEntityId());
        }

        void EditorVisibilityComponent::SetVisibilityFlag(bool flag)
        {
            if (m_visibilityFlag != flag)
            {
                m_visibilityFlag = flag;

                AzToolsFramework::ToolsApplicationRequestBus::Broadcast(
                    &AzToolsFramework::ToolsApplicationRequestBus::Events::AddDirtyEntity, m_entity->GetId());

                // notify individual entities connected to this bus
                EditorEntityVisibilityNotificationBus::Event(
                    m_entity->GetId(), &EditorEntityVisibilityNotifications::OnEntityVisibilityFlagChanged, flag);
            }
        }

        bool EditorVisibilityComponent::GetVisibilityFlag()
        {
            return m_visibilityFlag;
        }
    } // namespace Components
} // namespace AzToolsFramework
