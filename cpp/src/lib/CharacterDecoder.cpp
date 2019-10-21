
#include "CharacterDecoder.h"

#include "ByteDecoder.h"
#include "SourceManager.h"
#include "Utils.h"
#include "CharacterMaps.h"
#include "CodePoint.h"
#include "API.h"

#include <sstream>

CharacterDecoder::CharacterDecoder() : _currentWLCharacter(0), sourceCharacterQueue(), Issues(), libData() {}

void CharacterDecoder::init(WolframLibraryData libDataIn) {
    _currentWLCharacter = WLCharacter(0);
    sourceCharacterQueue.clear();
    Issues.clear();
    
    libData = libDataIn;
}

void CharacterDecoder::deinit() {
    sourceCharacterQueue.clear();
    Issues.clear();
    
    libData = nullptr;
}

//
// Returns a useful character
//
// Keeps track of character counts
//
WLCharacter CharacterDecoder::nextWLCharacter(NextWLCharacterPolicy policy) {
    
    //
    // handle the queue before anything else
    //
    // We know only single-SourceCharacter WLCharacters are queued
    //
    if (!sourceCharacterQueue.empty()) {
        
        auto p = sourceCharacterQueue[0];
        
        // erase first
        sourceCharacterQueue.erase(sourceCharacterQueue.begin());
        
        auto curSource = p.first;
        auto location = p.second;
        
        TheSourceManager->setSourceLocation(location);
        
        TheSourceManager->setWLCharacterStart();
        
        TheSourceManager->setWLCharacterEnd();
        
        _currentWLCharacter = WLCharacter(curSource.to_point());
        
        if (_currentWLCharacter.isStrange()) {
            
            auto Src = TheSourceManager->getWLCharacterSource();
            
            auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNEXPECTEDCHARACTER, "Unexpected character: ``" + _currentWLCharacter.graphicalString() + "``.", SYNTAXISSUESEVERITY_WARNING, Src, 0.95, {}));
            
            Issues.push_back(std::move(I));
        }
        
        return _currentWLCharacter;
    }
    
    auto curSource = TheByteDecoder->nextSourceCharacter();
    
    if (curSource == SourceCharacter(CODEPOINT_ENDOFFILE)) {
        
        TheSourceManager->setWLCharacterStart();
        TheSourceManager->setWLCharacterEnd();
        
        _currentWLCharacter = WLCharacter(CODEPOINT_ENDOFFILE);
        
        return _currentWLCharacter;
    }
    
    if (curSource != SourceCharacter('\\') ||
        ((policy & DISABLE_ESCAPES) == DISABLE_ESCAPES)) {
        
        TheSourceManager->setWLCharacterStart();
        TheSourceManager->setWLCharacterEnd();
        
        _currentWLCharacter = WLCharacter(curSource.to_point());
        
        if (_currentWLCharacter.isStrange()) {
            
            auto Src = TheSourceManager->getWLCharacterSource();
            
            auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNEXPECTEDCHARACTER, "Unexpected character: ``" + _currentWLCharacter.graphicalString() + "``.", SYNTAXISSUESEVERITY_WARNING, Src, 0.95, {}));
            
            Issues.push_back(std::move(I));
        }
        
        return _currentWLCharacter;
    }
    
    //
    // Handle \
    //
    // handle escapes like line continuation and special characters
    //
    
    TheSourceManager->setWLCharacterStart();
    auto CharacterStart = TheSourceManager->getWLCharacterStart();
    curSource = TheByteDecoder->nextSourceCharacter();
    
    switch (curSource.to_point()) {
        case '\n':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINECONTINUATION_LF, ESCAPE_SINGLE);
            break;
        case '\r': {
            
            //
            // Line continuation
            //
            
            if ((policy & LC_UNDERSTANDS_CRLF) == LC_UNDERSTANDS_CRLF) {
                
                auto CRLoc = TheSourceManager->getSourceLocation();
                
                auto c = TheByteDecoder->nextSourceCharacter();
                
                if (c != SourceCharacter('\n')) {
                    
                    //
                    // It is possible to have \ followed by a single \r, and no accompanying \n
                    // Keep things simple and just treat it like a regular line continuation.
                    // Stray \r is reported elsewhere
                    //
                    
                    auto Loc = TheSourceManager->getSourceLocation();
                    
                    TheSourceManager->setSourceLocation(CharacterStart);
                    TheSourceManager->setWLCharacterStart();
                    TheSourceManager->setSourceLocation(CRLoc);
                    TheSourceManager->setWLCharacterEnd();
                    
                    if (c.isEndOfFile()) {
                        append(c, Loc);
                    } else {
                        for (auto b : c) {
                            TheByteDecoder->append(b, Loc);
                        }
                    }
                    
                    _currentWLCharacter = WLCharacter(CODEPOINT_LINECONTINUATION_CR, ESCAPE_SINGLE);
                    
                } else {
                    _currentWLCharacter = WLCharacter(CODEPOINT_LINECONTINUATION_CRLF, ESCAPE_SINGLE);
                }
                
            } else {
                _currentWLCharacter = WLCharacter(CODEPOINT_LINECONTINUATION_CR, ESCAPE_SINGLE);
            }
        }
            break;
        case '[':
            _currentWLCharacter = handleLongName(curSource, CharacterStart, policy, false);
            break;
        case ':':
            _currentWLCharacter = handle4Hex(curSource, CharacterStart, policy);
            break;
        case '.':
            _currentWLCharacter = handle2Hex(curSource, CharacterStart, policy);
            break;
        case '|':
            _currentWLCharacter = handle6Hex(curSource, CharacterStart, policy);
            break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
            _currentWLCharacter = handleOctal(curSource, CharacterStart, policy);
            break;
        //
        // Simple escaped characters
        // \b \f \n \r \t
        //
        case 'b':
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_BACKSPACE, ESCAPE_SINGLE);
            break;
        case 'f':
            //
            // \f is NOT a space character (but inside of strings, it does have special meaning)
            //
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_FORMFEED, ESCAPE_SINGLE);
            break;
        case 'n':
            //
            // \n is NOT a newline character (but inside of strings, it does have special meaning)
            //
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_LINEFEED, ESCAPE_SINGLE);
            break;
        case 'r':
            //
            // \r is NOT a newline character (but inside of strings, it does have special meaning)
            //
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_CARRIAGERETURN, ESCAPE_SINGLE);
            break;
        case 't':
            //
            // \t is NOT a space character (but inside of strings, it does have special meaning)
            //
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_TAB, ESCAPE_SINGLE);
            break;
        //
        // \\ \" \< \>
        //
        // String meta characters
        // What are \< and \> ?
        // https://mathematica.stackexchange.com/questions/105018/what-are-and-delimiters-in-box-expressions
        // https://stackoverflow.com/q/6065887
        //
        case '"':
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_DOUBLEQUOTE, ESCAPE_SINGLE);
            break;
        case '\\':
            
            //
            // if inside a string, then test whether this \ is the result of the "feature" of
            // converting "\[Alpa]" into "\\[Alpa]", copying that, and then never giving any further warnings
            // when dealing with "\\[Alpa]"
            //
            
            if ((policy & UNLIKELY_ESCAPE_CHECKING) == UNLIKELY_ESCAPE_CHECKING) {
                
                auto oldSourceLoc = TheSourceManager->getSourceLocation();
                
                auto test = TheByteDecoder->nextSourceCharacter();
                
                if (test == SourceCharacter('[')) {
                    
                    auto tmpPolicy = policy;
                    
                    tmpPolicy = tmpPolicy | DISABLE_CHARACTERDECODINGISSUES;
                    
                    handleLongName(test, CharacterStart+1, tmpPolicy, true);
                    
                } else {
                    
                    //
                    // '[' was not after \\, so must put back whatever was after \\
                    //
                    // Must append in TheByteDecoder
                    // Cannot append in TheCharacterDecoder, because only single-SourceCharacter WLCharacters are expected in
                    // TheCharacterDecoder queue
                    //
                    
                    //
                    // test for '\n' here because we want to make sure to have the correct SourceLocation
                    // A newline means advancing to next line and resetting Column to 0
                    // Note: '\n' is not the only single-byte character that can start a newline
                    // FIXME: If test == SourceCharacter('\r'), then there could also be a newline, either
                    // as \r\n or as a stray \r that is not paired with a \n
                    //
                    if (test == SourceCharacter('\n')) {
                        
                        //
                        // CharacterStart is the first \, so then queuedCharacterStart is
                        // start of the next line
                        //
                        auto queuedCharacterStart = CharacterStart.nextLine();
                        
                        TheByteDecoder->append('\n', queuedCharacterStart);
                        
                    } else {
                        
                        //
                        // CharacterStart is the first \, so then queuedCharacterStart is the first
                        // character to be queued
                        //
                        auto queuedCharacterStart = CharacterStart+2;
                        
                        if (test.isEndOfFile()) {
                            append(test, queuedCharacterStart);
                        } else {
                            for (auto b : test) {
                                TheByteDecoder->append(b, queuedCharacterStart);
                            }
                        }
                    }
                    
                    //
                    // and make sure to reset SourceLoc
                    //
                    TheSourceManager->setSourceLocation(oldSourceLoc);
                }
            }
            
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_BACKSLASH, ESCAPE_SINGLE);
            break;
        case '<':
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_OPEN, ESCAPE_SINGLE);
            break;
        case '>':
            _currentWLCharacter = WLCharacter(CODEPOINT_STRINGMETA_CLOSE, ESCAPE_SINGLE);
            break;
        //
        // Linear syntax characters
        // \! \% \& \( \) \* \+ \/ \@ \^ \_ \` \<space>
        //
        case '!':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_BANG, ESCAPE_SINGLE);
            break;
        case '%':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_PERCENT, ESCAPE_SINGLE);
            break;
        case '&':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_AMP, ESCAPE_SINGLE);
            break;
        case '(':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_OPENPAREN, ESCAPE_SINGLE);
            break;
        case ')':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_CLOSEPAREN, ESCAPE_SINGLE);
            break;
        case '*':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_STAR, ESCAPE_SINGLE);
            break;
        case '+':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_PLUS, ESCAPE_SINGLE);
            break;
        case '/':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_SLASH, ESCAPE_SINGLE);
            break;
        case '@':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_AT, ESCAPE_SINGLE);
            break;
        case '^':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_CARET, ESCAPE_SINGLE);
            break;
        case '_':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_UNDER, ESCAPE_SINGLE);
            break;
        case '`':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_BACKTICK, ESCAPE_SINGLE);
            break;
        case ' ':
            _currentWLCharacter = WLCharacter(CODEPOINT_LINEARSYNTAX_SPACE, ESCAPE_SINGLE);
            break;
        default: {
            
            //
            // Anything else
            //
            
            auto Loc = TheSourceManager->getSourceLocation();
            
            //
            // Make the warnings a little more relevant
            //
            
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                if (curSource.isUpper() && curSource.isHex()) {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + curSourceGraphicalStr + "XXX]", Source(CharacterStart, Loc), "\\[" + curSourceGraphicalStr + "XXX]")));
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\:" + curSourceGraphicalStr + "XXX", Source(CharacterStart, Loc), "\\:" + curSourceGraphicalStr + "XXX")));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character ``\\") + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else if (curSource.isUpper()) {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + curSourceGraphicalStr + "XXX]", Source(CharacterStart, Loc), "\\[" + curSourceGraphicalStr + "XXX]")));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character ``\\") + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else if (curSource.isHex()) {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\:" + curSourceGraphicalStr + "xxx", Source(CharacterStart, Loc), "\\:" + curSourceGraphicalStr + "xxx")));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character ``\\") + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else if (curSource.isEndOfFile()) {
                    
                    //
                    // Do not know what a good suggestion would be for \<EOF>
                    //
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\" + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\" + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character ``\\") + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
            
            //
            // Keep these treated as 2 characters. This is how bad escapes are handled in WL strings.
            // And has the nice benefit of the single \ still giving an error at top-level
            //
            
            _currentWLCharacter = WLCharacter('\\');
            
            TheSourceManager->setSourceLocation(CharacterStart);
            TheSourceManager->setWLCharacterStart();
            TheSourceManager->setWLCharacterEnd();
            
            append(curSource, Loc);
            
            break;
        }
    }
    
    TheSourceManager->setWLCharacterEnd();
    
    if (_currentWLCharacter.isStrange()) {
        
        auto Src = TheSourceManager->getWLCharacterSource();
        
        auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNEXPECTEDCHARACTER, "Unexpected character: ``" + _currentWLCharacter.graphicalString() + "``.", SYNTAXISSUESEVERITY_WARNING, Src, 0.95, {}));
        
        Issues.push_back(std::move(I));
    }
    
    return _currentWLCharacter;
}

