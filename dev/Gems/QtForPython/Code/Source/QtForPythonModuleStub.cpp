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

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Module/Module.h>

namespace QtForPython
{
    class QtForPythonModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(QtForPythonModule, "{81545CD5-79FA-47CE-96F2-1A9C5D59B4B9}", AZ::Module);
        AZ_CLASS_ALLOCATOR(QtForPythonModule, AZ::SystemAllocator, 0);

        QtForPythonModule()
            : AZ::Module()
        {
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(QtForPython_cd50c7a1e31f4c9495dcffdacc3bde92, QtForPython::QtForPythonModule)
