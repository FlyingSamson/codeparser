
TODO: write up stuff about character encodings



# Raw

characters like \[RawReturn] are a way of escaping that character

Poorly understood

Perhaps essentially unused



A good philosophy that I follow is to treat the Raw characters as escaped versions of their normal characters

\[RawTab] is similar to \t, and is NOT the same as actual 0x09 character
\[NewLine] is similar to \n, and is NOT the same as actual 0x0a character
etc.








character bits


#
SourceCharacters

storing characters in uint32_t, we can spare 11 bits to store metadata


//
// Version 1 of SourceCharacter encoding
//
// 32 bits:
//
// vutsrqponmlkjihgfedcba9876543210
//            ^~~~~~~~~~~~~~~~~~~~~
//            Character bits (21 bits)
//           ^
//           Sign bit
//         ^~
//         Synchronization bits (2 bits)
//    ^~~~~
//    Test bits (5 bits)
// ^~~
// Currently unused (3 bits)
//




## synchronization bits

00 - the encompassing WLCharacter is a single SourceCharacter

01 - the encompassing WLCharacter is multi-SourceCharacter and starts here

10 - the encompassing WLCharacter is multi-SourceCharacter and middle here

11 - the encompassing WLCharacter is multi-SourceCharacter and ends here


a
00

\  [  A  l  p  h  a  ]
01 10 10 10 10 10 10 11


longest long name is DiscretionaryParagraphSeparator

31 SourceCharacters long

It would be... prohibitive... to encode a run of 31 "1" bits to encode the length 









## test bits

bit 0: isDigitOrAlpha
useful for longnames

bit 1: isHex (only set if digit or alpha)
useful for 2hex, 4hex, 6hex

bit 2: isOctal (only set if digit or alpha)
useful for octal

bit 3: isUpper (only set if alpha)
only used in warnings

bit 4: actual newline
an actual newline is either:
\n  or
\r  with NO \n immediately following (this is rare and mal-formed)






#
WLCharacters

storing characters in uint32_t, we can spare 11 bits to store metadata


//
// Version 1 of WLCharacter encoding
//
// 32 bits:
//
// vutsrqponmlkjihgfedcba9876543210
//            ^~~~~~~~~~~~~~~~~~~~~
//            Character bits (21 bits)
//           ^
//           Sign bit
//        ^~~
//        EscapeFormat bits (3 bits)
//       ^
//       Multi-byte (1 bit)
//    ^~~
//    Category (3 bits)
// ^~~
// Test bits (3 bits)
//







category is:
000 letterlike
001 digit
010 punctuation
011 space
100 newline
101 uninterpretable
110 <unused>
111 <unused>



escape is one of:
000 NONE
001 SINGLE
010 LONGNAME
011 2HEX
100 4HEX
101 6HEX
110 OCTAL
111 <unused>


test bits:

bit 0: isAlpha (only set if letterlike)

bit 1: isStrange (only set if letterlike)

bit 2: isLinearSyntax (only set if punctuation)


















