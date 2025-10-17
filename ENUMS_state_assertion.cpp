
// ---------------- State Types ----------------
enum StateType {
    STATE_CHAR = 0,      // consume a character
    STATE_SPLIT,         // ε-transition (alternation)
    STATE_MATCH,         // accepting state
    STATE_CHARCLASS,     // character class [a-z]
    STATE_ASSERTION,     // ^, $, \b
    STATE_CAPTURE_START, // start of capture group
    STATE_CAPTURE_END    // end of capture group
};

enum AssertionType {
    ASSERT_NONE = 0,
    ASSERT_START_LINE,   // ^
    ASSERT_END_LINE,     // $
    ASSERT_WORD_BOUND    // \b
};