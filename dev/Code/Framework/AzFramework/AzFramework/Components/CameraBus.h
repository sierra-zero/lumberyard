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
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Transform.h>

namespace Camera
{
    struct Configuration
    {
        float m_fovRadians = 0.f;
        float m_nearClipDistance = 0.f;
        float m_farClipDistance = 0.f;
        float m_frustumWidth = 0.f;
        float m_frustumHeight = 0.f;
    };

    /** 
     * Use this bus to send messages to a camera component on an entity
     * If you create your own camera you should implement this bus
     * Call like this:
     * Camera::CameraRequestBus::Event(cameraEntityId, &Camera::CameraRequestBus::Events::SetFov, newFov);
    */
    class CameraComponentRequests
        : public AZ::ComponentBus
    {
    public:
        virtual ~CameraComponentRequests() = default;
        /**
        * Gets the camera's field of view in degrees
        * @return The camera's field of view in degrees
        */
        virtual float GetFov()
        {
            AZ_WarningOnce("CameraBus", false, "GetFov is deprecated.  Please use GetFovDegrees or GetFovRadians.");
            return GetFovDegrees();
        }

        /**
        * Gets the camera's field of view in degrees
        * @return The camera's field of view in degrees
        */
        virtual float GetFovDegrees() = 0;

        /**
        * Gets the camera's field of view in radians
        * @return The camera's field of view in radians
        */
        virtual float GetFovRadians() = 0;
        
        /**
        * Gets the camera's distance from the near clip plane in meters
        * @return The camera's distance from the near clip plane in meters
        */
        virtual float GetNearClipDistance() = 0;

        /**
        * Gets the camera's distance from the far clip plane in meters
        * @return The camera's distance from the far clip plane in meters
        */
        virtual float GetFarClipDistance() = 0;

        /**
        * Gets the camera frustum's width
        * @return The camera frustum's width
        */
        virtual float GetFrustumWidth() = 0;

        /**
        * Gets the camera frustum's height
        * @return The camera frustum's height
        */
        virtual float GetFrustumHeight() = 0;

        /**
        * Sets the camera's field of view in degrees between 0 < fov < 180 degrees
        * @param fov The camera frustum's new field of view in degrees
        */
        virtual void SetFov(float fov)
        {
            AZ_WarningOnce("CameraBus", false, "SetFov is deprecated.  Please use SetFovDegrees or SetFovRadians.");
            SetFovDegrees(fov);
        }

        /**
        * Sets the camera's field of view in degrees between 0 < fov < 180 degrees
        * @param fov The camera frustum's new field of view in degrees
        */
        virtual void SetFovDegrees(float fovInDegrees) = 0;

        /**
        * Sets the camera's field of view in radians between 0 < fov < pi radians
        * @param fov The camera frustum's new field of view in radians
        */
        virtual void SetFovRadians(float fovInRadians) = 0;

        /**
        * Sets the near clip plane to a given distance from the camera in meters.  Should be small, but greater than 0
        * @param nearClipDistance The camera frustum's new near clip plane distance from camera
        */
        virtual void SetNearClipDistance(float nearClipDistance) = 0;

        /**
        * Sets the far clip plane to a given distance from the camera in meters.
        * @param farClipDistance The camera frustum's new far clip plane distance from camera
        */
        virtual void SetFarClipDistance(float farClipDistance) = 0;

        /**
        * Sets the camera frustum's width
        * @param width The camera frustum's new width
        */
        virtual void SetFrustumWidth(float width) = 0;

        /**
        * Sets the camera frustum's height
        * @param height The camera frustum's new height
        */
        virtual void SetFrustumHeight(float height) = 0;

        /**
        * Makes the camera the active view
        */
        virtual void MakeActiveView()
        {
            AZ_WarningOnce("CameraBus", false, "MakeActiveView is deprecated.  Please use SetActiveView.");
            SetActiveView(true);
        }

        /**
        * Projects a world point to a screen point
        */
        virtual bool ProjectWorldPointToScreen(const AZ::Vector3& worldPoint, AZ::Vector3& outScreenPoint) const
        {
            AZ_UNUSED(worldPoint);
            AZ_UNUSED(outScreenPoint);
            return false;
        }

        /**
        * Unproject a screen point to the world
        */
        virtual bool UnprojectScreenPointToWorld(const AZ::Vector3& screenPoint, AZ::Vector3& outWorldPoint) const
        {
            AZ_UNUSED(screenPoint);
            AZ_UNUSED(outWorldPoint);
            return false;
        }

        /**
        * Projects a world point to a viewport
        */
        virtual bool ProjectWorldPointToViewport(const AZ::Vector3& worldPoint, const AZ::Vector4& viewport, AZ::Vector3& outViewportPoint) const
        {
            AZ_UNUSED(worldPoint);
            AZ_UNUSED(viewport);
            AZ_UNUSED(outViewportPoint);
            return false;
        }

        /**
        * Unproject a viewport point to a world point
        */
        virtual bool UnprojectViewportPointToWorld(const AZ::Vector3& viewportPoint, const AZ::Vector4& viewport, AZ::Vector3& outWorldPoint) const
        {
            AZ_UNUSED(viewportPoint);
            AZ_UNUSED(viewport);
            AZ_UNUSED(outWorldPoint);
            return false;
        }

        /**
        * Get the projection matrix
        */
        virtual void GetProjectionMatrix(AZ::Matrix4x4& outProjectionMatrix) const
        {
            AZ_UNUSED(outProjectionMatrix);
        }

        /**
        * Check if the camera view is the active view
        */
        virtual bool IsActiveView() const
        {
            return false;
        }

        /**
        * Set active view to this camera
        * @param active If true, set the camera as active. 
        * If false and this camera is active, set the previous camera active
        */
        virtual void SetActiveView(bool active)
        {
            AZ_UNUSED(active);
        }


        virtual Configuration GetConfiguration() 
        {
            return Configuration
            {
                GetFovRadians(),
                GetNearClipDistance(),
                GetFarClipDistance(),
                GetFrustumWidth(),
                GetFrustumHeight()
            };
        }
    };
    using CameraRequestBus = AZ::EBus<CameraComponentRequests>;