//
// Append character c
//
void CharacterDecoder::append(SourceCharacter c, SourceLocation location) {
    sourceCharacterQueue.push_back(std::make_pair(c, location));
}

WLCharacter CharacterDecoder::currentWLCharacter() const {
    return _currentWLCharacter;
}

//
// return the next WL character
//
// CharacterStart: location of \
//
WLCharacter CharacterDecoder::handleLongName(SourceCharacter curSourceIn, SourceLocation CharacterStart, NextWLCharacterPolicy policy, bool unlikelyEscapeChecking) {
    
    auto curSource = curSourceIn;
    
    assert(curSource == SourceCharacter('['));
    
    //
    // Do not write leading \[ or trailing ] to LongName
    //
    std::ostringstream LongName;
    
    curSource = TheByteDecoder->nextSourceCharacter();
    
    auto wellFormed = false;
    
    auto atleast1DigitOrAlpha = false;
    
    //
    // Read at least 1 alnum before entering loop
    //
    if (curSource.isAlphaOrDigit()) {
        
        atleast1DigitOrAlpha = true;
        
        LongName.put(curSource.to_char());
        
        curSource = TheByteDecoder->nextSourceCharacter();
        
        while (true) {
            
            //
            // No need to check isAbort() inside decoder loops
            //
            
            if (curSource.isAlphaOrDigit()) {
                
                LongName.put(curSource.to_char());
                
                curSource = TheByteDecoder->nextSourceCharacter();
                
            } else if (curSource == SourceCharacter(']')) {
                
                wellFormed = true;
                
                break;
                
            } else {
                
                //
                // Unrecognized
                //
                // Something like \[A!] which is not a long name
                //
                
                break;
            }
        }
    }
    
    auto LongNameStr = LongName.str();
    
    if (!wellFormed) {
        
        //
        // Not well-formed
        //
        
        auto Loc = TheSourceManager->getSourceLocation();
        
        if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
            
            if (atleast1DigitOrAlpha) {
                
                //
                // Something like \[A!]
                // Something like \[CenterDot\]
                //
                // Make the warning message a little more relevant
                //
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \[A<EOF>
                    //
                    
                    auto suggestion = longNameSuggestion(LongNameStr);
                    
                    std::vector<CodeActionPtr> Actions;
                    
                    if (!suggestion.empty()) {
                        Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + suggestion + "]", Source(CharacterStart, CharacterStart+1+LongNameStr.size()), "\\[" + suggestion + "]")));
                    }
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\[") + LongNameStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+1+LongNameStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    auto suggestion = longNameSuggestion(LongNameStr);
                    
                    std::vector<CodeActionPtr> Actions;
                    
                    if (!suggestion.empty()) {
                        Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + suggestion + "]", Source(CharacterStart, Loc), "\\[" + suggestion + "]")));
                    }
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\[") + LongNameStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
                
            } else {
                
                //
                // Malformed some other way
                //
                // Something like \[!
                // Something like \[*
                //
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \[<EOF>
                    //
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\[" + LongNameStr, Source(CharacterStart, CharacterStart+1+LongNameStr.size()), "\\\\[" + LongNameStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\[") + LongNameStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+1+LongNameStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\[" + LongNameStr + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\[" + LongNameStr + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\[") + LongNameStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
        }
        //
        // Should we report "\\[]" as unlikely?
        //
//        else if (unlikelyEscapeChecking) {
//
//            auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNLIKELYESCAPESEQUENCE, std::string("Unlikely escape sequence: ``\\\\[") + LongNameStr + "``", SYNTAXISSUESEVERITY_REMARK, Source(CharacterStart-1, Loc), 0.33, {}));
//
//            Issues.push_back(std::move(I));
//        }
        
        TheSourceManager->setSourceLocation(CharacterStart);
        TheSourceManager->setWLCharacterStart();
        TheSourceManager->setWLCharacterEnd();
        
        TheByteDecoder->append('[', CharacterStart+1);
        for (size_t i = 0; i < LongNameStr.size(); i++) {
            TheByteDecoder->append(LongNameStr[i], CharacterStart+2+i);
        }
        if (curSource.isEndOfFile()) {
            append(curSource, Loc);
        } else {
            for (auto b : curSource) {
                TheByteDecoder->append(b, Loc);
            }
        }
        
        return WLCharacter('\\');
    }
    
    //
    // Well-formed
    //
    
    //
    // if unlikelyEscapeChecking, then make sure to append all of the Source characters again
    //
    
    auto it = LongNameToCodePointMap.find(LongNameStr);
    auto found = (it != LongNameToCodePointMap.end());
    if (!found || unlikelyEscapeChecking) {
        
        //
        // Unrecognized name
        //
        // If found and unlikelyEscapeChecking, then still come in here.
        //
        
        auto Loc = TheSourceManager->getSourceLocation();
        
        if (!found) {
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                auto suggestion = longNameSuggestion(LongNameStr);
                
                std::vector<CodeActionPtr> Actions;
                if (!suggestion.empty()) {
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + suggestion + "]", Source(CharacterStart, Loc), "\\[" + suggestion + "]")));
                }
                
                auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\[") + LongNameStr + "]``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                
                Issues.push_back(std::move(I));
                
            } else if (unlikelyEscapeChecking) {
                
                auto suggestion = longNameSuggestion(LongNameStr);
                
                std::vector<CodeActionPtr> Actions;
                if (!suggestion.empty()) {
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\[" + suggestion + "]", Source(CharacterStart-1, Loc), "\\[" + suggestion + "]")));
                }
                
                auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNEXPECTEDESCAPESEQUENCE, std::string("Unexpected escape sequence: ``\\\\[") + LongNameStr + "]``.", SYNTAXISSUESEVERITY_REMARK, Source(CharacterStart-1, Loc), 0.33, std::move(Actions)));
                
                Issues.push_back(std::move(I));
            }
        }
        
        TheSourceManager->setSourceLocation(CharacterStart);
        TheSourceManager->setWLCharacterStart();
        TheSourceManager->setWLCharacterEnd();
        
        TheByteDecoder->append('[', CharacterStart+1);
        for (size_t i = 0; i < LongNameStr.size(); i++) {
            TheByteDecoder->append(LongNameStr[i], CharacterStart+2+i);
        }
        TheByteDecoder->append(']', Loc);
        
        return WLCharacter('\\');
    }
    
    //
    // Success!
    //
    
    auto Loc = TheSourceManager->getSourceLocation();
    
    auto point = it->second;
    
    if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
        
        //
        // The well-formed, recognized name could still be unsupported or undocumented
        //
        if (Utils::isUnsupportedLongName(LongNameStr)) {
            
            auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNSUPPORTEDCHARACTER, std::string("Unsupported character: ``\\[") + LongNameStr + "]``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 0.95, {}));
            
            Issues.push_back(std::move(I));
            
        } else if (Utils::isUndocumentedLongName(LongNameStr)) {
            
            auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNDOCUMENTEDCHARACTER, std::string("Undocumented character: ``\\[") + LongNameStr + "]``.", SYNTAXISSUESEVERITY_REMARK, Source(CharacterStart, Loc), 0.95, {}));
            
            Issues.push_back(std::move(I));
        }
    }
    
    return WLCharacter(point, ESCAPE_LONGNAME);
}

