# HolyCC2
## Welcome
This is a holyC compiler on a mission to keep the line-count down. Its goal is to compile TempleOS and its programs on linux ,**and** to also be an educational compiler designed to show how compilers are made. Now,

## Usage
This compiler is currently for linux 32bit,and uses `yasm` to assemble the generated assembly. I will add 64bit and webassembly support later but i first want to create a HolyC texteditor. To link against libraries(make sure to install the 32bit version of the library), do
```
#link "lib"
extern I32 libFunc();
libFunc();
```
## BSAT debugger(Before Space and Time)
Compile your program,the debugger will be linked-into the program by default(disable it with `-dd`).
Each file linked into the program will have it's own init code(Including the HolyC runtime (HCRT.HC)). Type`c` to contiue ,or `help` for fun.
To luanch the debugger do 
```
HCC_DEBUG=1 ./prog
```

## Odditidies
Assembler statements in global scope are "skipped" across. This saves time for the programmer.
```
I32 x=10;
asm {
	IMPORT x;
	MOV I32 [x],12;
}
//x is 10
``` 

## Contributing 
I will handle adding the 64bit and webassembly support. You could help easily by adding to the HolyC runtime by aadding functions. If you want to get into the guts of the compiler,look at the developers manual that comes in the from of html. 
