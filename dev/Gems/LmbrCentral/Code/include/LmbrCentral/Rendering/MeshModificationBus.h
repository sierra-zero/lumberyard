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

#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/set.h>

struct IRenderMesh;

namespace LmbrCentral
{
    /*
    * MeshModificationRequestBus
    * Requests for a render mesh to be sent for editing.
    */
    class MeshModificationRequests
        : public AZ::ComponentBus
    {
    public:

        virtual void RequireSendingRenderMeshForModification(size_t lodIndex, size_t primitiveIndex) = 0;

        virtual void StopSendingRenderMeshForModification(size_t lodIndex, size_t primitiveIndex) = 0;

    protected:
        ~MeshModificationRequests() = default;
    };

    using MeshModificationRequestBus = AZ::EBus<MeshModificationRequests>;

    /*
    * MeshModificationRequestHelper
    * Helper class to manage storing indices for the render meshes to edit.
    */
    class MeshModificationRequestHelper
        : public MeshModificationRequestBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        struct MeshLODPrimIndex
        {
            size_t lodIndex;
            size_t primitiveIndex;

            bool operator<(const MeshLODPrimIndex& right) const
            {
                return (lodIndex < right.lodIndex)
                    || (lodIndex == right.lodIndex && primitiveIndex < right.primitiveIndex);
            }
        };

        void Connect(MeshModificationRequestBus::BusIdType busId)
        {
            MeshModificationRequestBus::Handler::BusConnect(busId);
            AZ::TickBus::Handler::BusConnect();
        }

        bool IsConnected()
        {
            return MeshModificationRequestBus::Handler::BusIsConnected();
        }

        void Disconnect()
        {
            AZ::TickBus::Handler::BusDisconnect();
            MeshModificationRequestBus::Handler::BusDisconnect();
        }

        void RequireSendingRenderMeshForModification(size_t lodIndex, size_t primitiveIndex) override
        {
            m_meshesToSendForEditing.insert({ lodIndex, primitiveIndex });
        }

        void StopSendingRenderMeshForModification(size_t lodIndex, size_t primitiveIndex) override
        {
            m_meshesToSendForEditing.erase({ lodIndex, primitiveIndex });
        }

        const AZStd::set<MeshLODPrimIndex>& MeshesToEdit() const
        {
            return m_meshesToSendForEditing;
        }

        bool GetMeshModified() const
        {
            return m_meshModified;
        }

        void SetMeshModified(bool value)
        {
            m_meshModified = value;
        }

    private:
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override
        {
            m_meshModified = false;
        }

        int GetTickOrder() override
        {
            return AZ::TICK_PRE_RENDER;
        }

        AZStd::set<MeshLODPrimIndex> m_meshesToSendForEditing;
        bool m_meshModified = false;
    };

    /*
    * MeshModificationNotificationBus
    * Sends an event when the render mesh data should be edited.
    */
    class MeshModificationNotifications
        : public AZ::ComponentBus
    {
    public:
        using MutexType = AZStd::mutex;

        virtual void ModifyMesh(size_t lodIndex, size_t primitiveIndex, IRenderMesh* renderMesh) {}

    protected:
        ~MeshModificationNotifications() = default;
    };

    using MeshModificationNotificationBus = AZ::EBus<MeshModificationNotifications>;
}// namespace LmbrCentral
