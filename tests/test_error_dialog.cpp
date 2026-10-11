// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/ime/error_dialog.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/trophy_data_state.h"

std::size_t ActiveErrorDialogLayers();
void DrawErrorDialogLayers();

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
        for (u32 slot = 0; slot < Libraries::Np::NpTrophy::TrophyDataState::MaxContexts; ++slot) {
            Libraries::Np::NpTrophy::trophy_data_state.Clear(slot);
        }
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

TEST_F(ErrorDialogTest, MissingTrophyDefinitionsAreAcknowledgedAfterRunning) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, false);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    DrawErrorDialogLayers();
    ASSERT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
}

TEST_F(ErrorDialogTest, CompleteTrophyDefinitionsKeepArgumentErrorsVisible) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, true);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, UnregisteredTrophyErrorsKeepTheirOriginalDialog) {
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, AnotherUsersMissingTrophiesDoNotDismissTheDialog) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 2, false);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, OneAvailableContextPreventsTrophyErrorAcknowledgement) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, false);
    Libraries::Np::NpTrophy::trophy_data_state.Register(1, 1, true);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, UnspecifiedUserRequiresAllRegisteredTrophiesToBeUnavailable) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, false);
    auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    param.userId = -1;
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    ASSERT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    ASSERT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
    Libraries::Np::NpTrophy::trophy_data_state.Register(1, 2, true);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, DestroyedTrophyContextDoesNotDismissArgumentErrors) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, false);
    Libraries::Np::NpTrophy::trophy_data_state.Clear(0);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, OtherTrophyErrorsAreNotAcknowledgedForMissingDefinitions) {
    Libraries::Np::NpTrophy::trophy_data_state.Register(0, 1, false);
    const auto param = MakeParam(ORBIS_NP_TROPHY_ERROR_INVALID_TROPHY_ID);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
}

TEST_F(ErrorDialogTest, OfflineSignedOutIsAutomaticallyAcknowledged) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    EXPECT_EQ(sceErrorDialogClose(), Error::NOT_RUNNING);
}

TEST_F(ErrorDialogTest, RepeatedOfflineNotificationsCompleteWithoutAUserClick) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    for (int occurrence = 0; occurrence < 3; ++occurrence) {
        ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
        ASSERT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
        DrawErrorDialogLayers();
        EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
        EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    }
}

TEST_F(ErrorDialogTest, GuestWaitingForRunningCanProceed) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    // A rendering frame may happen before the game's first status query.
    DrawErrorDialogLayers();
    ASSERT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
}

TEST_F(ErrorDialogTest, UpdateStatusOnlyCallerSeesRunningBeforeCompletion) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    DrawErrorDialogLayers();
    ASSERT_EQ(sceErrorDialogUpdateStatus(), Status::RUNNING);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::FINISHED);
}

TEST_F(ErrorDialogTest, PollingWithoutAFrameKeepsTheDialogRunning) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    for (int poll = 0; poll < 5; ++poll) {
        EXPECT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
        EXPECT_EQ(sceErrorDialogUpdateStatus(), Status::RUNNING);
    }
    EXPECT_EQ(ActiveErrorDialogLayers(), 1u);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
}

TEST_F(ErrorDialogTest, CloseBeforeAutomaticConfirmationStillSucceeds) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    ASSERT_EQ(sceErrorDialogClose(), Error::OK);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::FINISHED);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
}

TEST_F(ErrorDialogTest, TerminationCancelsPendingAutomaticConfirmation) {
    const auto param = MakeParam(ORBIS_NP_ERROR_SIGNED_OUT);
    ASSERT_EQ(sceErrorDialogOpen(&param), Error::OK);
    ASSERT_EQ(sceErrorDialogGetStatus(), Status::RUNNING);
    ASSERT_EQ(sceErrorDialogTerminate(), Error::OK);
    DrawErrorDialogLayers();
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::NONE);
    EXPECT_EQ(ActiveErrorDialogLayers(), 0u);
    ASSERT_EQ(sceErrorDialogInitialize(), Error::OK);
    EXPECT_EQ(sceErrorDialogGetStatus(), Status::INITIALIZED);
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
