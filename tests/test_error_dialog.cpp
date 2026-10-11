// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/ime/error_dialog.h"
#include "core/libraries/np/np_error.h"

std::size_t ActiveErrorDialogLayers();

using namespace Libraries::ErrorDialog;
using Libraries::CommonDialog::Error;
using Libraries::CommonDialog::Status;

class ErrorDialogTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool logging_ready = false;
        if (!logging_ready) {
            Common::Log::Setup("shad_error_dialog_test.log");
            logging_ready = true;
        }
        sceErrorDialogTerminate();
        EmulatorSettings.SetConfigMode(ConfigMode::Global);
        EmulatorSettings.SetConnectedToNetwork(false);
        EmulatorSettings.SetShadNetEnabled(false);
        EmulatorSettings.SetShadNetSessionDisabled(false);
        ASSERT_EQ(sceErrorDialogInitialize(), Error::OK);
    }

    void TearDown() override {
        sceErrorDialogTerminate();
        EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    }

    Param MakeParam(u32 code) {
        return {static_cast<s32>(sizeof(Param)), static_cast<s32>(code), 1, 0};
    }
};

TEST_F(ErrorDialogTest, OfflineSignedOutIsAutomaticallyAcknowledged) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    EXPECT_EQ(sceErrorDialogClose(), Error::NOT_RUNNING);
}

TEST_F(ErrorDialogTest, RepeatedOfflineNotificationsCompleteWithoutAUserClick) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    for (int occurrence = 0; occurrence < 3; ++occurrence) {
        ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
        EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
        EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    }
}

TEST_F(ErrorDialogTest, ConnectedNetworkKeepsTheSignOutDialogVisible) {
    EmulatorSettings.SetConnectedToNetwork(true);
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
    ASSERT_EQ(sceErrorDialogClose(), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
}

TEST_F(ErrorDialogTest, ShadNetKeepsTheSignOutDialogVisible) {
    EmulatorSettings.SetShadNetEnabled(true);
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, OtherOfflineErrorsStillRequireConfirmation) {
    const auto param = MakeParam(ORBIS_NP_ERROR_USER_NOT_FOUND);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, DisabledShadNetSessionKeepsConfiguredAccountDialogVisible) {
    EmulatorSettings.SetShadNetEnabled(true);
    EmulatorSettings.SetShadNetSessionDisabled(true);
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, SavedDataCorruptionErrorsAreNotDismissed) {
    const auto param = MakeParam(0x809F000Fu);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, NullParametersKeepTheirOriginalError) {
    EXPECT_EQ(sceErrorDialogOpen(nullptr), Error::ARG_NULL);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::INITIALIZED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
}

TEST_F(ErrorDialogTest, InitializationIsStillRequired) {
    ASSERT_EQ(sceErrorDialogTerminate(), Error::OK);
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    EXPECT_EQ(sceErrorDialogOpen(&param), Error::INVALID_STATE);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::NONE);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
}
