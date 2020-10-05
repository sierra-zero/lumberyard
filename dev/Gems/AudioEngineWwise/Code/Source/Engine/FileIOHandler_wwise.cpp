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

#include <FileIOHandler_wwise.h>
#include <AzCore/IO/Streamer.h>

#include <IAudioInterfacesCommonData.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AudioEngineWwise_Traits_Platform.h>

#include <platform.h>
#include <ICryPak.h>

#define MAX_NUMBER_STRING_SIZE      (10)    // 4G
#define ID_TO_STRING_FORMAT_BANK    AKTEXT("%u.bnk")
#define ID_TO_STRING_FORMAT_WEM     AKTEXT("%u.wem")
#define MAX_EXTENSION_SIZE          (4)     // .xxx
#define MAX_FILETITLE_SIZE          (MAX_NUMBER_STRING_SIZE + MAX_EXTENSION_SIZE + 1)   // null-terminated


namespace Audio
{
    // AkFileHandle must be able to store our AZ::IO::HandleType
    static_assert(sizeof(AkFileHandle) >= sizeof(AZ::IO::HandleType), "AkFileHandle must be able to store at least the size of a AZ::IO::HandleType");

    namespace Platform
    {
        AkFileHandle GetAkFileHandle(AZ::IO::HandleType realFileHandle);
        AZ::IO::HandleType GetRealFileHandle(AkFileHandle akFileHandle);

        void SetThreadProperties(AkThreadProperties& threadProperties);
    }

    AkFileHandle GetAkFileHandle(AZ::IO::HandleType realFileHandle)
    {
        if (realFileHandle == AZ::IO::InvalidHandle)
        {
            return InvalidAkFileHandle;
        }

        return Platform::GetAkFileHandle(realFileHandle);
    }

    AZ::IO::HandleType GetRealFileHandle(AkFileHandle akFileHandle)
    {
        if (akFileHandle == InvalidAkFileHandle)
        {
            return AZ::IO::InvalidHandle;
        }

        return Platform::GetRealFileHandle(akFileHandle);
    }

    CBlockingDevice_wwise::~CBlockingDevice_wwise()
    {
        Destroy();
    }

    bool CBlockingDevice_wwise::Init(size_t poolSize)
    {
        Destroy();

        AkDeviceSettings deviceSettings;
        AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
        deviceSettings.uIOMemorySize = poolSize;
        deviceSettings.uSchedulerTypeFlags = AK_SCHEDULER_BLOCKING;
        Platform::SetThreadProperties(deviceSettings.threadProperties);

        m_deviceID = AK::StreamMgr::CreateDevice(deviceSettings, this);
        return m_deviceID != AK_INVALID_DEVICE_ID;
    }

    void CBlockingDevice_wwise::Destroy()
    {
        if (m_deviceID != AK_INVALID_DEVICE_ID)
        {
            AK::StreamMgr::DestroyDevice(m_deviceID);
            m_deviceID = AK_INVALID_DEVICE_ID;
        }
    }

    bool CBlockingDevice_wwise::Open(const char* filename, AkOpenMode openMode, AkFileDesc& fileDesc)
    {
        const char* openModeString = nullptr;
        switch (openMode)
        {
        case AK_OpenModeRead:
            openModeString = "rbx";
            break;
        case AK_OpenModeWrite:
            openModeString = "wbx";
            break;
        case AK_OpenModeWriteOvrwr:
            openModeString = "w+bx";
            break;
        case AK_OpenModeReadWrite:
            openModeString = "abx";
            break;
        default:
            AZ_Assert(false, "Unknown Wwise file open mode.");
            return false;
        }

        const size_t fileSize = gEnv->pCryPak->FGetSize(filename);
        if (fileSize > 0)
        {
            AZ::IO::HandleType fileHandle = gEnv->pCryPak->FOpen(filename, openModeString, ICryPak::FOPEN_HINT_DIRECT_OPERATION);
            if (fileHandle != AZ::IO::InvalidHandle)
            {
                fileDesc.hFile = GetAkFileHandle(fileHandle);
                fileDesc.iFileSize = static_cast<AkInt64>(fileSize);
                fileDesc.uSector = 0;
                fileDesc.deviceID = m_deviceID;
                fileDesc.pCustomParam = nullptr;
                fileDesc.uCustomParamSize = 0;

                return true;
            }
        }

        return false;
    }