    /**
    * Use this broadcast bus to gather a list of all active cameras
    * If you create your own camera you should handle this bus
    * Call like this:
    * 
    * AZ::EBusAggregateResults<AZ::EntityId> results;
    * Camera::CameraBus::BroadcastResult(results, &Camera::CameraRequests::GetCameras);
    */
    class CameraRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~CameraRequests() = default;

        /// Get a list of all cameras
        virtual AZ::EntityId GetCameras() = 0;
    };
    using CameraBus = AZ::EBus<CameraRequests>;

    /**
    * Use this system broadcast for things like getting the active camera
    */
    class CameraSystemRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~CameraSystemRequests() = default;

        /**
        * returns the camera being used by the active view
        */
        virtual AZ::EntityId GetActiveCamera() = 0;
    };
    using CameraSystemRequestBus = AZ::EBus<CameraSystemRequests>;

    //! This system broadcast offer the active camera information
    //! even when the camera is not attached to an entity.
    class ActiveCameraRequests
        : public AZ::EBusTraits
    {
    public:
        virtual ~ActiveCameraRequests() = default;

        //! This returns the transform of the active view
        virtual const AZ::Transform& GetActiveCameraTransform() = 0;

        //! This returns the configuration of the active camera.
        virtual const Configuration& GetActiveCameraConfiguration() = 0;
    };
    using ActiveCameraRequestBus = AZ::EBus<ActiveCameraRequests>;

    /**
    * Handle this bus if you want to know when cameras are added or removed during edit or run time
    * You will get an OnCameraAdded event for each camera that is already active
    * If you create your own camera you should call this bus on activation/deactivation
    * Connect to the bus like this
    * Camera::CameraNotificationBus::Handler::Connect()
    */
    class CameraNotifications
        : public AZ::EBusTraits
    {
    public:
        template<class Bus>
        struct CameraNotificationConnectionPolicy
            : public AZ::EBusConnectionPolicy<Bus>
        {
            static void Connect(typename Bus::BusPtr& busPtr, typename Bus::Context& context, typename Bus::HandlerNode& handler, typename Bus::Context::ConnectLockGuard& connectLock, const typename Bus::BusIdType& id = 0)
            {
                AZ::EBusConnectionPolicy<Bus>::Connect(busPtr, context, handler, connectLock, id);

                AZ::EBusAggregateResults<AZ::EntityId> results;
                CameraBus::BroadcastResult(results, &CameraRequests::GetCameras);
                for (const AZ::EntityId& cameraId : results.values)
                {
                    handler->OnCameraAdded(cameraId);
                }
            }
        };
        /**
        * If the camera is active when a handler connects to the bus,
        * then OnCameraAdded() is immediately dispatched.
        */
        template<class Bus>
        using ConnectionPolicy = CameraNotificationConnectionPolicy<Bus>;

        virtual ~CameraNotifications() = default;

        /**
        * Called whenever a camera entity is added
        * @param cameraId The id of the camera added
        */
        virtual void OnCameraAdded(const AZ::EntityId& cameraId) = 0;

        /**
        * Called whenever a camera entity is removed
        * @param cameraId The id of the camera removed
        */
        virtual void OnCameraRemoved(const AZ::EntityId& cameraId) = 0;
    };
    using CameraNotificationBus = AZ::EBus<CameraNotifications>;

#define CameraComponentTypeId "{E409F5C0-9919-4CA5-9488-1FE8A041768E}"
#define EditorCameraComponentTypeId "{B99EFE3D-3F1D-4630-8A7B-31C70CC1F53C}"

} // namespace Camera
