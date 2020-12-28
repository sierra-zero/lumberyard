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
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/SystemFile.h>
#include <AzFramework/Physics/Utils.h>
#include <AzFramework/Physics/Material.h>
#include <AzFramework/Asset/AssetSystemBus.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <PhysX/MeshAsset.h>
#include <PhysX/HeightFieldAsset.h>
#include <Source/SystemComponent.h>
#include <Source/AzPhysXCpuDispatcher.h>
#include <Source/RigidBody.h>
#include <Source/RigidBodyStatic.h>
#include <Source/Utils.h>
#include <Source/Collision.h>
#include <Source/Shape.h>
#include <Source/Joint.h>
#include <Source/SphereColliderComponent.h>
#include <Source/BoxColliderComponent.h>
#include <Source/CapsuleColliderComponent.h>
#include <Source/Pipeline/MeshAssetHandler.h>
#include <Source/Pipeline/HeightFieldAssetHandler.h>
#include <Source/WindProvider.h>
#include <Terrain/Bus/LegacyTerrainBus.h>

#ifdef PHYSX_EDITOR
#include <Source/EditorColliderComponent.h>
#include <Editor/EditorWindow.h>
#include <Editor/PropertyTypes.h>
#include <AzToolsFramework/SourceControl/SourceControlAPI.h>
#include <AzToolsFramework/UI/PropertyEditor/PropertyEditorAPI.h>
#endif

namespace PhysX
{
    SystemComponent::PhysXSDKGlobals SystemComponent::m_physxSDKGlobals;
    static const char* const DefaultWorldName = "PhysX Default";
    static const char* const ProjectPhysicsConfigurationPath = "project.physicsconfiguration";
    static const char* const ProjectPhysXConfigurationPath = "project.physxconfiguration";
    // This config is undergoing deprecation, replaced by above configs.
    static const char* const DefaultConfigurationPath = "default.physxconfiguration";

    bool SystemComponent::VersionConverter(AZ::SerializeContext& context,
        AZ::SerializeContext::DataElementNode& classElement)
    {
        using GlobalCollisionDebugState = EditorConfiguration::GlobalCollisionDebugState;

        if (classElement.GetVersion() <= 1)
        {
            const int pvdTransportTypeElemIndex = classElement.FindElement(AZ_CRC("PvdTransportType", 0x91e0b21e));

            if (pvdTransportTypeElemIndex >= 0)
            {
                Settings::PvdTransportType pvdTransportTypeValue;
                AZ::SerializeContext::DataElementNode& pvdTransportElement = classElement.GetSubElement(pvdTransportTypeElemIndex);
                pvdTransportElement.GetData<Settings::PvdTransportType>(pvdTransportTypeValue);

                if (pvdTransportTypeValue == static_cast<Settings::PvdTransportType>(2))
                {
                    // version 2 removes Disabled (2) value default to Network instead.
                    const bool success = pvdTransportElement.SetData<Settings::PvdTransportType>(context, Settings::PvdTransportType::Network);
                    if (!success)
                    {
                        return false;
                    }
                }
            }
        }

        if (classElement.GetVersion() <= 2)
        {
            const int globalColliderDebugDrawElemIndex = classElement.FindElement(AZ_CRC("GlobalColliderDebugDraw", 0xca73ed43));

            if (globalColliderDebugDrawElemIndex >= 0)
            {
                bool oldGlobalColliderDebugDrawElemDebug = false;
                AZ::SerializeContext::DataElementNode& globalColliderDebugDrawElem = classElement.GetSubElement(globalColliderDebugDrawElemIndex);
                // Previously globalColliderDebugDraw was a bool indicating whether to always draw debug or to manually set on the element
                if (!globalColliderDebugDrawElem.GetData<bool>(oldGlobalColliderDebugDrawElemDebug))
                {
                    return false;
                }
                classElement.RemoveElement(globalColliderDebugDrawElemIndex);
                const GlobalCollisionDebugState newValue = oldGlobalColliderDebugDrawElemDebug ? GlobalCollisionDebugState::AlwaysOn : GlobalCollisionDebugState::Manual;
                classElement.AddElementWithData(context, "GlobalColliderDebugDraw", newValue);
            }
        }

        if (classElement.GetVersion() <= 3)
        {
            classElement.AddElementWithData(context, "ShowJointHierarchy", true);
            classElement.AddElementWithData(context, "JointHierarchyLeadColor",
                EditorConfiguration::JointLeadColor::Aquamarine);
            classElement.AddElementWithData(context, "JointHierarchyFollowerColor",
                EditorConfiguration::JointFollowerColor::Magenta);
            classElement.AddElementWithData(context, "jointHierarchyDistanceThreshold", 1.0f);
        }

        return true;
    }

#ifdef PHYSX_EDITOR
    static void OnEditorConfigurationChanged()
    {
        AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::RequestRefresh,
            AzToolsFramework::PropertyModificationRefreshLevel::Refresh_EntireTree);
    }