//
// return the next WL character
//
// CharacterStart: location of \
//
WLCharacter CharacterDecoder::handle4Hex(SourceCharacter curSourceIn, SourceLocation CharacterStart, NextWLCharacterPolicy policy) {
    
    auto curSource = curSourceIn;
    
    assert(curSource == SourceCharacter(':'));
    
    std::ostringstream Hex;
    
    
    for (auto i = 0; i < 4; i++) {
        
        curSource = TheByteDecoder->nextSourceCharacter();
        
        if (curSource.isHex()) {
            
            Hex.put(curSource.to_char());
            
        } else {
            
            //
            // Not well-formed
            //
            // Something like \:z
            //
            
            auto HexStr = Hex.str();
            
            auto Loc = TheSourceManager->getSourceLocation();
            
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \:<EOF>
                    //
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\:" + HexStr, Source(CharacterStart, CharacterStart+1+HexStr.size()), "\\\\:" + HexStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\:") + HexStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+1+HexStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\:" + HexStr + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\:" + HexStr + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\:") + HexStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
            
            TheSourceManager->setSourceLocation(CharacterStart);
            TheSourceManager->setWLCharacterStart();
            TheSourceManager->setWLCharacterEnd();
            
            TheByteDecoder->append(':', CharacterStart+1);
            for (size_t i = 0; i < HexStr.size(); i++) {
                TheByteDecoder->append(HexStr[i], CharacterStart+2+i);
            }
            if (curSource.isEndOfFile()) {
                append(curSource, Loc);
            } else {
                for (auto b : curSource) {
                    TheByteDecoder->append(b, Loc);
                }
            }
            
            return WLCharacter('\\');
        }
    }
    
    auto HexStr = Hex.str();
    
    auto it = ToSpecialMap.find(HexStr);
    if (it == ToSpecialMap.end()) {
        
        auto point = Utils::parseInteger(HexStr, 16);
        
        return WLCharacter(point, ESCAPE_4HEX);
    }
    
    auto point = it->second;
    
    return WLCharacter(point, ESCAPE_4HEX);
}

