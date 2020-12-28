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

#include "StdAfx.h"

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/EntityUtils.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentExport.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/IdUtils.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptSystemBus.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Slice/SliceMetadataInfoBus.h>

#include <AzFramework/API/ApplicationAPI.h>
#include <AzFramework/Entity/EntityContext.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ToolsComponents/EditorLayerComponent.h>
#include <AzToolsFramework/Commands/PreemptiveUndoCache.h>
#include <AzToolsFramework/Commands/EntityStateCommand.h>
#include <AzToolsFramework/Commands/SelectionCommand.h>
#include <AzToolsFramework/Commands/SliceDetachEntityCommand.h>
#include <AzToolsFramework/Commands/DetachSubSliceInstanceCommand.h>
#include <AzToolsFramework/Slice/SliceDataFlagsCommand.h>
#include <AzToolsFramework/Slice/SliceCompilation.h>

#include <AzToolsFramework/ToolsComponents/EditorEntityIconComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorInspectorComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorLayerComponentBus.h>
#include <AzToolsFramework/ToolsComponents/EditorLockComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorPendingCompositionComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorVisibilityComponent.h>
#include <AzToolsFramework/ToolsComponents/SelectionComponent.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorDisabledCompositionComponent.h>
#include <AzToolsFramework/ToolsComponents/EditorOnlyEntityComponent.h>
#include <AzToolsFramework/Entity/EditorEntitySortComponent.h>
#include <AzToolsFramework/Entity/EditorEntityHelpers.h>
#include <AzToolsFramework/Slice/SliceMetadataEntityContextBus.h>
#include <AzToolsFramework/Slice/SliceUtilities.h>

#include "EditorEntityContextComponent.h"

namespace AzToolsFramework
{
    namespace Internal
    {
        struct EditorEntityContextNotificationBusHandler final
            : public EditorEntityContextNotificationBus::Handler
            , public AZ::BehaviorEBusHandler
        {
            AZ_EBUS_BEHAVIOR_BINDER(EditorEntityContextNotificationBusHandler, "{159C07A6-BCB6-432E-BEBB-6AABF6C76989}", AZ::SystemAllocator,
                OnEditorEntityCreated, OnEditorEntityDeleted, OnSliceInstantiated, OnSliceInstantiationFailed);

            void OnEditorEntityCreated(const AZ::EntityId& entityId) override
            {
                Call(FN_OnEditorEntityCreated, entityId);
            }

            void OnEditorEntityDeleted(const AZ::EntityId& entityId) override
            {
                Call(FN_OnEditorEntityDeleted, entityId);
            }

            void OnSliceInstantiated(const AZ::Data::AssetId& sliceAssetId, AZ::SliceComponent::SliceInstanceAddress& sliceAddress, const AzFramework::SliceInstantiationTicket& ticket) override
            {
                Call(FN_OnSliceInstantiated, sliceAssetId, sliceAddress, ticket);
            }

            void OnSliceInstantiationFailed(const AZ::Data::AssetId& sliceAssetId, const AzFramework::SliceInstantiationTicket& ticket) override
            {
                Call(FN_OnSliceInstantiationFailed, sliceAssetId, ticket);
            }
        };
    }

