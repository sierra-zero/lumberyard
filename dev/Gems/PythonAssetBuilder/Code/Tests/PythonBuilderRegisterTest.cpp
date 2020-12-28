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

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Component/ComponentApplication.h>

#include <EditorPythonBindings/EditorPythonBindingsSymbols.h>

#include "Source/PythonAssetBuilderSystemComponent.h"
#include <PythonAssetBuilder/PythonAssetBuilderBus.h>

namespace UnitTest
{
    class PythonBuilderCreateJobsTest
        : public ScopedAllocatorSetupFixture
    {
    protected:
        AZStd::unique_ptr<AZ::ComponentApplication> m_app;
        AZ::Entity* m_systemEntity = nullptr;

        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor appDesc;
            m_app = AZStd::make_unique<AZ::ComponentApplication>();
            m_systemEntity = m_app->Create(appDesc);
        }

        void TearDown() override
        {
            delete m_systemEntity;
            m_app.reset();
        }
    };

    TEST_F(PythonBuilderCreateJobsTest, PythonBuilder_RegisterBuilder_Regex)
    {
        using namespace PythonAssetBuilder;

        m_app->RegisterComponentDescriptor(PythonAssetBuilderSystemComponent::CreateDescriptor());
        m_systemEntity->CreateComponent<PythonAssetBuilderSystemComponent>();
        m_systemEntity->Init();
        m_systemEntity->Activate();

        AssetBuilderSDK::AssetBuilderPattern buildPattern;
        buildPattern.m_pattern = R"_(^.*\.foo$)_";
        buildPattern.m_type = AssetBuilderSDK::AssetBuilderPattern::Regex;

        AssetBuilderSDK::AssetBuilderDesc builderDesc;
        builderDesc.m_busId = AZ::Uuid::CreateRandom();
        builderDesc.m_name = "Mock Builder Regex";
        builderDesc.m_patterns.push_back(buildPattern);
        builderDesc.m_version = 0;

        AZ::Outcome<bool, AZStd::string> result;
        PythonAssetBuilderRequestBus::BroadcastResult(result, &PythonAssetBuilderRequestBus::Events::RegisterAssetBuilder, builderDesc);
        EXPECT_TRUE(result.IsSuccess());
    }

    TEST_F(PythonBuilderCreateJobsTest, PythonBuilder_RegisterBuilder_Wildcard)
    {
        using namespace PythonAssetBuilder;

        m_app->RegisterComponentDescriptor(PythonAssetBuilderSystemComponent::CreateDescriptor());
        m_systemEntity->CreateComponent<PythonAssetBuilderSystemComponent>();
        m_systemEntity->Init();
        m_systemEntity->Activate();

        AssetBuilderSDK::AssetBuilderPattern buildPattern;
        buildPattern.m_pattern = "a/path/to/*.foo";
        buildPattern.m_type = AssetBuilderSDK::AssetBuilderPattern::Wildcard;

        AssetBuilderSDK::AssetBuilderDesc builderDesc;
        builderDesc.m_busId = AZ::Uuid::CreateRandom();
        builderDesc.m_name = "Mock Builder Wildcard";
        builderDesc.m_patterns.push_back(buildPattern);
        builderDesc.m_version = 0;

        AZ::Outcome<bool, AZStd::string> result;
        PythonAssetBuilderRequestBus::BroadcastResult(result, &PythonAssetBuilderRequestBus::Events::RegisterAssetBuilder, builderDesc);
        EXPECT_TRUE(result.IsSuccess());
    }
}