//
// return the next WL character
//
// CharacterStart: location of \
//
WLCharacter CharacterDecoder::handle2Hex(SourceCharacter curSourceIn, SourceLocation CharacterStart, NextWLCharacterPolicy policy) {
    
    auto curSource = curSourceIn;
    
    assert(curSource == SourceCharacter('.'));
    
    std::ostringstream Hex;
    
    for (auto i = 0; i < 2; i++) {
        
        curSource = TheByteDecoder->nextSourceCharacter();
        
        if (curSource.isHex()) {
            
            Hex.put(curSource.to_char());
            
        } else {
            
            //
            // Not well-formed
            //
            // Something like \.z
            //
            
            auto HexStr = Hex.str();
            
            auto Loc = TheSourceManager->getSourceLocation();
            
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \.<EOF>
                    //
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\." + HexStr, Source(CharacterStart, CharacterStart+1+HexStr.size()), "\\\\." + HexStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, "Unrecognized character: ``\\." + HexStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+1+HexStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\." + HexStr + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\." + HexStr + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, "Unrecognized character: ``\\." + HexStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
            
            TheSourceManager->setSourceLocation(CharacterStart);
            TheSourceManager->setWLCharacterStart();
            TheSourceManager->setWLCharacterEnd();
            
            TheByteDecoder->append('.', CharacterStart+1);
            for (size_t i = 0; i < HexStr.size(); i++) {
                TheByteDecoder->append(HexStr[i], CharacterStart+2+i);
            }
            if (curSource.isEndOfFile()) {
                append(curSource, Loc);
            } else {
                for (auto b : curSource) {
                    TheByteDecoder->append(b, Loc);
                }
            }
            
            return WLCharacter('\\');
        }
    }
    
    auto HexStr = Hex.str();
    
    auto it = ToSpecialMap.find(HexStr);
    if (it == ToSpecialMap.end()) {
        
        auto point = Utils::parseInteger(HexStr, 16);
        
        return WLCharacter(point, ESCAPE_2HEX);
    }
    
    auto point = it->second;
    
    return WLCharacter(point, ESCAPE_2HEX);
}