    AKRESULT CBlockingDevice_wwise::Read(AkFileDesc& fileDesc, const AkIoHeuristics&, void* buffer, AkIOTransferInfo& transferInfo)
    {
        AZ_Assert(buffer, "Wwise didn't provide a valid buffer to write to.");

        AZ::IO::HandleType fileHandle = GetRealFileHandle(fileDesc.hFile);
        const long currentFileReadPos = gEnv->pCryPak->FTell(fileHandle);
        const long wantedFileReadPos = static_cast<long>(transferInfo.uFilePosition);

        if (currentFileReadPos != wantedFileReadPos)
        {
            gEnv->pCryPak->FSeek(fileHandle, wantedFileReadPos, SEEK_SET);
        }

        const size_t bytesRead = gEnv->pCryPak->FReadRaw(buffer, 1, transferInfo.uRequestedSize, fileHandle);
        AZ_Assert(bytesRead == static_cast<size_t>(transferInfo.uRequestedSize), 
            "Number of bytes read (%zu) for Wwise request doesn't match the requested size (%u).", bytesRead, transferInfo.uRequestedSize);
        return (bytesRead > 0) ? AK_Success : AK_Fail;
    }

    AKRESULT CBlockingDevice_wwise::Write(AkFileDesc& fileDesc, const AkIoHeuristics&, void* data, AkIOTransferInfo& transferInfo)
    {
        AZ_Assert(data, "Wwise didn't provide a valid buffer to read from.");

        AZ::IO::HandleType fileHandle = GetRealFileHandle(fileDesc.hFile);

        const long currentFileWritePos = gEnv->pCryPak->FTell(fileHandle);
        const long wantedFileWritePos = static_cast<long>(transferInfo.uFilePosition);

        if (currentFileWritePos != wantedFileWritePos)
        {
            gEnv->pCryPak->FSeek(fileHandle, wantedFileWritePos, SEEK_SET);
        }

        const size_t bytesWritten = gEnv->pCryPak->FWrite(data, 1, static_cast<size_t>(transferInfo.uRequestedSize), fileHandle);
        if (bytesWritten != static_cast<size_t>(transferInfo.uRequestedSize))
        {
            AZ_Error("Wwise", false, "Number of bytes written (%zu) for Wwise request doesn't match the requested size (%u).",
                bytesWritten, transferInfo.uRequestedSize);
            return AK_Fail;
        }
        return AK_Success;
    }

    AKRESULT CBlockingDevice_wwise::Close(AkFileDesc& fileDesc)
    {
        return gEnv->pCryPak->FClose(GetRealFileHandle(fileDesc.hFile)) ? AK_Success : AK_Fail;
    }

    AkUInt32 CBlockingDevice_wwise::GetBlockSize(AkFileDesc& fileDesc)
    {
        // No constraint on block size (file seeking).
        return 1;
    }

    void CBlockingDevice_wwise::GetDeviceDesc(AkDeviceDesc& deviceDesc)
    {
        deviceDesc.bCanRead = true;
        deviceDesc.bCanWrite = true;
        deviceDesc.deviceID = m_deviceID;
        AK_CHAR_TO_UTF16(deviceDesc.szDeviceName, "CryPak", AZ_ARRAY_SIZE(deviceDesc.szDeviceName));
        deviceDesc.uStringSize = AKPLATFORM::AkUtf16StrLen(deviceDesc.szDeviceName);
    }
    
