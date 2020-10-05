﻿
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
#pragma once

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

#include <Editor/EditorSubComponentModeBase.h>
#include <Editor/EditorViewportEntityPicker.h>

namespace AzToolsFramework
{
    class LinearManipulator;
}

namespace PhysX
{
    /// This sub-component mode uses EditorViewportEntityPicker to get the position of an entity that the mouse is hovering over.
    /// Classes inheriting from this class can use the mouse-over entity position to perform custom actions.
    class EditorSubComponentModeSnap
        : public PhysX::EditorSubComponentModeBase
        , protected AzFramework::EntityDebugDisplayEventBus::Handler
    {
    public:
        EditorSubComponentModeSnap(
            const AZ::EntityComponentIdPair& entityComponentIdPair
            , const AZ::Uuid& componentType
            , const AZStd::string& name);
        virtual ~EditorSubComponentModeSnap() = default;

        // PhysX::EditorSubComponentModeBase
        void HandleMouseInteraction(
            const AzToolsFramework::ViewportInteraction::MouseInteractionEvent& mouseInteraction) override;
        void Refresh() override;

    protected:
        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        /// Override to draw specific snap type display
        virtual void DisplaySpecificSnapType(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay,
            const AZ::Vector3& jointPosition,
            const AZ::Vector3& snapDirection,
            float snapLength) {}

        virtual void InitMouseDownCallBack() = 0;

        AZStd::string GetPickedEntityName();
        AZ::Vector3 GetPosition() const;

        AZStd::shared_ptr<AzToolsFramework::LinearManipulator> m_manipulator;

        EditorViewportEntityPicker m_picker;
        AZ::EntityId m_pickedEntity;
        AZ::Aabb m_pickedEntityAabb = AZ::Aabb::CreateNull();
        AZ::Vector3 m_pickedPosition;
    };
} // namespace PhysX