#endif

    void SystemComponent::Reflect(AZ::ReflectContext* context)
    {
        D6JointLimitConfiguration::Reflect(context);
        Pipeline::MeshAssetData::Reflect(context);

        PhysX::ReflectionUtils::ReflectPhysXOnlyApi(context);

        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<Settings::ColliderProximityVisualization>()
                ->Version(1)
                ->Field("Enabled", &Settings::ColliderProximityVisualization::m_enabled)
                ->Field("CameraPosition", &Settings::ColliderProximityVisualization::m_cameraPosition)
                ->Field("Radius", &Settings::ColliderProximityVisualization::m_radius)
            ;

            serialize->Class<Settings>()
                ->Version(2, &VersionConverter)
                ->Field("PvdHost", &Settings::m_pvdHost)
                ->Field("PvdPort", &Settings::m_pvdPort)
                ->Field("PvdTimeout", &Settings::m_pvdTimeoutInMilliseconds)
                ->Field("PvdTransportType", &Settings::m_pvdTransportType)
                ->Field("PvdFileName", &Settings::m_pvdFileName)
                ->Field("PvdAutoConnectMode", &Settings::m_pvdAutoConnectMode)
                ->Field("PvdReconnect", &Settings::m_pvdReconnect)
                ->Field("ColliderProximityVisualization", &Settings::m_colliderProximityVisualization)
            ;

            serialize->Class<EditorConfiguration>()
                ->Version(4, &VersionConverter)
                ->Field("COMDebugSize", &EditorConfiguration::m_centerOfMassDebugSize)
                ->Field("COMDebugColor", &EditorConfiguration::m_centerOfMassDebugColor)
                ->Field("GlobalColliderDebugDraw", &EditorConfiguration::m_globalCollisionDebugDraw)
                ->Field("GlobalColliderDebugDrawColorMode", &EditorConfiguration::m_globalCollisionDebugDrawColorMode)
                ->Field("ShowJointHierarchy", &EditorConfiguration::m_showJointHierarchy)
                ->Field("JointHierarchyLeadColor", &EditorConfiguration::m_jointHierarchyLeadColor)
                ->Field("JointHierarchyFollowerColor", &EditorConfiguration::m_jointHierarchyFollowerColor)
                ->Field("JointHierarchyDistanceThreshold", &EditorConfiguration::m_jointHierarchyDistanceThreshold)
            ;

            serialize->Class<WindConfiguration>()
                ->Version(1)
                ->Field("GlobalWindTag", &WindConfiguration::m_globalWindTag)
                ->Field("LocalWindTag", &WindConfiguration::m_localWindTag)
            ;

            serialize->Class<PhysXConfiguration>()
                ->Version(1)
                ->Field("Settings", &PhysXConfiguration::m_settings)
                ->Field("EditorConfiguration", &PhysXConfiguration::m_editorConfiguration)
                ->Field("WindConfiguration", &PhysXConfiguration::m_windConfiguration)
            ;

            serialize->Class<PhysicsConfiguration>()
                ->Version(1)
                ->Field("WorldConfiguration", &PhysicsConfiguration::m_defaultWorldConfiguration)
                ->Field("CollisionConfiguration", &PhysicsConfiguration::m_collisionConfiguration)
                ->Field("MaterialLibrary", &PhysicsConfiguration::m_defaultMaterialLibrary)
            ;

            serialize->Class<SystemComponent, AZ::Component>()
                ->Version(1)
                ->Field("Enabled", &SystemComponent::m_enabled)
                ->Field("ConfigurationPath", &SystemComponent::m_configurationPath)
            ;

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<Settings>("PhysX PVD Settings", "PhysX PVD Settings")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &Settings::m_pvdTransportType,
                    "PVD Transport Type", "PVD supports writing to a TCP/IP network socket or to a file.")
                        ->EnumAttribute(Settings::PvdTransportType::Network, "Network")
                        ->EnumAttribute(Settings::PvdTransportType::File, "File")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)

                    ->DataElement(AZ::Edit::UIHandlers::Default, &Settings::m_pvdHost,
                    "PVD Host", "Host IP address of the PhysX Visual Debugger application")
                    ->Attribute(AZ::Edit::Attributes::Visibility, &Settings::IsNetworkDebug)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Settings::m_pvdPort,
                    "PVD Port", "Port of the PhysX Visual Debugger application")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &Settings::IsNetworkDebug)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Settings::m_pvdTimeoutInMilliseconds,
                    "PVD Timeout", "Timeout (in milliseconds) used when connecting to the PhysX Visual Debugger application")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &Settings::IsNetworkDebug)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Settings::m_pvdFileName,
                    "PVD FileName", "Filename to output PhysX Visual Debugger data.")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &Settings::IsFileDebug)

                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &Settings::m_pvdAutoConnectMode,
                    "PVD Auto Connect", "Automatically connect to the PhysX Visual Debugger "
                    "(Requires PhysX Debug gem for Editor and Game modes).")
                        ->EnumAttribute(Settings::PvdAutoConnectMode::Disabled, "Disabled")
                        ->EnumAttribute(Settings::PvdAutoConnectMode::Editor, "Editor")
                        ->EnumAttribute(Settings::PvdAutoConnectMode::Game, "Game")

                    ->DataElement(AZ::Edit::UIHandlers::CheckBox, &Settings::m_pvdReconnect,
                    "PVD Reconnect", "Reconnect (Disconnect and Connect) when switching between game and edit mode "
                    "(Requires PhysX Debug gem).")
                ;

                ec->Class<EditorConfiguration>("Editor Configuration", "Editor settings for PhysX")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &EditorConfiguration::m_centerOfMassDebugSize,
                    "Debug Draw Center of Mass Size", "The size of the debug draw circle representing the center of mass.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.1f)
                        ->Attribute(AZ::Edit::Attributes::Max, 5.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorConfiguration::m_centerOfMassDebugColor,
                    "Debug Draw Center of Mass Color", "The color of the debug draw circle representing the center of mass.")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorConfiguration::m_globalCollisionDebugDraw, "Global Collision Debug",
                        "Set up global collision debug draw."
                        "<ul style=\"margin-left:15px; margin-top:-10px; -qt-list-indent:0;\">"
                        "<li><b>Enable all colliders</b><br>Displays all PhysX collider shapes, including colliders previously set as hidden.\n</li>"
                        "<li><b>Disable all colliders</b><br>Hides all PhysX collider shapes, including colliders previously set as visible.\n</li>"
                        "<li><b>Set manually</b><br>You can update PhysX colliders on each entity. The default state is on.</li>"
                        "</ul>")
                        ->EnumAttribute(PhysX::EditorConfiguration::GlobalCollisionDebugState::AlwaysOn , "Enable all colliders")
                        ->EnumAttribute(PhysX::EditorConfiguration::GlobalCollisionDebugState::AlwaysOff, "Disable all colliders")
                        ->EnumAttribute(PhysX::EditorConfiguration::GlobalCollisionDebugState::Manual, "Set manually")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorConfiguration::m_globalCollisionDebugDrawColorMode, "Global Collision Debug Color Mode",
                        "Set up debug color mode."
                        "<ul style=\"margin-left:15px; margin-top:-10px; -qt-list-indent:0;\">"
                        "<li><b>Material Color Mode</b><br>Uses material's debug color specified in material library.\n</li>"
                        "<li><b>Error Mode</b><br>Shows glowing red error colors for cases like meshes with too many triangles.\n</li>"
                        "</ul>")
                        ->EnumAttribute(PhysX::EditorConfiguration::GlobalCollisionDebugColorMode::MaterialColor, "Material Color Mode")
                        ->EnumAttribute(PhysX::EditorConfiguration::GlobalCollisionDebugColorMode::ErrorColor, "Error Mode")

#ifdef PHYSX_EDITOR
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &OnEditorConfigurationChanged)
#endif
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorConfiguration::m_showJointHierarchy, 
                        "Display Joints Hierarchy", 
                        "Flag to switch on / off the display of joint lead-follower connections in the viewport.")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorConfiguration::m_jointHierarchyLeadColor,
                        "Joints Hierarchy Lead Color",
                        "Color of the lead half of a lead-follower joint connection line.")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::Aquamarine, "Aquamarine")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::AliceBlue, "AliceBlue")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::CadetBlue, "CadetBlue")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::Coral, "Coral")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::Green, "Green")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::DarkGreen, "DarkGreen")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::ForestGreen, "ForestGreen")
                        ->EnumAttribute(EditorConfiguration::JointLeadColor::Honeydew, "Honeydew")
                    ->DataElement(AZ::Edit::UIHandlers::ComboBox, &EditorConfiguration::m_jointHierarchyFollowerColor,
                        "Joints Hierarchy Follower Color",
                        "Color of the follower half of a lead-follower joint connection line.")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Chocolate, "Chocolate")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::HotPink, "HotPink")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Lavender, "Lavender")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Magenta, "Magenta")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::LightYellow, "LightYellow")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Maroon, "Maroon")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Red, "Red")
                        ->EnumAttribute(EditorConfiguration::JointFollowerColor::Yellow, "Yellow")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &EditorConfiguration::m_jointHierarchyDistanceThreshold,
                        "Joints Hierarchy Distance Threshold",
                        "Minimum distance required to draw from follower to joint. Distances shorter than this threshold will result in the line drawn from the joint to the lead.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.000001f)
                        ->Attribute(AZ::Edit::Attributes::Max, 20.0f)
                ;

                ec->Class<WindConfiguration>("Wind Configuration", "Wind settings for PhysX")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_globalWindTag,
                        "Global wind tag",
                        "Tag value that will be used to mark entities that provide global wind value.\n"
                        "Global wind has no bounds and affects objects across entire level.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &WindConfiguration::m_localWindTag,
                        "Local wind tag",
                        "Tag value that will be used to mark entities that provide local wind value.\n"
                        "Local wind is only applied within bounds defined by PhysX collider.")

