/// NFA_TEST_MAIN.cpp - Complete test suite with infix to postfix conversion
#include "NFA.hpp"
#include "implementation.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cctype>

using namespace std;
using namespace PzRegex;

/* ---------- INFIX TO POSTFIX CONVERSION ---------- */

/* Get operator precedence */
static int precedence(char op) {
    switch (op) {
        case '|': return 1;  /* Lowest */
        case '.': return 2;  /* Concatenation */
        case '*':
        case '+':
        case '?': return 3;  /* Highest (postfix) */
        default: return 0;
    }
}

/* Check if character is regular (alphanumeric, space, etc.) */
static bool isRegularChar(char c) {
    return isalnum(c) || c == '_' || c == ' ' || c == '-';
}

/* Preprocess: Add explicit concatenation operators */
static string preprocess(const string& regex) {
    string result;
    int len = regex.length();
    
    for (int i = 0; i < len; i++) {
        char c = regex[i];
        result += c;
        
        if (i + 1 < len) {
            char next = regex[i + 1];
            bool needConcat = false;
            
            /* Check if we need concatenation after current char */
            if (c == '*' || c == '+' || c == '?' || c == ')' || c == '}' || c == ']') {
                needConcat = true;
            } else if (c == '\\' && i + 1 < len) {
                /* After escape sequence */
                needConcat = true;
                result += regex[++i]; /* Skip the escaped char */
            } else if (c != '(' && c != '|' && c != '^') {
                /* Regular character */
                needConcat = true;
            }
            
            /* Don't concatenate before these */
            if (next == '*' || next == '+' || next == '?' || next == '|' || 
                next == ')' || next == '{') {
                needConcat = false;
            }
            
            /* Add concatenation operator */
            if (needConcat && (next == '(' || isRegularChar(next) || next == '[' || 
                next == '\\' || next == '^' || next == '$')) {
                result += '.';
            }
        }
    }
    
    return result;
}

/* Shunting-yard algorithm for operator precedence */
static string shuntingYard(const string& regex) {
    string output;
    vector<char> opStack;
    
    for (size_t i = 0; i < regex.length(); i++) {
        char c = regex[i];
        
        if (c == '(') {
            opStack.push_back(c);
        }
        else if (c == ')') {
            /* Pop until matching '(' */
            while (!opStack.empty() && opStack.back() != '(') {
                output += opStack.back();
                opStack.pop_back();
            }
            if (!opStack.empty() && opStack.back() == '(') {
                opStack.pop_back(); /* Remove '(' */
            }
        }
        else if (c == '|') {
            /* Pop higher or equal precedence */
            while (!opStack.empty() && opStack.back() != '(' && 
                   precedence(opStack.back()) >= precedence(c)) {
                output += opStack.back();
                opStack.pop_back();
            }
            opStack.push_back(c);
        }
        else if (c == '.') {
            /* Concatenation */
            while (!opStack.empty() && opStack.back() != '(' && 
                   precedence(opStack.back()) >= precedence(c)) {
                output += opStack.back();
                opStack.pop_back();
            }
            opStack.push_back(c);
        }
        else if (c == '*' || c == '+' || c == '?') {
            /* Postfix operator - output immediately */
            output += c;
        }
        else {
            /* Regular character */
            output += c;
        }
    }
    
    /* Pop remaining operators */
    while (!opStack.empty()) {
        output += opStack.back();
        opStack.pop_back();
    }
    
    return output;
}

/* Main conversion function */
string infixToPostfix(const string& regex) {
    string preprocessed = preprocess(regex);
    return shuntingYard(preprocessed);
}

/* ---------- TEST CASES ---------- */

struct TestCase {
    string pattern;
    string text;
    bool expected;
};

