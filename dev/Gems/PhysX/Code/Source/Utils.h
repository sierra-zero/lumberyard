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

#include <PhysX/ForceRegionComponentBus.h>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzFramework/Physics/Material.h>
#include <AzFramework/Physics/Shape.h>
#include <AzFramework/Physics/ShapeConfiguration.h>
#include <AzCore/std/optional.h>

#include <PxPhysicsAPI.h>

namespace Physics
{
    class RigidBodyConfiguration;
    class ColliderConfiguration;
    class ShapeConfiguration;
    class RigidBodyConfiguration;
    class WorldBodyConfiguration;
    class RigidBodyStatic;
    class CollisionGroup;
}

namespace PhysX
{
    class World;


    class Shape;
    class ActorData;
    class Material;
    struct TerrainConfiguration;

    namespace Pipeline
    {
        class MeshAssetData;
    } // namespace Pipeline

    namespace Utils
    {
        bool CreatePxGeometryFromConfig(const Physics::ShapeConfiguration& shapeConfiguration, physx::PxGeometryHolder& pxGeometry);

        physx::PxShape* CreatePxShapeFromConfig(
            const Physics::ColliderConfiguration& colliderConfiguration, 
            const Physics::ShapeConfiguration& shapeConfiguration, 
            Physics::CollisionGroup& assignedCollisionGroup
        );

        World* GetDefaultWorld();

        //! Creates a PhysX cooked mesh config from the given points.
        //! 
        //! @param pointList Vector of points to build the mesh from.
        //! @param scale Scale to be assigned to the cooked mesh.
        //! @return Either a valid cooked mesh or none if the cooking failed.
        //! 
        AZStd::optional<Physics::CookedMeshShapeConfiguration> CreatePxCookedMeshConfiguration(const AZStd::vector<AZ::Vector3>& pointList, const AZ::Vector3& scale);

        // 255 is the hard limit for PhysX number of vertices/faces. Upper bound is set to something sensible and less than this hard limit.
        constexpr AZ::u8 MinFrustumSubdivisions = 3;
        constexpr AZ::u8 MaxFrustumSubdivisions = 125; 

        //! Creates the points for a given frustum along the z axis as specified by the supplied arguements.
        //!
        //! @param height Height of the frustum. Must be greater than 0.
        //! @param bottomRadius Radius of bottom cace of frustum. Must be greater than 0 if topRadius is 0, otherwise can be 0.
        //! @param topRadius Radius of top face of frustum. Must be greater than 0 if bottompRadius is 0, otherwise can be 0.
        //! @param subdivisionsNumber of angular subdivisions. Must be between 3 and 125 inclusive.
        //! @return Either a valid point list or none if any of the arguements are invalid.
        //!
        AZStd::optional<AZStd::vector<AZ::Vector3>> CreatePointsAtFrustumExtents(float height, float bottomRadius, float topRadius, AZ::u8 subdivisions);
    
        AZStd::string ConvexCookingResultToString(physx::PxConvexMeshCookingResult::Enum convexCookingResultCode);
        AZStd::string TriMeshCookingResultToString(physx::PxTriangleMeshCookingResult::Enum triangleCookingResultCode);

        bool WriteCookedMeshToFile(const AZStd::string& filePath, const Pipeline::MeshAssetData& assetData);
        bool WriteCookedMeshToFile(const AZStd::string& filePath, const AZStd::vector<AZ::u8>& physxData, 
            Physics::CookedMeshShapeConfiguration::MeshType meshType);

        bool CookConvexToPxOutputStream(const AZ::Vector3* vertices, AZ::u32 vertexCount, physx::PxOutputStream& stream);

        bool CookTriangleMeshToToPxOutputStream(const AZ::Vector3* vertices, AZ::u32 vertexCount,
            const AZ::u32* indices, AZ::u32 indexCount, physx::PxOutputStream& stream);

