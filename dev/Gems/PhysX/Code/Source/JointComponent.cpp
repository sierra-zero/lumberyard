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

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzFramework/Physics/RigidBody.h>
#include <AzFramework/Physics/RigidBodyBus.h>

#include <PhysX/NativeTypeIdentifiers.h>
#include <Source/Joint.h>
#include <Source/JointComponent.h>
#include <Source/Utils.h>

namespace PhysX
{
    void JointComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<JointComponent, AZ::Component>()
                ->Version(1)
                ->Field("Joint Configuration", &JointComponent::m_configuration)
                ->Field("Joint Limits", &JointComponent::m_limits)
                ;
        }
    }

    JointComponent::JointComponent(const GenericJointConfiguration& config)
        : m_configuration(config)
    {
    }

    JointComponent::JointComponent(const GenericJointConfiguration& config
        , const GenericJointLimitsConfiguration& limits)
            : m_configuration(AZStd::move(config))
            , m_limits(limits)
    {
    }

    void JointComponent::Activate()
    {
        if (m_configuration.m_followerEntity.IsValid())
        {
            if (m_configuration.m_followerEntity == m_configuration.m_leadEntity)
            {
                AZ_Error("JointComponent::Activate()", 
                    false, 
                    "Joint's lead entity cannot be the same as the entity in which the joint resides. Joint failed to initialize.");
                return;
            }

            AZ::EntityBus::Handler::BusConnect(m_configuration.m_followerEntity);
        }
    }

    void JointComponent::Deactivate()
    {
        AZ::EntityBus::Handler::BusDisconnect();
        m_joint.reset();
    }

    physx::PxTransform JointComponent::GetJointLocalPose(const physx::PxRigidActor* actor
        , const physx::PxTransform& jointPose)
    {
        if (!actor)
        {
            AZ_Error("JointComponent::GetJointLocalPose", false, "Can't get pose for invalid actor pointer.");
            return physx::PxTransform();
        }

        physx::PxTransform actorPose = actor->getGlobalPose();
        physx::PxTransform actorTranslateInv(-actorPose.p);
        physx::PxTransform actorRotateInv(actorPose.q);
        actorRotateInv = actorRotateInv.getInverse();
        return actorRotateInv * actorTranslateInv * jointPose;
    }

    AZ::Transform JointComponent::GetJointTransform(AZ::EntityId entityId
        , const GenericJointConfiguration& jointConfig)
    {
        AZ::Transform jointTransform = PhysX::Utils::GetEntityWorldTransformWithoutScale(entityId);
        jointTransform = jointTransform * jointConfig.m_localTransformFromFollower;
        return jointTransform;
    }

    void JointComponent::InitGenericProperties()
    {
        if (!m_joint)
        {
            return;
        }

        physx::PxJoint* jointNative = static_cast<physx::PxJoint*>(m_joint->GetNativePointer());
        if (!jointNative)
        {
            return;
        }

        jointNative->setConstraintFlag(
            physx::PxConstraintFlag::eCOLLISION_ENABLED,
            m_configuration.GetFlag(GenericJointConfiguration::GenericJointFlag::SelfCollide));

        if (m_configuration.GetFlag(GenericJointConfiguration::GenericJointFlag::Breakable))
        {
            jointNative->setBreakForce(m_configuration.m_forceMax
                , m_configuration.m_torqueMax);
        }
    }

    void JointComponent::ObtainLeadFollowerInfo(JointComponent::LeadFollowerInfo& info)
    {
        info = LeadFollowerInfo();

        if (!m_configuration.m_followerEntity.IsValid())
        {
            return;
        }

        if (m_configuration.m_leadEntity.IsValid())
        {
            Physics::RigidBodyRequestBus::EventResult(info.m_leadBody
                , m_configuration.m_leadEntity
                , &Physics::RigidBodyRequests::GetRigidBody);

            if (!info.m_leadBody)
            {
                const AZStd::string entityWithoutBodyWarningMsg("Rigid body not found in lead entity associated with joint. Joint treated as constraint on global position.");
                WarnInvalidJointSetup(m_configuration.m_leadEntity, entityWithoutBodyWarningMsg);
            }
        }

        Physics::RigidBodyRequestBus::EventResult(info.m_followerBody
            , m_configuration.m_followerEntity
            , &Physics::RigidBodyRequests::GetRigidBody);

        if (!info.m_followerBody)
        {
            const AZStd::string entityWithoutBodyWarningMsg("Rigid body not found in follower entity associated with joint. Please add a rigid body component to the entity.");
            WarnInvalidJointSetup(m_configuration.m_followerEntity, entityWithoutBodyWarningMsg);
            return;
        }

        if (info.m_leadBody)
        {
            info.m_leadActor = static_cast<physx::PxRigidActor*>(info.m_leadBody->GetNativePointer());
        }
        info.m_followerActor = static_cast<physx::PxRigidActor*>(info.m_followerBody->GetNativePointer());

        const AZ::Transform jointTransform = GetJointTransform(GetEntityId(), m_configuration);

        physx::PxTransform jointPose = PxMathConvert(jointTransform);
        if (info.m_leadActor)
        {
            info.m_leadLocal = GetJointLocalPose(info.m_leadActor, jointPose); // joint position & orientation in lead actor's frame.
        }
        else
        {
            info.m_leadLocal = jointPose; // lead is null, attaching follower to global position of joint.
        }
        info.m_followerLocal = PxMathConvert(m_configuration.m_localTransformFromFollower);// joint position & orientation in follower actor's frame.
    }

    void JointComponent::WarnInvalidJointSetup(AZ::EntityId entityId, const AZStd::string& message)
    {
        const AZStd::vector<AZ::EntityId> entityIds = { entityId };
        const char* category = "PhysX Joint";

        PhysX::Utils::WarnEntityNames(entityIds, category, message.c_str());
    }

    void JointComponent::OnEntityActivated(const AZ::EntityId& entityId)
    {
        AZ::EntityBus::Handler::BusDisconnect();

        // If joint has no lead entity, it is a constraint on a global frame (position & orientation).
        // or, follower has been activated, this is the lead being activated.
        if (!m_configuration.m_leadEntity.IsValid() || entityId == m_configuration.m_leadEntity)
        {
            InitNativeJoint(); // Invoke overriden specific joint type instantiation
            InitGenericProperties();
        }
        // Else, follower entity is activated, subscribe to be notified that lead entity is activated.
        else
        {
            AZ::EntityBus::Handler::BusConnect(m_configuration.m_leadEntity);
        }
    }
} // namespace PhysX
