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

#include "WhiteBox_precompiled.h"

#include "WhiteBoxComponent.h"

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Rendering/WhiteBoxMaterial.h>
#include <Rendering/WhiteBoxRenderData.h>
#include <Rendering/WhiteBoxRenderMeshInterface.h>
#include <WhiteBox/WhiteBoxBus.h>

namespace WhiteBox
{
    void WhiteBoxComponent::Reflect(AZ::ReflectContext* context)
    {
        WhiteBoxRenderData::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<WhiteBoxComponent, AZ::Component>()->Version(1)->Field(
                "WhiteBoxRenderData", &WhiteBoxComponent::m_whiteBoxRenderData);
        }
    }

    WhiteBoxComponent::WhiteBoxComponent() = default;
    WhiteBoxComponent::~WhiteBoxComponent() = default;

    void WhiteBoxComponent::Activate()
    {
        WhiteBoxRequestBus::BroadcastResult(m_renderMesh, &WhiteBoxRequests::CreateRenderMeshInterface);

        const AZ::EntityId entityId = GetEntityId();

        AZ::Transform worldFromLocal = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldFromLocal, entityId, &AZ::TransformBus::Events::GetWorldTM);

        // generate the mesh
        m_renderMesh->BuildMesh(m_whiteBoxRenderData, worldFromLocal);
        m_renderMesh->SetVisiblity(m_whiteBoxRenderData.m_material.m_visible);

        AZ::TransformNotificationBus::Handler::BusConnect(entityId);
        WhiteBoxComponentRequestBus::Handler::BusConnect(AZ::EntityComponentIdPair(entityId, GetId()));
    }

    void WhiteBoxComponent::Deactivate()
    {
        WhiteBoxComponentRequestBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();

        m_renderMesh.reset();
    }

    void WhiteBoxComponent::GenerateWhiteBoxMesh(const WhiteBoxRenderData& whiteBoxRenderData)
    {
        m_whiteBoxRenderData = whiteBoxRenderData;
    }

    void WhiteBoxComponent::OnTransformChanged([[maybe_unused]] const AZ::Transform& local, const AZ::Transform& world)
    {
        m_renderMesh->UpdateTransform(world);
    }

    bool WhiteBoxComponent::WhiteBoxIsVisible() const
    {
        return m_renderMesh->IsVisible();
    }
} // namespace WhiteBox