#ifdef PHYSX_EDITOR
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &OnEditorConfigurationChanged)
#endif
                ;

                ec->Class<SystemComponent>("PhysX", "Global PhysX physics configuration")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "PhysX")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemComponent::m_enabled,
                    "Enabled", "Enables the PhysX system component.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SystemComponent::m_configurationPath,
                    "Configuration Path", "Path to PhysX configuration file relative to asset root")
                ;
            }
        }
    }

    void SystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("PhysXService", 0x75beae2d));
    }

    void SystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("PhysXService", 0x75beae2d));
    }

    void SystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
    }

    void SystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        (void)dependent;
    }

    SystemComponent::SystemComponent()
        : m_enabled(true)
        , m_configurationPath(DefaultConfigurationPath)
    {
    }

    SystemComponent::~SystemComponent() = default;

    // AZ::Component interface implementation
    void SystemComponent::Init()
    {
    }

    void SystemComponent::InitializePhysXSDK()
    {
        // Start PhysX allocator
        PhysXAllocator::Descriptor allocatorDescriptor;
        allocatorDescriptor.m_custom = &AZ::AllocatorInstance<AZ::SystemAllocator>::Get();
        AZ::AllocatorInstance<PhysXAllocator>::Create();

        // create PhysX basis
        m_physxSDKGlobals.m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_physxSDKGlobals.m_azAllocator, m_physxSDKGlobals.m_azErrorCallback);
        bool physXTrackOutstandingAllocations = false;
#ifdef AZ_PHYSICS_DEBUG_ENABLED
        physXTrackOutstandingAllocations = true;
        m_physxSDKGlobals.m_pvd = PxCreatePvd(*m_physxSDKGlobals.m_foundation);
#endif
        m_physxSDKGlobals.m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_physxSDKGlobals.m_foundation, physx::PxTolerancesScale(), physXTrackOutstandingAllocations, m_physxSDKGlobals.m_pvd);
        PxInitExtensions(*m_physxSDKGlobals.m_physics, m_physxSDKGlobals.m_pvd);

        // set up cooking for height fields, meshes etc.
        m_physxSDKGlobals.m_cooking =
#ifdef PHYSX_EDITOR
            // choose sensible defaults for cooking params when at edit-time (GetEditTimeCookingParams())
            PxCreateCooking(PX_PHYSICS_VERSION, *m_physxSDKGlobals.m_foundation, GetEditTimeCookingParams());
#else
            // choose sensible defaults for cooking params when at run-time (GetRealTimeCookingParams())
            PxCreateCooking(PX_PHYSICS_VERSION, *m_physxSDKGlobals.m_foundation, GetRealTimeCookingParams());
