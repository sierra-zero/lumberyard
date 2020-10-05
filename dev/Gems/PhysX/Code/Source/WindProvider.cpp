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
#include <PhysX_precompiled.h>

#include <WindProvider.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/containers/vector.h>
#include <LmbrCentral/Scripting/TagComponentBus.h>
#include <PhysX/ForceRegionComponentBus.h>
#include <PhysX/ColliderShapeBus.h>

namespace PhysX
{
    class EntityTransformHandler final
        : private AZ::TransformNotificationBus::Handler
    {
    public:
        using ChangeCallback = AZStd::function<void()>;

        EntityTransformHandler(AZ::EntityId entityId, ChangeCallback changeCallback)
            : m_changeCallback(changeCallback)
        {
            AZ::TransformNotificationBus::Handler::BusConnect(entityId);
        }

        ~EntityTransformHandler()
        {
            AZ::TransformNotificationBus::Handler::BusDisconnect();
        }

    private:
        // AZ::TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& /*local*/, const AZ::Transform& /*world*/) override
        {
            m_changeCallback();
        }

        ChangeCallback m_changeCallback;
    };

    class WindProvider::EntityGroupHandler final
        : private LmbrCentral::TagGlobalNotificationBus::Handler
        , private ForceRegionNotificationBus::Handler
    {
    public:
        using ChangeCallback = AZStd::function<void(const EntityGroupHandler&)>;

        EntityGroupHandler(AZ::Crc32 tag, ChangeCallback changeCallback)
            : m_changeCallback(changeCallback)
        {
            LmbrCentral::TagGlobalNotificationBus::Handler::BusConnect(tag);
            ForceRegionNotificationBus::Handler::BusConnect();
        }

        ~EntityGroupHandler()
        {
            LmbrCentral::TagGlobalNotificationBus::Handler::BusDisconnect();
            ForceRegionNotificationBus::Handler::BusDisconnect();
        }

        template <typename FilterEntityFunc>
        AZ::Vector3 GetWind(FilterEntityFunc&& filterFunc) const
        {
            AZ::Vector3 value = AZ::Vector3::CreateZero();

            for (AZ::EntityId entityId : m_entities)
            {
                if (!filterFunc(entityId))
                {
                    continue;
                }

                AZ::Vector3 direction = AZ::Vector3::CreateZero();
                ForceWorldSpaceRequestBus::EventResult(
                    direction, entityId, &ForceWorldSpaceRequestBus::Events::GetDirection);

                float magnitude = 0.0f;
                ForceWorldSpaceRequestBus::EventResult(
                    magnitude, entityId, &ForceWorldSpaceRequestBus::Events::GetMagnitude);

                if (!direction.IsZero())
                {
                    value += direction.GetNormalized() * magnitude;
                }
            }

            return value;
        }

        AZ::Vector3 GetWind() const
        {
            return GetWind(
                [](AZ::EntityId /*entityId*/) { return true; }
            );
        }

        const AZStd::vector<AZ::EntityId>& GetEntities() const
        {
            return m_entities;
        }

        const AZStd::vector<AZ::Aabb>& GetPendingAabbUpdates() const
        {
            return m_pendingAabbUpdates;
        }

        void OnTick()
        {
            if (m_changed)
            {
                m_changeCallback(*this);
                m_changed = false;
                m_pendingAabbUpdates.clear();
            }
        }

    private:
        // LmbrCentral::TagGlobalNotificationBus::MultiHandler
        void OnEntityTagAdded(const AZ::EntityId& entityId) override
        {
            AZ_Error(
                "PhysX Wind",
                AZStd::find(m_entities.begin(), m_entities.end(), entityId) == m_entities.end(),
                "Wind provider entity was already registered. ID: %llu.", entityId
            );

            m_entities.emplace_back(entityId);

            m_entityTransformHandlers.emplace_back(entityId,
                [this, entityId]()
                {
                    m_pendingAabbUpdates.push_back();
                    ColliderShapeRequestBus::EventResult(m_pendingAabbUpdates.back()
                        , entityId
                        , &ColliderShapeRequestBus::Events::GetColliderShapeAabb);

                    m_changed = true;
                }
            );

            m_changed = true;
        }

        void OnEntityTagRemoved(const AZ::EntityId& entityId) override
        {
            auto it = AZStd::find(m_entities.begin(), m_entities.end(), entityId);
            if (it != m_entities.end())
            {
                size_t index = AZStd::distance(m_entities.begin(), it);

                AZStd::swap(m_entities[index], m_entities.back());
                m_entities.pop_back();

                AZStd::swap(m_entityTransformHandlers[index], m_entityTransformHandlers.back());
                m_entityTransformHandlers.pop_back();

                m_changed = true;
            }
        }

        // ForceRegionNotificationBus::Handler
        void OnForceRegionForceChanged(AZ::EntityId entityId) override
        {
            auto it = AZStd::find(m_entities.begin(), m_entities.end(), entityId);
            if (it != m_entities.end())
            {
                m_changed = true;
            }
        }

        AZStd::vector<AZ::EntityId> m_entities;
        AZStd::vector<EntityTransformHandler> m_entityTransformHandlers;
        AZStd::vector<AZ::Aabb> m_pendingAabbUpdates;
        ChangeCallback m_changeCallback;
        bool m_changed = false;
    };

