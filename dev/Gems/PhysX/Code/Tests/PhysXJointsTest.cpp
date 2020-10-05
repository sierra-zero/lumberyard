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

#include <AzTest/AzTest.h>

#include <Tests/PhysXTestCommon.h>

#include <BoxColliderComponent.h>
#include <RigidBodyComponent.h>
#include <Joint.h>
#include <JointComponent.h>
#include <BallJointComponent.h>
#include <FixedJointComponent.h>
#include <HingeJointComponent.h>

#include <AzCore/Component/TransformBus.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <AzFramework/Physics/SystemBus.h>
#include <AzFramework/Physics/World.h>
#include <Physics/PhysicsTests.h>

namespace PhysX
{
    using PhysXJointsTest = Physics::GenericPhysicsInterfaceTest;

    template<typename JointComponentType>
    AZStd::unique_ptr<AZ::Entity> AddBodyColliderEntity(const AZ::Vector3& position, 
        const AZ::Vector3& initialLinearVelocity,
        AZStd::shared_ptr<GenericJointConfiguration> jointConfig = nullptr,
        AZStd::shared_ptr<GenericJointLimitsConfiguration> jointLimitsConfig = nullptr)
    {
        const char* entityName = "testEntity";
        auto entity = AZStd::make_unique<AZ::Entity>(entityName);

        AZ::TransformConfig transformConfig;
        transformConfig.m_worldTransform = AZ::Transform::CreateTranslation(position);
        entity->CreateComponent<AzFramework::TransformComponent>()->SetConfiguration(transformConfig);

        auto colliderConfiguration = AZStd::make_shared<Physics::ColliderConfiguration>();
        auto boxShapeConfiguration = AZStd::make_shared<Physics::BoxShapeConfiguration>();
        auto boxColliderComponent = entity->CreateComponent<BoxColliderComponent>();
        boxColliderComponent->SetShapeConfigurationList({ AZStd::make_pair(colliderConfiguration, boxShapeConfiguration) });

        Physics::RigidBodyConfiguration rigidBodyConfig;
        rigidBodyConfig.m_initialLinearVelocity = initialLinearVelocity;
        rigidBodyConfig.m_gravityEnabled = false;
        rigidBodyConfig.m_computeMass = false;
        // Make lead body very heavy
        if (!jointConfig)
        {
            rigidBodyConfig.m_mass = 9999.0f;
        }
        entity->CreateComponent<PhysX::RigidBodyComponent>(rigidBodyConfig);

        if (jointConfig)
        {
            jointConfig->m_followerEntity = entity->GetId();

            GenericJointLimitsConfiguration defaultJointLimitsConfig;
            entity->CreateComponent<JointComponentType>(
                    *jointConfig,
                    (jointLimitsConfig)? *jointLimitsConfig : defaultJointLimitsConfig);
        }

        entity->Init();
        entity->Activate();

        return entity;
    }

    AZ::Vector3 RunJointTest(AZ::EntityId followerEntityId)
    {
        AZ::Vector3 followerEndPosition(0.0f, 0.0f, 0.0f);

        AZStd::shared_ptr<Physics::World> world;
        Physics::DefaultWorldBus::BroadcastResult(world, &Physics::DefaultWorldRequests::GetDefaultWorld);
        
        //Run simulation for a while - bounces box once on force volume
        const float deltaTime = 1.0f / 60.0f;
        const AZ::u32 numSteps = 240;
        PhysX::TestUtils::UpdateDefaultWorld(deltaTime, numSteps);
        
        AZ::TransformBus::EventResult(followerEndPosition,
            followerEntityId, 
            &AZ::TransformBus::Events::GetWorldTranslation);

        return followerEndPosition;
    }

    TEST_F(PhysXJointsTest, Joints_FixedJoint_BodiesAreConstrainedAndMoveTogether)
    {
        // Place lead on the right of follower, tie them together with fixed joint and send the lead moving to the right.
        // The follower should be pulled along if the fixed joint works.

        const AZ::Vector3 followerPosition(-1.0f, 0.0f, 0.0f);
        const AZ::Vector3 followerInitialLinearVelocity(0.0f, 0.0f, 0.0f);

        const AZ::Vector3 leadPosition(1.0f, 0.0f, 0.0f);
        const AZ::Vector3 leadInitialLinearVelocity(10.0f, 0.0f, 0.0f);

        auto leadEntity = AddBodyColliderEntity<JointComponent>(leadPosition, // Templated joint component type is irrelevant since joint component is not created for this invokation.
            leadInitialLinearVelocity);

        auto jointConfig = AZStd::make_shared<GenericJointConfiguration>();
        jointConfig->m_leadEntity = leadEntity->GetId();
        auto followerEntity = AddBodyColliderEntity<FixedJointComponent>(followerPosition, 
            followerInitialLinearVelocity, 
            jointConfig);

        const AZ::Vector3 followerEndPosition = RunJointTest(followerEntity->GetId());

        EXPECT_TRUE(followerEndPosition.GetX().IsGreaterThan(followerPosition.GetX()));
    }