#endif
    }

    void SystemComponent::DestroyPhysXSDK()
    {
        m_physxSDKGlobals.m_cooking->release();
        m_physxSDKGlobals.m_cooking = nullptr;

        PxCloseExtensions();

        m_physxSDKGlobals.m_physics->release();
        m_physxSDKGlobals.m_physics = nullptr;

        if (m_physxSDKGlobals.m_pvd)
        {
            m_physxSDKGlobals.m_pvd->release();
            m_physxSDKGlobals.m_pvd = nullptr;
        }

        m_physxSDKGlobals.m_foundation->release();
        m_physxSDKGlobals.m_foundation = nullptr;

        AZ::AllocatorInstance<PhysXAllocator>::Destroy();
    }

    physx::PxAllocatorCallback* SystemComponent::GetPhysXAllocatorCallback()
    {
        AZ_Assert(m_physxSDKGlobals.m_physics, "Attempting to get the PhysX Allocator before the PhysX SDK has been initialized.");
        return &(m_physxSDKGlobals.m_azAllocator);
    }
    
    physx::PxErrorCallback* SystemComponent::GetPhysXErrorCallback()
    {
        AZ_Assert(m_physxSDKGlobals.m_physics, "Attempting to get the PhysX Error Callback before the PhysX SDK has been initialized.");
        return &(m_physxSDKGlobals.m_azErrorCallback);
    }

    physx::PxCpuDispatcher* SystemComponent::GetCpuDispatcher()
    {
        AZ_Assert(m_physxSDKGlobals.m_physics, "Attempting to get the PhysX CPU dispatcher before the PhysX SDK has been initialized.");
        AZ_Assert(m_cpuDispatcher, "Attempting to get the PhysX CPU dispatcher before it has been initialized.");
        return m_cpuDispatcher;
    }

    template<typename AssetHandlerT, typename AssetT>
    void RegisterAsset(AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler>>& assetHandlers)
    {
        AssetHandlerT* handler = aznew AssetHandlerT();
        AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequests::EnableCatalogForAsset, AZ::AzTypeInfo<AssetT>::Uuid());
        AZ::Data::AssetCatalogRequestBus::Broadcast(&AZ::Data::AssetCatalogRequests::AddExtension, AssetHandlerT::s_assetFileExtension);
        assetHandlers.emplace_back(handler);
    }

    void SystemComponent::Activate()
    {
        if (!m_enabled)
        {
            return;
        }

        m_materialManager.Connect();


        // Assets related work
        auto materialAsset = aznew AzFramework::GenericAssetHandler<Physics::MaterialLibraryAsset>("Physics Material", "Physics", "physmaterial");
        materialAsset->Register();
        m_assetHandlers.emplace_back(materialAsset);

        // Add asset types and extensions to AssetCatalog. Uses "AssetCatalogService".
        RegisterAsset<Pipeline::MeshAssetHandler, Pipeline::MeshAsset>(m_assetHandlers);
        RegisterAsset<Pipeline::HeightFieldAssetHandler, Pipeline::HeightFieldAsset>(m_assetHandlers);

        // Connect to relevant buses
        Physics::SystemRequestBus::Handler::BusConnect();
        PhysX::SystemRequestsBus::Handler::BusConnect();
        PhysX::ConfigurationRequestBus::Handler::BusConnect();
        CrySystemEventBus::Handler::BusConnect();
        Physics::CollisionRequestBus::Handler::BusConnect();

#ifdef PHYSX_EDITOR
        PhysX::Editor::RegisterPropertyTypes();
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
#endif

        // Set up CPU dispatcher
#if defined(AZ_PLATFORM_LINUX)
        // Temporary workaround for linux. At the moment using AzPhysXCpuDispatcher results in an assert at
        // PhysX mutex indicating it must be unlocked only by the thread that has already acquired lock.
        m_cpuDispatcher = physx::PxDefaultCpuDispatcherCreate(0);
#else
        m_cpuDispatcher = AzPhysXCpuDispatcherCreate();
#endif
        
        PxSetProfilerCallback(&m_pxAzProfilerCallback);

        // Set Physics API constants to PhysX-friendly values
        Physics::DefaultRigidBodyConfiguration::m_computeInertiaTensor = true;
        Physics::DefaultRigidBodyConfiguration::m_sleepMinEnergy = 0.005f;

        LoadConfiguration();

        m_windProvider = AZStd::make_unique<WindProvider>();
    }

    bool SystemComponent::ConnectToPvd()
    {
#ifdef AZ_PHYSICS_DEBUG_ENABLED
        DisconnectFromPvd();

        // Select current PhysX Pvd debug type
        switch (m_physxConfiguration.m_settings.m_pvdTransportType)
        {
            case PhysX::Settings::PvdTransportType::File:
            {
                // Use current timestamp in the filename.
                AZ::u64 currentTimeStamp = AZStd::GetTimeUTCMilliSecond() / 1000;

                // Strip any filename used as .pxd2 forced (only .pvd or .px2 valid for PVD version 3.2016.12.21494747)
                AzFramework::StringFunc::Path::StripExtension(m_physxConfiguration.m_settings.m_pvdFileName);

                // Create output filename (format: <TimeStamp>-<FileName>.pxd2)
                AZStd::string filename = AZStd::to_string(currentTimeStamp);
                AzFramework::StringFunc::Append(filename, "-");
                AzFramework::StringFunc::Append(filename, m_physxConfiguration.m_settings.m_pvdFileName.c_str());
                AzFramework::StringFunc::Append(filename, ".pxd2");

                AZStd::string rootDirectory;
                EBUS_EVENT_RESULT(rootDirectory, AzFramework::ApplicationRequests::Bus, GetAppRoot);

                // Create the full filepath.
                AZStd::string safeFilePath;
                AzFramework::StringFunc::Path::Join
                (
                    rootDirectory.c_str(),
                    filename.c_str(),
                    safeFilePath
                );

                m_pvdTransport = physx::PxDefaultPvdFileTransportCreate(safeFilePath.c_str());
                break;
            }
            case PhysX::Settings::PvdTransportType::Network:
            {
                m_pvdTransport = physx::PxDefaultPvdSocketTransportCreate
                    (
                    m_physxConfiguration.m_settings.m_pvdHost.c_str(),
                    m_physxConfiguration.m_settings.m_pvdPort,
                    m_physxConfiguration.m_settings.m_pvdTimeoutInMilliseconds
                    );
                break;
            }
            default:
            {
                AZ_Error("PhysX", false, "Invalid PhysX Visual Debugger (PVD) Debug Type used %d.", m_physxConfiguration.m_settings.m_pvdTransportType);
                break;
            }
        }

        bool pvdConnectionSuccessful = false;
        if (m_pvdTransport)
        {
            pvdConnectionSuccessful = m_physxSDKGlobals.m_pvd->connect(*m_pvdTransport, physx::PxPvdInstrumentationFlag::eALL);
            if (pvdConnectionSuccessful)
            {
                AZ_Printf("PhysX", "Successfully connected to the PhysX Visual Debugger (PVD).\n");
            }
            else
            {
                AZ_Printf("PhysX", "Failed to connect to the PhysX Visual Debugger (PVD).\n");
            }
        }

        return pvdConnectionSuccessful;
#else
        return false;
#endif
    }

    void SystemComponent::DisconnectFromPvd()
    {
#ifdef AZ_PHYSICS_DEBUG_ENABLED
        if (m_physxSDKGlobals.m_pvd)
        {
            m_physxSDKGlobals.m_pvd->disconnect();
        }

        if (m_pvdTransport)
        {
            m_pvdTransport->release();
            m_pvdTransport = nullptr;
            AZ_Printf("PhysX", "Successfully disconnected from the PhysX Visual Debugger (PVD).\n");
        }
#endif
    }

    void SystemComponent::UpdateColliderProximityVisualization(bool enabled, const AZ::Vector3& cameraPosition, float radius)
    {
        Settings::ColliderProximityVisualization& colliderProximityVisualization = m_physxConfiguration.m_settings.m_colliderProximityVisualization;
        colliderProximityVisualization.m_enabled = enabled;
        colliderProximityVisualization.m_cameraPosition = cameraPosition;
        colliderProximityVisualization.m_radius = radius;

#ifdef PHYSX_EDITOR
        // We update the editor world when the camera position has moved sufficiently from the last updated position.
        if (m_cameraPositionCache.GetDistance(cameraPosition) > radius * 0.5f)
        {
            Physics::EditorWorldBus::Broadcast(&Physics::EditorWorldRequests::MarkEditorWorldDirty);
            m_cameraPositionCache = cameraPosition;
        }
#endif
    }

    void SystemComponent::Deactivate()
    {
#ifdef PHYSX_EDITOR
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();
#endif
        Physics::CollisionRequestBus::Handler::BusDisconnect();
        CrySystemEventBus::Handler::BusDisconnect();
        PhysX::ConfigurationRequestBus::Handler::BusDisconnect();
        PhysX::SystemRequestsBus::Handler::BusDisconnect();
        Physics::SystemRequestBus::Handler::BusDisconnect();
        AZ::Data::AssetBus::Handler::BusDisconnect();

        // Reset material manager
        m_materialManager.ReleaseAllMaterials();

        m_materialManager.Disconnect();

        delete m_cpuDispatcher;
        m_cpuDispatcher = nullptr;
        m_assetHandlers.clear();

        m_windProvider.reset();

        // Clear the asset reference in deactivate. The asset system is shut down before destructors are called
        // for system components, causing any hanging asset references to become crashes on shutdown in release builds.
        m_physicsConfiguration.m_defaultMaterialLibrary = AZ::Data::Asset<Physics::MaterialLibraryAsset>();
    }

#ifdef PHYSX_EDITOR

    // AztoolsFramework::EditorEvents::Bus::Handler overrides
    void SystemComponent::PopulateEditorGlobalContextMenu(QMenu* menu, const AZ::Vector2& point, int flags)
    {
    }

    void SystemComponent::NotifyRegisterViews()
    {
        PhysX::Editor::EditorWindow::RegisterViewClass();
    }
