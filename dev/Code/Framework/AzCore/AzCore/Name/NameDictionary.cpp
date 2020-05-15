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

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Name/Internal/NameData.h>
#include <AzCore/std/hash.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/Module/Environment.h>
#include <cstring>

namespace AZ
{
    static const char* NameDictionaryInstanceName = "NameDictionaryInstance";

    namespace NameDictionaryInternal
    {
        static AZ::EnvironmentVariable<NameDictionary*> s_instance = nullptr;
    }

    void NameDictionary::Create(
#ifdef AZ_TESTS_ENABLED
        uint32_t maxUniqueHashes
#endif
    )
    {
        using namespace NameDictionaryInternal;

        AZ_Assert(!s_instance || !s_instance.Get(), "NameDictionary already created!");

        if (!s_instance)
        {
            s_instance = AZ::Environment::CreateVariable<NameDictionary*>(NameDictionaryInstanceName);
        }

        if (!s_instance.Get())
        {
            s_instance.Set(aznew NameDictionary());

#ifdef AZ_TESTS_ENABLED
            (*s_instance)->m_maxUniqueHashes = maxUniqueHashes;
#endif
        }
    }

    void NameDictionary::Destroy()
    {
        using namespace NameDictionaryInternal;

        AZ_Assert(s_instance, "NameDictionary not created!");
        delete (*s_instance);
        *s_instance = nullptr;
    }

    bool NameDictionary::IsReady()
    {
        using namespace NameDictionaryInternal;

        if (!s_instance)
        {
            s_instance = Environment::FindVariable<NameDictionary*>(NameDictionaryInstanceName);
        }

        return s_instance && *s_instance;
    }

    NameDictionary& NameDictionary::Instance()
    {
        using namespace NameDictionaryInternal;

        if (!s_instance)
        {
            s_instance = Environment::FindVariable<NameDictionary*>(NameDictionaryInstanceName);
        }

        AZ_Assert(s_instance && *s_instance, "NameDictionary has not been initialized yet.");

        return *(*s_instance);
    }
    
    NameDictionary::NameDictionary()
    {}

    NameDictionary::~NameDictionary()
    {
        for (const auto& keyValue : m_dictionary)
        {
            const int useCount = keyValue.second->m_useCount;
            AZ_TracePrintf("NameDictionary", "\tLeaked Name [%3d reference(s)]: hash 0x%08X, '%.*s'\n", useCount, keyValue.first, AZ_STRING_ARG(keyValue.second->GetName()));
        }

        AZ_Error("NameDictionary", m_dictionary.empty(),
            "AZ::NameDictionary still has active name references. See debug output for the list of leaked names.");
    }

    Name NameDictionary::FindName(Name::Hash hash) const
    {
        AZStd::shared_lock<AZStd::shared_mutex> lock(m_sharedMutex);
        auto iter = m_dictionary.find(hash);
        if (iter != m_dictionary.end())
        {
            return Name(iter->second);
        }
        return Name();
    }

    Name NameDictionary::MakeName(AZStd::string_view nameString)
    {
        // Null strings should return empty.
        if (nameString.empty())
        {
            return Name();
        }

        Name::Hash hash = MakeHash(nameString);

        // If we find the same name with the same hash, just return it. 
        // This path is faster than the loop below because FindName() takes a shared_lock whereas the
        // loop requires a unique_lock to modify the dictionary.
        Name name = FindName(hash);
        if (name.GetStringView() == nameString)
        {
            return AZStd::move(name);
        }

        // The name doesn't exist in the dictionary, so we have to lock and add it
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_sharedMutex);

