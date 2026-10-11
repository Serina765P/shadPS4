// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
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
