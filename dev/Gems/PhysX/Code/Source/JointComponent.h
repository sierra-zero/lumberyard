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

#include <PxPhysicsAPI.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TransformBus.h>

#include <Source/Joint.h>

namespace PhysX
{
    /// Base class for game-time generic joint components.
    class JointComponent: public AZ::Component
        , protected AZ::EntityBus::Handler
    {
    public:
        AZ_COMPONENT(JointComponent, "{B01FD1D2-1D91-438D-874A-BF5EB7E919A8}");
        static void Reflect(AZ::ReflectContext* context);

        JointComponent() = default;
        explicit JointComponent(const GenericJointConfiguration& config);
        JointComponent(const GenericJointConfiguration& config
            , const GenericJointLimitsConfiguration& limits);

    protected:
        /// Struct to provide subclasses with native pointers during joint initialization.
        /// See use of GetLeadFollowerInfo().
        struct LeadFollowerInfo
        {
            physx::PxRigidActor* m_leadActor = nullptr;
            physx::PxRigidActor* m_followerActor = nullptr;
            physx::PxTransform m_leadLocal = physx::PxTransform(physx::PxIdentity);
            physx::PxTransform m_followerLocal = physx::PxTransform(physx::PxIdentity);
            Physics::WorldBody* m_leadBody = nullptr;
            Physics::WorldBody* m_followerBody = nullptr;
        };

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::EntityBus
        void OnEntityActivated(const AZ::EntityId&) override;

        /// Invoked in OnPostPhysicsUpdate for specific joint types to instantiate native joint pointer.
        virtual void InitNativeJoint() {};

        physx::PxTransform GetJointLocalPose(const physx::PxRigidActor* actor,
            const physx::PxTransform& jointPose);

        AZ::Transform GetJointTransform(AZ::EntityId entityId,
            const GenericJointConfiguration& jointConfig);

        /// Initializes joint properties common to all native joint types after native joint creation.
        void InitGenericProperties();

        /// Used on initialization by sub-classes to get native pointers from entity IDs.
        /// This allows sub-classes to instantiate specific native types. This base class does not need knowledge of any specific joint type.
        void ObtainLeadFollowerInfo(LeadFollowerInfo& leadFollowerInfo);

        /// Issues warnings for invalid scenarios when initializing a joint from entity IDs.
        void WarnInvalidJointSetup(AZ::EntityId entityId, const AZStd::string& message);

        GenericJointConfiguration m_configuration;
        GenericJointLimitsConfiguration m_limits;
        AZStd::shared_ptr<PhysX::Joint> m_joint = nullptr;
    };
} // namespace PhysX
