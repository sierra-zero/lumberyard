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

#include <Source/HingeJointComponent.h>
#include <PhysX/MathConversion.h>

#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Physics/RigidBody.h>
#include <AzFramework/Physics/RigidBodyBus.h>

#include <PxPhysicsAPI.h>

namespace PhysX
{
    void HingeJointComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<HingeJointComponent, JointComponent>()
                ->Version(2)
            ;
        }
    }

    HingeJointComponent::HingeJointComponent(const GenericJointConfiguration& config
        , const GenericJointLimitsConfiguration& angularLimitConfig)
            : JointComponent(config, angularLimitConfig)
    {
    }

    void HingeJointComponent::InitNativeJoint()
    {
        if (m_joint)
        {
            return;
        }
        
        JointComponent::LeadFollowerInfo leadFollowerInfo;
        ObtainLeadFollowerInfo(leadFollowerInfo);
        if (!leadFollowerInfo.m_followerActor)
        {
            return;
        }
        m_joint = AZStd::make_shared<HingeJoint>(physx::PxRevoluteJointCreate(
            PxGetPhysics(),
            leadFollowerInfo.m_leadActor,
            leadFollowerInfo.m_leadLocal,
            leadFollowerInfo.m_followerActor,
            leadFollowerInfo.m_followerLocal),
            leadFollowerInfo.m_leadBody,
            leadFollowerInfo.m_followerBody);

        InitAngularLimits();
    }

    void HingeJointComponent::InitAngularLimits()
    {
        if (!m_joint)
        {
            return;
        }

        physx::PxRevoluteJoint* revoluteJointNative = static_cast<physx::PxRevoluteJoint*>(m_joint->GetNativePointer());
        if (!revoluteJointNative)
        {
            return;
        }

        if (!m_limits.m_isLimited)
        {
            revoluteJointNative->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, false);
            return;
        }

        physx::PxJointAngularLimitPair limitPair(AZ::DegToRad(m_limits.m_limitSecond)
            , AZ::DegToRad(m_limits.m_limitFirst)
            , m_limits.m_tolerance);
        if (m_limits.m_isSoftLimit)
        {
            limitPair.stiffness = m_limits.m_stiffness;
            limitPair.damping = m_limits.m_damping;
        }

        revoluteJointNative->setLimit(limitPair);
        revoluteJointNative->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, true);
    }
} // namespace PhysX
