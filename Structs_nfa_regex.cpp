// ---------------- Character Class ----------------
struct CharClass {
    std::set<char> chars;
    bool negated = false;
    
    CharClass() = default;
    
    void addRange(char start, char end) {
        for (char c = start; c <= end; c++) {
            chars.insert(c);
        }
    }
    
    void addChar(char c) {
        chars.insert(c);
    }
    
    bool matches(char c) const {
        bool in_set = chars.find(c) != chars.end();
        return negated ? !in_set : in_set;
    }
};

// ---------------- State ----------------
struct State {
    StateType type;
    int c;                                      // character for STATE_CHAR
    std::shared_ptr<State> out = nullptr;       // first transition
    std::shared_ptr<State> out1 = nullptr;      // second transition (for SPLIT)
    int lastlist = 0;                           // bookkeeping for simulation
    
    // Extended features
    std::shared_ptr<CharClass> charClass = nullptr; // for STATE_CHARCLASS
    AssertionType assertion = ASSERT_NONE;      // for STATE_ASSERTION
    int captureIndex = -1;                      // for capture groups
    bool greedy = true;                         // for controlling greedy/non-greedy
    
    State(StateType t, int ch = 0, std::shared_ptr<State> o = nullptr, std::shared_ptr<State> o1 = nullptr)
        : type(t), c(ch), out(o), out1(o1) {}
};

// ---------------- PtrList ----------------
struct PtrList {
    std::shared_ptr<State> *p;
    std::shared_ptr<PtrList> next;
    PtrList(std::shared_ptr<State> *ptr, std::shared_ptr<PtrList> n = nullptr) : p(ptr), next(n) {}
};

// ---------------- Fragment ----------------
struct Frag {
    std::shared_ptr<State> start;
    std::shared_ptr<PtrList> out;
    Frag(std::shared_ptr<State> s = nullptr, std::shared_ptr<PtrList> o = nullptr) : start(s), out(o) {}
};

// ---------------- Capture Group Info ----------------
struct CaptureGroup {
    int start_pos;
    int end_pos;
    std::string text;
};