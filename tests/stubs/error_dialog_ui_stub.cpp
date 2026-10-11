// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <imgui.h>
#include "imgui/imgui_layer.h"

namespace {
std::array<ImGui::Layer*, 4> layers{};
}

namespace ImGui {

void Layer::AddLayer(Layer* layer) {
    if (std::ranges::find(layers, layer) != layers.end()) {
        return;
    }
    if (const auto slot = std::ranges::find(layers, nullptr); slot != layers.end()) {
        *slot = layer;
    }
}

void Layer::RemoveLayer(Layer* layer) {
    if (const auto slot = std::ranges::find(layers, layer); slot != layers.end()) {
        *slot = nullptr;
    }
}

} // namespace ImGui

std::size_t ActiveErrorDialogLayers() {
    return std::ranges::count_if(layers, [](auto* layer) { return layer != nullptr; });
}

void DrawErrorDialogLayers() {
    auto* previous_context = ImGui::GetCurrentContext();
    if (!previous_context) {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2{1280.0f, 720.0f};
        io.DeltaTime = 1.0f / 60.0f;
        io.IniFilename = nullptr;
        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    }
    ImGui::NewFrame();
    const auto active_layers = layers;
    for (auto* layer : active_layers) {
        if (layer) {
            layer->Draw();
        }
    }
    ImGui::EndFrame();
    if (!previous_context) {
        ImGui::DestroyContext();
    }
}