    //=========================================================================
    // Reflect
    //=========================================================================
    void EditorEntityContextComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<EditorEntityContextComponent, AZ::Component>()
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<EditorEntityContextComponent>(
                    "Editor Entity Context", "System component responsible for owning the edit-time entity context")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Editor")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                    ;
            }
        }

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EBus<EditorEntityContextRequestBus>("EditorEntityContextRequestBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Category, "Editor")
                ->Attribute(AZ::Script::Attributes::Module, "editor")
                ->Event("GetEditorEntityContextId", &EditorEntityContextRequests::GetEditorEntityContextId)
                ;

            behaviorContext->EBus<EditorEntityContextNotificationBus>("EditorEntityContextNotificationBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                ->Attribute(AZ::Script::Attributes::Category, "Editor")
                ->Attribute(AZ::Script::Attributes::Module, "editor")
                ->Handler<Internal::EditorEntityContextNotificationBusHandler>()
                ->Event("OnEditorEntityCreated", &EditorEntityContextNotification::OnEditorEntityCreated)
                ->Event("OnEditorEntityDeleted", &EditorEntityContextNotification::OnEditorEntityDeleted)
                ->Event("OnSliceInstantiated", &EditorEntityContextNotification::OnSliceInstantiated)
                ->Event("OnSliceInstantiationFailed", &EditorEntityContextNotification::OnSliceInstantiationFailed)
                ;
        }
    }

    //=========================================================================
    // EditorEntityContextComponent ctor
    //=========================================================================
    EditorEntityContextComponent::EditorEntityContextComponent()
        : AzFramework::EntityContext(AzFramework::EntityContextId::CreateRandom())
        , m_isRunningGame(false)
        , m_requiredEditorComponentTypes
        // These are the components that will be force added to every entity in the editor
        ({
            azrtti_typeid<AzToolsFramework::Components::EditorDisabledCompositionComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorOnlyEntityComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorEntityIconComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorEntitySortComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorInspectorComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorLockComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorPendingCompositionComponent>(),
            azrtti_typeid<AzToolsFramework::Components::EditorVisibilityComponent>(),
            azrtti_typeid<AzToolsFramework::Components::SelectionComponent>(),
            azrtti_typeid<AzToolsFramework::Components::TransformComponent>()
    })
    {
    }

    //=========================================================================
    // EditorEntityContextComponent dtor
    //=========================================================================
    EditorEntityContextComponent::~EditorEntityContextComponent()
    {
    }

    //=========================================================================
    // Init
    //=========================================================================
    void EditorEntityContextComponent::Init()
    {
    }

    //=========================================================================
    // Activate
    //=========================================================================
    void EditorEntityContextComponent::Activate()
    {
        InitContext();

        GetRootSlice()->Instantiate();

        EditorEntityContextRequestBus::Handler::BusConnect();
        EditorEntityContextPickingRequestBus::Handler::BusConnect(GetContextId());
        EditorLegacyGameModeNotificationBus::Handler::BusConnect();
    }

    //=========================================================================
    // Deactivate
    //=========================================================================
    void EditorEntityContextComponent::Deactivate()
    {
        EditorLegacyGameModeNotificationBus::Handler::BusDisconnect();
        EditorEntityContextRequestBus::Handler::BusDisconnect();
        EditorEntityContextPickingRequestBus::Handler::BusDisconnect();

        DestroyContext();
    }

    //=========================================================================
    // EditorEntityContextRequestBus::ResetEditorContext
    //=========================================================================
    void EditorEntityContextComponent::ResetEditorContext()
    {
        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnContextReset);

        if (m_isRunningGame)
        {
            // Ensure we exit play-in-editor when the context is reset (switching levels).
            StopPlayInEditor();
        }

        ResetContext();
    }

    //=========================================================================
    // EditorEntityContextRequestBus::CreateEditorEntity
    //=========================================================================
    AZ::EntityId EditorEntityContextComponent::CreateNewEditorEntity(const char* name)
    {
        AZ::Entity* entity = CreateEntity(name);
        FinalizeEditorEntity(entity);
        return entity->GetId();
    }

    //=========================================================================
    // EditorEntityContextRequestBus::CreateEditorEntityWithId
    //=========================================================================
    AZ::EntityId EditorEntityContextComponent::CreateNewEditorEntityWithId(const char* name, const AZ::EntityId& entityId)
    {
        if (!entityId.IsValid())
        {
            AZ_Warning("EditorEntityContextComponent", false, "Cannot create an entity with an invalid ID.");
            return AZ::EntityId();
        }
        // Make sure this ID is not already in use.
        AZ::SliceComponent::EntityList existingEntities;
        GetRootSlice()->GetEntities(existingEntities);
        for (const AZ::Entity* existingEntity : existingEntities)
        {
            if (existingEntity->GetId() == entityId)
            {
                AZ_Warning(
                    "EditorEntityContextComponent",
                    false,
                    "An entity already exists with ID %s, a new entity will not be created.",
                    entityId.ToString().c_str());

                return AZ::EntityId();
            }
        }

        AZ::Entity* entity = aznew AZ::Entity(entityId, name);
        AddEntity(entity);
        FinalizeEditorEntity(entity);

        return entity->GetId();
    }

    //=========================================================================
    // EditorEntityContextComponent::FinalizeEditorEntity
    //=========================================================================
    void EditorEntityContextComponent::FinalizeEditorEntity(AZ::Entity* entity)
    {
        if (!entity)
        {
            return;
        }
        SetupEditorEntity(entity);

        // Store creation undo command.
        {
            ScopedUndoBatch undoBatch("Create Entity");

            EntityCreateCommand* command = aznew EntityCreateCommand(static_cast<AZ::u64>(entity->GetId()));
            command->Capture(entity);
            command->SetParent(undoBatch.GetUndoBatch());
        }

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntityCreated, entity->GetId());
    }

    //=========================================================================
    // EditorEntityContextRequestBus::AddEditorEntity
    //=========================================================================
    void EditorEntityContextComponent::AddEditorEntity(AZ::Entity* entity)
    {
        AZ_Assert(entity, "Supplied entity is invalid.");

        AddEntity(entity);
    }

    //=========================================================================
    // EditorEntityContextRequestBus::AddEditorEntities
    //=========================================================================
    void EditorEntityContextComponent::AddEditorEntities(const EntityList& entities)
    {
        AZ::SliceAsset* rootSlice = m_rootAsset.Get();

        for (AZ::Entity* entity : entities)
        {
            AZ_Assert(!AzFramework::EntityIdContextQueryBus::MultiHandler::BusIsConnectedId(entity->GetId()), "Entity already in context.");
            rootSlice->GetComponent()->AddEntity(entity);
        }

        HandleEntitiesAdded(entities);
    }

    void EditorEntityContextComponent::AddEditorSliceEntities(const EntityList& entities)
    {
        HandleEntitiesAdded(entities);
    }

    //=========================================================================
    // EditorEntityContextRequestBus::CloneEditorEntities
    //=========================================================================
    bool EditorEntityContextComponent::CloneEditorEntities(const EntityIdList& sourceEntities,
                                                           EntityList& resultEntities,
                                                           AZ::SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        resultEntities.clear();

        AZ::EntityUtils::SerializableEntityContainer sourceObjects;
        for (const AZ::EntityId& id : sourceEntities)
        {
            AZ::Entity* entity = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationRequests::FindEntity, id);
            if (entity)
            {
                sourceObjects.m_entities.push_back(entity);
            }
        }

        AZ::EntityUtils::SerializableEntityContainer* clonedObjects =
            AZ::IdUtils::Remapper<AZ::EntityId>::CloneObjectAndGenerateNewIdsAndFixRefs(&sourceObjects, sourceToCloneEntityIdMap);
        if (!clonedObjects)
        {
            AZ_Error("EditorEntityContext", false, "Failed to clone source entities.");
            sourceToCloneEntityIdMap.clear();
            return false;
        }

        resultEntities = clonedObjects->m_entities;

        delete clonedObjects;

        return true;
    }

    //=========================================================================
    // EditorEntityContextRequestBus::DestroyEditorEntity
    //=========================================================================
    bool EditorEntityContextComponent::DestroyEditorEntity(AZ::EntityId entityId)
    {
        if (DestroyEntityById(entityId))
        {
            EditorRequests::Bus::Broadcast(&EditorRequests::DestroyEditorRepresentation, entityId, false);
            return true;
        }

        return false;
    }

    //=========================================================================
    // EditorEntityContextRequestBus::DetachSliceEntities
    //=========================================================================
    void EditorEntityContextComponent::DetachSliceEntities(const AzToolsFramework::EntityIdList& entities)
    {
        const char* undoMsg = entities.size() == 1 ? "Detach Entity from Slice" : "Detach Entities from Slice";
        DetachFromSlice(entities, undoMsg);
    }

    //=========================================================================
    // EditorEntityContextRequestBus::DetachSliceInstance
    //=========================================================================
    void EditorEntityContextComponent::DetachSliceInstances(const AZ::SliceComponent::SliceInstanceAddressSet& instances)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        const char* undoMsg = instances.size() == 1 ? "Detach Instance from Slice" : "Detach Instances from Slice";

        // Get all entities in the input instance(s)
        AzToolsFramework::EntityIdList entities;
        for (const AZ::SliceComponent::SliceInstanceAddress& sliceInstanceAddress : instances)

        {
            if (!sliceInstanceAddress.IsValid())
            {
                continue;
            }

            const AZ::SliceComponent::InstantiatedContainer* instantiated = sliceInstanceAddress.GetInstance()->GetInstantiated();
            if (instantiated)
            {
                for (const AZ::Entity* entityInSlice : instantiated->m_entities)
                {
                    entities.push_back(entityInSlice->GetId());
                }
            }
        }

        DetachFromSlice(entities, undoMsg);
    }

    //=========================================================================
    // EditorEntityContextRequestBus::DetachSubsliceInstances
    //=========================================================================
    void EditorEntityContextComponent::DetachSubsliceInstances(const AZ::SliceComponent::SliceInstanceEntityIdRemapList& subsliceRootList)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (subsliceRootList.empty())
        {
            return;
        }

        const char* undoMsg = subsliceRootList.size() == 1 ? "Detach Subslice Instance from owning Slice Instance" : "Detach Subslice Instances from owning Slice Instance";

        ScopedUndoBatch undoBatch(undoMsg);

        AzToolsFramework::DetachSubsliceInstanceCommand* detachCommand = aznew AzToolsFramework::DetachSubsliceInstanceCommand(subsliceRootList, undoMsg);
        detachCommand->SetParent(undoBatch.GetUndoBatch());

        // Running Redo on the DetachSubsliceInstanceCommand will perform the Detach command
        ToolsApplicationRequestBus::Broadcast(&ToolsApplicationRequestBus::Events::RunRedoSeparately, undoBatch.GetUndoBatch());
    }

    void EditorEntityContextComponent::DetachFromSlice(const AzToolsFramework::EntityIdList& entities, const char* undoMessage)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (entities.empty())
        {
            return;
        }

        ScopedUndoBatch undoBatch(undoMessage);

        AzToolsFramework::SliceDetachEntityCommand* detachCommand = aznew AzToolsFramework::SliceDetachEntityCommand(entities, undoMessage);
        detachCommand->SetParent(undoBatch.GetUndoBatch());

        // Running Redo on the SliceDetachEntityCommand will perform the Detach command
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::RunRedoSeparately, undoBatch.GetUndoBatch());
    }

    //=========================================================================
    // EditorEntityContextRequestBus::ResetEntitiesToSliceDefaults
    //=========================================================================
    void EditorEntityContextComponent::ResetEntitiesToSliceDefaults(EntityIdList entities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        ScopedUndoBatch undoBatch("Resetting entities to slice defaults.");

        PreemptiveUndoCache* preemptiveUndoCache = nullptr;
        ToolsApplicationRequests::Bus::BroadcastResult(preemptiveUndoCache, &ToolsApplicationRequests::GetUndoCache);

        EntityIdList selectedEntities;
        ToolsApplicationRequestBus::BroadcastResult(selectedEntities, &ToolsApplicationRequests::GetSelectedEntities);

        SelectionCommand* selCommand = aznew SelectionCommand(
            selectedEntities, "Reset Entity to Slice Defaults");
        selCommand->SetParent(undoBatch.GetUndoBatch());

        SelectionCommand* newSelCommand = aznew SelectionCommand(
            selectedEntities, "Reset Entity to Slice Defaults");

        for (AZ::EntityId id : entities)
        {
            AZ::SliceComponent::SliceInstanceAddress sliceAddress;
            AzFramework::EntityIdContextQueryBus::EventResult(sliceAddress, id, &AzFramework::EntityIdContextQueries::GetOwningSlice);

            AZ::SliceComponent::SliceReference* sliceReference = sliceAddress.GetReference();
            AZ::SliceComponent::SliceInstance* sliceInstance = sliceAddress.GetInstance();

            AZ::EntityId sliceRootEntityId;
            ToolsApplicationRequestBus::BroadcastResult(sliceRootEntityId,
                &ToolsApplicationRequestBus::Events::GetRootEntityIdOfSliceInstance, sliceAddress);

            bool isSliceRootEntity = (id == sliceRootEntityId);

            if (sliceReference)
            {
                // Clear any data flags for entity
                auto clearDataFlagsCommand = aznew ClearSliceDataFlagsBelowAddressCommand(id, AZ::DataPatch::AddressType(), "Clear data flags");
                clearDataFlagsCommand->SetParent(undoBatch.GetUndoBatch());

                AZ::Entity* oldEntity = nullptr;
                AZ::ComponentApplicationBus::BroadcastResult(oldEntity, &AZ::ComponentApplicationBus::Events::FindEntity, id);
                AZ_Assert(oldEntity, "Couldn't find the entity we were looking for!");
                if (!oldEntity)
                {
                    continue;
                }

                // Clone the entity from the slice source (clean)
                auto sourceEntityIterator = sliceInstance->GetEntityIdToBaseMap().find(id);
                AZ_Assert(sourceEntityIterator != sliceInstance->GetEntityIdToBaseMap().end(), "Attempting to clone an invalid instance entity id for this slice instance!");
                if (sourceEntityIterator != sliceInstance->GetEntityIdToBaseMap().end())
                {
                    AZ::SliceComponent* dependentSlice = sliceReference->GetSliceAsset().Get()->GetComponent();
                    AZ::Entity* sourceEntity = dependentSlice->FindEntity(sourceEntityIterator->second);

                    AZ_Assert(sourceEntity, "Couldn't find source entity from sourceEntityId in slice reference!");
                    if (sourceEntity)
                    {
                        AZ::Entity* entityClone = dependentSlice->GetSerializeContext()->CloneObject(sourceEntity);
                        if (entityClone)
                        {
                            AZ::IdUtils::Remapper<AZ::EntityId>::ReplaceIdsAndIdRefs(
                                entityClone,
                                [sliceInstance](const AZ::EntityId& originalId, bool /*isEntityId*/, const AZStd::function<AZ::EntityId()>& /*idGenerator*/) -> AZ::EntityId
                                {
                                    auto findIt = sliceInstance->GetEntityIdMap().find(originalId);
                                    if (findIt == sliceInstance->GetEntityIdMap().end())
                                    {
                                        return originalId; // entityId is not being remapped
                                    }
                                    else
                                    {
                                        return findIt->second; // return the remapped id
                                    }
                                }, dependentSlice->GetSerializeContext());

                            // Only restore world position and rotation for the slice root entity.
                            if (isSliceRootEntity)
                            {
                                // Get the transform component on the cloned entity.  We cannot use the bus since it isn't activated.
                                Components::TransformComponent* transformComponent = entityClone->FindComponent<Components::TransformComponent>();
                                AZ_Assert(transformComponent, "Entity doesn't have a transform component!");
                                if (transformComponent)
                                {
                                    AZ::Vector3 oldEntityWorldPosition;
                                    AZ::TransformBus::EventResult(oldEntityWorldPosition, id, &AZ::TransformBus::Events::GetWorldTranslation);
                                    transformComponent->SetWorldTranslation(oldEntityWorldPosition);

                                    AZ::Quaternion oldEntityRotation;
                                    AZ::TransformBus::EventResult(oldEntityRotation, id, &AZ::TransformBus::Events::GetWorldRotationQuaternion);
                                    transformComponent->SetRotationQuaternion(oldEntityRotation);

                                    // Ensure the existing hierarchy is maintained
                                    AZ::EntityId oldParentEntityId;
                                    AZ::TransformBus::EventResult(oldParentEntityId, id, &AZ::TransformBus::Events::GetParentId);
                                    transformComponent->SetParent(oldParentEntityId);
                                }
                            }

                            // Create a state command and capture both the undo and redo data
                            EntityStateCommand* stateCommand = aznew EntityStateCommand(
                                static_cast<UndoSystem::URCommandID>(id));
                            stateCommand->Capture(oldEntity, true);
                            stateCommand->Capture(entityClone, false);
                            stateCommand->SetParent(undoBatch.GetUndoBatch());

                            // Delete our temporary entity clone
                            delete entityClone;
                        }
                    }
                }
            }
        }

        newSelCommand->SetParent(undoBatch.GetUndoBatch());

        // Run the redo in order to do the initial swap of entity data
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::RunRedoSeparately, undoBatch.GetUndoBatch());

        // Make sure to set selection to newly cloned entities
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::SetSelectedEntities, selectedEntities);
    }

    AZ::SliceComponent::SliceInstanceAddress EditorEntityContextComponent::CloneSubSliceInstance(
        const AZ::SliceComponent::SliceInstanceAddress& sourceSliceInstanceAddress,
        const AZStd::vector<AZ::SliceComponent::SliceInstanceAddress>& sourceSubSliceInstanceAncestry,
        const AZ::SliceComponent::SliceInstanceAddress& sourceSubSliceInstanceAddress,
        AZ::SliceComponent::EntityIdToEntityIdMap* out_sourceToCloneEntityIdMap)
    {
        AZ::SliceComponent::SliceInstanceAddress subsliceInstanceCloneAddress = 
            GetRootSlice()->CloneAndAddSubSliceInstance(sourceSliceInstanceAddress.GetInstance(), sourceSubSliceInstanceAncestry, sourceSubSliceInstanceAddress, out_sourceToCloneEntityIdMap);

        if (!subsliceInstanceCloneAddress.IsValid())
        {
            AZ_Assert(false, "Clone sub-slice instance failed.");
            return AZ::SliceComponent::SliceInstanceAddress();
        }

        return subsliceInstanceCloneAddress;
    }

    //=========================================================================
    // EditorEntityContextRequestBus::CloneEditorSliceInstance
    //=========================================================================
    AZ::SliceComponent::SliceInstanceAddress EditorEntityContextComponent::CloneEditorSliceInstance(
        AZ::SliceComponent::SliceInstanceAddress sourceInstance, AZ::SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (sourceInstance.IsValid())
        {
            AZ::SliceComponent::SliceInstanceAddress address = CloneSliceInstance(sourceInstance, sourceToCloneEntityIdMap);

            return address;
        }
        else
        {
            AZ_Error("EditorEntityContext", false, "Invalid slice source instance. Unable to clone.");
        }

        return AZ::SliceComponent::SliceInstanceAddress();
    }

    //=========================================================================
    // EditorEntityContextRequestBus::SaveToStreamForEditor
    //=========================================================================
    bool EditorEntityContextComponent::SaveToStreamForEditor(
        AZ::IO::GenericStream& stream,
        const EntityList& entitiesInLayers,
        AZ::SliceComponent::SliceReferenceToInstancePtrs& instancesInLayers)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ_Assert(stream.IsOpen(), "Invalid target stream.");
        AZ_Assert(m_rootAsset.Get() && m_rootAsset.Get()->GetComponent(), "The context is not initialized.");

        // Grab the entire entity list to make sure saving layers doesn't cause entity order to shuffle.
        EntityList entities = m_rootAsset.Get()->GetComponent()->GetNewEntities();

        if (!instancesInLayers.empty())
        {
            m_rootAsset.Get()->GetComponent()->RemoveAndCacheInstances(instancesInLayers);
        }

        if (!entitiesInLayers.empty())
        {
            // These entities were already saved out to individual layer files, so do not save them to the level file.
            m_rootAsset.Get()->GetComponent()->EraseEntities(entitiesInLayers);
        }

        bool saveResult = AZ::Utils::SaveObjectToStream<AZ::Entity>(stream, SliceUtilities::GetSliceStreamFormat(), m_rootAsset.Get()->GetEntity());

        // Restore the level slice's entities.
        if (!entities.empty())
        {
            m_rootAsset.Get()->GetComponent()->ReplaceEntities(entities);
        }

        if (!instancesInLayers.empty())
        {
            m_rootAsset.Get()->GetComponent()->RestoreCachedInstances();
        }

        return saveResult;
    }

    void EditorEntityContextComponent::GetLooseEditorEntities(EntityList& entityList)
    {
        AZ::SliceComponent* sliceComponent = m_rootAsset.Get()->GetComponent();
        AZ_Error("Editor", sliceComponent, "Root asset slice is not available.");
        if (sliceComponent)
        {
            entityList.insert(entityList.end(), sliceComponent->GetNewEntities().begin(), sliceComponent->GetNewEntities().end());
        }
    }

    //=========================================================================
    // EditorEntityContextRequestBus::SaveToStreamForGame
    //=========================================================================
    bool EditorEntityContextComponent::SaveToStreamForGame(AZ::IO::GenericStream& stream, AZ::DataStream::StreamType streamType)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ::SliceComponent::EntityList sourceEntities;
        GetRootSlice()->GetEntities(sourceEntities);

        AZStd::vector<AZStd::unique_ptr<AZ::Entity>> tempEntities;
        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnSaveStreamForGameBegin, stream, streamType, tempEntities);

        sourceEntities.insert(sourceEntities.end(), tempEntities.begin(), tempEntities.end());
        // Add the root slice metadata entity so that we export any level components
        sourceEntities.push_back(GetRootSlice()->GetMetadataEntity());

        // Create a source slice from our editor components.
        AZ::Entity* sourceSliceEntity = aznew AZ::Entity();
        AZ::SliceComponent* sourceSliceData = sourceSliceEntity->CreateComponent<AZ::SliceComponent>();
        AZ::Data::Asset<AZ::SliceAsset> sourceSliceAsset(aznew AZ::SliceAsset());
        sourceSliceAsset.Get()->SetData(sourceSliceEntity, sourceSliceData);

        for (AZ::Entity* sourceEntity : sourceEntities)
        {
            // This is a sanity check that the editor source entity was created and validated before starting export to game target entity
            // Note - this assert should *not* live directly in PreExportEntity implementation,
            //   because of the case where we are modifying system entities we do not want system components active, ticking tick buses, running simulations, etc.
            AZ_Assert(sourceEntity->GetState() == AZ::Entity::ES_ACTIVE, "Source entity must be active.");

            sourceSliceData->AddEntity(sourceEntity);
        }

        // Emulate client flags.
        AZ::PlatformTagSet platformTags = {AZ_CRC("renderer", 0xf199a19c)};

        // Compile the source slice into the runtime slice (with runtime components).
        WorldEditorOnlyEntityHandler worldEditorOnlyEntityHandler;
        EditorOnlyEntityHandlers handlers =
        {
            &worldEditorOnlyEntityHandler
        };
        SliceCompilationResult sliceCompilationResult = CompileEditorSlice(sourceSliceAsset, platformTags, *m_serializeContext, handlers);

        // This is a sanity check that the editor source entity was created and validated and didn't suddenly get out of an active state during export
        // Note - this assert should *not* live directly in PostExportEntity implementation,
        //   because of the case where we are modifying system entities we do not want system components active, ticking tick buses, running simulations, etc.
        for (AZ::Entity* sourceEntity : sourceEntities)
        {
            AZ_UNUSED(sourceEntity);
            AZ_Assert(sourceEntity->GetState() == AZ::Entity::ES_ACTIVE, "Source entity must be active.");
        }

        // Reclaim entities from the temporary source asset. We now have ownership of the entities.
        sourceSliceData->RemoveAllEntities(false);


        if (!sliceCompilationResult)
        {
            AZ_Error("Save Runtime Stream", false, "Failed to export entities for runtime:\n%s", sliceCompilationResult.GetError().c_str());
            EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnSaveStreamForGameFailure, sliceCompilationResult.GetError());
            return false;
        }

        // Export runtime slice representing the level, which is a completely flat list of entities.
        AZ::Data::Asset<AZ::SliceAsset> exportSliceAsset = sliceCompilationResult.GetValue();
        bool saveResult = AZ::Utils::SaveObjectToStream<AZ::Entity>(stream, streamType, exportSliceAsset.Get()->GetEntity());
        if (saveResult)
        {
            EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnSaveStreamForGameSuccess, stream);
        }
        else
        {
            EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnSaveStreamForGameFailure,
                AZStd::string::format("Unable to save slice asset %s to stream", exportSliceAsset.GetId().ToString<AZStd::string>().data()));
        }

        return saveResult;
    }

    //=========================================================================
    // EditorEntityContextRequestBus::LoadFromStream
    //=========================================================================
    bool EditorEntityContextComponent::LoadFromStream(AZ::IO::GenericStream& stream)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ_Assert(stream.IsOpen(), "Invalid source stream.");
        AZ_Assert(m_rootAsset, "The context has not been initialized.");

        EditorEntityContextNotificationBus::Broadcast(
            &EditorEntityContextNotification::OnEntityStreamLoadBegin);

        const bool loadedSuccessfully = AzFramework::EntityContext::LoadFromStream(stream, false, nullptr, AZ::ObjectStream::FilterDescriptor(&AZ::Data::AssetFilterSourceSlicesOnly));

        LoadFromStreamComplete(loadedSuccessfully);

        return loadedSuccessfully;
    }

    bool EditorEntityContextComponent::LoadFromStreamWithLayers(AZ::IO::GenericStream& stream, QString levelPakFile)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ_Assert(stream.IsOpen(), "Invalid source stream.");
        AZ_Assert(m_rootAsset, "The context has not been initialized.");

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEntityStreamLoadBegin);

        AZ::ObjectStream::FilterDescriptor filterDesc = AZ::ObjectStream::FilterDescriptor(&AZ::Data::AssetFilterSourceSlicesOnly);

        AZ::Entity* newRootEntity = AZ::Utils::LoadObjectFromStream<AZ::Entity>(stream, m_serializeContext, filterDesc);

        AZ::SliceComponent* newRootSlice = newRootEntity->FindComponent<AZ::SliceComponent>();
        EntityList entities = newRootSlice->GetNewEntities();
        AZ::SliceComponent::SliceAssetToSliceInstancePtrs layerSliceInstances;

        // Collect all of the layers by names, so duplicates can be found.
        // Layer name collision is handled here instead of in the layer component because none of these entities are connected to buses yet.
        using EntityAndLayerComponent = AZStd::pair<AZ::Entity*, Layers::EditorLayerComponent*>;
        AZStd::unordered_map<AZStd::string, AZStd::vector<EntityAndLayerComponent>> layerNameToLayerComponents;

        AZStd::unordered_map<AZ::EntityId, AZ::Entity*> uniqueEntities;
        for (AZ::Entity* entity : entities)
        {
            // The entities just loaded from the layer aren't going to be connected to any buses yet,
            // check for the layer component directly.
            Layers::EditorLayerComponent* layerComponent = entity->FindComponent<Layers::EditorLayerComponent>();
            
            if (layerComponent)
            {
                layerNameToLayerComponents[layerComponent->GetCachedLayerBaseFileName()].push_back(EntityAndLayerComponent(entity, layerComponent));
            }

            AZStd::unordered_map<AZ::EntityId, AZ::Entity*>::iterator existingEntity = uniqueEntities.find(entity->GetId());
            if (existingEntity != uniqueEntities.end())
            {
                AZ_Warning(
                    "Editor",
                    false,
                    "Multiple entities that exist in your level with ID %s and name %s were found. The last entity found has been loaded.",
                    entity->GetId().ToString().c_str(),
                    entity->GetName().c_str());
                AZ::Entity* duplicateEntity = existingEntity->second;
                existingEntity->second = entity;
                delete duplicateEntity;
            }
            else
            {
                uniqueEntities[entity->GetId()] = entity;
            }
        }
        
        for(auto &layersIterator : layerNameToLayerComponents)
        {
            if (layersIterator.second.size() > 1)
            {
                AZ_Error("Layers", false, "There is more than one layer with the name %s at the same hierarchy level. Rename these layers and try again.", layersIterator.first.c_str());
            }
            else
            {
                // Only load layers that have no collisions with other layer names.
                Layers::EditorLayerComponent* layerComponent(layersIterator.second[0].second);
                Layers::LayerResult layerReadResult =
                    layerComponent->ReadLayer(levelPakFile, layerSliceInstances, uniqueEntities);

                layerReadResult.MessageResult();
            }
        }

        EntityList uniqueEntityList;
        for (const auto& uniqueEntity : uniqueEntities)
        {
            uniqueEntityList.push_back(uniqueEntity.second);
        }

        // Add the layer entities before anything's been activated.
        // This allows the layer system to function with minimal changes to level loading flow in the editor,
        // no existing systems need to know about the layer system.
        AZStd::unordered_set<const AZ::SliceComponent::SliceInstance*> instances;
        newRootSlice->AddSliceInstances(layerSliceInstances, instances);
        newRootSlice->ReplaceEntities(uniqueEntityList);

        // Clean up layer data after adding the information to the slice, so it isn't lost.
        for (auto &layersIterator : layerNameToLayerComponents)
        {
            if (layersIterator.second.size() == 1)
            {
                // Only load layers that have no collisions with other layer names.
                Layers::EditorLayerComponent* layerComponent(layersIterator.second[0].second);
                layerComponent->CleanupLoadedLayer();
            }
        }

        // make sure that PRE_NOTIFY assets get their notify before we activate, so that we can preserve the order of 
        // (load asset) -> (notify) -> (init) -> (activate)
        AZ::Data::AssetManager::Instance().DispatchEvents();

        // for other kinds of instantiations, like slice instantiations, because they use the queued slice instantiation mechanism, they will always
        // be instantiated after their asset is already ready.

        const bool loadedSuccessfully = HandleLoadedRootSliceEntity(newRootEntity, false, nullptr);
        
        LoadFromStreamComplete(loadedSuccessfully);
        
        return loadedSuccessfully;
    }
    
    void EditorEntityContextComponent::LoadFromStreamComplete(bool loadedSuccessfully)
    {
        if (loadedSuccessfully)
        {
            AZ::SliceComponent::EntityList entities;
            GetRootSlice()->GetEntities(entities);

            GetRootSlice()->SetIsDynamic(true);

            SetupEditorEntities(entities);
            EditorEntityContextNotificationBus::Broadcast(
                &EditorEntityContextNotification::OnEntityStreamLoadSuccess);
        }
        else
        {
            EditorEntityContextNotificationBus::Broadcast(
                &EditorEntityContextNotification::OnEntityStreamLoadFailed);
        }
    }

    AZ::SliceComponent::SliceInstanceAddress EditorEntityContextComponent::PromoteEditorEntitiesIntoSlice(const AZ::Data::Asset<AZ::SliceAsset>& sliceAsset, const AZ::SliceComponent::EntityIdToEntityIdMap& liveToAssetMap)
    {
        AZ::SliceComponent* editorRootSlice = nullptr;
        EditorEntityContextRequestBus::BroadcastResult(editorRootSlice, &EditorEntityContextRequestBus::Events::GetEditorRootSlice);

        if (!sliceAsset.GetId().IsValid())
        {
            AZ_Error("EditorEntityContextComponent::PromoteEditorEntitiesIntoSlice", false, "SliceAsset with Invalid Asset ID passed in. Unable to promote entities into slice");
            return AZ::SliceComponent::SliceInstanceAddress();
        }

        if (!editorRootSlice)
        {
            AZ_Assert(editorRootSlice, "EditorEntityContextComponent::PromoteEditorEntitiesIntoSlice: Editor root slice not found! Unable to promote entities into slice");
            return AZ::SliceComponent::SliceInstanceAddress();
        }

        AzFramework::SliceInstantiationTicket ticket = GenerateSliceInstantiationTicket();

        PreSliceInstantiate(sliceAsset.GetId(), AZ::SliceComponent::SliceInstanceAddress(), ticket);

        AZ::SliceComponent::SliceInstanceAddress instanceAddress = editorRootSlice->AddSliceUsingExistingEntities(sliceAsset, liveToAssetMap);

        if (!instanceAddress.IsValid())
        {
            SliceInstantiateFailed(sliceAsset.GetId(), ticket);
            return AZ::SliceComponent::SliceInstanceAddress();
        }

        PostSliceInstantiate(sliceAsset.GetId(), instanceAddress, ticket);

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotificationBus::Events::OnSliceInstantiated, sliceAsset.GetId(), instanceAddress, ticket);

        const EntityList& instanceEntities = instanceAddress.GetInstance()->GetInstantiated()->m_entities;

        EntityIdSet instantiatedIdSet;
        AzToolsFramework::EntityIdList instantiatedIds;
        for (const AZ::Entity* instanceEntity : instanceEntities)
        {
            instantiatedIdSet.emplace(instanceEntity->GetId());
            instantiatedIds.emplace_back(instanceEntity->GetId());
        }

        UpdateSelectedEntitiesInHierarchy(instantiatedIdSet);

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotificationBus::Events::OnEditorEntitiesPromotedToSlicedEntities, instantiatedIds);

        return instanceAddress;
    }

    //=========================================================================
    // EditorEntityContextRequestBus::InstantiateEditorSlice
    //=========================================================================
    AzFramework::SliceInstantiationTicket EditorEntityContextComponent::InstantiateEditorSlice(const AZ::Data::Asset<AZ::Data::AssetData>& sliceAsset, const AZ::Transform& worldTransform)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        if (sliceAsset.GetId().IsValid())
        {
            m_instantiatingSlices.push_back(AZStd::make_pair(sliceAsset, worldTransform));

            const AzFramework::SliceInstantiationTicket ticket = InstantiateSlice(sliceAsset);
            if (ticket)
            {
                AzFramework::SliceInstantiationResultBus::MultiHandler::BusConnect(ticket);
            }

            return ticket;
        }

        return AzFramework::SliceInstantiationTicket();
    }

    //=========================================================================
    // EntityContextRequestBus::StartPlayInEditor
    //=========================================================================
    void EditorEntityContextComponent::StartPlayInEditor()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnStartPlayInEditorBegin);

        // Save the editor context to a memory stream.
        AZStd::vector<char> entityBuffer;
        AZ::IO::ByteContainerStream<AZStd::vector<char> > stream(&entityBuffer);
        const bool isRuntimeStreamValid = SaveToStreamForGame(stream, AZ::DataStream::ST_BINARY);

        if (!isRuntimeStreamValid)
        {
            AZ_Error("EditorEntityContext", false, "Failed to create runtime entity context for play-in-editor mode. Entities will not be created.");
        }

        // Deactivate the editor context.
        AZ::SliceComponent* rootSlice = GetRootSlice();
        if (rootSlice)
        {
            AZ::SliceComponent::EntityList entities;
            rootSlice->GetEntities(entities);

            AZ::Entity* rootMetaDataEntity = rootSlice->GetMetadataEntity();
            if (rootMetaDataEntity)
            {
                entities.push_back(rootMetaDataEntity);
            }

            for (AZ::Entity* entity : entities)
            {
                if (entity->GetState() == AZ::Entity::ES_ACTIVE)
                {
                    entity->Deactivate();
                }
            }
        }

        m_runtimeToEditorIdMap.clear();

        if (isRuntimeStreamValid)
        {
            // Load the exported stream into the game context.
            stream.Seek(0, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequests::LoadFromStream, stream, true);

            // Retrieve Id map from game entity context (editor->runtime).
            AzFramework::EntityContextId gameContextId = AzFramework::EntityContextId::CreateNull();
            AzFramework::GameEntityContextRequestBus::BroadcastResult(
                gameContextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
            AzFramework::EntityContextRequestBus::EventResult(
                m_editorToRuntimeIdMap, gameContextId, &AzFramework::EntityContextRequests::GetLoadedEntityIdMap);

            // Generate reverse lookup (runtime->editor).
            for (const auto& idMapping : m_editorToRuntimeIdMap)
            {
                m_runtimeToEditorIdMap.insert(AZStd::make_pair(idMapping.second, idMapping.first));
            }
        }

        m_isRunningGame = true;

        ToolsApplicationRequests::Bus::BroadcastResult(m_selectedBeforeStartingGame, &ToolsApplicationRequests::GetSelectedEntities);

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnStartPlayInEditor);
    }

    //=========================================================================
    // EntityContextRequestBus::StopPlayInEditor
    //=========================================================================
    void EditorEntityContextComponent::StopPlayInEditor()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        m_isRunningGame = false;

        m_editorToRuntimeIdMap.clear();
        m_runtimeToEditorIdMap.clear();

        // Reset the runtime context.
        AzFramework::GameEntityContextRequestBus::Broadcast(
            &AzFramework::GameEntityContextRequests::ResetGameContext);

        // Do a full lua GC.
        AZ::ScriptSystemRequestBus::Broadcast(&AZ::ScriptSystemRequests::GarbageCollect);

        // Re-activate the editor context.
        AZ::SliceComponent* rootSlice = GetRootSlice();
        if (rootSlice)
        {
            AZ::SliceComponent::EntityList entities;
            rootSlice->GetEntities(entities);

            AZ::Entity* rootMetaDataEntity = rootSlice->GetMetadataEntity();
            if (rootMetaDataEntity)
            {
                entities.push_back(rootMetaDataEntity);
            }

            for (AZ::Entity* entity : entities)
            {
                if (entity->GetState() == AZ::Entity::ES_CONSTRUCTED)
                {
                    entity->Init();
                }

                if (entity->GetState() == AZ::Entity::ES_INIT)
                {
                    entity->Activate();
                }
            }
            // we need to check for locked layers and re-apply the lock to their children.
            // This needs to be in a separate loop as we need to be sure all the entities are activated.
            for (AZ::Entity* entity : entities)
            {
                AZ::EntityId entityId = entity->GetId();
                bool locked = false;
                EditorLockComponentRequestBus::EventResult(locked, entityId, &EditorLockComponentRequests::GetLocked);
                if (locked)
                {
                    bool isLayer = false;
                    Layers::EditorLayerComponentRequestBus::EventResult(
                        isLayer, entityId, &Layers::EditorLayerComponentRequestBus::Events::HasLayer);
                    if (isLayer)
                    {
                        SetEntityLockState(entityId, true);
                    }
                }
            }
        }

        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::SetSelectedEntities, m_selectedBeforeStartingGame);
        m_selectedBeforeStartingGame.clear();

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnStopPlayInEditor);
    }

    //=========================================================================
    // EntityContextRequestBus::IsEditorRunningGame
    //=========================================================================
    bool EditorEntityContextComponent::IsEditorRunningGame()
    {
        return m_isRunningGame;
    }

    bool EditorEntityContextComponent::IsEditorRequestingGame()
    {
        return m_isRequestingGame;
    }

    //=========================================================================
    // EntityContextRequestBus::IsEditorEntity
    //=========================================================================
    bool EditorEntityContextComponent::IsEditorEntity(AZ::EntityId id)
    {
        AzFramework::EntityContextId contextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::EntityIdContextQueryBus::EventResult(contextId, id, &EntityIdContextQueries::GetOwningContextId);

        if (contextId == GetContextId())
        {
            return true;
        }

        return false;
    }

    //=========================================================================
    // EntityContextRequestBus::AddRequiredComponents
    //=========================================================================
    void EditorEntityContextComponent::AddRequiredComponents(AZ::Entity& entity)
    {
        for (const auto& componentType : m_requiredEditorComponentTypes)
        {
            if (!entity.FindComponent(componentType))
            {
                entity.CreateComponent(componentType);
            }
        }
    }

    //=========================================================================
    // EntityContextRequestBus::GetRequiredComponentTypes
    //=========================================================================
    const AZ::ComponentTypeList& EditorEntityContextComponent::GetRequiredComponentTypes()
    {
        return m_requiredEditorComponentTypes;
    }

    //=========================================================================
    // EntityContextRequestBus::RestoreSliceEntity
    //=========================================================================
    void EditorEntityContextComponent::RestoreSliceEntity(AZ::Entity* entity, const AZ::SliceComponent::EntityRestoreInfo& info, SliceEntityRestoreType restoreType)
    {
        AZ_Error("EditorEntityContext", info.m_assetId.IsValid(), "Invalid asset Id for entity restore.");

        // If asset isn't loaded when this request is made, we need to queue the load and process the request
        // when the asset is ready. Otherwise we'll immediately process the request when OnAssetReady is invoked
        // by the AssetBus connection policy.

        // Hold a reference to the slice asset so the underlying AssetData won't be ref-counted away when re-adding slice instance.
        AZ::Data::Asset<AZ::Data::AssetData> asset =
            AZ::Data::AssetManager::Instance().GetAsset<AZ::SliceAsset>(info.m_assetId, true);

        SliceEntityRestoreRequest request = {entity, info, asset, restoreType};

        // Remove all previous queued states of the same entity, only restore to the most recent state.
        AZ::EntityId entityId = entity->GetId();
        m_queuedSliceEntityRestores.erase(
            AZStd::remove_if(m_queuedSliceEntityRestores.begin(), m_queuedSliceEntityRestores.end(), 
                             [entityId](const SliceEntityRestoreRequest& request) { return request.m_entity->GetId() == entityId; }),
            m_queuedSliceEntityRestores.end());

        m_queuedSliceEntityRestores.emplace_back(request);

        AZ::Data::AssetBus::MultiHandler::BusConnect(asset.GetId());
    }

    //=========================================================================
    // EntityContextEventBus::MapEditorIdToRuntimeId
    //=========================================================================
    bool EditorEntityContextComponent::MapEditorIdToRuntimeId(const AZ::EntityId& editorId, AZ::EntityId& runtimeId)
    {
        auto iter = m_editorToRuntimeIdMap.find(editorId);
        if (iter != m_editorToRuntimeIdMap.end())
        {
            runtimeId = iter->second;
            return true;
        }

        return false;
    }

    //=========================================================================
    // EntityContextEventBus::MapRuntimeIdToEditorId
    //=========================================================================
    bool EditorEntityContextComponent::MapRuntimeIdToEditorId(const AZ::EntityId& runtimeId, AZ::EntityId& editorId)
    {
        auto iter = m_runtimeToEditorIdMap.find(runtimeId);
        if (iter != m_runtimeToEditorIdMap.end())
        {
            editorId = iter->second;
            return true;
        }

        return false;
    }

    //=========================================================================
    // EditorEntityContextPickingRequestBus::SupportsViewportEntityIdPicking
    //=========================================================================
    bool EditorEntityContextComponent::SupportsViewportEntityIdPicking()
    {
        return true;
    }

    //=========================================================================
    // EntityContextEventBus::OnSlicePreInstantiate
    //=========================================================================
    void EditorEntityContextComponent::OnSlicePreInstantiate(const AZ::Data::AssetId& sliceAssetId, const AZ::SliceComponent::SliceInstanceAddress& sliceAddress)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);
        const AzFramework::SliceInstantiationTicket ticket = *AzFramework::SliceInstantiationResultBus::GetCurrentBusId();

        // Start an undo that will wrap the entire slice instantiation event (unable to do this at a higher level since this is queued up by AzFramework and there's no undo concept at that level)
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::BeginUndoBatch, "Slice Instantiation");

        // Find the next ticket corresponding to this asset.
        // Given the desired world root, position entities in the instance.
        for (auto instantiatingIter = m_instantiatingSlices.begin(); instantiatingIter != m_instantiatingSlices.end(); ++instantiatingIter)
        {
            if (instantiatingIter->first.GetId() == sliceAssetId)
            {
                const AZ::Transform& worldTransform = instantiatingIter->second;

                const AZ::SliceComponent::EntityList& entities = sliceAddress.GetInstance()->GetInstantiated()->m_entities;

                for (AZ::Entity* entity : entities)
                {
                    auto* transformComponent = entity->FindComponent<Components::TransformComponent>();
                    if (transformComponent)
                    {
                        // Non-root entities will be positioned relative to their parents.
                        if (!transformComponent->GetParentId().IsValid())
                        {
                            // Note: Root slice entity always has translation at origin, so this maintains scale & rotation.
                            transformComponent->SetWorldTM(worldTransform * transformComponent->GetWorldTM());
                        }
                    }
                }

                break;
            }
        }
    }

    //=========================================================================
    // EntityContextEventBus::OnSliceInstantiated
    //=========================================================================
    void EditorEntityContextComponent::OnSliceInstantiated(const AZ::Data::AssetId& sliceAssetId, const AZ::SliceComponent::SliceInstanceAddress& sliceAddress)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        const AzFramework::SliceInstantiationTicket ticket = *AzFramework::SliceInstantiationResultBus::GetCurrentBusId();

        AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);

        // Make a copy of sliceAddress because a handler of EditorEntityContextNotification::OnSliceInstantiated (CCryEditDoc::OnSliceInstantiated)
        // can end up destroying the slice instance which invalidates the values stored in sliceAddress for the following handlers. 
        // If CCryEditDoc::OnSliceInstantiated destroys the slice instance, it will set SliceInstanceAddress::m_instance and SliceInstanceAddress::m_reference 
        // to nullptr making the sliceAddressCopy invalid for the following handlers.
        AZ::SliceComponent::SliceInstanceAddress sliceAddressCopy = sliceAddress; 

        // Close out the next ticket corresponding to this asset.
        for (auto instantiatingIter = m_instantiatingSlices.begin(); instantiatingIter != m_instantiatingSlices.end(); ++instantiatingIter)
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::OnSliceInstantiated:CloseTicket");
            if (instantiatingIter->first.GetId() == sliceAssetId)
            {
                const AZ::SliceComponent::EntityList& entities = sliceAddressCopy.GetInstance()->GetInstantiated()->m_entities;

                // Select slice roots when found, otherwise default to selecting all entities in slice
                EntityIdSet setOfEntityIds;
                for (const AZ::Entity* const entity : entities)
                {
                    setOfEntityIds.insert(entity->GetId());
                }

                UpdateSelectedEntitiesInHierarchy(setOfEntityIds);

                // Create a slice instantiation undo command.
                {
                    AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::OnSliceInstantiated:CloseTicket:CreateInstantiateUndo");
                    ScopedUndoBatch undoBatch("Instantiate Slice");
                    for (AZ::Entity* entity : entities)
                    {

                        EntityCreateCommand* command = aznew EntityCreateCommand(
                            reinterpret_cast<UndoSystem::URCommandID>(sliceAddressCopy.GetInstance()));
                        command->Capture(entity);
                        command->SetParent(undoBatch.GetUndoBatch());
                    }
                }

                EditorEntityContextNotificationBus::Broadcast(
                    &EditorEntityContextNotification::OnSliceInstantiated, sliceAssetId, sliceAddressCopy, ticket);

                m_instantiatingSlices.erase(instantiatingIter);

                break;
            }
        }

        // End the slice instantiation undo batch started in OnSlicePreInstantiate
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::Bus::Events::EndUndoBatch);
    }

    //=========================================================================
    // EntityContextEventBus::OnSliceInstantiationFailed
    //=========================================================================
    void EditorEntityContextComponent::OnSliceInstantiationFailed(const AZ::Data::AssetId& sliceAssetId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        const AzFramework::SliceInstantiationTicket ticket = *AzFramework::SliceInstantiationResultBus::GetCurrentBusId();

        AzFramework::SliceInstantiationResultBus::MultiHandler::BusDisconnect(ticket);

        for (auto instantiatingIter = m_instantiatingSlices.begin(); instantiatingIter != m_instantiatingSlices.end(); ++instantiatingIter)
        {
            if (instantiatingIter->first.GetId() == sliceAssetId)
            {
                EditorEntityContextNotificationBus::Broadcast(
                    &EditorEntityContextNotification::OnSliceInstantiationFailed, sliceAssetId, ticket);

                m_instantiatingSlices.erase(instantiatingIter);
                break;
            }
        }
    }

    //=========================================================================
    // PrepareForContextReset
    //=========================================================================
    void EditorEntityContextComponent::PrepareForContextReset()
    {
        EntityContext::PrepareForContextReset();
        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::PrepareForContextReset);
    }

    //=========================================================================
    // ValidateEntitiesAreValidForContext
    //=========================================================================
    bool EditorEntityContextComponent::ValidateEntitiesAreValidForContext(const EntityList& entities)
    {
        // All entities in a slice being instantiated in the level editor should
        // have the TransformComponent on them. Since it is not possible to create
        // a slice with entities from different contexts, it is OK to check
        // the first entity only
        if (entities.size() > 0)
        {
            return entities[0]->FindComponent<Components::TransformComponent>() != nullptr;
        }

        return true;
    }

    //=========================================================================
    // OnContextEntitiesAdded
    //=========================================================================
    void EditorEntityContextComponent::OnContextEntitiesAdded(const EntityList& entities)
    {
        EntityContext::OnContextEntitiesAdded(entities);

        // Any entities being added to the context that don't belong to another slice
        // need to be associated with the root slice metadata info component.
        for (const auto* entity : entities)
        {
            auto sliceAddress = m_rootAsset.Get()->GetComponent()->FindSlice(entity->GetId());
            if (!sliceAddress.IsValid())
            {
                AZ::SliceMetadataInfoManipulationBus::Event(m_rootAsset.Get()->GetComponent()->GetMetadataEntity()->GetId(), &AZ::SliceMetadataInfoManipulationBus::Events::AddAssociatedEntity, entity->GetId());
            }
        }

        SetupEditorEntities(entities);
    }

    //=========================================================================
    // HandleNewMetadataEntities
    //=========================================================================
    void EditorEntityContextComponent::HandleNewMetadataEntitiesCreated(AZ::SliceComponent& slice)
    {
        AZ::SliceComponent::EntityList metadataEntityIds;
        slice.GetAllMetadataEntities(metadataEntityIds);

        const auto& slices = slice.GetSlices();

        // Add the metadata entity from the root slice to the appropriate context
        // For now, the instance address of the root slice is <nullptr,nullptr>
        SliceMetadataEntityContextRequestBus::Broadcast(&SliceMetadataEntityContextRequestBus::Events::AddMetadataEntityToContext, AZ::SliceComponent::SliceInstanceAddress(), *slice.GetMetadataEntity());

        // Add the metadata entities from every slice instance streamed in as well. We need to grab them directly
        // from their slices so we have the appropriate instance address for each one.
        for (auto& subSlice : slices)
        {
            for (const auto& instance : subSlice.GetInstances())
            {
                if (auto instantiated = instance.GetInstantiated())
                {
                    for (auto* metadataEntity : instantiated->m_metadataEntities)
                    {
                        SliceMetadataEntityContextRequestBus::Broadcast(&SliceMetadataEntityContextRequestBus::Events::AddMetadataEntityToContext, AZ::SliceComponent::SliceInstanceAddress(const_cast<AZ::SliceComponent::SliceReference*>(&subSlice), const_cast<AZ::SliceComponent::SliceInstance*>(&instance)), *metadataEntity);
                    }
                }
            }
        }
    }

    //=========================================================================
    // OnContextEntityRemoved
    //=========================================================================
    void EditorEntityContextComponent::OnContextEntityRemoved(const AZ::EntityId& entityId)
    {
        EditorRequests::Bus::Broadcast(&EditorRequests::DestroyEditorRepresentation, entityId, false);

        EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntityDeleted, entityId);
    }

    //=========================================================================
    // SetupEditorEntity
    //=========================================================================
    void EditorEntityContextComponent::SetupEditorEntity(AZ::Entity* entity)
    {
        SetupEditorEntities({ entity });
    }

    //=========================================================================
    // SetupEditorEntities
    //=========================================================================
    void EditorEntityContextComponent::SetupEditorEntities(const EntityList& entities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        // All editor entities are automatically activated.

        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::SetupEditorEntities:ScrubEntities");

            // Scrub entities before initialization.
            // Anything could go wrong with entities loaded from disk.
            // Ex: There might be duplicates of components that do not tolerate
            // duplication and would crash during their Init().
            EntityCompositionRequestBus::Broadcast(&EntityCompositionRequestBus::Events::ScrubEntities, entities);
        }

        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::SetupEditorEntities:InitEntities");
            for (AZ::Entity* entity : entities)
            {
                if (entity->GetState() == AZ::Entity::ES_CONSTRUCTED)
                {
                    entity->Init();
                }
            }
        }

        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::SetupEditorEntities:CreateEditorRepresentations");
            for (AZ::Entity* entity : entities)
            {
                EditorRequests::Bus::Broadcast(&EditorRequests::CreateEditorRepresentation, entity);
            }
        }

        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzToolsFramework, "EditorEntityContextComponent::SetupEditorEntities:ActivateEntities");
            for (AZ::Entity* entity : entities)
            {
                if (entity->GetState() == AZ::Entity::ES_INIT)
                {
                    // Always invalidate the entity dependencies when loading in the editor
                    // (we don't know what code has changed since the last time the editor was run and the services provided/required
                    // by entities might have changed)
                    entity->InvalidateDependencies();
                    entity->Activate();
                }
            }

            // After activating all the entities, refresh their entries in the undo cache.  
            // We need to wait until after all the activations are complete.  Otherwise, it's possible that the data for an entity
            // will change based on other activations.  For example, if we activate a child entity before its parent, the Transform
            // component on the child will refresh its cache state on the parent activation.  This would cause the undo cache to be
            // out of sync with the child data.
            for (AZ::Entity* entity : entities)
            {
                PreemptiveUndoCache::Get()->UpdateCache(entity->GetId());
            }
        }
    }

    //=========================================================================
    // UpdateSelectedEntitiesInHierarchy
    //=========================================================================
    void EditorEntityContextComponent::UpdateSelectedEntitiesInHierarchy(const EntityIdSet& entityIdSet)
    {
        // Use a lambda to call multiple requests via one Broadcast
        ToolsApplicationRequestBus::Broadcast([&entityIdSet](ToolsApplicationRequests* toolsApplicationRequest)
        {
            EntityIdList selectedEntities;
            AZ::EntityId commonRoot;
            if (toolsApplicationRequest->FindCommonRoot(entityIdSet, commonRoot, &selectedEntities) || selectedEntities.empty())
            {
                selectedEntities.insert(selectedEntities.end(), entityIdSet.begin(), entityIdSet.end());
            }

            toolsApplicationRequest->SetSelectedEntities(selectedEntities);
        });
    }

    //=========================================================================
    // OnAssetReady
    //=========================================================================
    void EditorEntityContextComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        AZ::Data::AssetBus::MultiHandler::BusDisconnect(asset.GetId());

        AzFramework::EntityContext::EntityList deletedEntitiesRestored;
        AzFramework::EntityContext::EntityIdList detachedEntitiesRestored;

        AZ::SliceComponent* rootSlice = GetRootSlice();
        AZ_Assert(rootSlice, "Failed to retrieve editor root slice.");

        for (auto iter = m_queuedSliceEntityRestores.begin(); iter != m_queuedSliceEntityRestores.end(); )
        {
            SliceEntityRestoreRequest& request = *iter;
            if (asset.GetId() == request.m_asset.GetId())
            {
                AZ::SliceComponent::SliceInstanceAddress address =
                    rootSlice->RestoreEntity(request.m_entity, request.m_restoreInfo,
                        request.m_restoreType == SliceEntityRestoreType::Added);

                if (address.IsValid())
                {
                    switch (request.m_restoreType)
                    {
                        case SliceEntityRestoreType::Deleted:
                        {
                            deletedEntitiesRestored.push_back(request.m_entity);
                        } break;

                        case SliceEntityRestoreType::Added:
                        {
                            // Fall through and perform same operations as Detached
                        }
                        case SliceEntityRestoreType::Detached:
                        {
                            detachedEntitiesRestored.push_back(request.m_entity->GetId());
                            rootSlice->RemoveLooseEntity(request.m_entity->GetId());
                        } break;

                        default:
                        {
                            AZ_Error("EditorEntityContext", false, "Invalid slice entity restore type (%d). Entity: \"%s\" [%llu]",
                                request.m_restoreType, request.m_entity->GetName().c_str(), request.m_entity->GetId());
                        } break;
                    }
                }
                else
                {
                    AZ_Error("EditorEntityContext", false, "Failed to restore entity \"%s\" [%llu]",
                        request.m_entity->GetName().c_str(), request.m_entity->GetId());

                    if (request.m_restoreType == SliceEntityRestoreType::Deleted)
                    {
                        delete request.m_entity;
                    }
                }

                iter = m_queuedSliceEntityRestores.erase(iter);
            }
            else
            {
                ++iter;
            }
        }

        HandleEntitiesAdded(deletedEntitiesRestored);

        // Broadcast the created entity notification now after the restore is complete.
        for (auto deletedEntity : deletedEntitiesRestored)
        {
            EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntityCreated, deletedEntity->GetId());
        }

        if (!detachedEntitiesRestored.empty())
        {
            EditorEntityContextNotificationBus::Broadcast(&EditorEntityContextNotification::OnEditorEntitiesSliceOwnershipChanged, detachedEntitiesRestored);
        }
        // Pass on to base EntityContext.
        EntityContext::OnAssetReady(asset);
    }

    //=========================================================================
    // OnAssetReloaded - Root slice (or its dependents) has been reloaded.
    //=========================================================================
    void EditorEntityContextComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzToolsFramework);

        EntityIdList selectedEntities;
        ToolsApplicationRequests::Bus::BroadcastResult(selectedEntities, &ToolsApplicationRequests::GetSelectedEntities);

        EntityContext::OnAssetReloaded(asset);

        // Ensure selection set is preserved after applying the new level slice.
        ToolsApplicationRequests::Bus::Broadcast(&ToolsApplicationRequests::SetSelectedEntities, selectedEntities);
    }

    void EditorEntityContextComponent::OnStartGameModeRequest()
    {
        m_isRequestingGame = true;
    }

    void EditorEntityContextComponent::OnStopGameModeRequest()
    {
        m_isRequestingGame = false;
    }
} // namespace AzToolsFramework