    WindProvider::WindProvider()
    {
        Physics::WindRequestsBus::Handler::BusConnect();
        ConfigurationNotificationBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();

        CreateNewHandlers();
    }

    WindProvider::~WindProvider()
    {
        AZ::TickBus::Handler::BusDisconnect();
        ConfigurationNotificationBus::Handler::BusDisconnect();
        Physics::WindRequestsBus::Handler::BusDisconnect();
    }

    AZ::Vector3 WindProvider::GetGlobalWind() const
    {
        if (m_globalWindHandler)
        {
            return m_globalWindHandler->GetWind();
        }

        return AZ::Vector3::CreateZero();
    }

    AZ::Vector3 WindProvider::GetWind(const AZ::Vector3& worldPosition) const
    {
        if (m_localWindHandler)
        {
            return m_localWindHandler->GetWind(
                [&worldPosition](AZ::EntityId entityId)
                {
                    AZ::Aabb forceAabb;
                    ColliderShapeRequestBus::EventResult(forceAabb
                        , entityId
                        , &ColliderShapeRequestBus::Events::GetColliderShapeAabb);

                    return forceAabb.Contains(worldPosition);
                }
            );
        }

        return AZ::Vector3::CreateZero();
    }

    AZ::Vector3 WindProvider::GetWind(const AZ::Aabb& aabb) const
    {
        if (m_localWindHandler)
        {
            return m_localWindHandler->GetWind(
                [&aabb](AZ::EntityId entityId)
                {
                    AZ::Aabb forceAabb;
                    ColliderShapeRequestBus::EventResult(forceAabb
                        , entityId
                        , &ColliderShapeRequestBus::Events::GetColliderShapeAabb);

                    return forceAabb.Overlaps(aabb);
                }
            );
        }

        return AZ::Vector3::CreateZero();
    }

    void WindProvider::OnPhysXConfigurationLoaded()
    {
        CreateNewHandlers();
    }

    void WindProvider::OnPhysXConfigurationRefreshed(const PhysXConfiguration&)
    {
        CreateNewHandlers();
    }

    void WindProvider::CreateNewHandlers()
    {
        m_globalWindHandler.reset();
        m_localWindHandler.reset();

        const PhysXConfiguration& configuration =
            AZ::Interface<PhysX::ConfigurationRequests>::Get()->GetPhysXConfiguration();

        const AZ::Crc32 globalWindTag = AZ::Crc32(configuration.m_windConfiguration.m_globalWindTag);
        if (globalWindTag)
        {
            auto globalWindChangeCallback = [](const EntityGroupHandler& handler)
            {
                Physics::WindNotificationsBus::Broadcast(&Physics::WindNotifications::OnGlobalWindChanged);
            };

            m_globalWindHandler = AZStd::make_unique<EntityGroupHandler>(globalWindTag, globalWindChangeCallback);
        }

        const AZ::Crc32 localWindTag = AZ::Crc32(configuration.m_windConfiguration.m_localWindTag);
        if (localWindTag)
        {
            auto localWindChangeCallback = [](const EntityGroupHandler& handler)
            {
                for (const AZ::Aabb& aabb : handler.GetPendingAabbUpdates())
                {
                    Physics::WindNotificationsBus::Broadcast(&Physics::WindNotifications::OnWindChanged, aabb);
                }

                for (AZ::EntityId entityId : handler.GetEntities())
                {
                    AZ::Aabb forceAabb;
                    ColliderShapeRequestBus::EventResult(forceAabb
                        , entityId
                        , &ColliderShapeRequestBus::Events::GetColliderShapeAabb);

                    Physics::WindNotificationsBus::Broadcast(&Physics::WindNotifications::OnWindChanged, forceAabb);
                }
            };

            m_localWindHandler = AZStd::make_unique<EntityGroupHandler>(localWindTag, localWindChangeCallback);
        }
    }
    void WindProvider::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_globalWindHandler)
        {
            m_globalWindHandler->OnTick();
        }

        if (m_localWindHandler)
        {
            m_localWindHandler->OnTick();
        }
    }

    int WindProvider::GetTickOrder()
    {
        return AZ::TICK_PHYSICS;
    }
} // namespace PhysX