        auto iter = m_dictionary.find(hash);
        while (true)
        {
            // No existing entry, add a new one and we're done
            if (iter == m_dictionary.end())
            {
                Internal::NameData* nameData = aznew Internal::NameData(nameString, hash);
                m_dictionary.emplace(hash, nameData);
                return Name(nameData);
            }
            // Found the desired entry, return it
            else if (iter->second->GetName() == nameString)
            {
                return Name(iter->second);
            }
            // Hash collision, try a new hash
            else
            {
                ++hash;
                iter = m_dictionary.find(hash);
            }
        }
    }

    void NameDictionary::TryReleaseName(Internal::NameData* nameData)
    {
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_sharedMutex);

        /**
         * need to check the count again in here in case
         * someone was trying to get the name on another thread
         * Set it to -1 so only this thread will attempt to clean up the
         * dictionary and delete the name.
         */

        int32_t expectedRefCount = 0;
        if (nameData->m_useCount.compare_exchange_strong(expectedRefCount, -1))
        {
            m_dictionary.erase(nameData->GetHash());
            delete nameData;
        }

        ReportStats();
    }

    void NameDictionary::ReportStats() const
    {
#ifdef AZ_DEBUG_BUILD

        // At some point it would be nice to formalize this stats output to be activated on demand by a runtime
        // feature, like a CVAR. But for now we can just activate it by breaking in the debugger.
        static bool reportUsage = false;
        if (reportUsage)
        {
            size_t potentialStringMemoryUsed = 0;
            size_t actualStringMemoryUsed = 0;

            Internal::NameData* longestName = nullptr;
            Internal::NameData* mostRepeatedName = nullptr;

            for (auto& iter : m_dictionary)
            {
                const size_t nameLength = iter.second->m_name.size();
                actualStringMemoryUsed += nameLength;
                potentialStringMemoryUsed += (nameLength * iter.second->m_useCount);

                if (!longestName || longestName->m_name.size() < nameLength)
                {
                    longestName = iter.second;
                }

                if (!mostRepeatedName)
                {
                    mostRepeatedName = iter.second;
                }
                else
                {
                    const size_t mostIndividualSavings = mostRepeatedName->m_name.size() * (mostRepeatedName->m_useCount - 1);
                    const size_t currentIndividualSavings = nameLength * (iter.second->m_useCount - 1);
                    if (currentIndividualSavings > mostIndividualSavings)
                    {
                        mostRepeatedName = iter.second;
                    }
                }
            }

            AZ_TracePrintf("NameDictionary", "NameDictionary Stats\n");
            AZ_TracePrintf("NameDictionary", "Names:              %d\n", m_dictionary.size());
            AZ_TracePrintf("NameDictionary", "Total chars:        %d\n", actualStringMemoryUsed);
            AZ_TracePrintf("NameDictionary", "Logical chars:      %d\n", potentialStringMemoryUsed);
            AZ_TracePrintf("NameDictionary", "Memory saved:       %d\n", potentialStringMemoryUsed - actualStringMemoryUsed);
            if (longestName)
            {
                AZ_TracePrintf("NameDictionary", "Longest name:       \"%.*s\"\n", aznumeric_cast<int>(longestName->m_name.size()), longestName->m_name.data());
                AZ_TracePrintf("NameDictionary", "Longest name size:  %d\n", longestName->m_name.size());
            }
            if (mostRepeatedName)
            {
                AZ_TracePrintf("NameDictionary", "Most repeated name:        \"%.*s\"\n", aznumeric_cast<int>(mostRepeatedName->m_name.size()), mostRepeatedName->m_name.data());
                AZ_TracePrintf("NameDictionary", "Most repeated name size:   %d\n", mostRepeatedName->m_name.size());
                const int refCount = mostRepeatedName->m_useCount.load();
                AZ_TracePrintf("NameDictionary", "Most repeated name count:  %d\n", refCount);
            }

            reportUsage = false;
        }

#endif // AZ_DEBUG_BUILD
    }

    Name::Hash NameDictionary::MakeHash(AZStd::string_view name)
    {
        // AZStd::hash<AZStd::string_view> returns 64 bits but we want 32 bit hashes for the sake
        // of network synchronization. So just take the low 32 bits.
        uint32_t hash = AZStd::hash<AZStd::string_view>()(name) & 0xFFFFFFFF;

#ifdef AZ_TESTS_ENABLED
        if(m_maxUniqueHashes < UINT32_MAX)
        {
            hash %= m_maxUniqueHashes;
            // Rather than use this modded value as the hash, we run a string hash again to
            // get spread-out hash values that are similar to the real ones, and avoid clustering.
            char buffer[16];
            azsnprintf(buffer, 16, "%u", hash);
            hash = AZStd::hash<AZStd::string_view>()(buffer) & 0xFFFFFFFF;
        }
#endif

        return hash;
    }
}