//
// return the next WL character
//
// CharacterStart: location of \
//
WLCharacter CharacterDecoder::handleOctal(SourceCharacter curSourceIn, SourceLocation CharacterStart, NextWLCharacterPolicy policy) {
    
    auto curSource = curSourceIn;
    
    assert(curSource.isOctal());
    
    std::ostringstream Octal;
    
    Octal.put(curSource.to_char());
    
    for (auto i = 0; i < 3-1; i++) {
        
        curSource = TheByteDecoder->nextSourceCharacter();
        
        if (curSource.isOctal()) {
            
            Octal.put(curSource.to_char());
            
        } else {
            
            //
            // Not well-formed
            //
            // Something like \z
            //
            
            auto OctalStr = Octal.str();
            
            auto Loc = TheSourceManager->getSourceLocation();
            
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \0<EOF>
                    //
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\" + OctalStr, Source(CharacterStart, CharacterStart+OctalStr.size()), "\\\\" + OctalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\") + OctalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+OctalStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\" + OctalStr + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\" + OctalStr + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\") + OctalStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
            
            TheSourceManager->setSourceLocation(CharacterStart);
            TheSourceManager->setWLCharacterStart();
            TheSourceManager->setWLCharacterEnd();
            
            for (size_t i = 0; i < OctalStr.size(); i++) {
                TheByteDecoder->append(OctalStr[i], CharacterStart+1+i);
            }
            if (curSource.isEndOfFile()) {
                append(curSource, Loc);
            } else {
                for (auto b : curSource) {
                    TheByteDecoder->append(b, Loc);
                }
            }
            
            return WLCharacter('\\');
        }
    }
    
    auto OctalStr = Octal.str();
    
    auto it = ToSpecialMap.find(OctalStr);
    if (it == ToSpecialMap.end()) {
        
        auto point = Utils::parseInteger(OctalStr, 8);
        
        return WLCharacter(point, ESCAPE_OCTAL);
    }
    
    auto point = it->second;
    
    return WLCharacter(point, ESCAPE_OCTAL);
}

