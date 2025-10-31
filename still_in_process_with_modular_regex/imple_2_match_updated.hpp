// NFA_COMPLETE_IMPLEMENTATION.cpp - All class method definitions in one file
#ifndef NFA_IMPLE
#define NFA_IMPLE


/// implementation.cpp - All class method definitions in one file
#include "NFA.hpp"

using namespace PzRegex;



/* ========== CharClass Implementation ========== */

void CharClass::addRange(char start, char end) {
  for (char c = start; c <= end; c++) {
    chars.insert(c);
  }
}

void CharClass::addChar(char c) { 
  chars.insert(c); 
}

bool CharClass::matches(char c) const {
  bool in_set = chars.find(c) != chars.end();
  return negated ? !in_set : in_set;
}

/* ========== State Implementation ========== */

State::State(StateType t, int ch, std::shared_ptr<State> o, std::shared_ptr<State> o1)
    : type(t), c(ch), out(o), out1(o1) {}

State::State(int t, std::shared_ptr<State> o, std::shared_ptr<State> o1)
    : type(static_cast<StateType>(t)), c(0), out(o), out1(o1) {}

/* ========== PtrList Implementation ========== */

PtrList::PtrList(std::shared_ptr<State>* ptr)
    : p(ptr), next(nullptr) {}

/* ========== Frag Implementation ========== */

Frag::Frag() 
    : start(nullptr), out(nullptr) {}

Frag::Frag(std::shared_ptr<State> s, std::shared_ptr<PtrList> o)
    : start(s), out(o) {}

/* ========== NFABuilder Implementation ========== */

NFABuilder::NFABuilder() {
  matchstate_ = std::make_shared<State>(STATE_MATCH);
}

void NFABuilder::patch(std::shared_ptr<PtrList> l, std::shared_ptr<State> s) {
  PtrList* p = l.get();
  while (p) {
    *(p->p) = s;
    p = p->next.get();
  }
}

std::shared_ptr<PtrList> NFABuilder::append(std::shared_ptr<PtrList> l1,
                                             std::shared_ptr<PtrList> l2) {
  if (!l1) return l2;
  
  PtrList* p = l1.get();
  while (p->next) p = p->next.get();
  p->next = l2;
  
  return l1;
}

