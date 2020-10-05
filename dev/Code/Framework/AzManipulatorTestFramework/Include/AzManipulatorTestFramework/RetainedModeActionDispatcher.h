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

#include <AzManipulatorTestFramework/AzManipulatorTestFramework.h>
#include <AzManipulatorTestFramework/ActionDispatcher.h>
#include <AzManipulatorTestFramework/ImmediateModeActionDispatcher.h>
#include <AzCore/std/containers/list.h>
#include <AzCore/std/functional.h>

namespace AzManipulatorTestFramework
{
    //! Buffers actions to be dispatched upon a call to Execute().
    class RetainedModeActionDispatcher
        : public ActionDispatcher<RetainedModeActionDispatcher>
    {
    public:
        explicit RetainedModeActionDispatcher(ManipulatorViewportInteractionInterface& viewportManipulatorInteraction);
        //! Execute the sequence of actions and lock the dispatcher from adding further actions.
        RetainedModeActionDispatcher* Execute();
        //! Reset the sequence of actions and unlock the dispatcher from adding further actions.
        RetainedModeActionDispatcher* ResetSequence();

    protected:
        // ActionDispatcher ...
        void EnableSnapToGridImpl() override;
        void DisableSnapToGridImpl() override;
        void GridSizeImpl(float size) override;
        void CameraStateImpl(const AzFramework::CameraState& cameraState) override;
        void MouseLButtonDownImpl() override;
        void MouseLButtonUpImpl() override;
        void MouseRayMoveImpl(const AZ::Vector3& delta) override;
        void KeyboardModifierDownImpl(const AzToolsFramework::ViewportInteraction::KeyboardModifier& keyModifier) override;
        void KeyboardModifierUpImpl(const AzToolsFramework::ViewportInteraction::KeyboardModifier& keyModifier) override;
        void ExpectManipulatorBeingInteractedImpl() override;
        void ExpectNotManipulatorBeingInteractedImpl() override;

    private:
        using Action = AZStd::function<void()>;
        void AddActionToSequence(Action&& action);
        ImmediateModeActionDispatcher m_dispatcher;
        AZStd::list<Action> m_actions;
        bool m_locked = false;
    };
} // namespace AzManipulatorTestFramework
