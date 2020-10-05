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

#include <Vegetation/InstanceSpawner.h>

namespace Vegetation
{
    /**
    * Empty instance spawner.  This can be used to spawn empty spaces.
    */
    class EmptyInstanceSpawner
        : public InstanceSpawner
    {
    public:
        AZ_RTTI(EmptyInstanceSpawner, "{23C40FD4-A55F-4BD3-BE5B-DC5423F217C2}", InstanceSpawner);
        AZ_CLASS_ALLOCATOR(EmptyInstanceSpawner, AZ::SystemAllocator, 0);
        static void Reflect(AZ::ReflectContext* context);

        EmptyInstanceSpawner() = default;
        virtual ~EmptyInstanceSpawner() = default;

        //! Start loading any assets that the spawner will need.
        void LoadAssets() override { NotifyOnAssetsLoaded(); }

        //! Unload any assets that the spawner loaded.
        void UnloadAssets() override { NotifyOnAssetsUnloaded(); }

        //! Perform any extra initialization needed at the point of registering with the vegetation system.
        void OnRegisterUniqueDescriptor() override {}

        //! Perform any extra cleanup needed at the point of unregistering with the vegetation system.
        void OnReleaseUniqueDescriptor() override {}

        /* Does this exist but have empty asset references?
         * This answer is used with the Vegetation Spawner setting "Allow empty meshes" to
         * determine whether or not this is allowed to spawn empty space.  So technically
         * while the answer could be "true" for EmptyInstanceSpawner, we specifically
         * return false so that it *always* spawns empty space, regardless of the Vegetation
         * Spawner setting.  That setting is intended to apply only to spawners that have
         * asset references that are unset, as opposed to a spawner that intentionally doesn't
         * have an asset reference at all.
         */
        bool HasEmptyAssetReferences() const override { return false; }

        //! Has this finished loading any assets that are needed?
        bool IsLoaded() const override { return true; }

        //! Are the assets loaded, initialized, and spawnable?
        bool IsSpawnable() const override { return true; }

        //! Display name of the instances that will be spawned.
        AZStd::string GetName() const override { return "<empty>"; }

        //! Create a single instance.
        //! Return non-null value so that it looks like a successful instance creation.
        InstancePtr CreateInstance(const InstanceData& instanceData) override { return this; }

        //! Destroy a single instance.
        void DestroyInstance(InstanceId id, InstancePtr instance) override {}

    private:
        bool DataIsEquivalent(const InstanceSpawner& rhs) const override;
    };

} // namespace Vegetation
