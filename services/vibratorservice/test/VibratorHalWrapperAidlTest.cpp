/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *            http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "VibratorHalWrapperAidlTest"

#include <android/hardware/vibrator/IVibrator.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utils/Log.h>

#include <vibratorservice/VibratorHalWrapper.h>

#include "test_utils.h"

using android::binder::Status;

using android::hardware::vibrator::CompositeEffect;
using android::hardware::vibrator::CompositePrimitive;
using android::hardware::vibrator::Effect;
using android::hardware::vibrator::EffectStrength;
using android::hardware::vibrator::IVibrator;
using android::hardware::vibrator::IVibratorCallback;

using namespace android;
using namespace std::chrono_literals;
using namespace testing;

// -------------------------------------------------------------------------------------------------

class MockBinder : public BBinder {
public:
    MOCK_METHOD(status_t, linkToDeath,
                (const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags), (override));
    MOCK_METHOD(status_t, unlinkToDeath,
                (const wp<DeathRecipient>& recipient, void* cookie, uint32_t flags,
                 wp<DeathRecipient>* outRecipient),
                (override));
    MOCK_METHOD(status_t, pingBinder, (), (override));
};

class MockIVibrator : public IVibrator {
public:
    MOCK_METHOD(Status, getCapabilities, (int32_t * ret), (override));
    MOCK_METHOD(Status, off, (), (override));
    MOCK_METHOD(Status, on, (int32_t timeout, const sp<IVibratorCallback>& cb), (override));
    MOCK_METHOD(Status, perform,
                (Effect e, EffectStrength s, const sp<IVibratorCallback>& cb, int32_t* ret),
                (override));
    MOCK_METHOD(Status, getSupportedEffects, (std::vector<Effect> * ret), (override));
    MOCK_METHOD(Status, setAmplitude, (float amplitude), (override));
    MOCK_METHOD(Status, setExternalControl, (bool enabled), (override));
    MOCK_METHOD(Status, getCompositionDelayMax, (int32_t * ret), (override));
    MOCK_METHOD(Status, getCompositionSizeMax, (int32_t * ret), (override));
    MOCK_METHOD(Status, getSupportedPrimitives, (std::vector<CompositePrimitive> * ret),
                (override));
    MOCK_METHOD(Status, getPrimitiveDuration, (CompositePrimitive p, int32_t* ret), (override));
    MOCK_METHOD(Status, compose,
                (const std::vector<CompositeEffect>& e, const sp<IVibratorCallback>& cb),
                (override));
    MOCK_METHOD(Status, getSupportedAlwaysOnEffects, (std::vector<Effect> * ret), (override));
    MOCK_METHOD(Status, alwaysOnEnable, (int32_t id, Effect e, EffectStrength s), (override));
    MOCK_METHOD(Status, alwaysOnDisable, (int32_t id), (override));
    MOCK_METHOD(int32_t, getInterfaceVersion, (), (override));
    MOCK_METHOD(std::string, getInterfaceHash, (), (override));
    MOCK_METHOD(IBinder*, onAsBinder, (), (override));
};

// -------------------------------------------------------------------------------------------------

class VibratorHalWrapperAidlTest : public Test {
public:
    void SetUp() override {
        mMockBinder = new StrictMock<MockBinder>();
        mMockHal = new StrictMock<MockIVibrator>();
        mWrapper = std::make_unique<vibrator::AidlHalWrapper>(mMockHal);
        ASSERT_NE(mWrapper, nullptr);
    }

protected:
    std::unique_ptr<vibrator::HalWrapper> mWrapper = nullptr;
    sp<StrictMock<MockIVibrator>> mMockHal = nullptr;
    sp<StrictMock<MockBinder>> mMockBinder = nullptr;
};

// -------------------------------------------------------------------------------------------------

ACTION(TriggerCallbackInArg1) {
    if (arg1 != nullptr) {
        arg1->onComplete();
    }
}

ACTION(TriggerCallbackInArg2) {
    if (arg2 != nullptr) {
        arg2->onComplete();
    }
}

