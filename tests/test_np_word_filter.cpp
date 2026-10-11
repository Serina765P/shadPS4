// SPDX-FileCopyrightText: Copyright 2026 Serina765P
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "common/logging/log.h"
#include "core/libraries/np/np_error.h"
#include "core/libraries/np/np_handler.h"
#include "core/libraries/np/np_utility/np_utility.h"
#include "core/loader/symbols_resolver.h"

using namespace Libraries::Np::NpUtility;

class NpWordFilterTest : public ::testing::Test {
protected:
    std::vector<s32> contexts;

    void SetUp() override {
        static bool logging_ready = false;
        if (!logging_ready) {
            Common::Log::Setup("shad_np_word_filter_test.log");
            logging_ready = true;
        }
    }

    void TearDown() override {
        for (const auto context : contexts) {
            EXPECT_EQ(sceNpWordFilterDeleteTitleCtx(context), ORBIS_OK);
        }
    }

    s32 CreateContext() {
        const auto context = sceNpWordFilterCreateTitleCtxA(1000);
        if (context > 0) {
            contexts.push_back(context);
        }
        return context;
    }

    void DeleteContext(s32 context) {
        ASSERT_EQ(sceNpWordFilterDeleteTitleCtx(context), ORBIS_OK);
        std::erase(contexts, context);
    }

    OrbisNpWordFilterCreateAsyncRequestParameter AsyncParam() {
        return {sizeof(OrbisNpWordFilterCreateAsyncRequestParameter), 0, 0, {}};
    }
};

TEST_F(NpWordFilterTest, TitleContextHasAPositiveNonZeroId) {
    const auto contextId = CreateContext();
    ASSERT_GT(contextId, 0);
    EXPECT_FALSE(Libraries::Np::NpHandler::GetInstance().IsPsnSignedIn(1000));
}

TEST_F(NpWordFilterTest, StartupNidResolvesToTheImplementedApi) {
    Core::Loader::SymbolsResolver resolver;
    RegisterLib(&resolver);
    const Core::Loader::SymbolResolver query{
        .name = "6p9jvljuvsw",
        .library = "libSceNpUtility",
        .library_version = 1,
        .module = "libSceNpUtility",
        .type = Core::Loader::SymbolType::Function,
    };
    const auto* symbol = resolver.FindSymbol(query);
    ASSERT_NE(symbol, nullptr);
    using CreateTitleCtxA = s32(PS4_SYSV_ABI*)(s32);
    const auto create = reinterpret_cast<CreateTitleCtxA>(symbol->virtual_address);
    const auto contextId = create(1000);
    ASSERT_GT(contextId, 0);
    contexts.push_back(contextId);
}

TEST_F(NpWordFilterTest, ContextsAndRequestsHaveDistinctValidHandles) {
    const auto first = CreateContext();
    const auto second = CreateContext();
    ASSERT_GT(first, 0);
    ASSERT_GT(second, 0);
    ASSERT_NE(first, second);
    const auto request = sceNpWordFilterCreateRequest(first);
    ASSERT_GT(request, 0);
    EXPECT_NE(request, first);
    EXPECT_NE(request, second);
    EXPECT_EQ(sceNpWordFilterDeleteTitleCtx(request), ORBIS_NP_COMMUNITY_ERROR_INVALID_ID);
    ASSERT_EQ(sceNpWordFilterDeleteRequest(request), ORBIS_OK);
}

TEST_F(NpWordFilterTest, RepeatedCreateAndDeleteDoesNotExhaustTheContextLimit) {
    for (int iteration = 0; iteration < 64; ++iteration) {
        const auto context = CreateContext();
        ASSERT_GT(context, 0);
        DeleteContext(context);
    }
}

