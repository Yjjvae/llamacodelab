#include "llamacodelab/application/chat_session.hpp"

#include <stdexcept>
#include <utility>

namespace llcl {

void ChatSession::add(const Role role, std::string content) {
  if (state_ == ChatGenerationState::prefill || state_ == ChatGenerationState::decoding) {
    throw std::logic_error("cannot add a message while generation is active");
  }
  if (content.empty()) {
    throw std::invalid_argument("chat message content must not be empty");
  }
  messages_.push_back({role, std::move(content)});
  state_ = ChatGenerationState::idle;
}

void ChatSession::begin_prefill() {
  if (messages_.empty()) {
    throw std::logic_error("cannot generate without chat messages");
  }
  if (state_ != ChatGenerationState::idle && state_ != ChatGenerationState::completed &&
      state_ != ChatGenerationState::cancelled && state_ != ChatGenerationState::failed) {
    throw std::logic_error("chat session cannot enter prefill from its current state");
  }
  state_ = ChatGenerationState::prefill;
}

void ChatSession::begin_decoding() {
  if (state_ != ChatGenerationState::prefill) {
    throw std::logic_error("chat session can only decode after prefill");
  }
  state_ = ChatGenerationState::decoding;
}

void ChatSession::complete(std::string assistant_content) {
  if (state_ != ChatGenerationState::decoding) {
    throw std::logic_error("chat session can only complete while decoding");
  }
  if (!assistant_content.empty()) {
    messages_.push_back({Role::assistant, std::move(assistant_content)});
  }
  state_ = ChatGenerationState::completed;
}

void ChatSession::cancel() {
  if (state_ != ChatGenerationState::prefill && state_ != ChatGenerationState::decoding) {
    throw std::logic_error("chat session can only cancel an active generation");
  }
  state_ = ChatGenerationState::cancelled;
}

void ChatSession::fail() {
  state_ = ChatGenerationState::failed;
}

ChatGenerationState ChatSession::state() const noexcept {
  return state_;
}

std::span<const ChatMessage> ChatSession::messages() const noexcept {
  return messages_;
}

} // namespace llcl
