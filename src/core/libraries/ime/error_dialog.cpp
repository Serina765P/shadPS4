// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <utility>
#include <imgui.h>
#include <magic_enum/magic_enum.hpp>

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/trophy_data_state.h"
#include "core/libraries/system/commondialog.h"
#include "error_dialog.h"
#include "imgui/imgui_layer.h"
#include "imgui/imgui_std.h"

static constexpr ImVec2 BUTTON_SIZE{100.0f, 30.0f};

namespace Libraries::ErrorDialog {

using CommonDialog::Error;
using CommonDialog::Result;
using CommonDialog::Status;

static std::atomic_bool g_auto_acknowledge{false};
static std::atomic_bool g_running_status_observed{false};
static std::atomic<u32> g_auto_acknowledge_code{0};

class ErrorDialogUi final : public ImGui::Layer {
    bool first_render{false};

    std::atomic<Status>* status{nullptr};
    std::string err_message{};

public:
    explicit ErrorDialogUi(std::atomic<Status>* status = nullptr, std::string err_message = "")
        : status(status), err_message(std::move(err_message)) {
        if (status && status->load() == Status::RUNNING) {
            first_render = true;
            AddLayer(this);
        }
    }
    ~ErrorDialogUi() override {
        Finish();
    }
    ErrorDialogUi(const ErrorDialogUi& other) = delete;
    ErrorDialogUi(ErrorDialogUi&& other) noexcept
        : Layer(other), status(other.status), err_message(std::move(other.err_message)) {
        other.status = nullptr;
    }
    ErrorDialogUi& operator=(ErrorDialogUi other) {
        using std::swap;
        swap(status, other.status);
        swap(err_message, other.err_message);
        if (status && status->load() == Status::RUNNING) {
            first_render = true;
            AddLayer(this);
        }
        return *this;
    }

    void Finish() {
        if (status) {
            status->store(Status::FINISHED);
        }
        status = nullptr;
        RemoveLayer(this);
    }

    void Draw() override {
        using namespace ImGui;
        if (status == nullptr || status->load() != Status::RUNNING) {
            return;
        }
        if (g_auto_acknowledge.load()) {
            // Keep RUNNING observable even if rendering beats the game's first status query.
            // Complete on the UI thread through the same path as the OK button.
            if (g_running_status_observed.load()) {
                Finish();
                LOG_INFO(Lib_ErrorDialog,
                         "Automatically acknowledged dialog {:#x} after RUNNING was observed",
                         g_auto_acknowledge_code.load());
            }
            return;
        }
        const auto& io = GetIO();

        const ImVec2 window_size{
            std::min(io.DisplaySize.x, 500.0f),
            std::min(io.DisplaySize.y, 300.0f),
        };

        CentralizeNextWindow();
        SetNextWindowSize(window_size);
        SetNextWindowCollapsed(false);
        if (first_render || !io.NavActive) {
            SetNextWindowFocus();
        }
        KeepNavHighlight();
        if (Begin("Error Dialog##ErrorDialog", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            const auto ws = GetWindowSize();

            DrawPrettyBackground();
            const char* begin = &err_message.front();
            const char* end = &err_message.back() + 1;
            SetWindowFontScale(1.3f);
            DrawCenteredText(begin, end,
                             GetContentRegionAvail() - ImVec2{0.0f, 15.0f + BUTTON_SIZE.y});
            SetWindowFontScale(1.0f);

            SetCursorPos({
                ws.x / 2.0f - BUTTON_SIZE.x / 2.0f,
                ws.y - 10.0f - BUTTON_SIZE.y,
            });
            if (Button("OK", BUTTON_SIZE)) {
                Finish();
            }
            if (first_render) {
                SetItemCurrentNavFocus();
            }
        }
        End();

        first_render = false;
    }
};

static std::atomic<Status> g_status{Status::NONE};
static ErrorDialogUi g_dialog_ui;

static Status GetDialogStatus() {
    const auto status = g_status.load();
    if (status == Status::RUNNING && g_auto_acknowledge.load() &&
        !g_running_status_observed.exchange(true)) {
        LOG_INFO(Lib_ErrorDialog, "Dialog {:#x} RUNNING observed by guest",
                 g_auto_acknowledge_code.load());
    }
    // Return the snapshot so a concurrent UI acknowledgement cannot hide RUNNING from this query.
    return status;
}

Error PS4_SYSV_ABI sceErrorDialogClose() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (g_status.load() != Status::RUNNING) {
        return Error::NOT_RUNNING;
    }
    g_dialog_ui.Finish();
    return Error::OK;
}

Status PS4_SYSV_ABI sceErrorDialogGetStatus() {
    const auto status = GetDialogStatus();
    LOG_TRACE(Lib_ErrorDialog, "called status={}", magic_enum::enum_name(status));
    return status;
}

Error PS4_SYSV_ABI sceErrorDialogInitialize() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (g_status.load() != Status::NONE) {
        return Error::ALREADY_INITIALIZED;
    }
    g_auto_acknowledge.store(false);
    g_running_status_observed.store(false);
    g_status.store(Status::INITIALIZED);
    return Error::OK;
}

