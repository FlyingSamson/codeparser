
(*
UnhandledCharacter:
*)

Test[
	TokenizeString["\\[SkeletonIndicator]"]
	,
	{LeafNode[Token`Unhandled, 
  "\\[SkeletonIndicator]", <|Source -> {{1, 1}, {1, 20}}|>]}
	,
	TestID->"TokenErrors-20190520-B1H0A6"
]

Test[
	TokenizeString["a::\\\""]
	,
	{LeafNode[Symbol, "a", <|Source -> {{1, 1}, {1, 1}}|>], 
 LeafNode[Token`ColonColon, "::", <|Source -> {{1, 2}, {1, 3}}|>], 
 LeafNode[Token`Error`UnhandledCharacter, 
  "\\\"", <|Source -> {{1, 4}, {1, 5}}|>], 
 LeafNode[Token`Error`EmptyString, 
  "", <|Source -> {{2, 0}, {2, 0}}|>]}
	,
	TestID->"TokenErrors-20190520-L5N7B0"
]



(*
UnterminatedComment:
*)

Test[
	TokenizeString["(*"]
	,
	{LeafNode[Token`Error`UnterminatedComment, 
  "(*", <|Source -> {{1, 1}, {1, 2}}|>]}
	,
	TestID->"TokenErrors-20190520-C8W1P2"
]




(*
ExpectedAlphaOrDollar:
*)


Test[
	TokenizeString["aaa`1"]
	,
	{LeafNode[Token`Error`ExpectedLetterlike, 
  "aaa`", <|Source -> {{1, 1}, {1, 4}}|>], 
 LeafNode[Integer, "1", <|Source -> {{1, 5}, {1, 5}}|>]}
	,
	TestID->"TokenErrors-20190520-H9P0H9"
]




(*
EmptyString:
*)

Test[
	TokenizeString["a::"]
	,
	{LeafNode[Symbol, "a", <|Source -> {{1, 1}, {1, 1}}|>], LeafNode[Token`ColonColon, "::",
		<|Source -> {{1, 2}, {1, 3}}|>], LeafNode[Token`Error`EmptyString, "", <|Source -> {{2, 0}, {2, 0}}|>]}
	,
	TestID->"TokenErrors-20190520-R2P3A3"
]

Test[
	TokenizeString["a>>"]
	,
	{LeafNode[Symbol, "a", <|Source -> {{1, 1}, {1, 1}}|>],
		LeafNode[Token`GreaterGreater, ">>", <|Source -> {{1, 2}, {1, 3}}|>],
		LeafNode[Token`Error`EmptyString, "", <|Source -> {{2, 0}, {2, 0}}|>]}
	,
	TestID->"TokenErrors-20190520-M3H7E9"
]


(*
UnterminatedString:
*)

Test[
	TokenizeString["\""]
	,
	{LeafNode[Token`Error`UnterminatedString, "\"", <|Source -> {{1, 1}, {1, 1}}|>]}
	,
	TestID->"TokenErrors-20190520-L6N6S8"
]









(*
InvalidBase:
*)

Test[
	TokenizeString["37^^2"]
	,
	{LeafNode[Token`Error`InvalidBase, "37", <|Source -> {{1, 1}, {1, 2}}|>],
		LeafNode[Token`Caret, "^", <|Source -> {{1, 3}, {1, 3}}|>],
		LeafNode[Token`Caret, "^", <|Source -> {{1, 4}, {1, 4}}|>],
		LeafNode[Integer, "2", <|Source -> {{1, 5}, {1, 5}}|>]}
	,
	TestID->"TokenErrors-20190520-Q9B9R6"
]








(*
ExpectedDigitOrAlpha:
*)

Test[
	TokenizeString["2^^3"]
	,
	{LeafNode[Token`Error`ExpectedDigitOrAlpha, "2^^", <|Source -> {{1, 1}, {1, 3}}|>],
		LeafNode[Integer, "3", <|Source -> {{1, 4}, {1, 4}}|>]}
	,
	TestID->"TokenErrors-20190520-B7G4V4"
]


Test[
	TokenizeString["2^^@"]
	,
	{LeafNode[Token`Error`ExpectedDigitOrAlpha, "2^^", <|Source -> {{1, 1}, {1, 3}}|>],
		LeafNode[Token`At, "@", <|Source -> {{1, 4}, {1, 4}}|>]}
	,
	TestID->"TokenErrors-20190520-J3Q2S7"
]




(*
ExpectedAccuracy:
*)

Test[
	TokenizeString["1.2``->3"]
	,
	{LeafNode[Token`Error`ExpectedAccuracy, "1.2``-", <|Source -> {{1, 1}, {1, 6}}|>], LeafNode[Token`Greater, ">",
		<|Source -> {{1, 7}, {1, 7}}|>], LeafNode[Integer, "3", <|Source -> {{1, 8}, {1, 8}}|>]}
	,
	TestID->"TokenErrors-20190520-B2J9I4"
]








(*
ExpectedExponent:
*)

Test[
	TokenizeString["123*^"]
	,
	{LeafNode[Token`Error`ExpectedExponent, "123*^", <|Source -> {{1, 1}, {1, 5}}|>]}
	,
	TestID->"TokenErrors-20190520-L1J8C1"
]





(*
ExpectedEqual:
*)

Test[
	TokenizeString["a ^: f"]
	,
	{LeafNode[Symbol, "a", <|Source -> {{1, 1}, {1, 1}}|>], LeafNode[Token`WhiteSpace, " ",
		<|Source -> {{1, 2}, {1, 2}}|>], LeafNode[Token`Error`ExpectedEqual, "^:", <|Source -> {{1, 3}, {1, 4}}|>],
		LeafNode[Token`WhiteSpace, " ", <|Source -> {{1, 5}, {1, 5}}|>], LeafNode[Symbol, "f", <|Source -> {{1, 6}, {1, 6}}|>]}
	,
	TestID->"TokenErrors-20190520-M3N7T5"
]