#endif

    // PhysXSystemComponentRequestBus interface implementation
    physx::PxScene* SystemComponent::CreateScene(physx::PxSceneDesc& sceneDesc)
    {
        AZ_Assert(m_cpuDispatcher, "PhysX CPU dispatcher was not created");

#ifdef PHYSX_ENABLE_MULTI_THREADING
        sceneDesc.flags |= physx::PxSceneFlag::eREQUIRE_RW_LOCK;
#endif

        sceneDesc.cpuDispatcher = m_cpuDispatcher;
        return m_physxSDKGlobals.m_physics->createScene(sceneDesc);
    }

    physx::PxConvexMesh* SystemComponent::CreateConvexMesh(const void* vertices, AZ::u32 vertexNum, AZ::u32 vertexStride)
    {
        physx::PxConvexMeshDesc desc;
        desc.points.data = vertices;
        desc.points.count = vertexNum;
        desc.points.stride = vertexStride;
        // we provide points only, therefore the PxConvexFlag::eCOMPUTE_CONVEX flag must be specified
        desc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX;

        physx::PxConvexMesh* convex = m_physxSDKGlobals.m_cooking->createConvexMesh(desc,
            m_physxSDKGlobals.m_physics->getPhysicsInsertionCallback());
        AZ_Error("PhysX", convex, "Error. Unable to create convex mesh");

        return convex;
    }

    bool SystemComponent::CookConvexMeshToFile(const AZStd::string& filePath, const AZ::Vector3* vertices, AZ::u32 vertexCount)
    {
        AZStd::vector<AZ::u8> physxData;

        if (CookConvexMeshToMemory(vertices, vertexCount, physxData))
        {
            return Utils::WriteCookedMeshToFile(filePath, physxData, 
                Physics::CookedMeshShapeConfiguration::MeshType::Convex);
        }

        AZ_Error("PhysX", false, "CookConvexMeshToFile. Convex cooking failed for %s.", filePath);
        return false;
    }

    bool SystemComponent::CookTriangleMeshToFile(const AZStd::string& filePath, const AZ::Vector3* vertices, AZ::u32 vertexCount,
        const AZ::u32* indices, AZ::u32 indexCount)
    {   
        AZStd::vector<AZ::u8> physxData;

        if (CookTriangleMeshToMemory(vertices, vertexCount, indices, indexCount, physxData))
        {
            return Utils::WriteCookedMeshToFile(filePath, physxData,
                Physics::CookedMeshShapeConfiguration::MeshType::TriangleMesh);
        }

        AZ_Error("PhysX", false, "CookTriangleMeshToFile. Mesh cooking failed for %s.", filePath);
        return false;
    }

    bool SystemComponent::CookConvexMeshToMemory(const AZ::Vector3* vertices, AZ::u32 vertexCount, AZStd::vector<AZ::u8>& result)
    {
        physx::PxDefaultMemoryOutputStream memoryStream;
        
        bool cookingResult = Utils::CookConvexToPxOutputStream(vertices, vertexCount, memoryStream);
        
        if(cookingResult)
        {
            result.insert(result.end(), memoryStream.getData(), memoryStream.getData() + memoryStream.getSize());
        }
        
        return cookingResult;
    }

    bool SystemComponent::CookTriangleMeshToMemory(const AZ::Vector3* vertices, AZ::u32 vertexCount,
        const AZ::u32* indices, AZ::u32 indexCount, AZStd::vector<AZ::u8>& result)
    {
        physx::PxDefaultMemoryOutputStream memoryStream;
        bool cookingResult = Utils::CookTriangleMeshToToPxOutputStream(vertices, vertexCount, indices, indexCount, memoryStream);

        if (cookingResult)
        {
            result.insert(result.end(), memoryStream.getData(), memoryStream.getData() + memoryStream.getSize());
        }

        return cookingResult;
    }

    physx::PxConvexMesh* SystemComponent::CreateConvexMeshFromCooked(const void* cookedMeshData, AZ::u32 bufferSize)
    {
        physx::PxDefaultMemoryInputData inpStream(reinterpret_cast<physx::PxU8*>(const_cast<void*>(cookedMeshData)), bufferSize);
        return m_physxSDKGlobals.m_physics->createConvexMesh(inpStream);
    }

    physx::PxTriangleMesh* SystemComponent::CreateTriangleMeshFromCooked(const void* cookedMeshData, AZ::u32 bufferSize)
    {
        physx::PxDefaultMemoryInputData inpStream(reinterpret_cast<physx::PxU8*>(const_cast<void*>(cookedMeshData)), bufferSize);
        return m_physxSDKGlobals.m_physics->createTriangleMesh(inpStream);
    }

    AZStd::shared_ptr<Physics::World> SystemComponent::CreateWorld(AZ::Crc32 id)
    {
        return CreateWorldCustom(id, m_physicsConfiguration.m_defaultWorldConfiguration);
    }

    AZStd::shared_ptr<Physics::World> SystemComponent::CreateWorldCustom(AZ::Crc32 id, const Physics::WorldConfiguration& settings)
    {
        AZStd::shared_ptr<World> physxWorld = AZStd::make_shared<World>(id, settings);
        return physxWorld;
    }

    void SystemComponent::SetDefaultWorldConfiguration(const Physics::WorldConfiguration& worldConfiguration)
    {
        const bool gravityChanged =
            m_physicsConfiguration.m_defaultWorldConfiguration.m_gravity != worldConfiguration.m_gravity;
        const bool maxTimeStepChanged =
            m_physicsConfiguration.m_defaultWorldConfiguration.m_maxTimeStep != worldConfiguration.m_maxTimeStep;
        const bool fixedTimeStepChanged =
            m_physicsConfiguration.m_defaultWorldConfiguration.m_fixedTimeStep != worldConfiguration.m_fixedTimeStep;

        m_physicsConfiguration.m_defaultWorldConfiguration = worldConfiguration;

        if (gravityChanged)
        {
            Physics::WorldRequestBus::Broadcast(
                &Physics::WorldRequests::SetGravity, m_physicsConfiguration.m_defaultWorldConfiguration.m_gravity);
        }

        if (maxTimeStepChanged)
        {
            Physics::WorldRequestBus::Broadcast(
                &Physics::WorldRequests::SetMaxDeltaTime, m_physicsConfiguration.m_defaultWorldConfiguration.m_maxTimeStep);
        }

        if (fixedTimeStepChanged)
        {
            Physics::WorldRequestBus::Broadcast(
                &Physics::WorldRequests::SetFixedDeltaTime, m_physicsConfiguration.m_defaultWorldConfiguration.m_fixedTimeStep);
        }

        SaveConfiguration();
    }

    void SystemComponent::SetDefaultMaterialLibrary(const AZ::Data::Asset<Physics::MaterialLibraryAsset>& materialLibrary)
    {
        if (m_physicsConfiguration.m_defaultMaterialLibrary == materialLibrary)
        {
            return;
        }
        m_physicsConfiguration.m_defaultMaterialLibrary = materialLibrary;
        LoadDefaultMaterialLibrary();
#ifdef PHYSX_EDITOR
        PhysX::ConfigurationNotificationBus::Broadcast(
            &PhysX::ConfigurationNotifications::OnDefaultMaterialLibraryChanged, materialLibrary.GetId());
#endif
        SaveConfiguration();
    }

    AZStd::unique_ptr<Physics::RigidBodyStatic> SystemComponent::CreateStaticRigidBody(const Physics::WorldBodyConfiguration& configuration)
    {
        return AZStd::make_unique<PhysX::RigidBodyStatic>(configuration);
    }

    AZStd::unique_ptr<Physics::RigidBody> SystemComponent::CreateRigidBody(const Physics::RigidBodyConfiguration& configuration)
    {
        return AZStd::make_unique<RigidBody>(configuration);
    }

    AZStd::shared_ptr<Physics::Shape> SystemComponent::CreateShape(const Physics::ColliderConfiguration& colliderConfiguration, const Physics::ShapeConfiguration& configuration)
    {
        auto shapePtr = AZStd::make_shared<PhysX::Shape>(colliderConfiguration, configuration);

        if (shapePtr->GetPxShape())
        {
            return shapePtr;
        }

        AZ_Error("PhysX", false, "SystemComponent::CreateShape error. Unable to create a shape from configuration.");

        return nullptr;
    }

    AZStd::shared_ptr<Physics::Material> SystemComponent::CreateMaterial(const Physics::MaterialConfiguration& materialConfiguration)
    {
        return AZStd::make_shared<PhysX::Material>(materialConfiguration);
    }

    AZStd::vector<AZStd::shared_ptr<Physics::Material>> SystemComponent::CreateMaterialsFromLibrary(const Physics::MaterialSelection& materialSelection)
    {
        AZStd::vector<physx::PxMaterial*> pxMaterials;
        m_materialManager.GetPxMaterials(materialSelection, pxMaterials);

        AZStd::vector<AZStd::shared_ptr<Physics::Material>> genericMaterials;
        genericMaterials.reserve(pxMaterials.size());

        for (physx::PxMaterial* pxMaterial : pxMaterials)
        {
            genericMaterials.push_back(static_cast<PhysX::Material*>(pxMaterial->userData)->shared_from_this());
        }

        return genericMaterials;
    }

    AZStd::shared_ptr<Physics::Material> SystemComponent::GetDefaultMaterial()
    {
        return m_materialManager.GetDefaultMaterial();
    }

    void SystemComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        if (m_physicsConfiguration.m_defaultMaterialLibrary == asset)
        {
            m_physicsConfiguration.m_defaultMaterialLibrary = asset;

#ifdef PHYSX_EDITOR
            PhysX::ConfigurationNotificationBus::Broadcast(
                &PhysX::ConfigurationNotifications::OnDefaultMaterialLibraryChanged, asset.GetId());
#endif
        }
    }

    AZStd::vector<AZ::TypeId> SystemComponent::GetSupportedJointTypes()
    {
        return JointUtils::GetSupportedJointTypes();
    }

    AZStd::shared_ptr<Physics::JointLimitConfiguration> SystemComponent::CreateJointLimitConfiguration(AZ::TypeId jointType)
    {
        return JointUtils::CreateJointLimitConfiguration(jointType);
    }

    AZStd::shared_ptr<Physics::Joint> SystemComponent::CreateJoint(const AZStd::shared_ptr<Physics::JointLimitConfiguration>& configuration,
        Physics::WorldBody* parentBody, Physics::WorldBody* childBody)
    {
        return JointUtils::CreateJoint(configuration, parentBody, childBody);
    }

    void SystemComponent::GenerateJointLimitVisualizationData(
        const Physics::JointLimitConfiguration& configuration,
        const AZ::Quaternion& parentRotation,
        const AZ::Quaternion& childRotation,
        float scale,
        AZ::u32 angularSubdivisions,
        AZ::u32 radialSubdivisions,
        AZStd::vector<AZ::Vector3>& vertexBufferOut,
        AZStd::vector<AZ::u32>& indexBufferOut,
        AZStd::vector<AZ::Vector3>& lineBufferOut,
        AZStd::vector<bool>& lineValidityBufferOut)
    {
        JointUtils::GenerateJointLimitVisualizationData(configuration, parentRotation, childRotation, scale,
            angularSubdivisions, radialSubdivisions, vertexBufferOut, indexBufferOut, lineBufferOut, lineValidityBufferOut);
    }

    AZStd::unique_ptr<Physics::JointLimitConfiguration> SystemComponent::ComputeInitialJointLimitConfiguration(
        const AZ::TypeId& jointLimitTypeId,
        const AZ::Quaternion& parentWorldRotation,
        const AZ::Quaternion& childWorldRotation,
        const AZ::Vector3& axis,
        const AZStd::vector<AZ::Quaternion>& exampleLocalRotations)
    {
        return JointUtils::ComputeInitialJointLimitConfiguration(jointLimitTypeId, parentWorldRotation,
            childWorldRotation, axis, exampleLocalRotations);
    }

    void SystemComponent::ReleaseNativeMeshObject(void* nativeMeshObject)
    {
        if (nativeMeshObject)
        {
            static_cast<physx::PxBase*>(nativeMeshObject)->release();
        }
    }

    Physics::CollisionConfiguration SystemComponent::CreateDefaultCollisionConfiguration() const
    {
        Physics::CollisionConfiguration configuration;
        configuration.m_collisionLayers.SetName(Physics::CollisionLayer::Default, "Default");

        configuration.m_collisionGroups.CreateGroup("All", Physics::CollisionGroup::All, Physics::CollisionGroups::Id(), true);
        configuration.m_collisionGroups.CreateGroup("None", Physics::CollisionGroup::None, Physics::CollisionGroups::Id::Create(), true);

#ifdef TOUCHBENDING_LAYER_BIT
        configuration.m_collisionLayers.SetName(Physics::CollisionLayer::TouchBend, "TouchBend");
        configuration.m_collisionGroups.CreateGroup("All_NoTouchBend", Physics::CollisionGroup::All_NoTouchBend, Physics::CollisionGroups::Id::Create(), true);
#endif

        return configuration;
    }

    void SystemComponent::LoadConfiguration()
    {
        // Initialize collision configuration with default one.
        m_physicsConfiguration.m_collisionConfiguration = CreateDefaultCollisionConfiguration();

        // Load PhysX configuration.
        {
            bool loaded = AZ::Utils::LoadObjectFromFileInPlace<PhysXConfiguration>(ProjectPhysXConfigurationPath, m_physxConfiguration);

            if (loaded)
            {
                PhysX::ConfigurationNotificationBus::Broadcast(&PhysX::ConfigurationNotificationBus::Events::OnPhysXConfigurationLoaded);
            }
        }

        // Load Physics configuration.
        {
            AZ::Utils::LoadObjectFromFileInPlace<PhysicsConfiguration>(ProjectPhysicsConfigurationPath, m_physicsConfiguration);
        }

        SaveConfiguration();
    }

    void SystemComponent::SaveConfiguration()
    {
        // Save configuration to source folder when in edit mode.
#ifdef PHYSX_EDITOR
        auto assetRoot = AZ::IO::FileIOBase::GetInstance()->GetAlias("@devassets@");

        if (!assetRoot)
        {
            return;
        }

        // Save PhysX configuration.
        {
            AZStd::string fullPath;
            AzFramework::StringFunc::Path::Join(assetRoot, ProjectPhysXConfigurationPath, fullPath);

            bool saved = AZ::Utils::SaveObjectToFile<PhysXConfiguration>(fullPath.c_str(), AZ::DataStream::ST_XML,
                                                                         &m_physxConfiguration);
            AZ_Warning("PhysXSystemComponent", saved, "Failed to save PhysX configuration");
            if (saved)
            {
                PhysX::ConfigurationNotificationBus::Broadcast(
                    &PhysX::ConfigurationNotificationBus::Events::OnPhysXConfigurationRefreshed, m_physxConfiguration);
            }
        }

        // Save physics configuration.
        {
            AZStd::string fullPath;
            AzFramework::StringFunc::Path::Join(
                assetRoot, ProjectPhysicsConfigurationPath, fullPath);

            bool saved = AZ::Utils::SaveObjectToFile<PhysicsConfiguration>(
                fullPath.c_str(), AZ::DataStream::ST_XML, &m_physicsConfiguration);
            AZ_Warning("PhysXSystemComponent", saved, "Failed to save physics configuration");
        }
#endif
    }

    void SystemComponent::CheckoutConfiguration()
    {
        // Checkout the configuration files so we can write to it later on
#ifdef PHYSX_EDITOR
        using AzToolsFramework::SourceControlFileInfo;
        using AzToolsFramework::SourceControlCommandBus;
        
        const auto assetRoot = AZ::IO::FileIOBase::GetInstance()->GetAlias("@devassets@");

        for (const auto& path : {DefaultConfigurationPath, ProjectPhysicsConfigurationPath, ProjectPhysXConfigurationPath})
        {
            AZStd::string fullPath;
            AzFramework::StringFunc::Path::Join(assetRoot, path, fullPath);
            
            AzToolsFramework::SourceControlCommandBus::Broadcast(&AzToolsFramework::SourceControlCommandBus::Events::RequestEdit,
                fullPath.c_str(), true,
                [](bool /*success*/, const AzToolsFramework::SourceControlFileInfo& info)
            {
                // File is checked out
            });            
        }
#endif
    }

    void SystemComponent::AddColliderComponentToEntity(AZ::Entity* entity, const Physics::ColliderConfiguration& colliderConfiguration, const Physics::ShapeConfiguration& shapeConfiguration, bool addEditorComponents)
    {
        Physics::ShapeType shapeType = shapeConfiguration.GetShapeType();

#ifdef PHYSX_EDITOR
        if (addEditorComponents)
        {
            entity->CreateComponent<EditorColliderComponent>(colliderConfiguration, shapeConfiguration);
        }
        else
#else
        {
            if (shapeType == Physics::ShapeType::Sphere)
            {
                const Physics::SphereShapeConfiguration& sphereConfiguration = static_cast<const Physics::SphereShapeConfiguration&>(shapeConfiguration);
                auto sphereColliderComponent = entity->CreateComponent<SphereColliderComponent>();
                sphereColliderComponent->SetShapeConfigurationList({ AZStd::make_pair(
                    AZStd::make_shared<Physics::ColliderConfiguration>(colliderConfiguration),
                    AZStd::make_shared<Physics::SphereShapeConfiguration>(sphereConfiguration)) });
            }
            else if (shapeType == Physics::ShapeType::Box)
            {
                const Physics::BoxShapeConfiguration& boxConfiguration = static_cast<const Physics::BoxShapeConfiguration&>(shapeConfiguration);
                auto boxColliderComponent = entity->CreateComponent<BoxColliderComponent>();
                boxColliderComponent->SetShapeConfigurationList({ AZStd::make_pair(
                    AZStd::make_shared<Physics::ColliderConfiguration>(colliderConfiguration),
                    AZStd::make_shared<Physics::BoxShapeConfiguration>(boxConfiguration)) });
            }
            else if (shapeType == Physics::ShapeType::Capsule)
            {
                const Physics::CapsuleShapeConfiguration& capsuleConfiguration = static_cast<const Physics::CapsuleShapeConfiguration&>(shapeConfiguration);
                auto capsuleColliderComponent = entity->CreateComponent<CapsuleColliderComponent>();
                capsuleColliderComponent->SetShapeConfigurationList({ AZStd::make_pair(
                    AZStd::make_shared<Physics::ColliderConfiguration>(colliderConfiguration),
                    AZStd::make_shared<Physics::CapsuleShapeConfiguration>(capsuleConfiguration)) });
            }
        }

        AZ_Error("PhysX System", !addEditorComponents, "AddColliderComponentToEntity(): Trying to add an Editor collider component in a stand alone build.",
            static_cast<AZ::u8>(shapeType));

#endif
        {
            AZ_Error("PhysX System", shapeType == Physics::ShapeType::Sphere || shapeType == Physics::ShapeType::Box || shapeType == Physics::ShapeType::Capsule,
                "AddColliderComponentToEntity(): Using Shape of type %d is not implemented.", static_cast<AZ::u8>(shapeType));
        }
    }

    void SystemComponent::SetPhysXConfiguration(const PhysXConfiguration& configuration)
    {
        m_physxConfiguration = configuration;
#ifdef PHYSX_EDITOR
        OnEditorConfigurationChanged();
#endif
        SaveConfiguration();
    }

    const PhysXConfiguration& SystemComponent::GetPhysXConfiguration()
    {
        return m_physxConfiguration;
    }

    const Physics::WorldConfiguration& SystemComponent::GetDefaultWorldConfiguration() const
    {
        return m_physicsConfiguration.m_defaultWorldConfiguration;
    }

    const AZ::Data::Asset<Physics::MaterialLibraryAsset>* SystemComponent::GetDefaultMaterialLibraryAssetPtr()
    {
        return &m_physicsConfiguration.m_defaultMaterialLibrary;
    }

    void SystemComponent::OnCrySystemInitialized(ISystem&, const SSystemInitParams&)
    {
        // Configuration is loaded after cry system is initialised as this is when filesystem
        // is ready. It can't be loaded during Activate() as the asset root hasn't been
        // set at this time.
        LoadConfiguration();
        LoadDefaultMaterialLibrary();
    }

    void SystemComponent::OnCryEditorInitialized()
    {
        // Checkout configuration at startup ahead of time.
        CheckoutConfiguration();
    }

    Physics::CollisionLayer SystemComponent::GetCollisionLayerByName(const AZStd::string& layerName)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionLayers.GetLayer(layerName);
    }

    AZStd::string SystemComponent::GetCollisionLayerName(const Physics::CollisionLayer& layer)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionLayers.GetName(layer);
    }

    bool SystemComponent::TryGetCollisionLayerByName(const AZStd::string& layerName, Physics::CollisionLayer& layer)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionLayers.TryGetLayer(layerName, layer);
    }

    Physics::CollisionGroup SystemComponent::GetCollisionGroupByName(const AZStd::string& groupName)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionGroups.FindGroupByName(groupName);
    }

    bool SystemComponent::TryGetCollisionGroupByName(const AZStd::string& groupName, Physics::CollisionGroup& group)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionGroups.TryFindGroupByName(groupName, group);
    }

    AZStd::string SystemComponent::GetCollisionGroupName(const Physics::CollisionGroup& collisionGroup)
    {
        AZStd::string groupName;
        for (const auto& group : m_physicsConfiguration.m_collisionConfiguration.m_collisionGroups.GetPresets())
        {
            if (group.m_group.GetMask() == collisionGroup.GetMask())
            {
                groupName = group.m_name;
                break;
            }
        }
        return groupName;
    }

    Physics::CollisionGroup SystemComponent::GetCollisionGroupById(const Physics::CollisionGroups::Id& groupId)
    {
        return m_physicsConfiguration.m_collisionConfiguration.m_collisionGroups.FindGroupById(groupId);
    }

    void SystemComponent::SetCollisionLayerName(int index, const AZStd::string& layerName)
    {
        m_physicsConfiguration.m_collisionConfiguration.m_collisionLayers.SetName(index, layerName);
    }

    void SystemComponent::CreateCollisionGroup(const AZStd::string& groupName, const Physics::CollisionGroup& group)
    {
        m_physicsConfiguration.m_collisionConfiguration.m_collisionGroups.CreateGroup(groupName, group);
    }

    void SystemComponent::SetCollisionConfiguration(const Physics::CollisionConfiguration& collisionConfiguration)
    {
        m_physicsConfiguration.m_collisionConfiguration = collisionConfiguration;

        SaveConfiguration();
    }

    Physics::CollisionConfiguration SystemComponent::GetCollisionConfiguration()
    {
        return m_physicsConfiguration.m_collisionConfiguration;
    }

    physx::PxFilterData SystemComponent::CreateFilterData(const Physics::CollisionLayer& layer, const Physics::CollisionGroup& group)
    {
        return PhysX::Collision::CreateFilterData(layer, group);
    }

    AZStd::shared_ptr<Physics::Shape> SystemComponent::CreateWrappedNativeShape(physx::PxShape* nativeShape)
    {
        return AZStd::make_shared<Shape>(nativeShape);
    }

    physx::PxCooking* SystemComponent::GetCooking()
    {
        return m_physxSDKGlobals.m_cooking;
    }

    physx::PxControllerManager* SystemComponent::CreateControllerManager(physx::PxScene* scene)
    {
        return PxCreateControllerManager(*scene);
    }

    void SystemComponent::ReleaseControllerManager(physx::PxControllerManager* controllerManager)
    {
        controllerManager->release();
    }

    physx::PxCookingParams GetRealTimeCookingParams()
    {
        physx::PxCookingParams params { physx::PxTolerancesScale() };
        // disable mesh cleaning - perform mesh validation on development configurations
        params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
        // disable edge pre-compute, edges are set for each triangle, slows contact generation
        params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eDISABLE_ACTIVE_EDGES_PRECOMPUTE;

        return params;
    }

    physx::PxCookingParams GetEditTimeCookingParams()
    {
        physx::PxCookingParams params { physx::PxTolerancesScale() };
        // when set, mesh welding is performed - clean mesh must be enabled
        params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
        // note: default value in PxCookingParams is 0.0f;
        const float physx_CookWeldTolerance = 0.0001f;
        params.meshWeldTolerance = physx_CookWeldTolerance;

        return params;
    }

    bool SystemComponent::LoadDefaultMaterialLibrary()
    {
        AZ::Data::Asset<Physics::MaterialLibraryAsset>& materialLibrary 
            = m_physicsConfiguration.m_defaultMaterialLibrary;

        if (!materialLibrary.GetId().IsValid())
        {
            AZ_Warning("PhysX", false,
                "LoadDefaultMaterialLibrary: Default Material Library asset ID is invalid.");
            return false;
        }

        const bool queueLoadData = true;
        const AZ::Data::AssetFilterCB assetLoadFilterCB = nullptr;
        const bool loadBlocking = true;
        materialLibrary = AZ::Data::AssetManager::Instance().GetAsset<Physics::MaterialLibraryAsset>(
            materialLibrary.GetId(), queueLoadData, assetLoadFilterCB, loadBlocking);

        // Listen for material library asset modification events
        if (!AZ::Data::AssetBus::Handler::BusIsConnectedId(materialLibrary.GetId()))
        {
            AZ::Data::AssetBus::Handler::BusDisconnect();
            AZ::Data::AssetBus::Handler::BusConnect(materialLibrary.GetId());
        }

        AZ_Warning("PhysX", (materialLibrary.GetData() != nullptr),
            "LoadDefaultMaterialLibrary: Default Material Library asset data is invalid.");
        return materialLibrary.GetData() != nullptr;
    }

    bool SystemComponent::UpdateMaterialSelection(const Physics::ShapeConfiguration& shapeConfiguration,
        Physics::ColliderConfiguration& colliderConfiguration)
    {
        Physics::MaterialSelection& materialSelection = colliderConfiguration.m_materialSelection;

        // If the material library is still not set, we can't update the material selection
        if (!materialSelection.IsMaterialLibraryValid())
        {
            AZ_Warning("PhysX", false,
                "UpdateMaterialSelection: Material Selection tried to use an invalid/non-existing Physics material library: \"%s\". "
                "Please make sure the file exists or re-assign another library", materialSelection.GetMaterialLibraryAssetHint().c_str());
            return false;
        }

        // If there's no material library data loaded, try to load it
        if (materialSelection.GetMaterialLibraryAssetData() == nullptr)
        {
            AZ::Data::AssetId materialLibraryAssetId = materialSelection.GetMaterialLibraryAssetId();
            materialSelection.SetMaterialLibrary(materialLibraryAssetId);
        }

        // If there's still not material library data, we can't update the material selection 
        if (materialSelection.GetMaterialLibraryAssetData() == nullptr)
        {
            AZ::Data::AssetId materialLibraryAssetId = materialSelection.GetMaterialLibraryAssetId();

            auto materialLibraryAsset =
                AZ::Data::AssetManager::Instance().GetAsset<Physics::MaterialLibraryAsset>(materialLibraryAssetId, true, nullptr, true);

            // Log the asset path to help find out the incorrect library reference
            AZStd::string assetPath = materialLibraryAsset.GetHint();
            AZ_Warning("PhysX", false,
                "UpdateMaterialSelection: Unable to load the material library for a material selection."
                " Please check if the asset %s exists in the asset cache.", assetPath.c_str());

            return false;
        }

        if (shapeConfiguration.GetShapeType() == Physics::ShapeType::PhysicsAsset)
        {
            const Physics::PhysicsAssetShapeConfiguration& assetConfiguration =
                static_cast<const Physics::PhysicsAssetShapeConfiguration&>(shapeConfiguration);

            // Use the materials data from the asset to update the collider data
            return UpdateMaterialSelectionFromPhysicsAsset(assetConfiguration, colliderConfiguration);
        }

        return true;
    }

    bool SystemComponent::UpdateMaterialSelectionFromPhysicsAsset(
        const Physics::PhysicsAssetShapeConfiguration& assetConfiguration,
        Physics::ColliderConfiguration& colliderConfiguration)
    {
        Physics::MaterialSelection& materialSelection = colliderConfiguration.m_materialSelection;

        if (!assetConfiguration.m_asset.GetId().IsValid())
        {
            // Set the default selection if there's no physics asset.
            materialSelection.SetMaterialSlots(Physics::MaterialSelection::SlotsArray());
            return false;
        }

        if (!assetConfiguration.m_asset.IsReady())
        {
            // The asset is valid but is still loading, 
            // Do not set the empty slots in this case to avoid the entity being in invalid state
            return false;
        }

        Pipeline::MeshAsset* meshAsset = assetConfiguration.m_asset.GetAs<Pipeline::MeshAsset>();
        if (!meshAsset)
        {
            materialSelection.SetMaterialSlots(Physics::MaterialSelection::SlotsArray());
            AZ_Warning("PhysX", false, "UpdateMaterialSelectionFromPhysicsAsset: MeshAsset is invalid");
            return false;
        }

        // Set the slots from the mesh asset
        materialSelection.SetMaterialSlots(meshAsset->m_assetData.m_surfaceNames);

        if (!assetConfiguration.m_useMaterialsFromAsset)
        {
            return false;
        }

        const Physics::MaterialLibraryAsset* materialLibrary = materialSelection.GetMaterialLibraryAssetData();
        const AZStd::vector<AZStd::string>& meshMaterialNames = meshAsset->m_assetData.m_materialNames;

        // Update material IDs in the selection for each slot
        int slotIndex = 0;
        for (const AZStd::string& meshMaterialName : meshMaterialNames)
        {
            Physics::MaterialFromAssetConfiguration materialData;
            bool found = materialLibrary->GetDataForMaterialName(meshMaterialName, materialData);

            AZ_Warning("PhysX", found, 
                "UpdateMaterialSelectionFromPhysicsAsset: No material found for surfaceType (%s) in the collider material library", 
                meshMaterialName.c_str());

            if (found)
            {
                materialSelection.SetMaterialId(materialData.m_id, slotIndex);
            }

            slotIndex++;
        }

        return true;
    }

    void* PxAzProfilerCallback::zoneStart(const char* eventName, bool detached,
        [[maybe_unused]] uint64_t contextId)
    {
        if (!detached)
        {
            AZ_PROFILE_EVENT_BEGIN(AZ::Debug::ProfileCategory::Physics, eventName);
        }
        else
        {
            AZ_PROFILE_INTERVAL_START(AZ::Debug::ProfileCategory::Physics, AZ::Crc32(eventName), eventName);
        }
        return nullptr;
    }

    void PxAzProfilerCallback::zoneEnd([[maybe_unused]] void* profilerData,
        const char* eventName, bool detached,
        [[maybe_unused]] uint64_t contextId)
    {
        if (!detached)
        {
            AZ_PROFILE_EVENT_END(AZ::Debug::ProfileCategory::Physics);
        }
        else
        {
            AZ_PROFILE_INTERVAL_END(AZ::Debug::ProfileCategory::Physics, AZ::Crc32(eventName));
        }
    }

} // namespace PhysX