std::shared_ptr<State> NFABuilder::build(const std::string &postfix) {
  Frag stack[1000];
  int stackp = 0;

  for (size_t i = 0; i < postfix.length(); i++) {
    char ch = postfix[i];

    switch (ch) {
    case '.': { // Concatenation
      Frag e2 = std::move(stack[--stackp]);
      Frag e1 = std::move(stack[--stackp]);
      patch(e1.out, e2.start);
      stack[stackp++] = Frag(e1.start, e2.out);
      break;
    }
    case '|': { // Alternation
      Frag e2 = std::move(stack[--stackp]);
      Frag e1 = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, e1.start, e2.start);
      auto out_list = append(e1.out, e2.out);
      stack[stackp++] = Frag(s, out_list);
      break;
    }
    case '?': { // Zero or one (greedy)
      Frag e1 = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, e1.start, nullptr);
      s->greedy = true;
      auto out_list = append(e1.out, std::make_shared<PtrList>(&s->out1));
      stack[stackp++] = Frag(s, out_list);
      break;
    }
    case '~': { // Non-greedy zero or one (??)
      Frag e1 = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, nullptr, e1.start);
      s->greedy = false;
      auto out_list = append(e1.out, std::make_shared<PtrList>(&s->out));
      stack[stackp++] = Frag(s, out_list);
      break;
    }
    case '*': { // Zero or more (greedy)
      Frag e = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, e.start, nullptr);
      s->greedy = true;
      patch(e.out, s);
      auto out_list = std::make_shared<PtrList>(&s->out1);
      stack[stackp++] = Frag(s, out_list);
      break;
    }
    case '@': { // Non-greedy zero or more (*?)
      Frag e = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, nullptr, e.start);
      s->greedy = false;
      patch(e.out, s);
      auto out_list = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, out_list);
      break;
    }
    case '+': { // One or more (greedy)
      Frag e = std::move(stack[--stackp]);
      auto s = std::make_shared<State>(STATE_SPLIT, 0, e.start, nullptr);
      s->greedy = true;
      patch(e.out, s);
      auto out_list = std::make_shared<PtrList>(&s->out1);
      stack[stackp++] = Frag(e.start, out_list);
      break;
    }
    case '#': { // Quantifier {n,m}
      size_t j = i + 1;
      int min_count = 0, max_count = 0;
      std::string num;
      while (j < postfix.size() && isdigit(static_cast<unsigned char>(postfix[j])))
        num.push_back(postfix[j++]);
      if (!num.empty())
        min_count = std::stoi(num);
      if (j < postfix.size() && postfix[j] == '-') {
        ++j;
        std::string maxnum;
        while (j < postfix.size() && isdigit(static_cast<unsigned char>(postfix[j])))
          maxnum.push_back(postfix[j++]);
        if (!maxnum.empty())
          max_count = std::stoi(maxnum);
        else
          max_count = -1; // open upper bound
      } else {
        max_count = min_count; // exact N
      }

      Frag e = std::move(stack[--stackp]);
      Frag result;

      // Clone helper function
      auto cloneFrag = [&](Frag &f) -> Frag {
        if (!f.start)
          return Frag(nullptr, nullptr);
        std::unordered_map<State*, std::shared_ptr<State>> mp;
        std::vector<State*> st;
        st.push_back(f.start.get());

        // Clone states
        while (!st.empty()) {
          State* old = st.back();
          st.pop_back();
          if (!old || mp.count(old))
            continue;
          mp[old] = std::make_shared<State>(old->type, old->c);
          mp[old]->greedy = old->greedy;
          mp[old]->assertion = old->assertion;
          mp[old]->captureIndex = old->captureIndex;
          if (old->charClass) {
            mp[old]->charClass = std::make_unique<CharClass>(*old->charClass);
          }
          if (old->out && !mp.count(old->out.get()))
            st.push_back(old->out.get());
          if (old->out1 && !mp.count(old->out1.get()))
            st.push_back(old->out1.get());
        }

        // Wire cloned nodes
        for (auto &kv : mp) {
          State* old = kv.first;
          State* nw = kv.second.get();
          nw->out = (old->out && mp.count(old->out.get())) ? mp[old->out.get()] : old->out;
          nw->out1 = (old->out1 && mp.count(old->out1.get())) ? mp[old->out1.get()] : old->out1;
        }

        // Rebuild PtrList
        std::shared_ptr<PtrList> newHead = nullptr;
        std::shared_ptr<PtrList>* tail = &newHead;
        for (PtrList* p = f.out.get(); p; p = p->next.get()) {
          std::shared_ptr<State>* origPtr = p->p;
          std::shared_ptr<State>* newPtr = origPtr;
          for (auto &kv : mp) {
            State* old = kv.first;
            if (origPtr == &old->out) {
              newPtr = &kv.second->out;
              break;
            }
            if (origPtr == &old->out1) {
              newPtr = &kv.second->out1;
              break;
            }
          }
          auto node = std::make_shared<PtrList>(newPtr);
          *tail = node;
          tail = &(*tail)->next;
        }

        return Frag(mp[f.start.get()], newHead);
      };

      // Build min copies
      if (min_count == 0) {
        auto eps = std::make_shared<State>(STATE_SPLIT, 0);
        auto out_list = std::make_shared<PtrList>(&eps->out);
        out_list->next = std::make_shared<PtrList>(&eps->out1);
        result = Frag(eps, out_list);
      } else {
        result = cloneFrag(e);
        for (int k = 1; k < min_count; ++k) {
          Frag next = cloneFrag(e);
          patch(result.out, next.start);
          result = Frag(result.start, next.out);
        }
      }

      // Add optional copies
      if (max_count == -1) {
        Frag loop = cloneFrag(e);
        auto split = std::make_shared<State>(STATE_SPLIT, 0, loop.start, nullptr);
        patch(result.out, split);
        patch(loop.out, split);
        auto out_list = std::make_shared<PtrList>(&split->out1);
        result = Frag(result.start, out_list);
      } else {
        std::shared_ptr<PtrList> tail = result.out;
        for (int k = min_count; k < max_count; ++k) {
          Frag opt = cloneFrag(e);
          auto split = std::make_shared<State>(STATE_SPLIT, 0, opt.start, nullptr);
          patch(tail, split);
          tail = append(opt.out, std::make_shared<PtrList>(&split->out1));
        }
        result = Frag(result.start, tail);
      }

      stack[stackp++] = std::move(result);
      i = j - 1;
      break;
    }
    case '(': { // Start capture group
      int capIndex = next_capture_index_++;
      auto s = std::make_shared<State>(STATE_CAPTURE_START);
      s->captureIndex = capIndex;
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    case ')': { // End capture group
      Frag e2 = std::move(stack[--stackp]);
      Frag e1 = std::move(stack[--stackp]);
      auto endCap = std::make_shared<State>(STATE_CAPTURE_END);
      endCap->captureIndex = e1.start->captureIndex;

      patch(e1.out, e2.start);
      patch(e2.out, endCap);
      auto out_list = std::make_shared<PtrList>(&endCap->out);
      stack[stackp++] = Frag(e1.start, out_list);
      break;
    }
    case '^': { // Start of line
      auto s = std::make_shared<State>(STATE_ASSERTION);
      s->assertion = ASSERT_START_LINE;
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    case '$': { // End of line
      auto s = std::make_shared<State>(STATE_ASSERTION);
      s->assertion = ASSERT_END_LINE;
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    case 'B': { // Word boundary
      auto s = std::make_shared<State>(STATE_ASSERTION);
      s->assertion = ASSERT_WORD_BOUND;
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    case '[': { // Character class
      i++;
      auto cc = std::make_unique<CharClass>();

      if (i < postfix.length() && postfix[i] == '^') {
        cc->negated = true;
        i++;
      }

      while (i < postfix.length() && postfix[i] != ']') {
        char start = postfix[i];
        if (i + 2 < postfix.length() && postfix[i + 1] == '-' &&
            postfix[i + 2] != ']') {
          char end = postfix[i + 2];
          cc->addRange(start, end);
          i += 3;
        } else {
          cc->addChar(start);
          i++;
        }
      }

      auto s = std::make_shared<State>(STATE_CHARCLASS);
      s->charClass = std::move(cc);
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    default: { // Literal character
      auto s = std::make_shared<State>(STATE_CHAR, static_cast<int>(ch));
      auto ptrlist = std::make_shared<PtrList>(&s->out);
      stack[stackp++] = Frag(s, ptrlist);
      break;
    }
    }
  }

  Frag e = std::move(stack[--stackp]);
  patch(e.out, matchstate_);
  return e.start;
}

std::shared_ptr<State> NFABuilder::get_match_state() const { 
  return matchstate_; 
}

int NFABuilder::get_capture_count() const { 
  return next_capture_index_; 
}

/* ========== NFASimulator Implementation ========== */

void NFASimulator::List::clear() { 
  items.clear(); 
}

NFASimulator::NFASimulator(std::shared_ptr<State> start, std::shared_ptr<State> match, int numCaptures)
    : start_(start), matchstate_(match), num_capture_groups_(numCaptures) {}

void NFASimulator::addState(List *l, std::shared_ptr<State> s, const std::string &input,
                             int pos, std::vector<CaptureGroup> caps) {
  if (!s || s->lastlist == listid_)
    return;
  s->lastlist = listid_;

  if (s->type == STATE_SPLIT) {
    // For non-greedy, try out1 (skip) before out (match)
    if (s->greedy) {
      addState(l, s->out, input, pos, caps);
      addState(l, s->out1, input, pos, caps);
    } else {
      addState(l, s->out1, input, pos, caps);
      addState(l, s->out, input, pos, caps);
    }
  } else if (s->type == STATE_ASSERTION) {
    if (checkAssertion(s, input, pos)) {
      addState(l, s->out, input, pos, caps);
    }
  } else if (s->type == STATE_CAPTURE_START) {
    if (s->captureIndex < static_cast<int>(caps.size())) {
      caps[s->captureIndex].start_pos = pos;
    }
    addState(l, s->out, input, pos, caps);
  } else if (s->type == STATE_CAPTURE_END) {
    if (s->captureIndex < static_cast<int>(caps.size())) {
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

bool NFASimulator::checkAssertion(std::shared_ptr<State> s, const std::string &input,
                                   int pos) {
  switch (s->assertion) {
  case ASSERT_START_LINE:
    return pos == 0;
  case ASSERT_END_LINE:
    return pos == static_cast<int>(input.length());
  case ASSERT_WORD_BOUND:
    return isWordBoundary(input, pos);
  default:
    return true;
  }
}

bool NFASimulator::isWordBoundary(const std::string &input, int pos) {
  auto isWord = [](char c) -> bool { 
    return isalnum(static_cast<unsigned char>(c)) || c == '_'; 
  };
  bool before = (pos > 0) && isWord(input[pos - 1]);
  bool after = (pos < static_cast<int>(input.length())) && isWord(input[pos]);
  return before != after;
}

NFASimulator::List *NFASimulator::startList(std::shared_ptr<State> s, List *l, int pos) {
  listid_++;
  l->clear();
  std::vector<CaptureGroup> caps(num_capture_groups_);
  addState(l, s, "", pos, caps);
  return l;
}

void NFASimulator::step(List *clist, const std::string &input, int pos,
                        List *nlist) {
  listid_++;
  nlist->clear();

  for (auto &item : clist->items) {
    std::shared_ptr<State> s = item.state;
    if (s->type == STATE_CHAR &&
        s->c == static_cast<int>(input[pos])) {
      addState(nlist, s->out, input, pos + 1, item.caps);
    } else if (s->type == STATE_CHARCLASS &&
               s->charClass->matches(input[pos])) {
      addState(nlist, s->out, input, pos + 1, item.caps);
    }
  }
}

bool NFASimulator::isMatch(List *l, const std::string &input, int pos) {
  for (auto &item : l->items) {
    if (item.state == matchstate_) {
      captures_ = item.caps;
      return true;
    }
  }
  return false;
}

//Match function
bool NFASimulator::match(const std::string &s) {
for (int i = 0; i <= static_cast<int>(s.length()); i++) { 
captures_.clear();
captures_.resize(num_capture_groups_);

List *clist = startList(start_, &l1_, i);
List *nlist = &l2_;

if (isMatch(clist, s, i)) {
return true;
 }

for (size_t pos = i; pos < s.length(); pos++) {

step(clist, s, static_cast<int>(pos), nlist);
std::swap(clist, nlist);

if (clist->items.empty()) {
break;
}

    if (isMatch(clist, s, static_cast<int>(pos) + 1)) {
     return true;
    }
    }
 }

    return false;
    }


///getting capture count
std::string NFASimulator::get_capture(int index) const {
  if (index >= 0 && index < static_cast<int>(captures_.size())) {
    return captures_[index].text;
  }
  return "";
}

#endif