#include "NFA.hpp"

typedef PzRegex::CharClass CharClass;
typedef PzRegex::State State;
typedef PzRegex::PtrList PtrList;
typedef PzRegex::Frag Frag;
typedef PzRegex::NFABuilder NFABuilder;
typedef PzRegex::StateType StateType;
typedef PzRegex::AssertionType AssertionType;

// CharClass implementation
void CharClass::addRange(char start, char end) {
  for (char c = start; c <= end; c++) {
    chars.insert(c);
  }
}

void CharClass::addChar(char c) { chars.insert(c); }

bool CharClass::matches(char c) const {
  bool in_set = chars.find(c) != chars.end();
  return negated ? !in_set : in_set;
}

// State implementation
State::State(StateType t, st32 ch, State *o, State *o1)
    : type(t), c(ch), out(o), out1(o1) {}

State::State(st32 t, State *o, State *o1)
    : type(static_cast<StateType>(t)), c(0), out(o), out1(o1) {}

// PtrList implementation
PtrList::PtrList(State **ptr, std::unique_ptr<PtrList> n)
    : p(ptr), next(std::move(n)) {}

// Frag implementation
Frag::Frag(std::unique_ptr<State> s, std::unique_ptr<PtrList> o)
    : start(std::move(s)), out(std::move(o)) {}

// NFABuilder implementation
NFABuilder::NFABuilder() {
  matchstate_ = new State(StateType::STATE_MATCH);
}

void NFABuilder::patch(PtrList *l, State *s) {
  for (; l; l = l->next.get()) {
    *(l->p) = s;
  }
}

std::unique_ptr<PtrList> NFABuilder::append(std::unique_ptr<PtrList> l1,
                                             std::unique_ptr<PtrList> l2) {
  if (!l1)
    return l2;
  PtrList *p = l1.get();
  while (p->next)
    p = p->next.get();
  p->next = std::move(l2);
  return l1;
}

