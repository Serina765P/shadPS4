// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include "common/types.h"

namespace Libraries::Np::NpTrophy {

// Publish registration availability without reading the trophy worker's mutable context map.
class TrophyDataState {
public:
    static constexpr u32 MaxContexts = 8;

    void Register(u32 slot, s32 user_id, bool definitions_available) {
        if (slot < contexts.size()) {
            contexts[slot].store((u64(static_cast<u32>(user_id)) << 2) | Registered |
                                 (definitions_available ? Available : 0));
        }
    }

    void Clear(u32 slot) {
        if (slot < contexts.size()) {
            contexts[slot].store(0);
        }
    }

    bool UserHasUnavailableDefinitions(s32 user_id) const {
        bool found = false;
        for (const auto& context : contexts) {
            const auto state = context.load();
            if (!(state & Registered)) {
                continue;
            }
            // A dialog with no specific user must not hide an error if any registered
            // context has usable definitions. A specific user is isolated from other users.
            if (user_id > 0 && (state >> 2) != static_cast<u32>(user_id)) {
                continue;
            }
            if (state & Available) {
                return false;
            }
            found = true;
        }
        return found;
    }

private:
    static constexpr u64 Registered = 1;
    static constexpr u64 Available = 2;
    std::array<std::atomic<u64>, MaxContexts> contexts{};
};

inline TrophyDataState trophy_data_state;

} // namespace Libraries::Np::NpTrophy