TEST_F(VibratorHalWrapperAidlTest, TestPing) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), onAsBinder())
                .Times(Exactly(1))
                .WillRepeatedly(Return(mMockBinder.get()));
        EXPECT_CALL(*mMockBinder.get(), pingBinder()).Times(Exactly(1));
    }

    ASSERT_TRUE(mWrapper->ping().isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestOn) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), on(Eq(10), _))
                .Times(Exactly(1))
                .WillRepeatedly(DoAll(TriggerCallbackInArg1(), Return(Status())));
        EXPECT_CALL(*mMockHal.get(), on(Eq(100), _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(), on(Eq(1000), _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    std::unique_ptr<int32_t> callbackCounter = std::make_unique<int32_t>();
    auto callback = vibrator::TestFactory::createCountingCallback(callbackCounter.get());

    ASSERT_TRUE(mWrapper->on(10ms, callback).isOk());
    ASSERT_EQ(1, *callbackCounter.get());

    ASSERT_TRUE(mWrapper->on(100ms, callback).isUnsupported());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());

    ASSERT_TRUE(mWrapper->on(1000ms, callback).isFailed());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());
}

TEST_F(VibratorHalWrapperAidlTest, TestOff) {
    EXPECT_CALL(*mMockHal.get(), off())
            .Times(Exactly(3))
            .WillOnce(Return(Status()))
            .WillOnce(
                    Return(Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)))
            .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));

    ASSERT_TRUE(mWrapper->off().isOk());
    ASSERT_TRUE(mWrapper->off().isUnsupported());
    ASSERT_TRUE(mWrapper->off().isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestSetAmplitude) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), setAmplitude(FloatNear(0.1, 1e-2))).Times(Exactly(1));
        EXPECT_CALL(*mMockHal.get(), setAmplitude(FloatNear(0.2, 1e-2)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(), setAmplitude(FloatNear(0.5, 1e-2)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    ASSERT_TRUE(mWrapper->setAmplitude(std::numeric_limits<uint8_t>::max() / 10).isOk());
    ASSERT_TRUE(mWrapper->setAmplitude(std::numeric_limits<uint8_t>::max() / 5).isUnsupported());
    ASSERT_TRUE(mWrapper->setAmplitude(std::numeric_limits<uint8_t>::max() / 2).isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestSetExternalControl) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), setExternalControl(Eq(true))).Times(Exactly(1));
        EXPECT_CALL(*mMockHal.get(), setExternalControl(Eq(false)))
                .Times(Exactly(2))
                .WillOnce(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    ASSERT_TRUE(mWrapper->setExternalControl(true).isOk());
    ASSERT_TRUE(mWrapper->setExternalControl(false).isUnsupported());
    ASSERT_TRUE(mWrapper->setExternalControl(false).isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestAlwaysOnEnable) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(),
                    alwaysOnEnable(Eq(1), Eq(Effect::CLICK), Eq(EffectStrength::LIGHT)))
                .Times(Exactly(1));
        EXPECT_CALL(*mMockHal.get(),
                    alwaysOnEnable(Eq(2), Eq(Effect::TICK), Eq(EffectStrength::MEDIUM)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(),
                    alwaysOnEnable(Eq(3), Eq(Effect::POP), Eq(EffectStrength::STRONG)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    auto result = mWrapper->alwaysOnEnable(1, Effect::CLICK, EffectStrength::LIGHT);
    ASSERT_TRUE(result.isOk());
    result = mWrapper->alwaysOnEnable(2, Effect::TICK, EffectStrength::MEDIUM);
    ASSERT_TRUE(result.isUnsupported());
    result = mWrapper->alwaysOnEnable(3, Effect::POP, EffectStrength::STRONG);
    ASSERT_TRUE(result.isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestAlwaysOnDisable) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), alwaysOnDisable(Eq(1))).Times(Exactly(1));
        EXPECT_CALL(*mMockHal.get(), alwaysOnDisable(Eq(2)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(), alwaysOnDisable(Eq(3)))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    ASSERT_TRUE(mWrapper->alwaysOnDisable(1).isOk());
    ASSERT_TRUE(mWrapper->alwaysOnDisable(2).isUnsupported());
    ASSERT_TRUE(mWrapper->alwaysOnDisable(3).isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestGetCapabilities) {
    EXPECT_CALL(*mMockHal.get(), getCapabilities(_))
            .Times(Exactly(3))
            .WillOnce(DoAll(SetArgPointee<0>(IVibrator::CAP_ON_CALLBACK), Return(Status())))
            .WillOnce(
                    Return(Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)))
            .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));

    auto result = mWrapper->getCapabilities();
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(vibrator::Capabilities::ON_CALLBACK, result.value());
    ASSERT_TRUE(mWrapper->getCapabilities().isUnsupported());
    ASSERT_TRUE(mWrapper->getCapabilities().isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestGetSupportedEffects) {
    std::vector<Effect> supportedEffects;
    supportedEffects.push_back(Effect::CLICK);
    supportedEffects.push_back(Effect::TICK);

    EXPECT_CALL(*mMockHal.get(), getSupportedEffects(_))
            .Times(Exactly(3))
            .WillOnce(DoAll(SetArgPointee<0>(supportedEffects), Return(Status())))
            .WillOnce(
                    Return(Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)))
            .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));

    auto result = mWrapper->getSupportedEffects();
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(supportedEffects, result.value());
    ASSERT_TRUE(mWrapper->getSupportedEffects().isUnsupported());
    ASSERT_TRUE(mWrapper->getSupportedEffects().isFailed());
}

