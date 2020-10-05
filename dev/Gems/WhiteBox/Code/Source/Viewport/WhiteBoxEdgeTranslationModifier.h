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

#include "Viewport/WhiteBoxViewportConstants.h"

#include <AzCore/Component/ComponentBus.h>
#include <WhiteBox/WhiteBoxToolApi.h>

namespace AzToolsFramework
{
    class PlanarManipulator;

    namespace ViewportInteraction
    {
        struct MouseInteraction;
    }
} // namespace AzToolsFramework

namespace WhiteBox
{
    class ManipulatorViewEdge;

    //! EdgeTranslationModifier provides the ability to select and draw an edge in the viewport.
    class EdgeTranslationModifier
    {
    public:
        AZ_CLASS_ALLOCATOR_DECL

        EdgeTranslationModifier(
            const AZ::EntityComponentIdPair& entityComponentIdPair, Api::EdgeHandle edgeHandle,
            const AZ::Vector3& intersectionPoint);
        ~EdgeTranslationModifier();

        bool MouseOver() const;
        void ForwardMouseOverEvent(const AzToolsFramework::ViewportInteraction::MouseInteraction& interaction);

        Api::EdgeHandle GetHandle() const; //!< Return the currently hovered edge (generic context version).
        Api::EdgeHandle GetEdgeHandle() const; //!< Return the currently hovered edge.
        Api::EdgeHandles::const_iterator EdgeHandlesBegin() const; //!< Return begin iterator to edge handles.
        Api::EdgeHandles::const_iterator EdgeHandlesEnd() const; //!< Return end iterator to edge handles.
        void SetEdgeHandle(Api::EdgeHandle edgeHandle);

        void SetColors(const AZ::Color& color, const AZ::Color& hoverColor);
        void SetWidths(float width, float hoverWidth);

        void Refresh();
        void CreateView();
        bool PerformingAction() const;

    private:
        void CreateManipulator();
        void DestroyManipulator();

        Api::EdgeHandles m_edgeHandles; //!< The edge handles this modifier is currently associated with (edge group).
        Api::EdgeHandle m_hoveredEdgeHandle; //!< The edge handle the mouse is currently over.
        //! The entity and component id this modifier is associated with.
        AZ::EntityComponentIdPair m_entityComponentIdPair;
        //! Manipulators for performing edge translations.
        AZStd::shared_ptr<AzToolsFramework::PlanarManipulator> m_translationManipulator;
        //! Manipulator views used to represent mesh edges for translation.
        AZStd::vector<AZStd::shared_ptr<ManipulatorViewEdge>> m_edgeViews;
        AZ::Color m_color = cl_whiteBoxEdgeUserColor; //!< The color to use for the regular edge.
        AZ::Color m_hoverColor = cl_whiteBoxEdgeHoveredColor; //!< The color to use for the selected/highlighted edge.
        float m_width = cl_whiteBoxEdgeVisualWidth; //!< The width to use for the regular edge.
        //! The visible width to use for the selected/highlighted edge.
        float m_hoverWidth = cl_whiteBoxSelectedEdgeVisualWidth;
    };

    AZStd::array<AZ::Vector3, 2> GetEdgeNormalAxes(const AZ::Vector3& start, const AZ::Vector3& end);

    inline Api::EdgeHandle EdgeTranslationModifier::GetHandle() const
    {
        return GetEdgeHandle();
    }

    inline Api::EdgeHandle EdgeTranslationModifier::GetEdgeHandle() const
    {
        return m_hoveredEdgeHandle;
    }

    inline Api::EdgeHandles::const_iterator EdgeTranslationModifier::EdgeHandlesBegin() const
    {
        return m_edgeHandles.cbegin();
    }

    inline Api::EdgeHandles::const_iterator EdgeTranslationModifier::EdgeHandlesEnd() const
    {
        return m_edgeHandles.cend();
    }

    inline void EdgeTranslationModifier::SetEdgeHandle(const Api::EdgeHandle edgeHandle)
    {
        m_hoveredEdgeHandle = edgeHandle;
    }
} // namespace WhiteBox
