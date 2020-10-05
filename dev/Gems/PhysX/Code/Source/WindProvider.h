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

#include <AzCore/Component/TickBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Physics/WindBus.h>
#include <PhysX/ConfigurationBus.h>

namespace PhysX
{
    //! Implementation of Physics::WindRequests EBus.
    //! Uses wind tag values to identify entities that serve as wind data providers and
    //! PhysX World Force Regions for wind velocity values.
    class WindProvider final
        : public AZ::Interface<Physics::WindRequests>::Registrar
        , private Physics::WindRequestsBus::Handler
        , private ConfigurationNotificationBus::Handler
        , private AZ::TickBus::Handler
    {
    public:
        WindProvider();
        ~WindProvider();

        // AZ::Interface<Physics::WindRequests>::Registrar
        AZ::Vector3 GetGlobalWind() const override;
        AZ::Vector3 GetWind(const AZ::Vector3& worldPosition) const override;
        AZ::Vector3 GetWind(const AZ::Aabb& aabb) const override;

    private:
        // ConfigurationNotificationBus::Handler
        void OnPhysXConfigurationLoaded() override;
        void OnPhysXConfigurationRefreshed(const PhysXConfiguration& configuration) override;

        // AZ::TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        int GetTickOrder() override;

        void CreateNewHandlers();

        class EntityGroupHandler;
        AZStd::unique_ptr<EntityGroupHandler> m_globalWindHandler;
        AZStd::unique_ptr<EntityGroupHandler> m_localWindHandler;
    };
} // namespace PhysX