void runTests() {
    vector<TestCase> tests = {
        /* Basic literals */
        {"a", "a", true},
        {"a", "b", false},
        {"abc", "abc", true},
        {"abc", "abd", false},
        
        /* Alternation */
        {"a|b", "a", true},
        {"a|b", "b", true},
        {"a|b", "c", false},
        {"cat|dog", "cat", true},
        {"cat|dog", "dog", true},
        {"cat|dog", "bird", false},
        
        /* Concatenation */
        {"ab", "ab", true},
        {"ab", "a", false},
        {"ab", "abc", false},
        
        /* Star (zero or more) */
        {"a*", "", true},
        {"a*", "a", true},
        {"a*", "aaa", true},
        {"a*b", "b", true},
        {"a*b", "ab", true},
        {"a*b", "aaab", true},
        
        /* Plus (one or more) */
        {"a+", "", false},
        {"a+", "a", true},
        {"a+", "aaa", true},
        {"a+b", "b", false},
        {"a+b", "ab", true},
        {"a+b", "aaaab", true},
        
        /* Question mark (zero or one) */
        {"a?", "", true},
        {"a?", "a", true},
        {"a?", "aa", false},
        {"a?b", "b", true},
        {"a?b", "ab", true},
        
        /* Combined operators */
        {"(a|b)*", "", true},
        {"(a|b)*", "a", true},
        {"(a|b)*", "abab", true},
        {"(a|b)*", "c", false},
        {"a(b|c)d", "abd", true},
        {"a(b|c)d", "acd", true},
        {"a(b|c)d", "aed", false},
        
        /* Complex patterns */
        {"(a|b)*abb", "abb", true},
        {"(a|b)*abb", "aabb", true},
        {"(a|b)*abb", "babb", true},
        {"(a|b)*abb", "abababb", true},
        {"(a|b)*abb", "ab", false},
        
        /* Multiple alternatives */
        {"a|b|c|d", "a", true},
        {"a|b|c|d", "d", true},
        {"a|b|c|d", "e", false},
        
        /* Nested groups */
        {"((a|b)(c|d))", "ac", true},
        {"((a|b)(c|d))", "bd", true},
        {"((a|b)(c|d))", "ae", false},
        
        /* Edge cases */
        {"", "", true},
        {"a*b*", "", true},
        {"a*b*", "aaabbb", true},
        {"(a*)*", "", true},
        {"(a*)*", "aaa", true},
        
        /* More complex */
        {"(ab|cd)*", "abcd", true},
        {"(ab|cd)*", "cdab", true},
        {"(ab|cd)*", "abcdabcd", true},
        {"x(yz)*", "x", true},
        {"x(yz)*", "xyz", true},
        {"x(yz)*", "xyzyz", true},
        
        /* Character classes (if supported in postfix) */
        // Note: These may need special handling depending on your postfix syntax
        
        /* Anchors (if supported) */
        // {"^abc", "abc", true},
        // {"^abc", "xabc", false},
        // {"abc$", "abc", true},
        // {"abc$", "abcx", false},
    };
    
    int passed = 0;
    int failed = 0;
    
    cout << "Running " << tests.size() << " test cases...\n\n";
    
    for (size_t i = 0; i < tests.size(); i++) {
        /* Convert infix to postfix */
        string postfix = infixToPostfix(tests[i].pattern);
        
        /* Build NFA */
        NFABuilder builder;
        auto nfa_start = builder.build(postfix);
        auto nfa_match = builder.get_match_state();
        int capture_count = builder.get_capture_count();
        
        /* Test match */
        NFASimulator simulator(nfa_start, nfa_match, capture_count);
        bool result = simulator.match(tests[i].text);
        bool success = (result == tests[i].expected);
        
        if (success) {
            passed++;
            cout << "✓ Test " << (i + 1) << " PASSED: pattern='" << tests[i].pattern 
                 << "' text='" << tests[i].text << "' postfix='" << postfix << "'\n";
        } else {
            failed++;
            cout << "✗ Test " << (i + 1) << " FAILED: pattern='" << tests[i].pattern 
                 << "' text='" << tests[i].text << "' postfix='" << postfix << "'\n";
            cout << "  Expected: " << tests[i].expected << ", Got: " << result << "\n";
        }
    }
    
    cout << "\n========================================\n";
    cout << "Results: " << passed << "/" << tests.size() << " passed, " << failed << " failed\n";
    cout << "Success rate: " << (100.0 * passed) / tests.size() << "%\n";
    cout << "========================================\n";
}

int main() {
    cout << "=== NFA Regex Matcher Test Suite ===\n";
    cout << "Using shared_ptr for State and PtrList with move semantics\n\n";
    
    runTests();
    
    /* Interactive mode */
    cout << "\n=== Interactive Mode ===\n";
    cout << "Type 'quit' or 'exit' to quit\n";
    string pattern, text;
    
    while (true) {
        cout << "\nPattern (infix): ";
        if (!getline(cin, pattern)) break;
        
        if (pattern == "quit" || pattern == "exit") break;
        if (pattern.empty()) continue;
        
        cout << "Text to match: ";
        if (!getline(cin, text)) break;
        
        /* Convert and match */
        string postfix = infixToPostfix(pattern);
        cout << "Postfix: " << postfix << "\n";
        
        /* Build NFA */
        NFABuilder builder;
        auto nfa_start = builder.build(postfix);
        auto nfa_match = builder.get_match_state();
        int capture_count = builder.get_capture_count();
        
        /* Test match */
        NFASimulator simulator(nfa_start, nfa_match, capture_count);
        bool result = simulator.match(text);
        
        cout << "Result: " << (result ? "✓ MATCH" : "✗ NO MATCH") << "\n";
        
        /* Show captures if any */
        if (result && capture_count > 0) {
            cout << "Captures:\n";
            for (int i = 0; i < capture_count; i++) {
                string cap = simulator.get_capture(i);
                cout << "  Group " << i << ": '" << cap << "'\n";
            }
        }
    }
    
    cout << "\nGoodbye!\n";
    return 0;
}