#ifndef PZ_REGEX_HPP
#define PZ_REGEX_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cctype>

/** @brief namespace PzRegex */
namespace PzRegex {

/**
 * @name State type enumeration
 */
enum  StateType : int {
  STATE_CHAR = 0,      // Consume a character
  STATE_SPLIT,         // ε-transition (alternation)
  STATE_MATCH,         // Accepting state
  STATE_CHARCLASS,     // Character class [a-z]
  STATE_ASSERTION,     // ^, $, \b
  STATE_CAPTURE_START, // Start of capture group
  STATE_CAPTURE_END    // End of capture group
};

/**
 * @name Assertion type enumeration
 */
enum  AssertionType : int {
  ASSERT_NONE = 0,   // No assertion
  ASSERT_START_LINE, // ^ - Start of line
  ASSERT_END_LINE,   // $ - End of line
  ASSERT_WORD_BOUND  // \b - Word boundary
};


// Forward declarations - AFTER enum definitions
struct CharClass;    // Character class representation
struct State;        // NFA state node
struct PtrList;      // Linked list for patching transitions
struct Frag;         // NFA fragment during construction
struct CaptureGroup; // Capture group information
class NFABuilder;    // NFA construction from postfix regex
class NFASimulator;  // NFA simulation engine

/**
 * @brief Character class for pattern matching
 */
struct CharClass {
  std::set<char> chars; // Characters in the class
  bool negated = false; // If true, matches characters NOT in set

  CharClass() = default;

  void addRange(char start, char end); // Add character range [start-end]
  void addChar(char c);                // Add single character
  bool matches(char c) const;          // Check if character matches class
};

/**
 * @brief NFA state node (now using shared_ptr)
 */
struct State {
  StateType type;                        // Type of state
  int c;                                 // Character for STATE_CHAR
  std::shared_ptr<State> out;            // First transition (shared_ptr)
  std::shared_ptr<State> out1;           // Second transition (shared_ptr for SPLIT)
  int lastlist = 0;                      // Bookkeeping for simulation

  // Extended features
  std::unique_ptr<CharClass> charClass = nullptr; // For STATE_CHARCLASS
  AssertionType assertion = AssertionType::ASSERT_NONE; // For STATE_ASSERTION
  int captureIndex = -1;                 // For capture groups
  bool greedy = true;                    // Greedy vs non-greedy matching

  State(StateType t, int ch = 0, std::shared_ptr<State> o = nullptr, 
        std::shared_ptr<State> o1 = nullptr);

  State(int t, std::shared_ptr<State> o = nullptr, 
        std::shared_ptr<State> o1 = nullptr);
};

/**
 * @brief Linked list of state pointers for patching (now using shared_ptr)
 */
struct PtrList {
  std::shared_ptr<State>* p;             // Pointer to a shared_ptr<State> field
  std::shared_ptr<PtrList> next;         // Next item in list

  PtrList(std::shared_ptr<State>* ptr);
};

/**
 * @brief NFA fragment during construction (now using shared_ptr with move semantics)
 */
struct Frag {
  std::shared_ptr<State> start;          // Start state of fragment (shared_ptr)
  std::shared_ptr<PtrList> out;          // Dangling output pointers

  Frag();
  Frag(std::shared_ptr<State> s, std::shared_ptr<PtrList> o);
};

/**
 * @brief Captured group information
 */
struct CaptureGroup {
  int start_pos;   // Starting position
  int end_pos;     // Ending position
  std::string text; // Captured text
};

/**
 * @brief NFA builder from postfix regex
 */
class NFABuilder {
private:
  std::shared_ptr<State> matchstate_;     // The accepting state (shared_ptr)
  int next_capture_index_ = 0;            // Counter for capture groups

  static void patch(std::shared_ptr<PtrList> l, std::shared_ptr<State> s); // Patch dangling pointers
  static std::shared_ptr<PtrList>
  append(std::shared_ptr<PtrList> l1,
         std::shared_ptr<PtrList> l2);    // Append PtrLists

public:
  NFABuilder();

  std::shared_ptr<State>
  build(const std::string &postfix);      // Build NFA from postfix regex
  std::shared_ptr<State> get_match_state() const; // Get match state
  int get_capture_count() const;          // Get capture group count
};

/**
 * @brief NFA simulation engine for pattern matching
 */
class NFASimulator {
private:
  struct ListItem {
    std::shared_ptr<State> state;         // shared_ptr to state
    std::vector<CaptureGroup> caps;
  };

  struct List {
    std::vector<ListItem> items;
    void clear();
  };

  std::shared_ptr<State> start_;          // Start state (shared_ptr)
  std::shared_ptr<State> matchstate_;     // Match state (shared_ptr)
  int listid_ = 0;                        // Generation counter
  List l1_, l2_;                          // Current and next state lists
  int num_capture_groups_;                // Number of capture groups
  std::vector<CaptureGroup> captures_;    // Storage for captures

  void addState(List *l, std::shared_ptr<State> s, const std::string &input, int pos,
                std::vector<CaptureGroup> caps); // Add state to list
  bool checkAssertion(std::shared_ptr<State> s, const std::string &input,
                      int pos); // Check assertion at position
  bool isWordBoundary(const std::string &input,
                      int pos); // Check word boundary
  List *startList(std::shared_ptr<State> s, List *l, int pos); // Initialize state list
  void step(List *clist, const std::string &input, int pos,
            List *nlist); // Execute simulation step
  bool isMatch(List *l, const std::string &input,
               int pos); // Check for match state

public:
  NFASimulator(std::shared_ptr<State> start, std::shared_ptr<State> match, int numCaptures);

  bool match(const std::string &s);         // Match string against NFA
  std::string get_capture(int index) const; // Get captured text
};

} // namespace PzRegex

#endif // PZ_REGEX_HPP