    TEST_F(PhysXJointsTest, Joint_HingeJoint_FollowerSwingsAroundLead)
    {
        // Place lead on the right of follower, tie them together with hinge joint and send the follower moving up.
        // The follower should swing around the lead.

        const AZ::Vector3 followerPosition(-1.0f, 0.0f, 0.0f);
        const AZ::Vector3 followerInitialLinearVelocity(0.0f, 0.0f, 10.0f);

        const AZ::Vector3 leadPosition(1.0f, 0.0f, 0.0f);
        const AZ::Vector3 leadInitialLinearVelocity(0.0f, 0.0f, 0.0f);

        const AZ::Vector3 jointLocalPosition(1.0f, 0.0f, 0.0f);
        const AZ::Quaternion jointLocalRotation = AZ::Quaternion::CreateRotationZ(90.0f);
        const AZ::Transform jointLocalTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            jointLocalRotation, 
            jointLocalPosition);

        auto leadEntity = AddBodyColliderEntity<JointComponent>(leadPosition, // Templated joint component type is irrelevant since joint component is not created for this invokation.
            leadInitialLinearVelocity);

        auto jointConfig = AZStd::make_shared<GenericJointConfiguration>();
        jointConfig->m_leadEntity = leadEntity->GetId();
        jointConfig->m_localTransformFromFollower = jointLocalTransform;

        auto jointLimits = AZStd::make_shared <GenericJointLimitsConfiguration>();
        jointLimits->m_isLimited = false;

        auto followerEntity = AddBodyColliderEntity<HingeJointComponent>(followerPosition,
            followerInitialLinearVelocity,
            jointConfig,
            jointLimits);

        const AZ::Vector3 followerEndPosition = RunJointTest(followerEntity->GetId());

        EXPECT_TRUE(followerEndPosition.GetX().IsGreaterThan(followerPosition.GetX()));
        EXPECT_TRUE(!followerEndPosition.GetZ().IsClose(0.0f));
    }

    TEST_F(PhysXJointsTest, Joint_BallJoint_FollowerSwingsUpAboutLead)
    {
        // Place lead on top of follower, tie them together with ball joint and send the follower moving sideways in the X and Y directions.
        // The follower should swing up about lead.

        const AZ::Vector3 followerPosition(0.0f, 0.0f, -1.0f);
        const AZ::Vector3 followerInitialLinearVelocity(10.0f, 10.0f, 0.0f);

        const AZ::Vector3 leadPosition(0.0f, 0.0f, 1.0f);
        const AZ::Vector3 leadInitialLinearVelocity(0.0f, 0.0f, 0.0f);

        const AZ::Vector3 jointLocalPosition(0.0f, 0.0f, 2.0f);
        const AZ::Quaternion jointLocalRotation = AZ::Quaternion::CreateRotationY(90.0f);
        const AZ::Transform jointLocalTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            jointLocalRotation,
            jointLocalPosition);

        auto leadEntity = AddBodyColliderEntity<JointComponent>(leadPosition, // Templated joint component type is irrelevant since joint component is not created for this invokation.
            leadInitialLinearVelocity);

        auto jointConfig = AZStd::make_shared<GenericJointConfiguration>();
        jointConfig->m_leadEntity = leadEntity->GetId();
        jointConfig->m_localTransformFromFollower = jointLocalTransform;

        auto jointLimits = AZStd::make_shared <GenericJointLimitsConfiguration>();
        jointLimits->m_isLimited = false;

        auto followerEntity = AddBodyColliderEntity<BallJointComponent>(followerPosition,
            followerInitialLinearVelocity,
            jointConfig,
            jointLimits);

        const AZ::Vector3 followerEndPosition = RunJointTest(followerEntity->GetId());

        EXPECT_TRUE(followerEndPosition.GetZ().IsGreaterThan(followerPosition.GetZ()));
    }
}