        bool MeshDataToPxGeometry(physx::PxBase* meshData, physx::PxGeometryHolder &pxGeometry, const AZ::Vector3& scale);

        /// Terrain Utils
        AZStd::unique_ptr<Physics::RigidBodyStatic> CreateTerrain(
            const PhysX::TerrainConfiguration& terrainConfiguration, const AZ::EntityId& entityId, const AZStd::string_view& name);

        void GetMaterialList(
            AZStd::vector<physx::PxMaterial*>& pxMaterials, const AZStd::vector<int>& materialIndexMapping,
            const Physics::TerrainMaterialSurfaceIdMap& terrainMaterialsToSurfaceIds);
        /// Returns all connected busIds of the specified type.
        template<typename BusT>
        AZStd::vector<typename BusT::BusIdType> FindConnectedBusIds()
        {
            AZStd::vector<typename BusT::BusIdType> busIds;
            BusT::EnumerateHandlers([&busIds](typename BusT::Events* /*handler*/)
            {
                busIds.emplace_back(*BusT::GetCurrentBusId());
                return true;
            });
            return busIds;
        }

        /// Logs a warning message using the names of the entities provided.
        void WarnEntityNames(const AZStd::vector<AZ::EntityId>& entityIds, const char* category, const char* message);

        /// Logs a warning if there is more than one connected bus of the particular type.
        template<typename BusT>
        void LogWarningIfMultipleComponents(const char* messageCategroy, const char* messageFormat)
        {
            const auto entityIds = FindConnectedBusIds<BusT>();
            if (entityIds.size() > 1)
            {
                WarnEntityNames(entityIds, messageCategroy, messageFormat);
            }
        }

        /// Converts collider position and orientation offsets to a transform.
        AZ::Transform GetColliderLocalTransform(const AZ::Vector3& colliderRelativePosition
            , const AZ::Quaternion& colliderRelativeRotation);

        /// Combines collider position and orientation offsets and world transform to a transform.
        AZ::Transform GetColliderWorldTransform(const AZ::Transform& worldTransform
            , const AZ::Vector3& colliderRelativePosition
            , const AZ::Quaternion& colliderRelativeRotation);

        /// Converts points in a collider's local space to world space positions 
        /// accounting for collider position and orientation offsets.
        void ColliderPointsLocalToWorld(AZStd::vector<AZ::Vector3>& pointsInOut
            , const AZ::Transform& worldTransform
            , const AZ::Vector3& colliderRelativePosition
            , const AZ::Quaternion& colliderRelativeRotation);

        /// Returns AABB of collider by constructing PxGeometry from collider and shape configuration,
        /// and invoking physx::PxGeometryQuery::getWorldBounds.
        /// This function is used only by editor components.
        AZ::Aabb GetColliderAabb(const AZ::Transform& worldTransform
            , const ::Physics::ShapeConfiguration& shapeConfiguration
            , const ::Physics::ColliderConfiguration& colliderConfiguration);

        bool TriggerColliderExists(AZ::EntityId entityId);

        void GetShapesFromAsset(const Physics::PhysicsAssetShapeConfiguration& assetConfiguration,
            const Physics::ColliderConfiguration& masterColliderConfiguration,
            AZStd::vector<AZStd::shared_ptr<Physics::Shape>>& resultingShapes);

        void GetColliderShapeConfigsFromAsset(const Physics::PhysicsAssetShapeConfiguration& assetConfiguration,
            const Physics::ColliderConfiguration& masterColliderConfiguration,
            Physics::ShapeConfigurationList& resultingColliderShapes);

        AZ::Vector3 GetNonUniformScale(AZ::EntityId entityId);
        AZ::Vector3 GetUniformScale(AZ::EntityId entityId);

        /// Returns defaultValue if the input is infinite or NaN, otherwise returns the input unchanged.
        const AZ::Vector3& Sanitize(const AZ::Vector3& input, const AZ::Vector3& defaultValue = AZ::Vector3::CreateZero());

