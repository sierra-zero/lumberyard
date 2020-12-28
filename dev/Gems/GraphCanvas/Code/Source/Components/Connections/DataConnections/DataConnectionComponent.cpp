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
#include "precompiled.h"

#include <Components/Connections/DataConnections/DataConnectionComponent.h>

#include <Components/Connections/DataConnections/DataConnectionVisualComponent.h>
#include <Components/StylingComponent.h>
#include <Source/Components/Connections/ConnectionLayerControllerComponent.h>
#include <GraphCanvas/Components/Slots/SlotBus.h>
#include <GraphCanvas/Components/Slots/Data/DataSlotBus.h>

namespace GraphCanvas
{
    ////////////////////////////
    // DataConnectionComponent
    ////////////////////////////

    void DataConnectionComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<DataConnectionComponent, ConnectionComponent>()
                ->Version(1)
            ;
        }
    }

    AZ::Entity* DataConnectionComponent::CreateDataConnection(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection, const AZStd::string& substyle)
    {
        // Create this Connection's entity.
        AZ::Entity* entity = aznew AZ::Entity("Connection");

        entity->CreateComponent<DataConnectionComponent>(sourceEndpoint, targetEndpoint, createModelConnection);
        entity->CreateComponent<StylingComponent>(Styling::Elements::Connection, AZ::EntityId(), substyle);
        entity->CreateComponent<DataConnectionVisualComponent>();
        entity->CreateComponent<ConnectionLayerControllerComponent>();

        return entity;
    }
    
    DataConnectionComponent::DataConnectionComponent(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection)
        : ConnectionComponent(sourceEndpoint, targetEndpoint, createModelConnection)
    {
    }

    bool DataConnectionComponent::AllowNodeCreation() const
    {
        DataSlotType slotType = DataSlotType::Value;

        if (m_sourceEndpoint.IsValid())
        {            
            DataSlotRequestBus::EventResult(slotType, m_sourceEndpoint.GetSlotId(), &DataSlotRequests::GetDataSlotType);
        }
        else if (m_targetEndpoint.IsValid())
        {
            DataSlotRequestBus::EventResult(slotType, m_targetEndpoint.GetSlotId(), &DataSlotRequests::GetDataSlotType);
        }

        return slotType == DataSlotType::Value;
    }
    
    ConnectionComponent::ConnectionMoveResult DataConnectionComponent::OnConnectionMoveComplete(const QPointF& scenePos, const QPoint& screenPos, AZ::EntityId groupTarget)
    {
        ConnectionMoveResult retVal = ConnectionMoveResult::DeleteConnection;

        // If we are missing an endpoint, default to the normal behavior
        if (!m_sourceEndpoint.IsValid() || !m_targetEndpoint.IsValid())
        {
            retVal = ConnectionComponent::OnConnectionMoveComplete(scenePos, screenPos, groupTarget);
        }
        else
        {
            DataSlotType sourceSlotType = DataSlotType::Unknown;
            DataSlotRequestBus::EventResult(sourceSlotType, GetSourceSlotId(), &DataSlotRequests::GetDataSlotType);

            DataSlotType targetSlotType = DataSlotType::Unknown;
            DataSlotRequestBus::EventResult(targetSlotType, GetTargetSlotId(), &DataSlotRequests::GetDataSlotType);

            bool converted = false;

            if (m_dragContext == DragContext::MoveTarget)
            {
                if (sourceSlotType == DataSlotType::Value)
                {
                    DataSlotRequestBus::EventResult(converted, GetTargetSlotId(), &DataSlotRequests::ConvertToValue);
                }
                else if (sourceSlotType == DataSlotType::Reference)
                {
                    DataSlotRequestBus::EventResult(converted, GetTargetSlotId(), &DataSlotRequests::ConvertToReference);
                }
            }
            else if (m_dragContext == DragContext::MoveSource)
            {
                if (targetSlotType == DataSlotType::Value)
                {
                    DataSlotRequestBus::EventResult(converted, GetSourceSlotId(), &DataSlotRequests::ConvertToValue);
                }
                else if (targetSlotType == DataSlotType::Reference)
                {
                    DataSlotRequestBus::EventResult(converted, GetSourceSlotId(), &DataSlotRequests::ConvertToReference);
                }
            }
            else if (m_dragContext == DragContext::TryConnection)
            {
                converted = true;
            }

            if (converted)
            {
                DataSlotType targetSlotType = DataSlotType::Unknown;
                DataSlotRequestBus::EventResult(targetSlotType, GetTargetSlotId(), &DataSlotRequests::GetDataSlotType);

                if (targetSlotType == DataSlotType::Value)
                {
                    retVal = ConnectionComponent::OnConnectionMoveComplete(scenePos, screenPos, groupTarget);
                }
                else if (targetSlotType == DataSlotType::Reference)
                {
                    GraphId graphId;
                    SceneMemberRequestBus::EventResult(graphId, GetEntityId(), &SceneMemberRequests::GetScene);

                    if (m_dragContext == DragContext::MoveSource)
                    {
                        GraphModelRequestBus::Event(graphId, &GraphModelRequests::SynchronizeReferences, GetTargetEndpoint(), GetSourceEndpoint());
                    }
                    else if (m_dragContext == DragContext::MoveTarget)
                    {
                        GraphModelRequestBus::Event(graphId, &GraphModelRequests::SynchronizeReferences, GetSourceEndpoint(), GetTargetEndpoint());
                    }
                    
                    // We don't want the connection to persist.
                    retVal = ConnectionMoveResult::DeleteConnection;
                }
            }
        }
        
        return retVal;
    }
}
