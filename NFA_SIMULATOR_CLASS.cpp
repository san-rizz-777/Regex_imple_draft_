#include "NFA.hpp"


typedef PzRegex::State State;
typedef PzRegex::StateType StateType;
typedef PzRegex::AssertionType AssertionType;
typedef PzRegex::CaptureGroup CaptureGroup;
typedef PzRegex::NFASimulator NFASimulator;

// List implementation
void NFASimulator::List::clear() { items.clear(); }

// NFASimulator implementation
NFASimulator::NFASimulator(State *start, State *match, st32 numCaptures)
    : start_(start), matchstate_(match), num_capture_groups_(numCaptures) {}

void NFASimulator::addState(List *l, State *s, const std::string &input,
                             st32 pos, std::vector<CaptureGroup> caps) {
  if (!s || s->lastlist == listid_)
    return;
  s->lastlist = listid_;

  if (s->type == StateType::STATE_SPLIT) {
    // For non-greedy, try out1 (skip) before out (match)
    if (s->greedy) {
      addState(l, s->out, input, pos, caps);
      addState(l, s->out1, input, pos, caps);
    } else {
      addState(l, s->out1, input, pos, caps);
      addState(l, s->out, input, pos, caps);
    }
  } else if (s->type == StateType::STATE_ASSERTION) {
    if (checkAssertion(s, input, pos)) {
      addState(l, s->out, input, pos, caps);
    }
  } else if (s->type == StateType::STATE_CAPTURE_START) {
    if (s->captureIndex < static_cast<st32>(caps.size())) {
      caps[s->captureIndex].start_pos = pos;
    }
    addState(l, s->out, input, pos, caps);
  } else if (s->type == StateType::STATE_CAPTURE_END) {
    if (s->captureIndex < static_cast<st32>(caps.size())) {
      caps[s->captureIndex].end_pos = pos;
      caps[s->captureIndex].text =
          input.substr(caps[s->captureIndex].start_pos,
                       pos - caps[s->captureIndex].start_pos);
    }
    addState(l, s->out, input, pos, caps);
  } else {
    l->items.push_back({s, caps});
  }
}

bool NFASimulator::checkAssertion(State *s, const std::string &input,
                                   st32 pos) {
  switch (s->assertion) {
  case AssertionType::ASSERT_START_LINE:
    return pos == 0;
  case AssertionType::ASSERT_END_LINE:
    return pos == static_cast<st32>(input.length());
  case AssertionType::ASSERT_WORD_BOUND:
    return isWordBoundary(input, pos);
  default:
    return true;
  }
}

bool NFASimulator::isWordBoundary(const std::string &input, st32 pos) {
  auto isWord = [](char c) -> bool { return isalnum(static_cast<ut8>(c)) || c == '_'; };
  bool before = (pos > 0) && isWord(input[pos - 1]);
  bool after = (pos < static_cast<st32>(input.length())) && isWord(input[pos]);
  return before != after;
}

NFASimulator::List *NFASimulator::startList(State *s, List *l, st32 pos) {
  listid_++;
  l->clear();
  std::vector<CaptureGroup> caps(num_capture_groups_);
  addState(l, s, "", pos, caps);
  return l;
}

void NFASimulator::step(List *clist, const std::string &input, st32 pos,
                        List *nlist) {
  listid_++;
  nlist->clear();

  for (auto &item : clist->items) {
    State *s = item.state;
    if (s->type == StateType::STATE_CHAR &&
        s->c == static_cast<st32>(input[pos])) {
      addState(nlist, s->out, input, pos + 1, item.caps);
    } else if (s->type == StateType::STATE_CHARCLASS &&
               s->charClass->matches(input[pos])) {
      addState(nlist, s->out, input, pos + 1, item.caps);
    }
  }
}

bool NFASimulator::isMatch(List *l, const std::string &input, st32 pos) {
  for (auto &item : l->items) {
    if (item.state == matchstate_) {
      captures_ = item.caps;
      return true;
    }
  }
  return false;
}

bool NFASimulator::match(const std::string &s) {
  captures_.clear();
  captures_.resize(num_capture_groups_);

  List *clist = startList(start_, &l1_, 0);
  List *nlist = &l2_;

  for (size_t pos = 0; pos < s.length(); pos++) {
    step(clist, s, static_cast<st32>(pos), nlist);
    std::swap(clist, nlist);
  }

  return isMatch(clist, s, static_cast<st32>(s.length()));
}

std::string NFASimulator::get_capture(st32 index) const {
  if (index >= 0 && index < static_cast<st32>(captures_.size())) {
    return captures_[index].text;
  }
  return "";
}