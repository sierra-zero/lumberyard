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

#include "EditorBaseShapeComponent.h"
#include "QuadShapeComponent.h"
#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <LmbrCentral/Shape/QuadShapeComponentBus.h>

namespace LmbrCentral
{
    //! Editor representation of a rectangle in 3d space.
    class EditorQuadShapeComponent
        : public EditorBaseShapeComponent
        , private AzFramework::EntityDebugDisplayEventBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(LmbrCentral::EditorQuadShapeComponent, EditorQuadShapeComponentTypeId, EditorBaseShapeComponent);
        static void Reflect(AZ::ReflectContext* context);

        EditorQuadShapeComponent() = default;

        //! AZ::Component overrides...
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        //! EditorComponentBase overrides...
        void BuildGameEntity(AZ::Entity* gameEntity) override;

    protected:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

    private:
        AZ_DISABLE_COPY_MOVE(EditorQuadShapeComponent)

        //! AzFramework::EntityDebugDisplayEventBus overrides...
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        void ConfigurationChanged();

        QuadShape m_quadShape; //! Stores underlying quad representation for this component.
    };
} // namespace LmbrCentral
