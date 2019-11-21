

AST docs


## Nodes

All nodes have a uniform structure:

`Node[tag or operator, contents or children, opts]`




We take advantage of the symbolic nature of WL and use the function symbols themselves for tags:


a+b is parsed as:
InfixNode[Plus, {LeafNode[Symbol, "a", <||>], LeafNode[Token`Plus, "+", <||>], LeafNode[Symbol, "b", <||>]}, <||>]

and a::b is parsed as:
InfixNode[MessageName, {LeafNode[Symbol, "a", <||>], LeafNode[Token`ColonColon, "::", <||>], LeafNode[String, "b", <||>]





# Philosophy

there is an analogy here:

if a character is not something else, then it is letterlike

if a token is not something else, then it is prefix

if an expression is not something else, then it is leaf









# Compatibility

AST aims to be source compatible for Mathematica versions >= 11.0





# Caveats


concrete syntax does not preserve line continuations

concrete syntax does not preserve \[Raw] character escapes




linear syntax is not parsed, just tokenized

linear syntax is not abstracted, don't want to reimplement MakeExpression

boxes are not abstracted, don't want to reimplement MakeExpression