//
// return the next WL character
//
// CharacterStart: location of \
//
WLCharacter CharacterDecoder::handle6Hex(SourceCharacter curSourceIn, SourceLocation CharacterStart, NextWLCharacterPolicy policy) {
    
    auto curSource = curSourceIn;
    
    assert(curSource == SourceCharacter('|'));
    
    std::ostringstream Hex;
    
    for (auto i = 0; i < 6; i++) {
        
        curSource = TheByteDecoder->nextSourceCharacter();
        
        if (curSource.isHex()) {
            
            Hex.put(curSource.to_char());
            
        } else {
            
            //
            // Not well-formed
            //
            // Something like \|z
            //
            
            auto HexStr = Hex.str();
            
            auto Loc = TheSourceManager->getSourceLocation();
            
            if ((policy & DISABLE_CHARACTERDECODINGISSUES) != DISABLE_CHARACTERDECODINGISSUES) {
                
                if (curSource.isEndOfFile()) {
                    
                    //
                    // Special case of \|<EOF>
                    //
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\|" + HexStr, Source(CharacterStart, CharacterStart+1+HexStr.size()), "\\\\|" + HexStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\|") + HexStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, CharacterStart+1+HexStr.size()), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                    
                } else {
                    
                    auto curSourceGraphicalStr = WLCharacter(curSource.to_point()).graphicalString();
                    
                    std::vector<CodeActionPtr> Actions;
                    Actions.push_back(CodeActionPtr(new ReplaceTextCodeAction("Replace with \\\\|" + HexStr + curSourceGraphicalStr, Source(CharacterStart, Loc), "\\\\|" + HexStr + curSourceGraphicalStr)));
                    
                    auto I = std::unique_ptr<Issue>(new SyntaxIssue(SYNTAXISSUETAG_UNRECOGNIZEDCHARACTER, std::string("Unrecognized character: ``\\|") + HexStr + curSourceGraphicalStr + "``.", SYNTAXISSUESEVERITY_ERROR, Source(CharacterStart, Loc), 1.0, std::move(Actions)));
                    
                    Issues.push_back(std::move(I));
                }
            }
            
            TheSourceManager->setSourceLocation(CharacterStart);
            TheSourceManager->setWLCharacterStart();
            TheSourceManager->setWLCharacterEnd();
            
            TheByteDecoder->append('|', CharacterStart+1);
            for (size_t i = 0; i < HexStr.size(); i++) {
                TheByteDecoder->append(HexStr[i], CharacterStart+2+i);
            }
            if (curSource.isEndOfFile()) {
                append(curSource, Loc);
            } else {
                for (auto b : curSource) {
                    TheByteDecoder->append(b, Loc);
                }
            }
            
            return WLCharacter('\\');
        }
    }
    
    auto HexStr = Hex.str();
    
    auto it = ToSpecialMap.find(HexStr);
    if (it == ToSpecialMap.end()) {
        
        auto point = Utils::parseInteger(HexStr, 16);
        
        return WLCharacter(point, ESCAPE_6HEX);
    }
    
    auto point = it->second;
    
    return WLCharacter(point, ESCAPE_6HEX);
}

std::vector<std::unique_ptr<Issue>>& CharacterDecoder::getIssues() {
    return Issues;
}

//
// example:
// input: Alpa
// return Alpha
//
// Return empty string if no suggestion.
//
std::string CharacterDecoder::longNameSuggestion(std::string input) {
    
    if (libData) {
        MLINK link = libData->getMathLink(libData);
        MLPutFunction(link, "EvaluatePacket", 1);
        MLPutFunction(link, "AST`Library`LongNameSuggestion", 1);
        MLPutUTF8String(link, reinterpret_cast<unsigned const char *>(input.c_str()), static_cast<int>(input.size()));
        libData->processMathLink(link);
        auto pkt = MLNextPacket(link);
        if (pkt == RETURNPKT) {
            
            ScopedMLString str(link);
            str.read();
            
            return reinterpret_cast<const char *>(str.get());
        }
    }
    
    return "";
}

std::unique_ptr<CharacterDecoder> TheCharacterDecoder = nullptr;

