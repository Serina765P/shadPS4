// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string_view>

#include "common/logging/log.h"
#include "core/libraries/error_codes.h"
#include "core/libraries/libs.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_handler.h"
#include "np_utility.h"
#include "np_utility_ctx.h"

namespace Libraries::Np::NpUtility {

std::mutex g_word_filter_mutex;
std::map<OrbisNpWordFilterTitleCtxId, WordFilterTitleCtx> g_word_filter_title_ctxs;
std::map<OrbisNpWordFilterRequestId, std::shared_ptr<WordFilterRequestCtx>> g_word_filter_requests;
OrbisNpWordFilterTitleCtxId g_word_filter_next_ctx_id =
    ORBIS_NP_WORD_FILTER_TITLE_CTX_ID_OFFSET + 1;
OrbisNpWordFilterRequestId g_word_filter_next_req_id = ORBIS_NP_WORD_FILTER_REQUEST_ID_OFFSET + 1;

std::string_view OnlineIdView(const OrbisNpOnlineId& id) {
    return std::string_view(id.data, strnlen(id.data, ORBIS_NP_ONLINEID_MAX_LENGTH));
}

// The below are effectively stubbed as they don't have a true implementation yet.
// Word filtering is a NP Community server provided service.
// We would eventually want to replace the below with proper handling via a shadnet command.
// The library sends requests to check input against a stored XML list of censored words.

static std::shared_ptr<WordFilterRequestCtx> GetWordFilterRequest(
    OrbisNpWordFilterRequestId reqId) {
    std::lock_guard lock(g_word_filter_mutex);
    const auto it = g_word_filter_requests.find(reqId);
    return it != g_word_filter_requests.end() ? it->second : nullptr;
}

static s32 StartWordFilterRequest(const std::shared_ptr<WordFilterRequestCtx>& req) {
    std::lock_guard lock(req->mutex);
    if (req->aborted) {
        return ORBIS_NP_COMMUNITY_ERROR_ABORTED;
    }
    if (req->used) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_TYPE;
    }
    req->used = true;
    return ORBIS_OK;
}

static s32 CompleteWordFilterRequest(const std::shared_ptr<WordFilterRequestCtx>& req, s32 result) {
    req->SetResult(result);
    return req->isAsync ? ORBIS_OK : result;
}

