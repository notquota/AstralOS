
#include "font.h"

namespace Astral::Graphics {

static const uint8_t upper[26][7] = {
    {14,17,17,31,17,17,17}, // A
    {30,17,17,30,17,17,30}, // B
    {14,17,16,16,16,17,14}, // C
    {30,17,17,17,17,17,30}, // D
    {31,16,16,30,16,16,31}, // E
    {31,16,16,30,16,16,16}, // F
    {14,17,16,23,17,17,14}, // G
    {17,17,17,31,17,17,17}, // H
    {31,4,4,4,4,4,31},       // I
    {7,2,2,2,18,18,12},      // J
    {17,18,20,24,20,18,17},  // K
    {16,16,16,16,16,16,31},  // L
    {17,27,21,21,17,17,17},  // M
    {17,25,21,19,17,17,17},  // N
    {14,17,17,17,17,17,14},  // O
    {30,17,17,30,16,16,16},  // P
    {14,17,17,17,21,18,13},  // Q
    {30,17,17,30,20,18,17},  // R
    {15,16,16,14,1,1,30},    // S
    {31,4,4,4,4,4,4},        // T
    {17,17,17,17,17,17,14},  // U
    {17,17,17,17,17,10,4},   // V
    {17,17,17,21,21,21,10},  // W
    {17,17,10,4,10,17,17},   // X
    {17,17,10,4,4,4,4},     // Y
    {31,1,2,4,8,16,31}     // Z
};

static const uint8_t lower[26][7] = {
    {0,0,14,1,15,17,15},     // a
    {16,16,30,17,17,17,30},  // b
    {0,0,14,17,16,17,14},    // c
    {1,1,15,17,17,17,15},    // d
    {0,0,14,17,31,16,14},    // e
    {6,9,8,28,8,8,8},        // f
    {0,15,17,17,15,1,14},    // g
    {16,16,30,17,17,17,17},  // h
    {4,0,12,4,4,4,14},       // i
    {2,0,6,2,2,18,12},       // j
    {16,16,18,20,24,20,18},  // k
    {12,4,4,4,4,4,14},       // l
    {0,0,26,21,21,21,21},   // m
    {0,0,30,17,17,17,17},   // n
    {0,0,14,17,17,17,14},   // o
    {0,30,17,17,30,16,16},  // p
    {0,15,17,17,15,1,1},    // q
    {0,0,22,25,16,16,16},   // r
    {0,0,15,16,14,1,30},    // s
    {8,8,28,8,8,9,6},        // t
    {0,0,17,17,17,19,13},   // u
    {0,0,17,17,17,10,4},    // v
    {0,0,17,17,21,21,10},   // w
    {0,0,17,10,4,10,17},    // x
    {0,17,17,17,15,1,14},   // y
    {0,0,31,2,4,8,31}       // z
};

static const uint8_t digits[10][7] = {
    {14,17,19,21,25,17,14},
    {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2},
    {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},
    {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

#define GLYPH(name, ...) \
    static const uint8_t name[7] = {__VA_ARGS__}

GLYPH(blank,    0,0,0,0,0,0,0);
GLYPH(question, 14,17,1,2,4,0,4);
GLYPH(dot,      0,0,0,0,0,12,12);
GLYPH(comma,    0,0,0,0,6,4,8);
GLYPH(colon,    0,4,4,0,4,4,0);
GLYPH(semicolon,0,4,4,0,4,4,8);
GLYPH(dash,     0,0,0,31,0,0,0);
GLYPH(underscore,0,0,0,0,0,0,31);
GLYPH(slash,    1,1,2,4,8,16,16);
GLYPH(backslash,16,16,8,4,2,1,1);
GLYPH(at,       14,17,23,21,23,16,14);
GLYPH(hash,     10,10,31,10,31,10,10);
GLYPH(dollar,   4,15,20,14,5,30,4);
GLYPH(percent,  25,25,2,4,8,19,19);
GLYPH(ampersand,12,18,20,8,21,18,13);
GLYPH(asterisk, 0,21,14,31,14,21,0);
GLYPH(plus,     0,4,4,31,4,4,0);
GLYPH(equal,    0,0,31,0,31,0,0);
GLYPH(exclaim,  4,4,4,4,4,0,4);
GLYPH(quote,    10,10,10,0,0,0,0);
GLYPH(apostrophe,4,4,8,0,0,0,0);
GLYPH(backtick, 8,4,0,0,0,0,0);
GLYPH(tilde,    0,0,9,22,0,0,0);
GLYPH(caret,    4,10,17,0,0,0,0);
GLYPH(pipe,     4,4,4,4,4,4,4);
GLYPH(leftParen,2,4,8,8,8,4,2);
GLYPH(rightParen,8,4,2,2,2,4,8);
GLYPH(leftBracket,14,8,8,8,8,8,14);
GLYPH(rightBracket,14,2,2,2,2,2,14);
GLYPH(leftBrace,3,4,4,8,4,4,3);
GLYPH(rightBrace,24,4,4,2,4,4,24);
GLYPH(less,     2,4,8,16,8,4,2);
GLYPH(greater,  8,4,2,1,2,4,8);

#undef GLYPH

const uint8_t* glyph(char c) {
    if (c >= 'A' && c <= 'Z')
        return upper[c - 'A'];

    if (c >= 'a' && c <= 'z')
        return lower[c - 'a'];

    if (c >= '0' && c <= '9')
        return digits[c - '0'];

    switch (c) {
        case ' ': return blank;
        case '.': return dot;
        case ',': return comma;
        case ':': return colon;
        case ';': return semicolon;
        case '-': return dash;
        case '_': return underscore;
        case '/': return slash;
        case '\\': return backslash;
        case '@': return at;
        case '#': return hash;
        case '$': return dollar;
        case '%': return percent;
        case '&': return ampersand;
        case '*': return asterisk;
        case '+': return plus;
        case '=': return equal;
        case '!': return exclaim;
        case '"': return quote;
        case '\'': return apostrophe;
        case '`': return backtick;
        case '~': return tilde;
        case '^': return caret;
        case '|': return pipe;
        case '(': return leftParen;
        case ')': return rightParen;
        case '[': return leftBracket;
        case ']': return rightBracket;
        case '{': return leftBrace;
        case '}': return rightBrace;
        case '<': return less;
        case '>': return greater;
        case '?': return question;
        default: return question;
    }
}

}
