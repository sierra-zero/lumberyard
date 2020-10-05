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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#pragma once

#include <AudioAllocators.h>
#include <FileIOHandler_wwise.h>
#include <ATLEntities_wwise.h>
#include <IAudioSystemImplementation.h>


namespace Audio
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    class CAudioSystemImpl_wwise
        : public AudioSystemImplementation
    {
    public:
        AUDIO_IMPL_CLASS_ALLOCATOR(CAudioSystemImpl_wwise)

        explicit CAudioSystemImpl_wwise(const char* assetsPlatformName);
        ~CAudioSystemImpl_wwise() override;

        // AudioSystemImplementationNotificationBus
        void OnAudioSystemLoseFocus() override;
        void OnAudioSystemGetFocus() override;
        void OnAudioSystemMuteAll() override;
        void OnAudioSystemUnmuteAll() override;
        void OnAudioSystemRefresh() override;
        // ~AudioSystemImplementationNotificationBus

        // AudioSystemImplementationRequestBus
        void Update(const float updateIntervalMS) override;

        EAudioRequestStatus Initialize() override;
        EAudioRequestStatus ShutDown() override;
        EAudioRequestStatus Release() override;

        EAudioRequestStatus StopAllSounds() override;

        EAudioRequestStatus RegisterAudioObject(
            IATLAudioObjectData* const audioObjectData,
            const char* const objectName) override;
        EAudioRequestStatus UnregisterAudioObject(IATLAudioObjectData* const audioObjectData) override;
        EAudioRequestStatus ResetAudioObject(IATLAudioObjectData* const audioObjectData) override;
        EAudioRequestStatus UpdateAudioObject(IATLAudioObjectData* const audioObjectData) override;

        EAudioRequestStatus PrepareTriggerSync(
            IATLAudioObjectData* const audioObjectData,
            const IATLTriggerImplData* const triggerData) override;
        EAudioRequestStatus UnprepareTriggerSync(
            IATLAudioObjectData* const audioObjectData,
            const IATLTriggerImplData* const triggerData) override;
        EAudioRequestStatus PrepareTriggerAsync(
            IATLAudioObjectData* const audioObjectData,
            const IATLTriggerImplData* const triggerData,
            IATLEventData* const eventData) override;
        EAudioRequestStatus UnprepareTriggerAsync(
            IATLAudioObjectData* const audioObjectData,
            const IATLTriggerImplData* const triggerData,
            IATLEventData* const eventData) override;
        EAudioRequestStatus ActivateTrigger(
            IATLAudioObjectData* const audioObjectData,
            const IATLTriggerImplData* const triggerData,
            IATLEventData* const eventData,
            const SATLSourceData* const pSourceData) override;
        EAudioRequestStatus StopEvent(
            IATLAudioObjectData* const audioObjectData,
            const IATLEventData* const eventData) override;
        EAudioRequestStatus StopAllEvents(
            IATLAudioObjectData* const audioObjectData) override;
        EAudioRequestStatus SetPosition(
            IATLAudioObjectData* const audioObjectData,
            const SATLWorldPosition& worldPosition) override;
        EAudioRequestStatus SetMultiplePositions(
            IATLAudioObjectData* const audioObjectData,
            const MultiPositionParams& multiPositionParams) override;
        EAudioRequestStatus SetEnvironment(
            IATLAudioObjectData* const audioObjectData,
            const IATLEnvironmentImplData* const environmentData,
            const float amount) override;
        EAudioRequestStatus SetRtpc(
            IATLAudioObjectData* const audioObjectData,
            const IATLRtpcImplData* const rtpcData,
            const float value) override;
        EAudioRequestStatus SetSwitchState(
            IATLAudioObjectData* const audioObjectData,
            const IATLSwitchStateImplData* const switchStateData) override;
        EAudioRequestStatus SetObstructionOcclusion(
            IATLAudioObjectData* const audioObjectData,
            const float obstruction,
            const float occlusion) override;
        EAudioRequestStatus SetListenerPosition(
            IATLListenerData* const listenerData,
            const SATLWorldPosition& newPosition) override;
        EAudioRequestStatus ResetRtpc(
            IATLAudioObjectData* const audioObjectData,
            const IATLRtpcImplData* const rtpcData) override;

        EAudioRequestStatus RegisterInMemoryFile(SATLAudioFileEntryInfo* const audioFileEntry) override;
        EAudioRequestStatus UnregisterInMemoryFile(SATLAudioFileEntryInfo* const audioFileEntry) override;

        EAudioRequestStatus ParseAudioFileEntry(const AZ::rapidxml::xml_node<char>* audioFileEntryNode, SATLAudioFileEntryInfo* const fileEntryInfo) override;
        void DeleteAudioFileEntryData(IATLAudioFileEntryData* const oldAudioFileEntryData) override;
        const char* const GetAudioFileLocation(SATLAudioFileEntryInfo* const fileEntryInfo) override;

        IATLTriggerImplData* NewAudioTriggerImplData(const AZ::rapidxml::xml_node<char>* audioTriggerNode) override;
        void DeleteAudioTriggerImplData(IATLTriggerImplData* const oldTriggerImplData) override;

        IATLRtpcImplData* NewAudioRtpcImplData(const AZ::rapidxml::xml_node<char>* audioRtpcNode) override;
        void DeleteAudioRtpcImplData(IATLRtpcImplData* const oldRtpcImplData) override;

        IATLSwitchStateImplData* NewAudioSwitchStateImplData(const AZ::rapidxml::xml_node<char>* audioSwitchStateNode) override;
        void DeleteAudioSwitchStateImplData(IATLSwitchStateImplData* const oldSwitchStateImplData) override;

        IATLEnvironmentImplData* NewAudioEnvironmentImplData(const AZ::rapidxml::xml_node<char>* audioEnvironmentNode) override;
        void DeleteAudioEnvironmentImplData(IATLEnvironmentImplData* const oldEnvironmentImplData) override;

        SATLAudioObjectData_wwise* NewGlobalAudioObjectData(const TAudioObjectID objectId) override;
        SATLAudioObjectData_wwise* NewAudioObjectData(const TAudioObjectID objectId) override;
        void DeleteAudioObjectData(IATLAudioObjectData* const oldObjectData) override;

        SATLListenerData_wwise* NewDefaultAudioListenerObjectData(const TATLIDType objectId) override;
        SATLListenerData_wwise* NewAudioListenerObjectData(const TATLIDType objectId) override;
        void DeleteAudioListenerObjectData(IATLListenerData* const oldListenerData) override;

        SATLEventData_wwise* NewAudioEventData(const TAudioEventID eventId) override;
        void DeleteAudioEventData(IATLEventData* const oldEventData) override;
        void ResetAudioEventData(IATLEventData* const eventData) override;

        const char* const GetImplSubPath() const override;
        void SetLanguage(const char* const language) override;

        // Below data is only used when INCLUDE_WWISE_IMPL_PRODUCTION_CODE is defined!
        const char* const GetImplementationNameString() const override;
        void GetMemoryInfo(SAudioImplMemoryInfo& memoryInfo) const override;
        AZStd::vector<AudioImplMemoryPoolInfo> GetMemoryPoolInfo() override;

        bool CreateAudioSource(const SAudioInputConfig& sourceConfig) override;
        void DestroyAudioSource(TAudioSourceId sourceId) override;

        void SetPanningMode(PanningMode mode) override;
        // ~AudioSystemImplementationRequestBus

    protected:
        void SetBankPaths();

        AZStd::string m_soundbankFolder;
        AZStd::string m_localizedSoundbankFolder;
        AZStd::string m_assetsPlatform;

    private:
        static const char* const WwiseImplSubPath;
        static const char* const WwiseGlobalAudioObjectName;
        static const float ObstructionOcclusionMin;
        static const float ObstructionOcclusionMax;

        struct SEnvPairCompare
        {
            bool operator()(const AZStd::pair<const AkAuxBusID, float>& pair1, const AZStd::pair<const AkAuxBusID, float>& pair2) const;
        };

        SATLSwitchStateImplData_wwise* ParseWwiseSwitchOrState(const AZ::rapidxml::xml_node<char>* node, EWwiseSwitchType type);
        SATLSwitchStateImplData_wwise* ParseWwiseRtpcSwitch(const AZ::rapidxml::xml_node<char>* node);
        void ParseRtpcImpl(const AZ::rapidxml::xml_node<char>* node, AkRtpcID& akRtpcId, float& mult, float& shift);

        EAudioRequestStatus PrepUnprepTriggerSync(
            const IATLTriggerImplData* const triggerData,
            bool prepare);
        EAudioRequestStatus PrepUnprepTriggerAsync(
            const IATLTriggerImplData* const triggerData,
            IATLEventData* const eventData,
            bool prepare);

        EAudioRequestStatus PostEnvironmentAmounts(IATLAudioObjectData* const audioObjectData);

        AkGameObjectID m_globalGameObjectID;
        AkGameObjectID m_defaultListenerGameObjectID;
        AkBankID m_initBankID;
        CFileIOHandler_wwise m_fileIOHandler;

#if !defined(WWISE_FOR_RELEASE)
        bool m_isCommSystemInitialized;
#endif // !WWISE_FOR_RELEASE

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
        AZStd::vector<AudioImplMemoryPoolInfo> m_debugMemoryPoolInfo;
        AZStd::string m_fullImplString;
        AZStd::string m_speakerConfigString;
#endif // INCLUDE_WWISE_IMPL_PRODUCTION_CODE
    };
} // namespace Audio
