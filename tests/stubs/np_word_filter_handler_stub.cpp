// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/libraries/np/np_handler.h"

namespace Libraries::Np {

NpHandler& NpHandler::GetInstance() {
    static NpHandler instance;
    return instance;
}

OrbisNpId NpHandler::GetNpId(s32 /*user_id*/) const {
    return {};
}

s32 NpHandler::GetUserIdByOnlineId(const OrbisNpOnlineId& /*online_id*/) const {
    return -1;
}

bool NpHandler::IsPsnSignedIn(s32 /*user_id*/) const {
    return false;
}

} // namespace Libraries::Np
