﻿/*
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

#include <PhysX_precompiled.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/IntersectSegment.h>
#include <AzCore/Math/VectorFloat.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/ViewportSelection/EditorSelectionUtil.h>

#include <PhysX/ConfigurationBus.h>
#include <Editor/EditorJointComponentMode.h>
#include <Source/EditorColliderComponent.h>
#include <Source/EditorJointComponent.h>
#include <Source/EditorRigidBodyComponent.h>
#include <Source/Utils.h>

namespace PhysX
{
    void EditorJointComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorJointComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(1)
                ->Field("Configuration", &EditorJointComponent::m_config)
                ;

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorJointComponent>(
                    "PhysX Joint", "The joint constrains the position and orientation of a body to another.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "PhysX")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(0, &EditorJointComponent::m_config, "Standard Joint Parameters", "Joint parameters shared by all joint types")
                    ;
            }
        }
    }

    void EditorJointComponent::Activate()
    {
        AzToolsFramework::Components::EditorComponentBase::Activate();

        m_config.m_followerEntity = GetEntityId();

        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(m_config.m_followerEntity);
        AzToolsFramework::EditorComponentSelectionNotificationsBus::Handler::BusConnect(m_config.m_followerEntity);
        PhysX::EditorJointRequestBus::Handler::BusConnect(AZ::EntityComponentIdPair(m_config.m_followerEntity, GetId()));
        AzFramework::EntityDebugDisplayEventBus::Handler::BusConnect(m_config.m_followerEntity);
    }

    void EditorJointComponent::Deactivate()
    {
        AzFramework::EntityDebugDisplayEventBus::Handler::BusDisconnect();
        PhysX::EditorJointRequestBus::Handler::BusDisconnect();

        AzToolsFramework::EditorComponentSelectionNotificationsBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        AzToolsFramework::Components::EditorComponentBase::Deactivate();
    }

    AZ::Aabb EditorJointComponent::GetEditorSelectionBoundsViewport(
        const AzFramework::ViewportInfo& viewportInfo)
    {
        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(
            worldTM, GetEntityId(), &AZ::TransformInterface::GetWorldTM);
        const AZ::Vector3 position = worldTM.GetPosition();

        AzFramework::CameraState cameraState;
        AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::EventResult(
            cameraState, viewportInfo.m_viewportId,
            &AzToolsFramework::ViewportInteraction::ViewportInteractionRequestBus::Events::GetCameraState);

        const float screenToWorldScale = AzToolsFramework::CalculateScreenToWorldMultiplier(
            position, cameraState);

        const float halfAABBSize = 0.5f * screenToWorldScale;
        const AZ::Vector3 selectionSize(halfAABBSize, halfAABBSize, halfAABBSize);
        const AZ::Vector3 positionMin = position - selectionSize;
        const AZ::Vector3 positionMax = position + selectionSize;

        AZ::Aabb aabb = AZ::Aabb::CreateFromMinMax(positionMin, positionMax);

        return aabb;
    }

    bool EditorJointComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        const AZ::Vector3& src, const AZ::Vector3& dir, AZ::VectorFloat& distance)
    {
        const float rayLength = 10000.0f;
        AZ::Vector3 startNormal;
        AZ::VectorFloat tEnd;
        AZ::VectorFloat tStart;
        const AZ::Vector3 scaledDir = dir * rayLength;
        const AZ::Aabb aabb = GetEditorSelectionBoundsViewport(viewportInfo);
        const int intersection = AZ::Intersect::IntersectRayAABB(
            src, scaledDir, scaledDir.GetReciprocal(), aabb, tStart, tEnd, startNormal);

        if (intersection == AZ::Intersect::RayAABBIsectTypes::ISECT_RAY_AABB_ISECT)
        {
            distance = tStart * rayLength;
            return true;
        }
        else
        {
            return false;
        }
    }

    AZ::u32 EditorJointComponent::GetBoundingBoxDisplayType()
    {
        return BoundingBox;
    }

    bool EditorJointComponent::GetBoolValue(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterComponentMode)
        {
            return m_config.m_inComponentMode;
        }
        else if (parameterName == PhysX::EditorJointComponentMode::s_parameterSelectOnSnap)
        {
            return m_config.m_selectLeadOnSnap;
        }
        AZ_Error("EditorJointComponent::GetBoolValue", false, "bool parameter not recognized: %s", parameterName.c_str());
        return false;
    }

    AZ::EntityId EditorJointComponent::GetEntityIdValue(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterLeadEntity)
        {
            return m_config.m_leadEntity;
        }
        AZ_Error("EditorJointComponent::GetEntityIdValue", false, "EntityId parameter not recognized: %s", parameterName.c_str());
        AZ::EntityId defaultEntityId;
        defaultEntityId.SetInvalid();
        return defaultEntityId;
    }

    float EditorJointComponent::GetLinearValue(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterMaxForce)
        {
            return m_config.m_forceMax;
        }
        else if (parameterName == PhysX::EditorJointComponentMode::s_parameterMaxTorque)
        {
            return m_config.m_torqueMax;
        }
        AZ_Error("EditorJointComponent::GetLinearValue", false, "Linear value parameter not recognized: %s", parameterName.c_str());
        return 0.0f;
    }

    AngleLimitsFloatPair EditorJointComponent::GetLinearValuePair(const AZStd::string& parameterName)
    {
        AZ_UNUSED(parameterName);
        AZ_Error("EditorJointComponent::GetLinearValuePair", false, "Linear value pair parameter not recognized: %s", parameterName.c_str());
        return AngleLimitsFloatPair();
    }

    AZ::Transform EditorJointComponent::GetTransformValue(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterTransform)
        {
            return AZ::Transform::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateFromEulerAnglesDegrees(m_config.m_localRotation),
                m_config.m_localPosition);
        }
        AZ_Error("EditorJointComponent::GetTransformValue", false, "Transform value parameter not recognized: %s", parameterName.c_str());
        return AZ::Transform::CreateIdentity();
    }

    AZ::Vector3 EditorJointComponent::GetVector3Value(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterPosition)
        {
            return m_config.m_localPosition;
        }
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterRotation)
        {
            return m_config.m_localRotation;
        }
        AZ_Error("EditorJointComponent::GetVector3Value", false, "Vector3 value parameter not recognized: %s", parameterName.c_str());
        return AZ::Vector3::CreateZero();
    }

    bool EditorJointComponent::IsParameterUsed(const AZStd::string& parameterName)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterMaxForce
            || parameterName == PhysX::EditorJointComponentMode::s_parameterMaxTorque
            )
        {
            return m_config.m_breakable;
        }
        return true; // Sub-component mode always enabled unless disabled explicitly.
    }

    void EditorJointComponent::SetLinearValue(const AZStd::string& parameterName, float value)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterMaxForce)
        {
            m_config.m_forceMax = value;
        }
        else if (parameterName == PhysX::EditorJointComponentMode::s_parameterMaxTorque)
        {
            m_config.m_torqueMax = value;
        }
    }

    void EditorJointComponent::SetLinearValuePair(const AZStd::string& parameterName, const AngleLimitsFloatPair& valuePair)
    {
        AZ_UNUSED(parameterName);
        AZ_UNUSED(valuePair);
    }

    void EditorJointComponent::SetVector3Value(const AZStd::string& parameterName, const AZ::Vector3& value)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterPosition)
        {
            m_config.m_localPosition = value;
        }
        else if (parameterName == PhysX::EditorJointComponentMode::s_parameterRotation)
        {
            m_config.m_localRotation = value;
        }
    }

    void EditorJointComponent::SetBoolValue(const AZStd::string& parameterName, bool value)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterComponentMode)
        {
            m_config.m_inComponentMode = value;

            AzToolsFramework::ToolsApplicationEvents::Bus::Broadcast(
                &AzToolsFramework::ToolsApplicationEvents::InvalidatePropertyDisplay
                , AzToolsFramework::Refresh_EntireTree);
        }
        else if (parameterName == PhysX::EditorJointComponentMode::s_parameterSelectOnSnap)
        {
            m_config.m_selectLeadOnSnap = value;
        }
    }

    void EditorJointComponent::SetEntityIdValue(const AZStd::string& parameterName, AZ::EntityId value)
    {
        if (parameterName == PhysX::EditorJointComponentMode::s_parameterLeadEntity)
        {
            m_config.SetLeadEntityId(value);
        }
    }

    void EditorJointComponent::DisplayEntityViewport(
        const AzFramework::ViewportInfo& viewportInfo,
        AzFramework::DebugDisplayRequests& debugDisplay)
    {
        AZ_UNUSED(viewportInfo);

        PhysX::PhysXConfiguration globalConfiguration;
        PhysX::ConfigurationRequestBus::BroadcastResult(globalConfiguration, &ConfigurationRequests::GetPhysXConfiguration);

        if (globalConfiguration.m_editorConfiguration.m_showJointHierarchy)
        {
            const AZ::Color leadLineColor = globalConfiguration.m_editorConfiguration.GetJointLeadColor();
            const AZ::Color followerLineColor = globalConfiguration.m_editorConfiguration.GetJointFollowerColor();

            const AZ::Transform followerWorldTransform = PhysX::Utils::GetEntityWorldTransformWithoutScale(m_config.m_followerEntity);
            const AZ::Vector3 followerWorldPosition = followerWorldTransform.GetPosition();

            const AZ::Transform jointLocalTransform = AZ::Transform::CreateFromQuaternionAndTranslation(AZ::Quaternion::CreateFromEulerAnglesDegrees(m_config.m_localRotation),
                m_config.m_localPosition);
            const AZ::Vector3 jointWorldPosition = PhysX::Utils::ComputeJointWorldTransform(jointLocalTransform, followerWorldTransform).GetPosition();

            const AZ::VectorFloat distance = followerWorldPosition.GetDistance(jointWorldPosition);

            const float lineWidth = 4.0f;

            AZ::u32 stateBefore = debugDisplay.GetState();
            debugDisplay.DepthTestOff();
            debugDisplay.SetColor(leadLineColor);
            debugDisplay.SetLineWidth(lineWidth);

            if (m_config.m_leadEntity.IsValid() &&
                distance < globalConfiguration.m_editorConfiguration.m_jointHierarchyDistanceThreshold)
            {
                const AZ::Transform leadWorldTransform = PhysX::Utils::GetEntityWorldTransformWithoutScale(m_config.m_leadEntity);
                const AZ::Vector3 leadWorldPosition = leadWorldTransform.GetPosition();

                const AZ::Vector3 midPoint = (jointWorldPosition + leadWorldPosition) * 0.5f;

                debugDisplay.DrawLine(jointWorldPosition, midPoint);
                debugDisplay.SetColor(followerLineColor);
                debugDisplay.DrawLine(midPoint, leadWorldPosition);
            }
            else
            {
                const AZ::Vector3 midPoint = (jointWorldPosition + followerWorldPosition) * 0.5f;

                debugDisplay.DrawLine(jointWorldPosition, midPoint);
                debugDisplay.SetColor(followerLineColor);
                debugDisplay.DrawLine(midPoint, followerWorldPosition);

            }

            debugDisplay.SetState(stateBefore);
        }
    }
}