s32 PS4_SYSV_ABI sceNpWordFilterAbortRequest(OrbisNpWordFilterRequestId reqId) {
    const auto req = GetWordFilterRequest(reqId);
    if (!req) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    {
        std::lock_guard lock(req->mutex);
        req->aborted = true;
    }
    req->SetResult(ORBIS_NP_COMMUNITY_ERROR_ABORTED);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpWordFilterCensorComment(OrbisNpWordFilterRequestId reqId, const char* comment,
                                              void* option) {
    // Requests and reports result of censorship
    if (comment == nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    }
    if (option != nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const size_t length = strnlen(comment, ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH + 1);
    if (length > ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const auto req = GetWordFilterRequest(reqId);
    if (!req) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    const s32 result = StartWordFilterRequest(req);
    if (result < 0) {
        return result;
    }
    LOG_INFO(Lib_NpUtility, "reqId={} len={}", reqId, length);
    // Insert shadnet request
    // ORBIS_NP_COMMUNITY_SERVER_ERROR_CENSORED would imply rejection
    // Rejection would possibly lead to sanitize call depending on the title.
    return CompleteWordFilterRequest(req, ORBIS_OK);
}

static s32 CreateWordFilterRequest(OrbisNpWordFilterTitleCtxId titleCtxId, bool isAsync) {
    std::lock_guard lock(g_word_filter_mutex);
    const auto ctx_it = g_word_filter_title_ctxs.find(titleCtxId);
    if (ctx_it == g_word_filter_title_ctxs.end()) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    if (static_cast<s32>(g_word_filter_requests.size()) >= ORBIS_NP_WORD_FILTER_MAX_REQUEST_NUM) {
        return ORBIS_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    }
    const OrbisNpWordFilterRequestId id = g_word_filter_next_req_id++;
    auto req = std::make_shared<WordFilterRequestCtx>();
    req->titleCtxId = titleCtxId;
    req->isAsync = isAsync;
    req->timeouts = ctx_it->second.timeouts;
    g_word_filter_requests.emplace(id, std::move(req));
    LOG_INFO(Lib_NpUtility, "id={} titleCtxId={} async={}", id, titleCtxId, isAsync);
    return id;
}

s32 PS4_SYSV_ABI
sceNpWordFilterCreateAsyncRequest(OrbisNpWordFilterTitleCtxId titleCtxId,
                                  const OrbisNpWordFilterCreateAsyncRequestParameter* param) {
    if (param == nullptr || param->size != sizeof(OrbisNpWordFilterCreateAsyncRequestParameter)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    return CreateWordFilterRequest(titleCtxId, true);
}

s32 PS4_SYSV_ABI sceNpWordFilterCreateRequest(OrbisNpWordFilterTitleCtxId titleCtxId) {
    return CreateWordFilterRequest(titleCtxId, false);
}

static s32 CreateWordFilterTitleCtx(s32 userId, const OrbisNpId& selfNpId) {
    std::lock_guard lock(g_word_filter_mutex);
    if (static_cast<s32>(g_word_filter_title_ctxs.size()) >= ORBIS_NP_WORD_FILTER_MAX_CTX_NUM) {
        return ORBIS_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS;
    }
    const OrbisNpWordFilterTitleCtxId id = g_word_filter_next_ctx_id++;
    g_word_filter_title_ctxs.emplace(id,
                                     WordFilterTitleCtx{.userId = userId, .selfNpId = selfNpId});
    LOG_INFO(Lib_NpUtility, "id={} userId={} onlineId='{}'", id, userId,
             OnlineIdView(selfNpId.handle));
    return id;
}

s32 PS4_SYSV_ABI sceNpWordFilterCreateTitleCtx(const OrbisNpId* selfNpId) {
    if (selfNpId == nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    }
    const s32 userId = NpHandler::GetInstance().GetUserIdByOnlineId(selfNpId->handle);
    return CreateWordFilterTitleCtx(userId, *selfNpId);
}

s32 PS4_SYSV_ABI sceNpWordFilterCreateTitleCtxA(UserService::OrbisUserServiceUserId userId) {
    return CreateWordFilterTitleCtx(userId, NpHandler::GetInstance().GetNpId(userId));
}

s32 PS4_SYSV_ABI sceNpWordFilterDeleteRequest(OrbisNpWordFilterRequestId reqId) {
    std::shared_ptr<WordFilterRequestCtx> req;
    {
        std::lock_guard lock(g_word_filter_mutex);
        const auto it = g_word_filter_requests.find(reqId);
        if (it == g_word_filter_requests.end()) {
            return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
        }
        req = it->second;
        g_word_filter_requests.erase(it);
    }
    {
        std::lock_guard lock(req->mutex);
        req->aborted = true;
    }
    req->SetResult(ORBIS_NP_COMMUNITY_ERROR_ABORTED);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpWordFilterDeleteTitleCtx(OrbisNpWordFilterTitleCtxId titleCtxId) {
    std::lock_guard lock(g_word_filter_mutex);
    if (!g_word_filter_title_ctxs.contains(titleCtxId)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    for (auto it = g_word_filter_requests.begin(); it != g_word_filter_requests.end();) {
        if (it->second->titleCtxId == titleCtxId) {
            {
                std::lock_guard req_lock(it->second->mutex);
                it->second->aborted = true;
            }
            it->second->SetResult(ORBIS_NP_COMMUNITY_ERROR_ABORTED);
            it = g_word_filter_requests.erase(it);
        } else {
            ++it;
        }
    }
    g_word_filter_title_ctxs.erase(titleCtxId);
    return ORBIS_OK;
}

s32 PS4_SYSV_ABI sceNpWordFilterPollAsync(OrbisNpWordFilterRequestId reqId, s32* result) {
    if (result == nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const auto req = GetWordFilterRequest(reqId);
    if (!req || !req->isAsync) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    std::lock_guard lock(req->mutex);
    if (!req->result.has_value()) {
        return ORBIS_NP_WORD_FILTER_POLL_ASYNC_RET_RUNNING;
    }
    *result = *req->result;
    return ORBIS_NP_WORD_FILTER_POLL_ASYNC_RET_FINISHED;
}

s32 PS4_SYSV_ABI sceNpWordFilterSanitizeComment(OrbisNpWordFilterRequestId reqId,
                                                const char* comment, char* sanitizedComment,
                                                void* option) {
    // Replace nono word with non-nono word
    if (comment == nullptr || sanitizedComment == nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT;
    }
    if (option != nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const size_t length = strnlen(comment, ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH + 1);
    if (length > ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const auto req = GetWordFilterRequest(reqId);
    if (!req) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    const s32 result = StartWordFilterRequest(req);
    if (result < 0) {
        return result;
    }
    // Insert shadnet request for word replacement, store as sanitizedComment
    // Copy it over and process completion or failure.
    // Invalid response would return ORBIS_NP_COMMUNITY_ERROR_BAD_RESPONSE
    std::memcpy(sanitizedComment, comment, length + 1);
    LOG_INFO(Lib_NpUtility, "reqId={} len={}", reqId, length);
    return CompleteWordFilterRequest(req, ORBIS_OK);
}

s32 PS4_SYSV_ABI sceNpWordFilterSetTimeout(s32 id, s32 resolveRetry, u32 resolveTimeout,
                                           u32 connTimeout, u32 sendTimeout, u32 recvTimeout) {
    if (resolveRetry < 0 || (resolveTimeout != 0 && resolveTimeout < 1'000'000) ||
        (connTimeout != 0 && connTimeout < 10'000'000) ||
        (sendTimeout != 0 && sendTimeout < 10'000'000) ||
        (recvTimeout != 0 && recvTimeout < 10'000'000) ||
        (resolveRetry == 0 && resolveTimeout == 0 && connTimeout == 0 && sendTimeout == 0 &&
         recvTimeout == 0)) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const WordFilterTimeouts timeouts{
        .resolveRetry = resolveRetry,
        .resolveTimeout = resolveTimeout,
        .connTimeout = connTimeout,
        .sendTimeout = sendTimeout,
        .recvTimeout = recvTimeout,
    };
    std::lock_guard lock(g_word_filter_mutex);
    if (auto ctx_it = g_word_filter_title_ctxs.find(id); ctx_it != g_word_filter_title_ctxs.end()) {
        ctx_it->second.timeouts = timeouts;
        return ORBIS_OK;
    }
    if (auto req_it = g_word_filter_requests.find(id); req_it != g_word_filter_requests.end()) {
        std::lock_guard req_lock(req_it->second->mutex);
        req_it->second->timeouts = timeouts;
        return ORBIS_OK;
    }
    return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
}

s32 PS4_SYSV_ABI sceNpWordFilterWaitAsync(OrbisNpWordFilterRequestId reqId, s32* result) {
    if (result == nullptr) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT;
    }
    const auto req = GetWordFilterRequest(reqId);
    if (!req || !req->isAsync) {
        return ORBIS_NP_COMMUNITY_ERROR_INVALID_ID;
    }
    *result = req->Wait();
    return ORBIS_OK;
}

void RegisterLib(Core::Loader::SymbolsResolver* sym) {
    LIB_FUNCTION("rAOOqDAxBIk", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterAbortRequest);
    LIB_FUNCTION("1dMndqL-QgE", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterCensorComment);
    LIB_FUNCTION("IEB+vgVoQbw", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterCreateAsyncRequest);
    LIB_FUNCTION("iCq5xW5KQW4", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterCreateRequest);
    LIB_FUNCTION("r9BgI0PfJZg", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterCreateTitleCtx);
    LIB_FUNCTION("6p9jvljuvsw", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterCreateTitleCtxA);
    LIB_FUNCTION("PYFS1H70bDs", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterDeleteRequest);
    LIB_FUNCTION("t0P5z5yuFPA", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterDeleteTitleCtx);
    LIB_FUNCTION("ur5SShyG0dk", "libSceNpUtility", 1, "libSceNpUtility", sceNpWordFilterPollAsync);
    LIB_FUNCTION("Jj4mkpFO2gE", "libSceNpUtility", 1, "libSceNpUtility",
                 sceNpWordFilterSanitizeComment);
    LIB_FUNCTION("Fa4dVWgmffk", "libSceNpUtility", 1, "libSceNpUtility", sceNpWordFilterSetTimeout);
    LIB_FUNCTION("87ivWj5yKzg", "libSceNpUtility", 1, "libSceNpUtility", sceNpWordFilterWaitAsync);
    LIB_FUNCTION("r9BgI0PfJZg", "libSceNpUtilityCompat", 1, "libSceNpUtility",
                 sceNpWordFilterCreateTitleCtx);
}

} // namespace Libraries::Np::NpUtility
