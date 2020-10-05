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
#include <Source/PythonCommon.h>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include "PythonTraceMessageSink.h"
#include "PythonTestingUtility.h"

#include <Source/PythonSystemComponent.h>
#include <Source/PythonReflectionComponent.h>
#include <Source/PythonProxyObject.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Casting/numeric_cast.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include <AzCore/Component/Entity.h>

namespace UnitTest
{
    //////////////////////////////////////////////////////////////////////////
    // test class/struts
    struct PythonReflectionObjectProxyPropertyTester
    {
        AZ_TYPE_INFO(PythonReflectionObjectProxyPropertyTester, "{F7966C89-7671-43F1-9DA2-677898DACED1}");

        float m_myFloat = 0.0f;
        AZ::s64 m_s64 = 0;

        float GetFloat() const
        {
            AZ_TracePrintf("python", "ReflectingObjectProxySimple_GetFloat");
            return m_myFloat;
        }

        void SetFloat(float value)
        {
            AZ_TracePrintf("python", "ReflectingObjectProxySimple_SetFloat");
            m_myFloat = value;
        }

        AZ::u16 PrintMessage(const char* message)
        {
            AZ_TracePrintf("python", message);
            return static_cast<AZ::u16>(::strlen(message));
        }

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<PythonReflectionObjectProxyPropertyTester>("TestObjectProxy")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Property("myFloat", [](PythonReflectionObjectProxyPropertyTester* that) { return that->GetFloat(); }, [](PythonReflectionObjectProxyPropertyTester* that, float value) { that->SetFloat(value); })
                    ->Property("mySignedInt64", [](PythonReflectionObjectProxyPropertyTester* that) { return that->m_s64; }, [](PythonReflectionObjectProxyPropertyTester* that, AZ::s64 value) { that->m_s64 = value; })
                    ->Property("s8", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::s8>(-8); }, nullptr)
                    ->Property("u8", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::u8>(8); }, nullptr)
                    ->Property("s16", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::s16>(-16); }, nullptr)
                    ->Property("u16", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::u16>(16); }, nullptr)
                    ->Property("s32", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::s32>(-32); }, nullptr)
                    ->Property("u32", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::u32>(32); }, nullptr)
                    ->Property("s64", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::s64>(-64); }, nullptr)
                    ->Property("u64", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<AZ::u64>(64); }, nullptr)
                    ->Property("f32", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<float>(32.0f); }, nullptr)
                    ->Property("d64", [](PythonReflectionObjectProxyPropertyTester* that) { return static_cast<double>(64.0); }, nullptr)
                    ->Method("printMessage", &PythonReflectionObjectProxyPropertyTester::PrintMessage)
                    ;
            }
        }
    };

    struct ClassWithOnlyConstructors final
    {
        AZ_TYPE_INFO(ClassWithOnlyConstructors, "{CF0EC37A-B846-4637-BFDC-15B29AE82D3E}");

        ClassWithOnlyConstructors() = default;
        ClassWithOnlyConstructors(int value) { m_value = value; }

        int m_value = 0;

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<ClassWithOnlyConstructors>("ClassWithOnlyConstructors")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Constructor()
                    ->Constructor<int>()
                    ;
            }
        }
    };

    struct ClassWithOnlyProperties
    {
        AZ_TYPE_INFO(ClassWithOnlyProperties, "{3D9FC921-6768-4456-A460-FE1BCF7D0155}");

        int m_value = 0;

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<ClassWithOnlyProperties>("ClassWithOnlyProperties")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Property("the_value", BehaviorValueProperty(&ClassWithOnlyProperties::m_value))
                    ;
            }
        }
    };

    struct PythonReflectionObjectProxyTester
    {
        AZ_TYPE_INFO(PythonReflectionObjectProxyTester, "{4FC01B6B-D738-46AD-BF74-6F72506DD9B1}");

        int DoAdd(int a, int b)
        {
            return a + b;
        }

        AZStd::string_view m_testString;
        AZStd::string m_testBuffer = "initial";
        AZ::s32 m_answer = 0;

        void SetBuffer(AZStd::string_view buffer)
        {
            m_testBuffer = buffer;
            AZ_TracePrintf("python", m_testBuffer.c_str());
        }

        AZStd::string_view GetBuffer() const
        {
            return AZStd::string_view{ m_testBuffer };
        }

        AZ::s32 GetAnswer() const
        {
            return m_answer;
        }

        void SetAnswer(AZ::s32 value)
        {
            m_answer = value;
        }

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<PythonReflectionObjectProxyTester>("TestObject")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Method("doAdd", &PythonReflectionObjectProxyTester::DoAdd)
                    ->Property("myString", [](PythonReflectionObjectProxyTester* that) { return that->m_testString; }, [](PythonReflectionObjectProxyTester* that, AZStd::string_view value) { that->m_testString = value; })
                    ->Property("theBuffer", &PythonReflectionObjectProxyTester::GetBuffer, &PythonReflectionObjectProxyTester::SetBuffer)
                    ->Method("GetAnswer", &PythonReflectionObjectProxyTester::GetAnswer)
                    ->Attribute(AZ::Script::Attributes::Alias, "get_answer")
                    ->Method("SetAnswer", &PythonReflectionObjectProxyTester::SetAnswer)
                    ->Attribute(AZ::Script::Attributes::Alias, "set_answer")
                    ->Property("Answer", &PythonReflectionObjectProxyTester::GetAnswer, &PythonReflectionObjectProxyTester::SetAnswer)
                    ->Attribute(AZ::Script::Attributes::Alias, "answer")
                    ;
            }
        }
    };

    struct EntityIdByValueTester
        : public AZ::EntityId 
    {
        AZ_TYPE_INFO(EntityIdByValueTester, "{DE8A9968-B6E1-49D1-86B4-8DC946AC3FC7}");
        AZ_CLASS_ALLOCATOR(EntityIdByValueTester, AZ::SystemAllocator, 0);

        EntityIdByValueTester() = default;

        explicit EntityIdByValueTester(AZ::u64 id)
            : AZ::EntityId(id)
        {}

        static void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<EntityIdByValueTester>("EntityIdByValueTester")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "entity")
                    ->Method("is_valid", &EntityId::IsValid)
                    ->Method("to_string", &EntityId::ToString)
                    ->Method("equal", &EntityId::operator==)
                    ;
            }
        }
    };

    class PythonObjectBaseTester
    {
    private:
        AZ::s64 m_value = -1;
        AZ::EntityId m_entityId;
        EntityIdByValueTester m_testerId;

    public:
        AZ_TYPE_INFO(PythonObjectBaseTester, "{99978809-BB9F-4915-81B2-E44DF0C59A9E}");
        AZ_CLASS_ALLOCATOR(PythonObjectBaseTester, AZ::SystemAllocator, 0);

        PythonObjectBaseTester()
        {
            m_entityId = AZ::EntityId(0xbabb1e);
            m_testerId = EntityIdByValueTester(0x1010);
        }

        void AcceptAzType(PythonObjectBaseTester* that)
        {
            AZ_TracePrintf("python", "this value:%d, that value:%d", m_value, that->m_value);
            m_value = that->m_value;
        }

        PythonObjectBaseTester* ResultTest(int value)
        {
            PythonObjectBaseTester* tester = aznew PythonObjectBaseTester{};
            tester->m_value = value;
            return tester;
        }

        void SetEntityId(const AZ::EntityId& value)
        {
            m_entityId = value;
            AZ_TracePrintf("python", "setting entity = %s", m_entityId.ToString().c_str() );
        }

        const AZ::EntityId& GetEntityId() const
        {
            return m_entityId;
        }

        EntityIdByValueTester GetEntityIdByValue()
        {
            return m_testerId;
        }

        bool CompareEntityIdByValue(EntityIdByValueTester entityId)
        {
            return m_testerId == entityId;
        }

        AZStd::vector<int> ReturnVectorByValue()
        {
            AZStd::vector<int> aTemp;
            aTemp.push_back(1);
            aTemp.push_back(2);
            aTemp.push_back(3);
            return aTemp;
        }

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<PythonObjectBaseTester>("TestObjectBase")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Property("value", [](PythonObjectBaseTester* that) { return that->m_value; }, [](PythonObjectBaseTester* that, AZ::s64 value) { that->m_value = value; })
                    ->Property("entityId", &PythonObjectBaseTester::GetEntityId, &PythonObjectBaseTester::SetEntityId)
                    ->Method("acceptAzType", &PythonObjectBaseTester::AcceptAzType)
                    ->Method("resultTest", &PythonObjectBaseTester::ResultTest)
                    ->Method("get_entity_id_by_value", &PythonObjectBaseTester::GetEntityIdByValue)
                    ->Method("compare_entity_id_by_value", &PythonObjectBaseTester::CompareEntityIdByValue)
                    ->Method("return_vector_by_value", &PythonObjectBaseTester::ReturnVectorByValue)
                    ;
            }
        }
    };

    class PythonObjectConstructionTester
    {
    private:
        AZ::s64 m_s64 = 0;
        AZStd::string m_text;

    public:
        AZ_TYPE_INFO(PythonObjectConstructionTester, "{35F7EE10-CA36-4F77-95B5-8001BA384E5A}");
        AZ_CLASS_ALLOCATOR(PythonObjectConstructionTester, AZ::SystemAllocator, 0);

        PythonObjectConstructionTester()
        {
            m_text = "default";
        }

        PythonObjectConstructionTester(const AZStd::string& textValue)
        {
            m_text = textValue;
        }

        PythonObjectConstructionTester(AZ::s64 longValue)
        {
            m_text = "with_int";
            m_s64 = longValue;
        }

        PythonObjectConstructionTester(const AZStd::string& textValue, AZ::s64 longValue)
        {
            m_text = textValue;
            m_s64 = longValue;
        }

        const AZStd::string& GetText() const
        {
            return m_text;
        }

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->Class<PythonObjectConstructionTester>("TestConstruct")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test")
                    ->Constructor<const AZStd::string&>()
                    ->Constructor<AZ::s64>()
                    ->Constructor<const AZStd::string&, AZ::s64>()
                    ->Property("s64", [](PythonObjectConstructionTester* that) { return that->m_s64; }, nullptr)
                    ->Property("text", &PythonObjectConstructionTester::GetText, nullptr)
                    ;
            }
        }
    };

    class PythonObjectLambdaTester
    {
    public:
        int m_myInt = 42;

    public:
        AZ_TYPE_INFO(PythonObjectLambdaTester, "{E423E0ED-038F-4496-97D3-00932289AF72}");
        AZ_CLASS_ALLOCATOR(PythonObjectLambdaTester, AZ::SystemAllocator, 0);

        void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                auto testLambda = [](PythonObjectLambdaTester* testerPtr) -> int { return testerPtr->m_myInt; };

                behaviorContext->Class<PythonObjectLambdaTester>("PythonObjectLambdaTester")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Automation)
                    ->Attribute(AZ::Script::Attributes::Module, "test.proxy")
                    ->Method("testLambda", testLambda)
                    ;
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // fixtures

    struct PythonObjectProxyTests
        : public PythonTestingFixture
    {
        PythonTraceMessageSink m_testSink;

        void SetUp() override
        {
            PythonTestingFixture::SetUp();
            PythonTestingFixture::RegisterComponentDescriptors();
        }

        void TearDown() override
        {
            // clearing up memory
            m_testSink = PythonTraceMessageSink();
            PythonTestingFixture::TearDown();
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // tests

    TEST_F(PythonObjectProxyTests, ObjectProxyProperties)
    {
        enum class LogTypes
        {
            Skip = 0,
            ReflectingObjectProxySimple_SetFloat,
            ReflectingObjectProxySimple_GetFloat,
            ReflectingObjectProxySimple_CreateTestObjectProxy,
            ReflectingObjectProxySimple_TestObjectProxyTypename,
            ReflectingObjectProxySimple_mySignedInt64,
            ReflectingObjectProxySimple_printedMessage
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_CreateTestObjectProxy"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_CreateTestObjectProxy);
                }
                else if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_GetFloat"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_GetFloat);
                }
                else if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_SetFloat"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_SetFloat);
                }
                else if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_TestObjectProxyTypename"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_TestObjectProxyTypename);
                }
                else if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_mySignedInt64"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_mySignedInt64);
                }
                else if (AzFramework::StringFunc::Equal(message, "ReflectingObjectProxySimple_printedMessage"))
                {
                    return static_cast<int>(LogTypes::ReflectingObjectProxySimple_printedMessage);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyPropertyTester pythonReflectionObjectProxyPropertyTester;
        pythonReflectionObjectProxyPropertyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObjectProxy')
                if proxy is not None:
                    print ('ReflectingObjectProxySimple_CreateTestObjectProxy')
                if proxy.typename == 'TestObjectProxy':
                    print ('ReflectingObjectProxySimple_TestObjectProxyTypename')
                proxy.set_property('myFloat', 20.19)
                value = proxy.get_property('myFloat')
                print ('ReflectingObjectProxySimple_{}'.format(value))
                # int64
                proxy.set_property('mySignedInt64', 729)
                value = proxy.get_property('mySignedInt64')
                if value == 729:
                    print ('ReflectingObjectProxySimple_mySignedInt64')
                value = proxy.invoke('printMessage', 'ReflectingObjectProxySimple_printedMessage')
                if (value == 42):
                    print ('ReflectingObjectProxySimple_printedMessage')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_CreateTestObjectProxy)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_TestObjectProxyTypename)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_GetFloat)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_SetFloat)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_mySignedInt64)]);
        EXPECT_EQ(2, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ReflectingObjectProxySimple_printedMessage)]);        
    }

    TEST_F(PythonObjectProxyTests, AsNativelyUsed)
    {
        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        EditorPythonBindings::PythonProxyObject obj(azrtti_typeid<PythonReflectionObjectProxyTester>());
        EXPECT_STREQ(obj.GetWrappedTypeName(), "TestObject");
        EXPECT_TRUE(obj.GetWrappedType().has_value());
        EXPECT_EQ(obj.GetWrappedType(), azrtti_typeid<PythonReflectionObjectProxyTester>());
    }

    TEST_F(PythonObjectProxyTests, OutputTypes)
    {
        enum class LogTypes
        {
            Skip = 0,
            OutputTypes_ReturnCheck,
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "OutputTypes_ReturnCheck"))
                {
                    return static_cast<int>(LogTypes::OutputTypes_ReturnCheck);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyPropertyTester pythonReflectionObjectProxyPropertyTester;
        pythonReflectionObjectProxyPropertyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObjectProxy')

                fValue = proxy.get_property('f32')
                if(fValue == 32.0):
                    print('OutputTypes_ReturnCheck')
        
                dValue = proxy.get_property('d64')
                if(dValue == 64.0):
                    print('OutputTypes_ReturnCheck')

                typeList = [8, 16, 32, 64]
                for typeValue in typeList:
                    signed = proxy.get_property('s{}'.format(str(typeValue)))
                    if( (-signed) == typeValue):
                        print('OutputTypes_ReturnCheck')

                    unsigned = proxy.get_property('u{}'.format(str(typeValue)))
                    if( unsigned == typeValue):
                        print('OutputTypes_ReturnCheck')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(10, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::OutputTypes_ReturnCheck)]);
    }

    TEST_F(PythonObjectProxyTests, ObjectProxyFeatures)
    {
        enum class LogTypes
        {
            Skip = 0,
            ObjectProxyFeatures_ChangeType,
            ObjectProxyFeatures_TestObjectProxyTypename,
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "ObjectProxyFeatures_ChangeType"))
                {
                    return static_cast<int>(LogTypes::ObjectProxyFeatures_ChangeType);
                }
                else if (AzFramework::StringFunc::Equal(message, "ObjectProxyFeatures_TestObjectProxyTypename"))
                {
                    return static_cast<int>(LogTypes::ObjectProxyFeatures_TestObjectProxyTypename);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyPropertyTester pythonReflectionObjectProxyPropertyTester;
        pythonReflectionObjectProxyPropertyTester.Reflect(m_app.GetBehaviorContext());

        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObjectProxy')
                if proxy.typename == 'TestObjectProxy':
                    print ('ObjectProxyFeatures_TestObjectProxyTypename')

                proxy.set_type('TestObject')
                value = proxy.invoke('doAdd', 2, 3)
                if (proxy.typename == 'TestObject') and (value == 5):
                    print ('ObjectProxyFeatures_ChangeType')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ObjectProxyFeatures_ChangeType)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ObjectProxyFeatures_TestObjectProxyTypename)]);
    }

    TEST_F(PythonObjectProxyTests, DecoratedObjectProxy)
    {
        enum class LogTypes
        {
            Skip = 0,
            DidAdd,
            PropertyIsFish
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "DidAdd"))
                {
                    return static_cast<int>(LogTypes::DidAdd);
                }
                else if (AzFramework::StringFunc::Equal(message, "PropertyIsFish"))
                {
                    return static_cast<int>(LogTypes::PropertyIsFish);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObject')
                value = proxy.doAdd(40, 2)
                if (value == 42):
                    print ('DidAdd')

                proxy.myString = 'fish'
                if (proxy.myString == 'fish'):
                    print ('PropertyIsFish')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::DidAdd)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertyIsFish)]);
    }

    TEST_F(PythonObjectProxyTests, DecoratedObjectProperties)
    {
        enum class LogTypes
        {
            Skip = 0,
            PropertyFetch,
            PropertySet,
            PropertyMatch,
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "PropertyFetch"))
                {
                    return static_cast<int>(LogTypes::PropertyFetch);
                }
                else if (AzFramework::StringFunc::Equal(message, "PropertySet"))
                {
                    return static_cast<int>(LogTypes::PropertySet);
                }
                else if (AzFramework::StringFunc::Equal(message, "PropertyMatch"))
                {
                    return static_cast<int>(LogTypes::PropertyMatch);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObject')
                value = proxy.theBuffer
                if (value == 'initial'):
                    print ('PropertyFetch')

                theMatchValue = 'PropertySet'
                proxy.theBuffer = 'PropertySet'
                if (proxy.theBuffer == theMatchValue):
                    print ('PropertyMatch')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertyFetch)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertySet)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertyMatch)]);
    }

    TEST_F(PythonObjectProxyTests, PythonicDecorations)
    {
        enum class LogTypes
        {
            Skip = 0,
            MethodGetAnswer,
            MethodSetAnswer,
            PropertyFetchAnswer,
            PropertyStoreAnswer
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "MethodGetAnswer"))
                {
                    return static_cast<int>(LogTypes::MethodGetAnswer);
                }
                else if (AzFramework::StringFunc::Equal(message, "MethodSetAnswer"))
                {
                    return static_cast<int>(LogTypes::MethodSetAnswer);
                }
                else if (AzFramework::StringFunc::Equal(message, "PropertyFetchAnswer"))
                {
                    return static_cast<int>(LogTypes::PropertyFetchAnswer);
                }
                else if (AzFramework::StringFunc::Equal(message, "PropertyStoreAnswer"))
                {
                    return static_cast<int>(LogTypes::PropertyStoreAnswer);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                proxy = azlmbr.object.create('TestObject')
                value = proxy.get_answer()
                if (value == 0):
                    print ('MethodGetAnswer')

                proxy.set_answer(40)
                value = proxy.get_answer()
                if (value == 40):
                    print ('MethodSetAnswer')

                if (proxy.answer == 40):
                    print ('PropertyFetchAnswer')

                proxy.answer = proxy.answer + 2
                if (proxy.answer == 42):
                    print ('PropertyStoreAnswer')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::MethodGetAnswer)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::MethodSetAnswer)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertyFetchAnswer)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PropertyStoreAnswer)]);
    }

    TEST_F(PythonObjectProxyTests, ObjectAzTypePassing)
    {
        enum class LogTypes
        {
            Skip = 0,
            ObjectAzTypePassing_Input,
            ObjectAzTypePassing_Output,
            ObjectAzTypePassing_EntityPassed
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "this value:22, that value:11"))
                {
                    return static_cast<int>(LogTypes::ObjectAzTypePassing_Input);
                }
                else if (AzFramework::StringFunc::Equal(message, "ObjectAzTypePassing_Output"))
                {
                    return static_cast<int>(LogTypes::ObjectAzTypePassing_Output);
                }
                else if (AzFramework::StringFunc::Equal(message, "setting entity = [12237598]"))
                {
                    return static_cast<int>(LogTypes::ObjectAzTypePassing_EntityPassed);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonObjectBaseTester pythonObjectBaseTester;
        pythonObjectBaseTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object

                payload = azlmbr.object.PythonProxyObject('TestObjectBase')
                payload.set_property('value', 11)

                target = azlmbr.object.PythonProxyObject('TestObjectBase')
                target.set_property('value', 22)
                target.invoke('acceptAzType', payload)

                result = target.invoke('resultTest', 33)
                if(result.get_property('value') == 33):
                    print ('ObjectAzTypePassing_Output')

                entityId = target.get_property('entityId')
                target.set_property('entityId', entityId)
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ObjectAzTypePassing_Input)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ObjectAzTypePassing_Output)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ObjectAzTypePassing_EntityPassed)]);        
    }

    TEST_F(PythonObjectProxyTests, ConstructWithArgs)
    {
        enum class LogTypes
        {
            Skip = 0,
            ConstructWithDefault,
            ConstructWithInt,
            ConstructWithString,
            ConstructWithStringAndInt,
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "ConstructWithDefault"))
                {
                    return static_cast<int>(LogTypes::ConstructWithDefault);
                }
                else if (AzFramework::StringFunc::Equal(message, "ConstructWithInt"))
                {
                    return static_cast<int>(LogTypes::ConstructWithInt);
                }
                else if (AzFramework::StringFunc::Equal(message, "ConstructWithString"))
                {
                    return static_cast<int>(LogTypes::ConstructWithString);
                }
                else if (AzFramework::StringFunc::Equal(message, "ConstructWithStringAndInt"))
                {
                    return static_cast<int>(LogTypes::ConstructWithStringAndInt);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonObjectConstructionTester pythonObjectConstructionTester;
        pythonObjectConstructionTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object

                defaultObj = azlmbr.object.construct('TestConstruct')
                if defaultObj.get_property('text') == 'default':
                    print ('ConstructWithDefault')

                defaultObj = azlmbr.object.construct('TestConstruct', 101)
                if defaultObj.get_property('text') == 'with_int':
                    print ('ConstructWithInt')

                defaultObj = azlmbr.object.construct('TestConstruct', 'with_string')
                if defaultObj.get_property('text') == 'with_string':
                    print ('ConstructWithString')

                defaultObj = azlmbr.object.construct('TestConstruct', 'foo', 201)
                if defaultObj.get_property('text') == 'foo':
                    print ('ConstructWithStringAndInt')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ConstructWithDefault)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ConstructWithInt)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ConstructWithString)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::ConstructWithStringAndInt)]);
    }

    TEST_F(PythonObjectProxyTests, PassByValue)
    {
        enum class LogTypes
        {
            Skip = 0,
            PassByValue_CreateReturnByValue,
            PassByValue_CallReturnByValue,
            PassByValue_InputByValue
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "PassByValue_CreateReturnByValue"))
                {
                    return static_cast<int>(LogTypes::PassByValue_CreateReturnByValue);
                }
                else if (AzFramework::StringFunc::Equal(message, "PassByValue_CallReturnByValue"))
                {
                    return static_cast<int>(LogTypes::PassByValue_CallReturnByValue);
                }
                else if (AzFramework::StringFunc::Equal(message, "PassByValue_InputByValue"))
                {
                    return static_cast<int>(LogTypes::PassByValue_InputByValue);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(m_app.GetSerializeContext()))
        {
            serializeContext->RegisterGenericType<AZStd::vector<int>>();
        }

        EntityIdByValueTester::Reflect(m_app.GetBehaviorContext());

        PythonObjectBaseTester pythonObjectBaseTester;
        pythonObjectBaseTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.entity
                import azlmbr.object
                import azlmbr.test.proxy

                proxyEntityId = azlmbr.object.create('EntityIdByValueTester')
                if proxyEntityId.typename == 'EntityIdByValueTester':
                    print ('PassByValue_CreateReturnByValue')

                testObjectBase = azlmbr.object.create('TestObjectBase')
                entityIdValue = testObjectBase.invoke('get_entity_id_by_value')
                if (entityIdValue.typename == 'EntityIdByValueTester'):
                    print ('PassByValue_CallReturnByValue')

                if (testObjectBase.invoke('compare_entity_id_by_value', entityIdValue)):
                    print ('PassByValue_InputByValue')

                intList = testObjectBase.invoke('return_vector_by_value')
                if (len(intList) == 3):
                    print ('PassByValue_CallReturnByValue')                    
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PassByValue_CreateReturnByValue)]);
        EXPECT_EQ(2, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PassByValue_CallReturnByValue)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[static_cast<int>(LogTypes::PassByValue_InputByValue)]);
    }

    TEST_F(PythonObjectProxyTests, CallLambdaAsMember)
    {
        enum class LogTypes
        {
            Skip = 0,
            PythonObjectLambdaTester_CreateObject,
            PythonObjectLambdaTester_InvokeLambda
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "PythonObjectLambdaTester_CreateObject"))
                {
                    return static_cast<int>(LogTypes::PythonObjectLambdaTester_CreateObject);
                }
                else if (AzFramework::StringFunc::Equal(message, "PythonObjectLambdaTester_InvokeLambda"))
                {
                    return static_cast<int>(LogTypes::PythonObjectLambdaTester_InvokeLambda);
                }
            }
            return static_cast<int>(LogTypes::Skip);
        };

        PythonObjectLambdaTester pythonObjectLambdaTester;
        pythonObjectLambdaTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                import azlmbr.test.proxy

                proxy = azlmbr.object.create('PythonObjectLambdaTester')
                if proxy is not None:
                    print ('PythonObjectLambdaTester_CreateObject')
                value = proxy.invoke('testLambda')
                if (value == 42):
                    print ('PythonObjectLambdaTester_InvokeLambda')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::PythonObjectLambdaTester_CreateObject)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::PythonObjectLambdaTester_InvokeLambda)]);
    }

    TEST_F(PythonObjectProxyTests, ObjectAsType)
    {
        enum class LogTypes
        {
            Skip = 0,
            Vector2_Non_None,
            Vector2_Constructed
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::Equal(message, "Vector2_Non_None"))
                {
                    return aznumeric_cast<int>(LogTypes::Vector2_Non_None);
                }
                else if (AzFramework::StringFunc::Equal(message, "Vector2_Constructed"))
                {
                    return aznumeric_cast<int>(LogTypes::Vector2_Constructed);
                }
            }
            return aznumeric_cast<int>(LogTypes::Skip);
        };

        PythonObjectLambdaTester pythonObjectLambdaTester;
        pythonObjectLambdaTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.math
                proxy = azlmbr.math.Vector2(40.0, 2.0)
                if proxy is not None:
                    print ('Vector2_Non_None')
                value = proxy.x + proxy.y
                if (azlmbr.math.Math_IsClose(value,  42.0)):
                    print ('Vector2_Constructed')
            )");
        }
        catch (const std::exception& e)
        {
            AZStd::string failReason = AZStd::string::format("Failed on with Python exception: %s", e.what());
            //AZ_Warning("UnitTest", false, );
            GTEST_FATAL_FAILURE_(failReason.c_str());
        }

        e.Deactivate();

        EXPECT_EQ(1, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::Vector2_Non_None)]);
        EXPECT_EQ(1, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::Vector2_Constructed)]);
    }

    TEST_F(PythonObjectProxyTests, Crc32Type)
    {
        enum class LogTypes
        {
            Skip = 0,
            Crc32Type_Created,
            Crc32Type_Read
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::StartsWith(message, "Crc32Type_Created"))
                {
                    return aznumeric_cast<int>(LogTypes::Crc32Type_Created);
                }
                else if (AzFramework::StringFunc::StartsWith(message, "Crc32Type_Read"))
                {
                    return aznumeric_cast<int>(LogTypes::Crc32Type_Read);
                }
            }
            return aznumeric_cast<int>(LogTypes::Skip);
        };

        PythonObjectBaseTester pythonObjectBaseTester;
        pythonObjectBaseTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);
        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.math

                result = azlmbr.math.Crc32()
                if result is not None:
                    print ('Crc32Type_Created_basic')

                result = azlmbr.math.Crc32('withstring')
                if result is not None:
                    print ('Crc32Type_Created_withstring')

                if (result.value == 3101708170): # CRC32 of withstring
                    print ('Crc32Type_Read_matches')

                if (azlmbr.math.Crc32('withstring').value == azlmbr.math.Crc32('WithString').value):
                    print ('Crc32Type_Read_matches_with_mixed_string_cases')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(2, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::Crc32Type_Created)]);
        EXPECT_EQ(2, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::Crc32Type_Read)]);
    }

    TEST_F(PythonObjectProxyTests, AlmostEmptyClasses)
    {
        enum class LogTypes
        {
            Skip = 0,
            FoundClassWithOnlyConstructors,
            FoundClassWithOnlyProperties
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::StartsWith(message, "FoundClassWithOnlyConstructors"))
                {
                    return aznumeric_cast<int>(LogTypes::FoundClassWithOnlyConstructors);
                }
                else if (AzFramework::StringFunc::StartsWith(message, "FoundClassWithOnlyProperties"))
                {
                    return aznumeric_cast<int>(LogTypes::FoundClassWithOnlyProperties);
                }
            }
            return aznumeric_cast<int>(LogTypes::Skip);
        };

        ClassWithOnlyConstructors classWithOnlyConstructors;
        classWithOnlyConstructors.Reflect(m_app.GetBehaviorContext());
        ClassWithOnlyProperties classWithOnlyProperties;
        classWithOnlyProperties.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);
        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.test.proxy

                instance = azlmbr.test.proxy.ClassWithOnlyConstructors()
                print ('FoundClassWithOnlyConstructors_created')

                instance = azlmbr.test.proxy.ClassWithOnlyConstructors(42)
                print ('FoundClassWithOnlyConstructors_created_with_value')

                instance = azlmbr.test.proxy.ClassWithOnlyProperties()
                print ('FoundClassWithOnlyProperties_created')
                instance.the_value = 42
                print ('FoundClassWithOnlyProperties_updated')
                if (instance.the_value == 42):
                    print ('FoundClassWithOnlyProperties_read')
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Error("UnitTest", false, "Failed on with Python exception: %s", e.what());
        }

        e.Deactivate();

        EXPECT_EQ(2, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::FoundClassWithOnlyConstructors)]);
        EXPECT_EQ(3, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::FoundClassWithOnlyProperties)]);
    }

    TEST_F(PythonObjectProxyTests, ObjectDirectory)
    {
        enum class LogTypes
        {
            Skip = 0,
            Found
        };

        m_testSink.m_evaluateMessage = [](const char* window, const char* message) -> int
        {
            if (AzFramework::StringFunc::Equal(window, "python"))
            {
                if (AzFramework::StringFunc::StartsWith(message, "Found"))
                {
                    return aznumeric_cast<int>(LogTypes::Found);
                }
            }
            return aznumeric_cast<int>(LogTypes::Skip);
        };

        PythonReflectionObjectProxyTester pythonReflectionObjectProxyTester;
        pythonReflectionObjectProxyTester.Reflect(m_app.GetBehaviorContext());

        AZ::Entity e;
        Activate(e);

        SimulateEditorBecomingInitialized();

        try
        {
            pybind11::exec(R"(
                import azlmbr.object
                import azlmbr.test.proxy

                proxyTest = azlmbr.test.proxy.TestObject()

                listOfAttributes = azlmbr.object.dir(proxyTest)
                if (listOfAttributes is not None):
                    print ('Found_list')

                for attribute in listOfAttributes:
                    if (attribute == 'doAdd'):
                        print ('Found_{}'.format(attribute))
                    elif (attribute == 'myString'):
                        print ('Found_{}'.format(attribute))
                    elif (attribute == 'theBuffer'):
                        print ('Found_{}'.format(attribute))
                    elif (attribute == 'get_answer'):
                        print ('Found_{}'.format(attribute))
                    elif (attribute == 'set_answer'):
                        print ('Found_{}'.format(attribute))
                    elif (attribute == 'answer'):
                        print ('Found_{}'.format(attribute))
            )");
        }
        catch (const std::exception& e)
        {
            AZ_Warning("UnitTest", false, "Failed on with Python exception: %s", e.what());
            FAIL();
        }

        e.Deactivate();

        EXPECT_EQ(7, m_testSink.m_evaluationMap[aznumeric_cast<int>(LogTypes::Found)]);
    }
}