TEST_F(NpWordFilterTest, SynchronousSanitizePreservesTextAndRejectsReuse) {
    const auto context = CreateContext();
    ASSERT_GT(context, 0);
    const auto request = sceNpWordFilterCreateRequest(context);
    ASSERT_GT(request, 0);
    std::array<char, ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH + 1> sanitized{};
    EXPECT_EQ(sceNpWordFilterSanitizeComment(request, "Producer", sanitized.data(), nullptr),
              ORBIS_OK);
    EXPECT_STREQ(sanitized.data(), "Producer");
    EXPECT_EQ(sceNpWordFilterCensorComment(request, "Producer", nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_TYPE);
}

TEST_F(NpWordFilterTest, AsyncPollAndWaitObserveCompletion) {
    const auto context = CreateContext();
    ASSERT_GT(context, 0);
    const auto param = AsyncParam();
    const auto request = sceNpWordFilterCreateAsyncRequest(context, &param);
    ASSERT_GT(request, 0);
    s32 result = -123;
    EXPECT_EQ(sceNpWordFilterPollAsync(request, &result),
              ORBIS_NP_WORD_FILTER_POLL_ASYNC_RET_RUNNING);
    EXPECT_EQ(result, -123);
    ASSERT_EQ(sceNpWordFilterCensorComment(request, "Producer", nullptr), ORBIS_OK);
    EXPECT_EQ(sceNpWordFilterPollAsync(request, &result),
              ORBIS_NP_WORD_FILTER_POLL_ASYNC_RET_FINISHED);
    EXPECT_EQ(result, ORBIS_OK);
    result = -123;
    EXPECT_EQ(sceNpWordFilterWaitAsync(request, &result), ORBIS_OK);
    EXPECT_EQ(result, ORBIS_OK);
}

TEST_F(NpWordFilterTest, AbortCompletesTheRequestAndContextDeletionInvalidatesIt) {
    const auto context = CreateContext();
    ASSERT_GT(context, 0);
    const auto param = AsyncParam();
    const auto request = sceNpWordFilterCreateAsyncRequest(context, &param);
    ASSERT_GT(request, 0);
    ASSERT_EQ(sceNpWordFilterAbortRequest(request), ORBIS_OK);
    s32 result = 0;
    EXPECT_EQ(sceNpWordFilterWaitAsync(request, &result), ORBIS_OK);
    EXPECT_EQ(result, ORBIS_NP_COMMUNITY_ERROR_ABORTED);
    DeleteContext(context);
    EXPECT_EQ(sceNpWordFilterPollAsync(request, &result), ORBIS_NP_COMMUNITY_ERROR_INVALID_ID);
}

TEST_F(NpWordFilterTest, InvalidArgumentsAndDeletedHandlesAreRejected) {
    EXPECT_EQ(sceNpWordFilterCreateTitleCtx(nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT);
    const auto context = CreateContext();
    ASSERT_GT(context, 0);
    EXPECT_EQ(sceNpWordFilterCreateAsyncRequest(context, nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    const auto request = sceNpWordFilterCreateRequest(context);
    ASSERT_GT(request, 0);
    EXPECT_EQ(sceNpWordFilterCensorComment(request, nullptr, nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INSUFFICIENT_ARGUMENT);
    EXPECT_EQ(sceNpWordFilterCensorComment(request, "Producer", &contexts),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    const std::string long_comment(ORBIS_NP_WORD_FILTER_COMMENT_MAX_LENGTH + 1, 'a');
    EXPECT_EQ(sceNpWordFilterCensorComment(request, long_comment.c_str(), nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(sceNpWordFilterPollAsync(request, nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_ARGUMENT);
    ASSERT_EQ(sceNpWordFilterDeleteRequest(request), ORBIS_OK);
    EXPECT_EQ(sceNpWordFilterCensorComment(request, "Producer", nullptr),
              ORBIS_NP_COMMUNITY_ERROR_INVALID_ID);
}

TEST_F(NpWordFilterTest, ContextLimitReturnsAnErrorInsteadOfZero) {
    for (int context_index = 0; context_index < ORBIS_NP_WORD_FILTER_MAX_CTX_NUM;
         ++context_index) {
        ASSERT_GT(CreateContext(), 0);
    }
    EXPECT_EQ(sceNpWordFilterCreateTitleCtxA(1000), ORBIS_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS);
}