std::unique_ptr<State> NFABuilder::build(const std::string &postfix) {
  Frag stack[1000], *stackp = stack;

  for (size_t i = 0; i < postfix.length(); i++) {
    char ch = postfix[i];

    switch (ch) {
    case '.': { // Concatenation
      Frag e2 = std::move(*--stackp);
      Frag e1 = std::move(*--stackp);
      patch(e1.out.get(), e2.start.get());
      *stackp++ = Frag(std::move(e1.start), std::move(e2.out));
      break;
    }
    case '|': { // Alternation
      Frag e2 = std::move(*--stackp);
      Frag e1 = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0, e1.start.get(),
                                       e2.start.get());
      auto out_list = append(std::move(e1.out), std::move(e2.out));
      e1.start.release();
      e2.start.release();
      *stackp++ = Frag(std::move(s), std::move(out_list));
      break;
    }
    case '?': { // Zero or one (greedy)
      Frag e1 = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0,
                                       e1.start.get(), nullptr);
      s->greedy = true;
      auto out_list = append(std::move(e1.out),
                             std::make_unique<PtrList>(&s->out1));
      e1.start.release();
      *stackp++ = Frag(std::move(s), std::move(out_list));
      break;
    }
    case '~': { // Non-greedy zero or one (??)
      Frag e1 = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0, nullptr,
                                       e1.start.get());
      s->greedy = false;
      auto out_list = append(std::move(e1.out),
                             std::make_unique<PtrList>(&s->out));
      e1.start.release();
      *stackp++ = Frag(std::move(s), std::move(out_list));
      break;
    }
    case '*': { // Zero or more (greedy)
      Frag e = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0,
                                       e.start.get(), nullptr);
      s->greedy = true;
      patch(e.out.get(), s.get());
      auto out_list = std::make_unique<PtrList>(&s->out1);
      e.start.release();
      *stackp++ = Frag(std::move(s), std::move(out_list));
      break;
    }
    case '@': { // Non-greedy zero or more (*?)
      Frag e = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0, nullptr,
                                       e.start.get());
      s->greedy = false;
      patch(e.out.get(), s.get());
      auto out_list = std::make_unique<PtrList>(&s->out);
      e.start.release();
      *stackp++ = Frag(std::move(s), std::move(out_list));
      break;
    }
    case '+': { // One or more (greedy)
      Frag e = std::move(*--stackp);
      auto s = std::make_unique<State>(StateType::STATE_SPLIT, 0,
                                       e.start.get(), nullptr);
      s->greedy = true;
      State *s_ptr = s.get();
      patch(e.out.get(), s_ptr);
      auto out_list = std::make_unique<PtrList>(&s->out1);
      auto start = std::move(e.start);
      s.release();
      *stackp++ = Frag(std::move(start), std::move(out_list));
      break;
    }
    case '#': { // Quantifier {n,m}
      size_t j = i + 1;
      st32 min = 0, max = 0;
      std::string num;
      while (j < postfix.size() && isdigit(static_cast<ut8>(postfix[j])))
        num.push_back(postfix[j++]);
      if (!num.empty())
        min = std::stoi(num);
      if (j < postfix.size() && postfix[j] == '-') {
        ++j;
        std::string maxnum;
        while (j < postfix.size() && isdigit(static_cast<ut8>(postfix[j])))
          maxnum.push_back(postfix[j++]);
        if (!maxnum.empty())
          max = std::stoi(maxnum);
        else
          max = -1; // open upper bound
      } else {
        max = min; // exact N
      }

      Frag e = std::move(*--stackp);
      Frag result;

      // Clone helper function
      auto cloneFrag = [&](Frag &f) -> Frag {
        if (!f.start)
          return Frag(nullptr, nullptr);
        std::unordered_map<State *, std::unique_ptr<State>> mp;
        std::vector<State *> st;
        st.push_back(f.start.get());

        // Clone states
        while (!st.empty()) {
          State *old = st.back();
          st.pop_back();
          if (!old || mp.count(old))
            continue;
          mp[old] = std::make_unique<State>(old->type, old->c);
          mp[old]->greedy = old->greedy;
          mp[old]->assertion = old->assertion;
          mp[old]->captureIndex = old->captureIndex;
          if (old->charClass) {
            mp[old]->charClass = std::make_unique<CharClass>(*old->charClass);
          }
          if (old->out && !mp.count(old->out))
            st.push_back(old->out);
          if (old->out1 && !mp.count(old->out1))
            st.push_back(old->out1);
        }

        // Wire cloned nodes
        for (auto &kv : mp) {
          State *old = kv.first;
          State *nw = kv.second.get();
          nw->out = (old->out && mp.count(old->out)) ? mp[old->out].get() : old->out;
          nw->out1 = (old->out1 && mp.count(old->out1)) ? mp[old->out1].get() : old->out1;
        }

        // Rebuild PtrList
        std::unique_ptr<PtrList> newHead = nullptr;
        std::unique_ptr<PtrList> *tail = &newHead;
        for (PtrList *p = f.out.get(); p; p = p->next.get()) {
          State **origPtr = p->p;
          State **newPtr = origPtr;
          for (auto &kv : mp) {
            State *old = kv.first;
            State *clone = kv.second.get();
            if (origPtr == &old->out) {
              newPtr = &clone->out;
              break;
            }
            if (origPtr == &old->out1) {
              newPtr = &clone->out1;
              break;
            }
          }
          auto node = std::make_unique<PtrList>(newPtr);
          *tail = std::move(node);
          tail = &(*tail)->next;
        }

        auto start_state = std::move(mp[f.start.get()]);
        for (auto &kv : mp) {
          if (kv.second.get() != start_state.get()) {
            kv.second.release();
          }
        }
        return Frag(std::move(start_state), std::move(newHead));
      };

      // Build min copies
      if (min == 0) {
        auto eps = std::make_unique<State>(StateType::STATE_SPLIT, 0);
        auto out_list = std::make_unique<PtrList>(&eps->out,
                         std::make_unique<PtrList>(&eps->out1));
        result = Frag(std::move(eps), std::move(out_list));
      } else {
        result = cloneFrag(e);
        for (st32 k = 1; k < min; ++k) {
          Frag next = cloneFrag(e);
          patch(result.out.get(), next.start.get());
          result = Frag(std::move(result.start), std::move(next.out));
        }
      }

      // Add optional copies
      if (max == -1) {
        Frag loop = cloneFrag(e);
        auto split = std::make_unique<State>(StateType::STATE_SPLIT, 0,
                                             loop.start.get(), nullptr);
        State *split_ptr = split.get();
        patch(result.out.get(), split_ptr);
        patch(loop.out.get(), split_ptr);
        auto out_list = std::make_unique<PtrList>(&split->out1);
        loop.start.release();
        result = Frag(std::move(result.start), std::move(out_list));
        split.release();
      } else {
        std::unique_ptr<PtrList> tail = std::move(result.out);
        for (st32 k = min; k < max; ++k) {
          Frag opt = cloneFrag(e);
          auto split = std::make_unique<State>(StateType::STATE_SPLIT, 0,
                                               opt.start.get(), nullptr);
          patch(tail.get(), split.get());
          tail = append(std::move(opt.out),
                       std::make_unique<PtrList>(&split->out1));
          opt.start.release();
          split.release();
        }
        result = Frag(std::move(result.start), std::move(tail));
      }

      *stackp++ = std::move(result);
      i = j - 1;
      break;
    }
    case '(': { // Start capture group
      st32 capIndex = next_capture_index_++;
      auto s = std::make_unique<State>(StateType::STATE_CAPTURE_START);
      s->captureIndex = capIndex;
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
      break;
    }
    case ')': { // End capture group
      Frag e2 = std::move(*--stackp);
      Frag e1 = std::move(*--stackp);
      auto endCap = std::make_unique<State>(StateType::STATE_CAPTURE_END);
      endCap->captureIndex = e1.start->captureIndex;

      patch(e1.out.get(), e2.start.get());
      patch(e2.out.get(), endCap.get());
      auto out_list = std::make_unique<PtrList>(&endCap->out);
      e2.start.release();
      *stackp++ = Frag(std::move(e1.start), std::move(out_list));
      endCap.release();
      break;
    }
    case '^': { // Start of line
      auto s = std::make_unique<State>(StateType::STATE_ASSERTION);
      s->assertion = AssertionType::ASSERT_START_LINE;
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
      break;
    }
    case '$': { // End of line
      auto s = std::make_unique<State>(StateType::STATE_ASSERTION);
      s->assertion = AssertionType::ASSERT_END_LINE;
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
      break;
    }
    case 'B': { // Word boundary
      auto s = std::make_unique<State>(StateType::STATE_ASSERTION);
      s->assertion = AssertionType::ASSERT_WORD_BOUND;
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
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

      auto s = std::make_unique<State>(StateType::STATE_CHARCLASS);
      s->charClass = std::move(cc);
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
      break;
    }
    default: { // Literal character
      auto s = std::make_unique<State>(StateType::STATE_CHAR, static_cast<st32>(ch));
      *stackp++ = Frag(std::move(s), std::make_unique<PtrList>(&(*(stackp-1)).start->out));
      break;
    }
    }
  }

  Frag e = std::move(*--stackp);
  patch(e.out.get(), matchstate_);
  return std::move(e.start);
}

State *NFABuilder::get_match_state() const { return matchstate_; }

st32 NFABuilder::get_capture_count() const { return next_capture_index_; }