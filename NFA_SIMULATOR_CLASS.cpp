// ---------------- Enhanced NFA Simulator ----------------
class NFASimulator {
public:
    NFASimulator(State *start, State *match, int numCaptures)
        : start(start), matchstate(match), numCaptureGroups(numCaptures) {}
    
    bool match(const std::string &s) {
        captures.clear();
        captures.resize(numCaptureGroups);
        
        List *clist = startList(start, &l1, 0);
        List *nlist = &l2;
        
        for (size_t pos = 0; pos < s.length(); pos++) {
            step(clist, s, pos, nlist);
            std::swap(clist, nlist);
        }
        
        return isMatch(clist, s, s.length());
    }
    
    std::string getCapture(int index) const {
        if (index >= 0 && index < (int)captures.size()) {
            return captures[index].text;
        }
        return "";
    }

private:
    struct ListItem {
        State *state;
        std::vector<CaptureGroup> caps;
    };
    
    struct List {
        std::vector<ListItem> items;
        void clear() { items.clear(); }
    };
    
    State *start;
    State *matchstate;
    int listid = 0;
    List l1, l2;
    int numCaptureGroups;
    std::vector<CaptureGroup> captures;
    
    void addState(List *l, State *s, const std::string &input, int pos, 
                  std::vector<CaptureGroup> caps) {
        if (!s || s->lastlist == listid) return;
        s->lastlist = listid;
        
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
            if (s->captureIndex < (int)caps.size()) {
                caps[s->captureIndex].start_pos = pos;
            }
            addState(l, s->out, input, pos, caps);
        } else if (s->type == STATE_CAPTURE_END) {
            if (s->captureIndex < (int)caps.size()) {
                caps[s->captureIndex].end_pos = pos;
                caps[s->captureIndex].text = input.substr(
                    caps[s->captureIndex].start_pos,
                    pos - caps[s->captureIndex].start_pos
                );
            }
            addState(l, s->out, input, pos, caps);
        } else {
            l->items.push_back({s, caps});
        }
    }
    
    bool checkAssertion(State *s, const std::string &input, int pos) {
        switch (s->assertion) {
            case ASSERT_START_LINE:
                return pos == 0;
            case ASSERT_END_LINE:
                return pos == (int)input.length();
            case ASSERT_WORD_BOUND:
                return isWordBoundary(input, pos);
            default:
                return true;
        }
    }
    
    bool isWordBoundary(const std::string &input, int pos) {
        auto isWord = [](char c) { return isalnum(c) || c == '_'; };
        bool before = (pos > 0) && isWord(input[pos-1]);
        bool after = (pos < (int)input.length()) && isWord(input[pos]);
        return before != after;
    }
    
    List *startList(State *s, List *l, int pos) {
        listid++;
        l->clear();
        std::vector<CaptureGroup> caps(numCaptureGroups);
        addState(l, s, "", pos, caps);
        return l;
    }
    
    void step(List *clist, const std::string &input, int pos, List *nlist) {
        listid++;
        nlist->clear();
        
        for (auto &item : clist->items) {
            State *s = item.state;
            if (s->type == STATE_CHAR && s->c == input[pos]) {
                addState(nlist, s->out, input, pos + 1, item.caps);
            } else if (s->type == STATE_CHARCLASS && 
                       s->charClass->matches(input[pos])) {
                addState(nlist, s->out, input, pos + 1, item.caps);
            }
        }
    }
    
    bool isMatch(List *l, const std::string &input, int pos) {
        for (auto &item : l->items) {
            if (item.state == matchstate) {
                captures = item.caps;
                return true;
            }
        }
        return false;
    }
};