Error PS4_SYSV_ABI sceErrorDialogOpen(const Param* param) {
    const auto status = g_status.load();
    if (status != Status::INITIALIZED && status != Status::FINISHED) {
        LOG_INFO(Lib_ErrorDialog, "called without initialize");
        return Error::INVALID_STATE;
    }
    if (param == nullptr) {
        LOG_DEBUG(Lib_ErrorDialog, "called param:(NULL)");
        return Error::ARG_NULL;
    }
    const auto err = static_cast<u32>(param->errorCode);
    LOG_INFO(Lib_ErrorDialog, "called param->errorCode = {:#x}, userId = {}", err, param->userId);
    ASSERT(param->size == sizeof(Param));

    const bool offline_signed_out = err == static_cast<u32>(ORBIS_NP_ERROR_SIGNED_OUT) &&
                                    !EmulatorSettings.IsConnectedToNetwork() &&
                                    !EmulatorSettings.IsShadNetEnabledSetting();
    const bool unavailable_trophy =
        err == static_cast<u32>(ORBIS_NP_TROPHY_ERROR_INVALID_ARGUMENT) &&
        Libraries::Np::NpTrophy::trophy_data_state.UserHasUnavailableDefinitions(param->userId);
    const bool auto_acknowledge = offline_signed_out || unavailable_trophy;
    g_running_status_observed.store(false);
    g_auto_acknowledge_code.store(err);
    g_auto_acknowledge.store(auto_acknowledge);
    if (auto_acknowledge) {
        LOG_INFO(Lib_ErrorDialog, "Queued dialog {:#x} for UI acknowledgement: {}", err,
                 unavailable_trophy ? "registered trophy definitions unavailable"
                                    : "offline PSN sign-out");
    }

    const std::string err_message = fmt::format("An error has occurred. \nCode: {:#X}", err);
    g_status.store(Status::RUNNING);
    g_dialog_ui = ErrorDialogUi{&g_status, err_message};
    return Error::OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenDetail() {
    LOG_ERROR(Lib_ErrorDialog, "(STUBBED) called");
    return ORBIS_OK;
}

int PS4_SYSV_ABI sceErrorDialogOpenWithReport() {
    LOG_ERROR(Lib_ErrorDialog, "(STUBBED) called");
    return ORBIS_OK;
}

Error PS4_SYSV_ABI sceErrorDialogTerminate() {
    LOG_DEBUG(Lib_ErrorDialog, "called");
    if (g_status.load() == Status::RUNNING) {
        sceErrorDialogClose();
    }
    if (g_status.load() == Status::NONE) {
        return Error::NOT_INITIALIZED;
    }
    g_auto_acknowledge.store(false);
    g_running_status_observed.store(false);
    g_status.store(Status::NONE);
    return Error::OK;
}

Status PS4_SYSV_ABI sceErrorDialogUpdateStatus() {
    const auto status = GetDialogStatus();
    LOG_TRACE(Lib_ErrorDialog, "called status={}", magic_enum::enum_name(status));
    return status;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("ekXHb1kDBl0", "libSceErrorDialog", 1, "libSceErrorDialog", sceErrorDialogClose);
    LIB_FUNCTION("t2FvHRXzgqk", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogGetStatus);
    LIB_FUNCTION("I88KChlynSs", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogInitialize);
    LIB_FUNCTION("M2ZF-ClLhgY", "libSceErrorDialog", 1, "libSceErrorDialog", sceErrorDialogOpen);
    LIB_FUNCTION("jrpnVQfJYgQ", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogOpenDetail);
    LIB_FUNCTION("wktCiyWoDTI", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogOpenWithReport);
    LIB_FUNCTION("9XAxK2PMwk8", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogTerminate);
    LIB_FUNCTION("WWiGuh9XfgQ", "libSceErrorDialog", 1, "libSceErrorDialog",
                 sceErrorDialogUpdateStatus);
};

} // namespace Libraries::ErrorDialog
