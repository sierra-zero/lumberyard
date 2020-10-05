/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <LmbrCentral_precompiled.h>
#include "DependencyBuilderWorker.h"

#include <AzFramework/StringFunc/StringFunc.h>

namespace DependencyBuilder
{
    DependencyBuilderWorker::DependencyBuilderWorker(AZStd::string jobKey, bool critical)
        : m_jobKey(jobKey)
        , m_critical(critical)
    {
    }

    void DependencyBuilderWorker::ShutDown()
    {
        m_isShuttingDown = true;
    }

    void DependencyBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        // Add source dependencies to CreateJobsResponse
        auto sourceDependencesResult = GetSourceDependencies(request);
        if (!sourceDependencesResult.IsSuccess())
        {
            AZ_Error(AssetBuilderSDK::ErrorWindow, false, sourceDependencesResult.TakeError().c_str());
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
            return;
        }
        response.m_sourceFileDependencyList = sourceDependencesResult.TakeValue();

        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
    }

    AZ::Outcome<AZStd::vector<AssetBuilderSDK::SourceFileDependency>, AZStd::string> DependencyBuilderWorker::GetSourceDependencies(
        const AssetBuilderSDK::CreateJobsRequest& /*request*/) const
    {
        return AZ::Success(AZStd::vector<AssetBuilderSDK::SourceFileDependency>{});
    }

    void DependencyBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "DependencyBuilderWorker Starting Job.\n");

        if (m_isShuttingDown)
        {
            AZ_TracePrintf(AssetBuilderSDK::WarningWindow, "Cancelled job %s because shutdown was requested.\n", request.m_fullPath.c_str());
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            return;
        }

        response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
    }

}
