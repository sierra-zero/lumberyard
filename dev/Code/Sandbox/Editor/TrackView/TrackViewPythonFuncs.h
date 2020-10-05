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

#include <AzCore/Component/Component.h>
#include "EditorTrackViewEventsBus.h"

namespace AzToolsFramework
{
    //! A legacy component to reflect scriptable commands for the Editor
    class TrackViewFuncsHandler
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(TrackViewFuncsHandler, "{5315678D-2951-4CF6-A9DC-CE21CD23C9C9}")

        static void Reflect(AZ::ReflectContext* context);

        // AZ::Component ...
        void Activate() override {}
        void Deactivate() override {}
    };

    //! Component to access the TrackView
    class TrackViewComponent final
        : public AZ::Component
        , public EditorLayerTrackViewRequestBus::Handler
    {
    public:
        AZ_COMPONENT(TrackViewComponent, "{3CF943CC-6F10-4B19-88FC-CFB697558FFD}")

        TrackViewComponent() = default;
        ~TrackViewComponent() override = default;

        static void Reflect(AZ::ReflectContext* context);

        // Component...
        void Activate() override;
        void Deactivate() override;

        int GetNumSequences() override;

        void NewSequence(const char* name, int sequenceType) override;

        void PlaySequence() override;

        void StopSequence() override;

        void SetSequenceTime(float time) override;

        void AddSelectedEntities() override;

        void AddLayerNode() override;

        void AddTrack(const char* paramName, const char* nodeName, const char* parentDirectorName) override;

        void DeleteTrack(const char* paramName, uint32 index, const char* nodeName, const char* parentDirectorName) override;

        int GetNumTrackKeys(const char* paramName, int trackIndex, const char* nodeName, const char* parentDirectorName) override;

        void SetRecording(bool bRecording) override;

        void DeleteSequence(const char* name) override;

        void SetCurrentSequence(const char* name) override;

        AZStd::string GetSequenceName(unsigned int index) override;

        Range GetSequenceTimeRange(const char* name) override;

        void AddNode(const char* nodeTypeString, const char* nodeName) override;

        void DeleteNode(AZStd::string_view nodeName, AZStd::string_view parentDirectorName) override;

        int GetNumNodes(AZStd::string_view parentDirectorName) override;

        AZStd::string GetNodeName(int index, AZStd::string_view parentDirectorName) override;

        AZStd::any GetKeyValue(const char* paramName, int trackIndex, int keyIndex, const char* nodeName, const char* parentDirectorName) override;

        AZStd::any GetInterpolatedValue(const char* paramName, int trackIndex, float time, const char* nodeName, const char* parentDirectorName) override;

        void SetSequenceTimeRange(const char* name, float start, float end) override;

    };

} // namespace AzToolsFramework
