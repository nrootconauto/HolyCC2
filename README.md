# HolyCC2
## Welcome
This is a holyC compiler on a mission to keep the line-count down. Its goal is to compile TempleOS and its programs on linux ,**and** to also be an educational compiler designed to show how compilers are made. **This compiler caches,meaning it only recompiles the functions that change** Now,

## Usage
```
hcc program.HC program2.HC #Sybols from program.HC will be visible from program2.HC 
hcc --help
```
**This compiler is currently for linux 32bit**(so if linking with libraries be sure to have the 32bit version installed),and uses `yasm` to assemble the generated assembly andd `gcc` to link agianst the c library . I will add 64bit and webassembly support later but i first want to create a HolyC texteditor. To link against libraries(make sure to install the 32bit version of the library), do
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

## Building
This project uses cmake,
```
mkdir build 
cd build 
cmake ..
sudo make install
```
## Installing in a chroot on a 64bit system
```
mkdir chroot2
sudo  debootstrap --arch amd64 buster chroot2 http://deb.debian.org/debian/
echo "cp /etc/resolv.conf chroot2/etc/resolv.conf" >> holyChroot.sh
echo "cp /etc/hosts chroot2/etc/hosts" >> holyChroot.sh
echo "mount proc chroot2/proc -t proc" >> holyChroot.sh
echo "mount sysfs chroot2/sys -t sysfs" >> holyChroot.sh
echo "mount --rbind /dev chroot2/dev" >> holyChroot.sh
echo "mount --rbind chroot2/ chroot2/" >> holyChroot.sh
echo "mount --rbind /run chroot2/run/" >> holyChroot.sh
echo "chroot chroot2 /bin/bash" >> holyChroot.sh 
```

Then within the chroot(`sudo sh ./holyChroot.sh`)
```
# Type `sudo sh ./holyChroot.sh` to enter the chroot,do this after install too
# Do this within the chroot
apt install git cmake make  gcc yasm 
cd
git clone https://github.com/nrootconauto/HolyCC2
mkdir build && cd build
cmake ../HolyCC2/
make install
dpkg --add-architecture i386
apt install gcc-multilib
nano HI.HC # "Put "Hello World\n"; " in here 
hcc HI.HC
```
