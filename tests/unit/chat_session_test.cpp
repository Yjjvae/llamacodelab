#include "llamacodelab/application/chat_session.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace llcl {
namespace {

TEST(ChatSessionTest, TracksNormalGenerationAndStoresAssistantReply) {
  ChatSession session;
  session.add(Role::system, "Be concise.");
  session.add(Role::user, "What is RAII?");
  session.begin_prefill();
  session.begin_decoding();
  session.complete("A resource lifetime pattern.");

  EXPECT_EQ(session.state(), ChatGenerationState::completed);
  ASSERT_EQ(session.messages().size(), 3U);
  EXPECT_EQ(session.messages().back().role, Role::assistant);
}

TEST(ChatSessionTest, RejectsMutationDuringGenerationAndSupportsCancellation) {
  ChatSession session;
  session.add(Role::user, "hello");
  session.begin_prefill();
  EXPECT_THROW(session.add(Role::user, "second"), std::logic_error);
  session.cancel();
  EXPECT_EQ(session.state(), ChatGenerationState::cancelled);
}

} // namespace
} // namespace llcl