TEST_F(VibratorHalWrapperAidlTest, TestPerformEffect) {
    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), perform(Eq(Effect::CLICK), Eq(EffectStrength::LIGHT), _, _))
                .Times(Exactly(1))
                .WillRepeatedly(
                        DoAll(SetArgPointee<3>(1000), TriggerCallbackInArg2(), Return(Status())));
        EXPECT_CALL(*mMockHal.get(), perform(Eq(Effect::POP), Eq(EffectStrength::MEDIUM), _, _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(), perform(Eq(Effect::THUD), Eq(EffectStrength::STRONG), _, _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    std::unique_ptr<int32_t> callbackCounter = std::make_unique<int32_t>();
    auto callback = vibrator::TestFactory::createCountingCallback(callbackCounter.get());

    auto result = mWrapper->performEffect(Effect::CLICK, EffectStrength::LIGHT, callback);
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(1000ms, result.value());
    ASSERT_EQ(1, *callbackCounter.get());

    result = mWrapper->performEffect(Effect::POP, EffectStrength::MEDIUM, callback);
    ASSERT_TRUE(result.isUnsupported());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());

    result = mWrapper->performEffect(Effect::THUD, EffectStrength::STRONG, callback);
    ASSERT_TRUE(result.isFailed());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());
}

TEST_F(VibratorHalWrapperAidlTest, TestPerformComposedEffect) {
    std::vector<CompositeEffect> emptyEffects, singleEffect, multipleEffects;
    singleEffect.push_back(
            vibrator::TestFactory::createCompositeEffect(CompositePrimitive::CLICK, 10ms, 0.0f));
    multipleEffects.push_back(
            vibrator::TestFactory::createCompositeEffect(CompositePrimitive::SPIN, 100ms, 0.5f));
    multipleEffects.push_back(
            vibrator::TestFactory::createCompositeEffect(CompositePrimitive::THUD, 1000ms, 1.0f));

    {
        InSequence seq;
        EXPECT_CALL(*mMockHal.get(), compose(Eq(emptyEffects), _))
                .Times(Exactly(1))
                .WillRepeatedly(DoAll(TriggerCallbackInArg1(), Return(Status())));
        EXPECT_CALL(*mMockHal.get(), compose(Eq(singleEffect), _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(
                        Status::fromExceptionCode(Status::Exception::EX_UNSUPPORTED_OPERATION)));
        EXPECT_CALL(*mMockHal.get(), compose(Eq(multipleEffects), _))
                .Times(Exactly(1))
                .WillRepeatedly(Return(Status::fromExceptionCode(Status::Exception::EX_SECURITY)));
    }

    std::unique_ptr<int32_t> callbackCounter = std::make_unique<int32_t>();
    auto callback = vibrator::TestFactory::createCountingCallback(callbackCounter.get());

    auto result = mWrapper->performComposedEffect(emptyEffects, callback);
    ASSERT_TRUE(result.isOk());
    ASSERT_EQ(1, *callbackCounter.get());

    result = mWrapper->performComposedEffect(singleEffect, callback);
    ASSERT_TRUE(result.isUnsupported());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());

    result = mWrapper->performComposedEffect(multipleEffects, callback);
    ASSERT_TRUE(result.isFailed());
    // Callback not triggered
    ASSERT_EQ(1, *callbackCounter.get());
}