    AkUInt32 CBlockingDevice_wwise::GetDeviceData()
    {
        return 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    struct AsyncUserData
    {
        AZ_CLASS_ALLOCATOR(AsyncUserData, AZ::SystemAllocator, 0)
        AZStd::shared_ptr<AZ::IO::Request> m_request;
    };

    CStreamingDevice_wwise::~CStreamingDevice_wwise()
    {
        Destroy();
    }

    bool CStreamingDevice_wwise::Init(size_t poolSize)
    {
        Destroy();

        AkDeviceSettings deviceSettings;
        AK::StreamMgr::GetDefaultDeviceSettings(deviceSettings);
        deviceSettings.uIOMemorySize = poolSize;
        deviceSettings.uSchedulerTypeFlags = AK_SCHEDULER_DEFERRED_LINED_UP;
        Platform::SetThreadProperties(deviceSettings.threadProperties);

        m_deviceID = AK::StreamMgr::CreateDevice(deviceSettings, this);
        return m_deviceID != AK_INVALID_DEVICE_ID;
    }

    void CStreamingDevice_wwise::Destroy()
    {
        if (m_deviceID != AK_INVALID_DEVICE_ID)
        {
            AK::StreamMgr::DestroyDevice(m_deviceID);
            m_deviceID = AK_INVALID_DEVICE_ID;
        }
    }

    bool CStreamingDevice_wwise::Open(const char* filename, AkOpenMode openMode, AkFileDesc& fileDesc)
    {
        AZ_Assert(openMode == AK_OpenModeRead, "Wwise Async File IO - Only supports opening files for reading.\n");
        const size_t fileSize = gEnv->pCryPak->FGetSize(filename);
        if (fileSize)
        {
            fileDesc.hFile = AkFileHandle();
            fileDesc.iFileSize = static_cast<AkInt64>(fileSize);
            fileDesc.uSector = 0;
            fileDesc.deviceID = m_deviceID;
            fileDesc.pCustomParam = azcreate(AZStd::string, (filename));
            fileDesc.uCustomParamSize = sizeof(AZStd::string*);

            AZ::IO::Streamer::Instance().CreateDedicatedCache(filename);

            return true;
        }
        return false;
    }

    AKRESULT CStreamingDevice_wwise::Read(AkFileDesc& fileDesc, const AkIoHeuristics& heuristics, AkAsyncIOTransferInfo& transferInfo)
    {
        AZ_Assert(fileDesc.pCustomParam, "Wwise Async File IO - Reading a file before it has been opened.\n");

        auto callback = [&transferInfo](const AZStd::shared_ptr<AZ::IO::Request>& requestHandle, AZ::IO::Streamer::SizeType, void*, AZ::IO::Request::StateType state)
        {
            AZ_UNUSED(requestHandle);

            AZ_Assert(transferInfo.pUserData, "Wwise has already cleared the handle, which is unexpected.");
            AsyncUserData* request = reinterpret_cast<AsyncUserData*>(transferInfo.pUserData);
            AZ_Assert(request->m_request == requestHandle, "Handle provided in the callback from AZ::IO::Streamer is not the same Wwise used to queue the file read.");

            switch (state)
            {
            case AZ::IO::Request::StateType::ST_COMPLETED:
                [[fallthrough]];
            case AZ::IO::Request::StateType::ST_CANCELLED:
                transferInfo.pCallback(&transferInfo, AK_Success);
                delete request;
                break;
            default:
                AZ_Assert(false, "Unexpected state (%i) received from AZ::IO::Streamer.", state);
                [[fallthrough]];
            case AZ::IO::Request::StateType::ST_ERROR_FAILED_IN_OPERATION:
                [[fallthrough]];
            case AZ::IO::Request::StateType::ST_ERROR_FAILED_TO_OPEN_FILE:
                transferInfo.pCallback(&transferInfo, AK_Fail);
                delete request;
                break;
            }
        };

        // The priorities for AZ::IO::Streamer are not equally distributed, while Wwise does have a smooth range.
        // For instance the normal priority for AZ::IO::Streamer is option 3 of 4, while for Wwise it's 50 of 100.
        // The following mapping is used:
        // [ 0 - 14] Below normal
        // [15 - 64] Normal
        // [65 - 90] Above normal
        // [91 -100] Critical
        AZ::IO::Request::PriorityType priority = AZ::IO::Request::PriorityType::DR_PRIORITY_NORMAL;
        static_assert(AK_MIN_PRIORITY == 0, "The minimum priority for Wwise has changed, please update the conversion to AZ::IO::Streamers priority.");
        static_assert(AK_DEFAULT_PRIORITY == 50, "The default priority for Wwise has changed, please update the conversion to AZ::IO::Streamers priority.");
        static_assert(AK_MAX_PRIORITY == 100, "The maximum priority for Wwise has changed, please update the conversion to AZ::IO::Streamers priority.");
        if (heuristics.priority >= AK_MIN_PRIORITY && heuristics.priority < 15)
        {
            priority = AZ::IO::Request::PriorityType::DR_PRIORITY_BELOW_NORMAL;
        }
        else if (heuristics.priority < 65)
        {
            priority = AZ::IO::Request::PriorityType::DR_PRIORITY_NORMAL;
        }
        else if (heuristics.priority < 90)
        {
            priority = AZ::IO::Request::PriorityType::DR_PRIORITY_ABOVE_NORMAL;
        }
        else if (heuristics.priority <= AK_MAX_PRIORITY)
        {
            priority = AZ::IO::Request::PriorityType::DR_PRIORITY_CRITICAL;
        }
        else
        {
            AZ_Assert(false, "Wwise priority '%i' couldn't be mapped to a priority for AZ::IO::Streamer.", heuristics.priority);
            return AK_Fail;
        }

        auto filename = reinterpret_cast<AZStd::string*>(fileDesc.pCustomParam);
        auto offset = static_cast<AZ::IO::Streamer::SizeType>(transferInfo.uFilePosition);
        auto size = static_cast<AZ::IO::Streamer::SizeType>(transferInfo.uRequestedSize);
        AZStd::chrono::microseconds deadline = AZStd::chrono::duration<float, AZStd::milli>(heuristics.fDeadline);

        AZStd::shared_ptr<AZ::IO::Request> request = AZ::IO::Streamer::Instance().CreateAsyncRead(
            filename->c_str(), offset, size, transferInfo.pBuffer,
            callback, deadline, priority, "Wwise", false);
        if (!request)
        {
            return AK_Fail;
        }

        AsyncUserData* userData = aznew AsyncUserData();
        userData->m_request = request;
        transferInfo.pUserData = userData;

        return AZ::IO::Streamer::Instance().QueueRequest(AZStd::move(request)) ? AK_Success : AK_Fail;
    }

    AKRESULT CStreamingDevice_wwise::Write(AkFileDesc&, const AkIoHeuristics&, AkAsyncIOTransferInfo&)
    {
        AZ_Assert(false, "Wwise Async File IO - Writing audio data is not supported for AZ::IO::Streamer based device.\n");
        return AK_Fail;
    }

    AKRESULT CStreamingDevice_wwise::Close(AkFileDesc& fileDesc)
    {
        AZ_Assert(fileDesc.pCustomParam, "Wwise Async File IO - Closing a file before it has been opened.\n");
        auto filename = reinterpret_cast<AZStd::string*>(fileDesc.pCustomParam);
        AZ::IO::Streamer::Instance().DestroyDedicatedCache(filename->c_str());
        azdestroy(filename);
        return AK_Success;
    }

    AkUInt32 CStreamingDevice_wwise::GetBlockSize(AkFileDesc& fileDesc)
    {
        // No constraint on block size (file seeking).
        return 1;
    }

    void CStreamingDevice_wwise::GetDeviceDesc(AkDeviceDesc& deviceDesc)
    {
        deviceDesc.bCanRead = true;
        deviceDesc.bCanWrite = false;
        deviceDesc.deviceID = m_deviceID;
        AK_CHAR_TO_UTF16(deviceDesc.szDeviceName, "Streamer", AZ_ARRAY_SIZE(deviceDesc.szDeviceName));
        deviceDesc.uStringSize = AKPLATFORM::AkUtf16StrLen(deviceDesc.szDeviceName);
    }

    AkUInt32 CStreamingDevice_wwise::GetDeviceData()
    {
        return 2;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    CFileIOHandler_wwise::CFileIOHandler_wwise()
        : m_useAsyncOpen(false)
    {
        memset(m_bankPath, 0, AK_MAX_PATH * sizeof(AkOSChar));
        memset(m_languageFolder, 0, AK_MAX_PATH * sizeof(AkOSChar));
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    AKRESULT CFileIOHandler_wwise::Init(size_t poolSize)
    {
        // If the Stream Manager's File Location Resolver was not set yet, set this object as the
        // File Location Resolver (this I/O hook is also able to resolve file location).
        if (!AK::StreamMgr::GetFileLocationResolver())
        {
            AK::StreamMgr::SetFileLocationResolver(this);
        }

        if (!m_streamingDevice.Init(poolSize))
        {
            return AK_Fail;
        }

        if (!m_blockingDevice.Init(poolSize))
        {
            return AK_Fail;
        }
        return AK_Success;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CFileIOHandler_wwise::ShutDown()
    {
        if (AK::StreamMgr::GetFileLocationResolver() == this)
        {
            AK::StreamMgr::SetFileLocationResolver(nullptr);
        }

        m_blockingDevice.Destroy();
        m_streamingDevice.Destroy();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    AKRESULT CFileIOHandler_wwise::Open(const AkOSChar* fileName, AkOpenMode openMode, AkFileSystemFlags* flags, bool& syncOpen, AkFileDesc& fileDesc)
    {
        AKRESULT akResult = AK_Fail;

        if (syncOpen || !m_useAsyncOpen)
        {
            syncOpen = true;
            AkOSChar finalFilePath[AK_MAX_PATH] = { '\0' };
            AKPLATFORM::SafeStrCat(finalFilePath, m_bankPath, AK_MAX_PATH);

            if (flags && openMode == AK_OpenModeRead)
            {
                // Add language folder if the file is localized.
                if (flags->uCompanyID == AKCOMPANYID_AUDIOKINETIC && flags->uCodecID == AKCODECID_BANK && flags->bIsLanguageSpecific)
                {
                    AKPLATFORM::SafeStrCat(finalFilePath, m_languageFolder, AK_MAX_PATH);
                }
            }

            AKPLATFORM::SafeStrCat(finalFilePath, fileName, AK_MAX_PATH);

            char* tempStr = nullptr;
            CONVERT_OSCHAR_TO_CHAR(finalFilePath, tempStr);
            if (openMode == AK_OpenModeRead)
            {
                return m_streamingDevice.Open(tempStr, openMode, fileDesc) ? AK_Success : AK_Fail;
            }
            return m_blockingDevice.Open(tempStr, openMode, fileDesc) ? AK_Success : AK_Fail;
        }
        return akResult;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    AKRESULT CFileIOHandler_wwise::Open(AkFileID fileID, AkOpenMode openMode, AkFileSystemFlags* flags, bool& syncOpen, AkFileDesc& fileDesc)
    {
        AKRESULT akResult = AK_Fail;

        if (syncOpen || !m_useAsyncOpen)
        {
            syncOpen = true;
            AkOSChar finalFilePath[AK_MAX_PATH] = { '\0' };
            AKPLATFORM::SafeStrCat(finalFilePath, m_bankPath, AK_MAX_PATH);

            if (flags && openMode == AK_OpenModeRead)
            {
                // Add language folder if the file is localized.
                if (flags->uCompanyID == AKCOMPANYID_AUDIOKINETIC && flags->bIsLanguageSpecific)
                {
                    AKPLATFORM::SafeStrCat(finalFilePath, m_languageFolder, AK_MAX_PATH);
                }
            }

            AkOSChar fileName[MAX_FILETITLE_SIZE] = { '\0' };

            const AkOSChar* const filenameFormat = (flags->uCodecID == AKCODECID_BANK ? ID_TO_STRING_FORMAT_BANK : ID_TO_STRING_FORMAT_WEM);

            AK_OSPRINTF(fileName, MAX_FILETITLE_SIZE, filenameFormat, static_cast<int unsigned>(fileID));

            AKPLATFORM::SafeStrCat(finalFilePath, fileName, AK_MAX_PATH);

            char* tempStr = nullptr;
            CONVERT_OSCHAR_TO_CHAR(finalFilePath, tempStr);
            if (openMode == AK_OpenModeRead)
            {
                return m_streamingDevice.Open(tempStr, openMode, fileDesc) ? AK_Success : AK_Fail;
            }
            return m_blockingDevice.Open(tempStr, openMode, fileDesc) ? AK_Success : AK_Fail;
        }
        return akResult;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CFileIOHandler_wwise::SetBankPath(const char* const bankPath)
    {
        const AkOSChar* akBankPath = nullptr;
        CONVERT_CHAR_TO_OSCHAR(bankPath, akBankPath);
        AKPLATFORM::SafeStrCpy(m_bankPath, akBankPath, AK_MAX_PATH);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    void CFileIOHandler_wwise::SetLanguageFolder(const char* const languageFolder)
    {
        const AkOSChar* akLanguageFolder = nullptr;
        CONVERT_CHAR_TO_OSCHAR(languageFolder, akLanguageFolder);
        AKPLATFORM::SafeStrCpy(m_languageFolder, akLanguageFolder, AK_MAX_PATH);
    }

} // namespace Audio
