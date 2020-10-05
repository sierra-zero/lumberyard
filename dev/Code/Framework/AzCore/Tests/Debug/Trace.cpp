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

#include <AzCore/UnitTest/TestTypes.h>
#include <Console/Console.h>
#include <Console/IConsole.h>
#include <Interface/Interface.h>

using namespace AZ;

namespace UnitTest
{
    struct TraceTests
        : ScopedAllocatorSetupFixture
        , Debug::TraceMessageBus::Handler
    {
        bool OnPreAssert(const char*, int, const char*, const char*) override
        {
            m_assert = true;

            return true;
        }

        bool OnPreError(const char*, const char*, int, const char*, const char*) override
        {
            m_error = true;

            return true;
        }

        bool OnPreWarning(const char*, const char*, int, const char*, const char*) override
        {
            m_warning = true;

            return true;
        }

        bool OnPrintf(const char*, const char*) override
        {
            m_printf = true;

            return true;
        }

        void SetUp() override
        {
            BusConnect();

            if (!AZ::Interface<AZ::IConsole>::Get())
            {
                new Console;
                AZ::Interface<AZ::IConsole>::Get()->LinkDeferredFunctors(AZ::ConsoleFunctorBase::GetDeferredHead());
            }

            AZ_TEST_START_TRACE_SUPPRESSION;
        }

        void TearDown() override
        {
            AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;
            BusDisconnect();
        }

        bool m_assert = false;
        bool m_error = false;
        bool m_warning = false;
        bool m_printf = false;
    };

    TEST_F(TraceTests, Level0)
    {
        Interface<IConsole>::Get()->PerformCommand("bg_traceLogLevel 0");

        AZ_Assert(false, "test");
        AZ_Error("UnitTest", false, "test");
        AZ_Warning("UnitTest", false, "test");
        AZ_TracePrintf("UnitTest", "test");

        ASSERT_TRUE(m_assert);
        ASSERT_FALSE(m_error);
        ASSERT_FALSE(m_warning);
        ASSERT_FALSE(m_printf);
    }

    TEST_F(TraceTests, Level1)
    {
        Interface<IConsole>::Get()->PerformCommand("bg_traceLogLevel 1");

        AZ_Assert(false, "test");
        AZ_Error("UnitTest", false, "test");
        AZ_Warning("UnitTest", false, "test");
        AZ_TracePrintf("UnitTest", "test");

        ASSERT_TRUE(m_assert);
        ASSERT_TRUE(m_error);
        ASSERT_FALSE(m_warning);
        ASSERT_FALSE(m_printf);
    }

    TEST_F(TraceTests, Level2)
    {
        Interface<IConsole>::Get()->PerformCommand("bg_traceLogLevel 2");

        AZ_Assert(false, "test");
        AZ_Error("UnitTest", false, "test");
        AZ_Warning("UnitTest", false, "test");
        AZ_TracePrintf("UnitTest", "test");

        ASSERT_TRUE(m_assert);
        ASSERT_TRUE(m_error);
        ASSERT_TRUE(m_warning);
        ASSERT_FALSE(m_printf);
    }

    TEST_F(TraceTests, Level3)
    {
        Interface<IConsole>::Get()->PerformCommand("bg_traceLogLevel 3");

        AZ_Assert(false, "test");
        AZ_Error("UnitTest", false, "test");
        AZ_Warning("UnitTest", false, "test");
        AZ_TracePrintf("UnitTest", "test");

        ASSERT_TRUE(m_assert);
        ASSERT_TRUE(m_error);
        ASSERT_TRUE(m_warning);
        ASSERT_TRUE(m_printf);
    }
}
