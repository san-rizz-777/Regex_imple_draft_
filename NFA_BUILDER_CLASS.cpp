// ---------------- Enhanced NFA Builder ----------------
class NFABuilder {
public:
    NFABuilder() {
        matchstate = new State(STATE_MATCH);
    }
    
    // Build NFA from postfix regex with extended syntax
    State *build(const std::string &postfix) {
        Frag stack[1000], *stackp = stack;
        
        for (size_t i = 0; i < postfix.length(); i++) {
            char ch = postfix[i];
            
            switch (ch) {
                case '.': { // Concatenation
                    Frag e2 = *--stackp;
                    Frag e1 = *--stackp;
                    patch(e1.out, e2.start);
                    *stackp++ = Frag(e1.start, e2.out);
                    break;
                }
                case '|': { // Alternation
                    Frag e2 = *--stackp;
                    Frag e1 = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, e1.start, e2.start);
                    *stackp++ = Frag(s, append(e1.out, e2.out));
                    break;
                }
                case '?': { // Zero or one
                    Frag e1 = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, e1.start, nullptr);
                    s->greedy = true;
                    *stackp++ = Frag(s, append(e1.out, new PtrList(&s->out1)));
                    break;
                }
                case '~': { // Non-greedy zero or one (??)
                    Frag e1 = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, nullptr, e1.start);
                    s->greedy = false;
                    *stackp++ = Frag(s, append(e1.out, new PtrList(&s->out)));
                    break;
                }
                case '*': { // Zero or more
                    Frag e = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, e.start, nullptr);
                    s->greedy = true;
                    patch(e.out, s);
                    *stackp++ = Frag(s, new PtrList(&s->out1));
                    break;
                }
                case '@': { // Non-greedy zero or more (*?)
                    Frag e = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, nullptr, e.start);
                    s->greedy = false;
                    patch(e.out, s);
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                case '+': { // One or more
                    Frag e = *--stackp;
                    State *s = new State(STATE_SPLIT, 0, e.start, nullptr);
                    s->greedy = true;
                    patch(e.out, s);
                    *stackp++ = Frag(e.start, new PtrList(&s->out1));
                    break;
                }
                      case '#': {
                    size_t j = i + 1;
                    int min = 0, max = 0;
                    std::string num;
                    while (j < postfix.size() && isdigit((unsigned char)postfix[j]))
                        num.push_back(postfix[j++]);
                    if (!num.empty()) min = std::stoi(num);
                    if (j < postfix.size() && postfix[j] == '-') {
                        ++j;
                        std::string maxnum;
                        while (j < postfix.size() && isdigit((unsigned char)postfix[j]))
                            maxnum.push_back(postfix[j++]);
                        if (!maxnum.empty()) max = std::stoi(maxnum);
                        else max = -1; // open upper bound
                    } else {
                        // "#N" means exact N
                        max = min;
                    }

                    Frag e = *--stackp;
                    Frag result;

                    // local clone helper: deep-clone the fragment graph reachable from f.start
                    auto cloneFrag = [&](const Frag &f) -> Frag {
                        if (!f.start) return Frag(nullptr, nullptr);
                        std::unordered_map<State*, State*> mp;
                        std::vector<State*> st;
                        st.push_back(f.start);
                        while (!st.empty()) {
                            State *old = st.back(); st.pop_back();
                            if (!old || mp.count(old)) continue;
                            mp[old] = new State(old->c, nullptr, nullptr);
                            if (old->out && !mp.count(old->out)) st.push_back(old->out);
                            if (old->out1 && !mp.count(old->out1)) st.push_back(old->out1);
                        }
                        // wire cloned nodes
                        for (auto &kv : mp) {
                            State *old = kv.first;
                            State *nw  = kv.second;
                            nw->out  = (old->out  && mp.count(old->out))  ? mp[old->out]  : old->out;
                            nw->out1 = (old->out1 && mp.count(old->out1)) ? mp[old->out1] : old->out1;
                        }
                        // rebuild PtrList for out pointers (map &old->out -> &clone->out)
                        PtrList *newHead = nullptr;
                        PtrList **tail = &newHead;
                        for (PtrList *p = f.out; p; p = p->next) {
                            State **origPtr = p->p;
                            State **newPtr = origPtr;
                            for (auto &kv : mp) {
                                State *old = kv.first;
                                State *clone = kv.second;
                                if (origPtr == &old->out)  { newPtr = &clone->out;  break; }
                                if (origPtr == &old->out1) { newPtr = &clone->out1; break; }
                            }
                            PtrList *node = new PtrList(newPtr);
                            *tail = node;
                            tail = &node->next;
                        }
                        return Frag(mp[f.start], newHead);
                    };

                    // --- Build min copies in sequence (use clones, do NOT reuse e) ---
                    if (min == 0) {
                        // create an epsilon (split) fragment whose both branches are dangling
                        State *eps = new State(SPLIT, nullptr, nullptr);
                        result = Frag(eps, new PtrList(&eps->out, new PtrList(&eps->out1)));
                    } else {
                        // first copy
                        result = cloneFrag(e);
                        for (int k = 1; k < min; ++k) {
                            Frag next = cloneFrag(e);
                            patch(result.out, next.start);
                            result = Frag(result.start, next.out);
                        }
                    }

                    // --- Add optional copies up to max ---
                    if (max == -1) {
                        // min .. infinity: add a looping clone
                        Frag loop = cloneFrag(e);
                        State *split = new State(SPLIT, loop.start, nullptr);
                        patch(result.out, split);       // after prefix go to split
                        patch(loop.out, split);         // after a loop copy go back to split
                        result = Frag(result.start, new PtrList(&split->out1)); // split->out1 is exit (dangling)
                    } else {
                        // finite upper bound: for each optional repetition attach a SPLIT that goes to clone or skips
                        PtrList *tail = result.out;
                        for (int k = min; k < max; ++k) {
                            Frag opt = cloneFrag(e);
                            State *split = new State(SPLIT, opt.start, nullptr);
                            patch(tail, split); // connect previous exits to this split
                            // new dangling outputs are opt.out and split->out1 (skip)
                            tail = append(opt.out, new PtrList(&split->out1));
                        }
                        result = Frag(result.start, tail);
                    }

                    *stackp++ = result;
                    i = j - 1; // advance parser past the numbers
                    break;
                }
                case '(': { // Start capture group
                    int capIndex = nextCaptureIndex++;
                    State *s = new State(STATE_CAPTURE_START);
                    s->captureIndex = capIndex;
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                case ')': { // End capture group - pop start, concatenate
                    Frag e2 = *--stackp; // the content
                    Frag e1 = *--stackp; // capture start
                    State *endCap = new State(STATE_CAPTURE_END);
                    endCap->captureIndex = e1.start->captureIndex;
                    
                    patch(e1.out, e2.start);
                    patch(e2.out, endCap);
                    *stackp++ = Frag(e1.start, new PtrList(&endCap->out));
                    break;
                }
                case '^': { // Start of line assertion
                    State *s = new State(STATE_ASSERTION);
                    s->assertion = ASSERT_START_LINE;
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                case '$': { // End of line assertion
                    State *s = new State(STATE_ASSERTION);
                    s->assertion = ASSERT_END_LINE;
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                case 'B': { // Word boundary assertion (\b)
                    State *s = new State(STATE_ASSERTION);
                    s->assertion = ASSERT_WORD_BOUND;
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                case '[': { // Character class - parse until ']'
                    i++; // skip '['
                    CharClass *cc = new CharClass();
                    
                    // Check for negation
                    if (i < postfix.length() && postfix[i] == '^') {
                        cc->negated = true;
                        i++;
                    }
                    
                    while (i < postfix.length() && postfix[i] != ']') {
                        char start = postfix[i];
                        if (i + 2 < postfix.length() && postfix[i+1] == '-' 
                            && postfix[i+2] != ']') {
                            // Range
                            char end = postfix[i+2];
                            cc->addRange(start, end);
                            i += 3;
                        } else {
                            // Single char
                            cc->addChar(start);
                            i++;
                        }
                    }
                    
                    State *s = new State(STATE_CHARCLASS);
                    s->charClass = cc;
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
                default: { // Literal character
                    State *s = new State(STATE_CHAR, (int)ch);
                    *stackp++ = Frag(s, new PtrList(&s->out));
                    break;
                }
            }
        }
        
        // Final fragment
        Frag e = *--stackp;
        patch(e.out, matchstate);
        return e.start;
    }
    
    State *getMatchState() const { return matchstate; }
    int getCaptureCount() const { return nextCaptureIndex; }

private:
    State *matchstate;
    int nextCaptureIndex = 0;
    
    static void patch(PtrList *l, State *s) {
        for (; l; l = l->next) *(l->p) = s;
    }
    
    static PtrList *append(PtrList *l1, PtrList *l2) {
        if (!l1) return l2;
        PtrList *p = l1;
        while (p->next) p = p->next;
        p->next = l2;
        return l1;
    }
};