        namespace Geometry
        {
            using PointList = AZStd::vector<AZ::Vector3>;

            /// Generates a list of points on a box.
            PointList GenerateBoxPoints(const AZ::Vector3& min, const AZ::Vector3& max);

            /// Generates a list of points on the surface of a sphere.
            PointList GenerateSpherePoints(float radius);

            /// Generates a list of points on the surface of a cylinder.
            PointList GenerateCylinderPoints(float height, float radius);

            /// Generates vertices and indices representing the provided box geometry 
            void GetBoxGeometry(const physx::PxBoxGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices);

            /// Generates vertices and indices representing the provided capsule geometry 
            void GetCapsuleGeometry(const physx::PxCapsuleGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices, const AZ::u32 stacks, const AZ::u32 slices);

            /// Generates vertices and indices representing the provided convex mesh geometry 
            void GetConvexMeshGeometry(const physx::PxConvexMeshGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices);

            /// Generates vertices and indices representing the provided heightfield geometry, optionally limited to a bounding box
            void GetHeightFieldGeometry(const physx::PxHeightFieldGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices, AZ::Aabb* optionalBounds);

            /// Generates vertices and indices representing the provided sphere geometry and optional stacks and slices
            void GetSphereGeometry(const physx::PxSphereGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices, const AZ::u32 stacks, const AZ::u32 slices);

            /// Generates vertices and indices representing the provided triangle mesh geometry 
            void GetTriangleMeshGeometry(const physx::PxTriangleMeshGeometry& geometry, AZStd::vector<AZ::Vector3>& vertices, AZStd::vector<AZ::u32>& indices);
        } // namespace Geometry

        namespace RayCast
        {
            Physics::RayCastHit ClosestRayHitAgainstShapes(const Physics::RayCastRequest& request,
                const AZStd::vector<AZStd::shared_ptr<PhysX::Shape>>& shapes, const AZ::Transform& parentTransform);
        } // namespace RayCast


        /// Returns the World transform of an entity with scale. 
        /// This can be used if ComputeJointLocalTransform will be invoked with the result as an argument since ComputeJointLocalTransform  will remove scale.
        AZ::Transform GetEntityWorldTransformWithScale(AZ::EntityId entityId);

        /// Returns the World transform of an entity without scale.
        AZ::Transform GetEntityWorldTransformWithoutScale(AZ::EntityId entityId);

        /// Computes the local transform of joint from an entity given the joint's world transform and the entity's world transform.
        AZ::Transform ComputeJointLocalTransform(const AZ::Transform& jointWorldTransform,
            const AZ::Transform& entityWorldTransform);

        /// Computes the world transform of joint given the joint's local transform from an entity and the entity's world transform.
        AZ::Transform ComputeJointWorldTransform(const AZ::Transform& jointLocalTransform,
            const AZ::Transform& entityWorldTransform);
    } // namespace Utils

    namespace ReflectionUtils
    {
        /// Reflect API specific to PhysX physics. Generic physics API should be reflected in Physics::ReflectionUtils::ReflectPhysicsApi.
        void ReflectPhysXOnlyApi(AZ::ReflectContext* context);
    } // namespace ReflectionUtils

    namespace PxActorFactories
    {
        physx::PxRigidDynamic* CreatePxRigidBody(const Physics::RigidBodyConfiguration& configuration);
        physx::PxRigidStatic* CreatePxStaticRigidBody(const Physics::WorldBodyConfiguration& configuration);

        void ReleaseActor(physx::PxActor* actor);
    } // namespace PxActorFactories

    namespace StaticRigidBodyUtils
    {
        bool CanCreateRuntimeComponent(const AZ::Entity& editorEntity);
        bool TryCreateRuntimeComponent(const AZ::Entity& editorEntity, AZ::Entity& gameEntity);
    } // namespace StaticRigidBodyComponent
} // namespace PhysX
