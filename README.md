# HolyCC2
## Welcome
	This is a holyC compiler on a mission to keep the line-count down. Its goal is to compile TempleOS and its programs on linux ,**and** to also be an educational compiler designed to show how compilers are made. Now, here is a rundown on the source structure.

## Source Code structure 😀
	The source code is all present in 1 directory except for the tests,which are in their own folder. This allows for easily finding a file without shuffling through folders. Here is a list of items by subject

### Structures
 - graph.c/h. This isnt a plot of a function,its a network of things that are connected by nodes and edges. It is central to the compiler
 - hashTable.c/h. This maps a string to a value, usefull for variable names and such!
 - linkedList.c/h. A linked list is a list of items that are connected togheter by pointers,like a diasy chain. The previous item is pointed to by a node,and so is the next. This allows for easy modifications as memory isn’t moved,only the pointers are changed.
 - str.c/h. Dont be fooled by the name. String is actually an array of elements(which may be text). It even includes powerfull functions for set theory,**(just be sure to keep things sorted if you plan on using set theory functions!!!)**

### Algorithms
	Algorithms are the juicy secret suace of the compiler. You may need to look at the links within the source files for more info about thier inner workings,but dont be scared as I have made working versions of these algorithms you can look at. So here they are:
	
- base64.c/h. This converts data into a encoded string,usefull for turning pointers into keys for maps.
- graphColoring.c/h. This assigns values to a network of nodes so that they will have a minimum number of adjacent values next to each other. It isn’t perfect but that is good as we get a decent time.
- graphDominance.c/h. This tells us which nodes domianate other nodes. It also tells us there an unrelated path meets up with another path(also known as a _dominance frontier_). This is usefull for telling where we need to swap out variable values(See SSA.c)
- intExponet.c/h. This is a exponet function for integers.
- IRLiveness.c/h. This performs liveness analysis on a graph for variables. This in conjunction with the graph colorer is usefull for assigning registers to variables. See regAllocater.c/h. 
- SSA.c/h. This is for single static assignment. It basically increments a variable’s version every time it is assigned,which is usefull for breaking up live-spans so that the old version of a variable isnt kept in memory when a new version is avialable
- subExprElim.c/h. This finds common sub-expressions in an _intermediate representation_ graph. Who doesn’t love optimizations.
- subGraph.c/h. This isolates sub-graphs within a network of nodes.
- topoSort.c/h. This topologically sorts a graph,so that a node incoming to another node is numbered lesser that the said current node.(Graphs with cycles dont have topological orderings so be carefull)

### HolyC specific + misc
The holyC compiler compiles holyC,so here is a list of things that work to turn holyC into an _intermeddiate representation_ graph.
- diagMsh.c/h. This reports errors and warnings,we can’t have a compiler without bugs and stuff:’(.
- diary.txt. A (secret) todo list.
- escaper.c/h. This escapes a string,which is usefull for debugging purposes.
- exprParser.c/h. This validates an expression from the parser and assigns types to the nodes.
- IR.c/h. This is for the _intermeddiate representation_ and misc things to help out with it,when it doubt put something IR related in this file.
- IRExec.c/h. This is for “evaluating” IR networks. This is usefull for writing tests,just dont expect to do full simulations of IR graphs with this,though this could be usefull for optimaizations.
- lexer.c/h. This turns text into tokens(Data that the compiler can understand)
- parserA.c/h. This is the bulk of the parser
- parserB.c/h TODO
- regAllocated.c/h. This allocates registers for the _intermediate represenation_ graph,it also does some optimizations on it too,like aliasing variables that can share the same spot in memory(or a register).
- registers.c/h. This contains information about the registers for an architecture.
- stringParser.c/h. This parses a string for the lexer.
- textMapper.c/h. This mapped a preprocessed file’s source code back to the source file in conjunction with preprocessor.c/h.
- preprocessor.c/h. This (as the name suggests) expands macros into a final form used by the lexer,thus creating a preproccessed file. 