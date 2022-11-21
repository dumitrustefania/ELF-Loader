# ELF LOADER

322CA - Bianca Ștefania Dumitru \
Operating Systems Course

### November 2022
----------------------------------------------------------------------------------------------------
## Introduction

* ELF Loader
  *  the program is an implementation of an ELF binary on-demand loader in Linux
  *  it loads the file into memory page by page
  *  it generates system calls, using functions for mapping memory, like mmap
     or mprotect

## Content
The project consists of three components, each of them in its own
directory:

* `loader` - a dynamic library that can be used to run ELF binaries. It
consists of the following files:
  * `exec_parser.c` - Implements an ELF binary parser
  * `exec_parser.h` - The header exposed by the ELF parser
  * `loader.h` - The interface of the loader that provides 2 methods:
    * `int so_init_loader(void);` - initializes the on-demand loader
    * `int so_execute(char *path, char *argv[]);` - executes the binary located in
      `path` with the required `argv` arguments
  * `loader.c` - This is where the loader is actually implemented

* `exec` - a program that uses the `libso_loader.so` library to run an ELF
binary received as argument

* `test_prog` - an ELF binary used to test the loader implementation

## How does it work?

The program tries to execute an ELF binary that is not loaded into
memory, so it runs into a SIGSEGV error. Its default handler is
modified to the on-demand loader. So, when a SIGSEGV signal is 
received, the handler gets called and tries to add a new page into
memory and read data from the binary. The ELF binary is divided
into smaller pieces of data, called segments.

To perform the loading correctly, I first have to check the address
where the SIGSEGV signal is coming from. There are 2 possibilities:

* the addres is not part of any segment, so I call the default handler,
that returns SEGFAULT
* the address is part of one segment

In the second case, 2 other possibilities arise:

* a page has already been mapped, containing the faulty address. This means
I would now be trying to map the same section again, which is an illegal operation,
so I call the default handler, returning SEGFAULT. To check whether a page has
already been mapped, I am storing an array in the data field of each segment.
All its elements are initially 0, being transformed to 1 once the respective
page gets mapped.
* the page has not been mapped yet, so I am now trying to do it

To map the page, I am using the mmap function, only giving writing permissions
for now. I am zeroing the whole page, to have an easier time when taking care of
the .bss memory zone. Then, I copy the data from the ELF binary into the newly
mapped page and give the required permissions using the mprotect function.
I am also memorizing that the page has been mapped for the segment.

When all segments are loaded, the ELF binary is ready to be executed!


## How to build and use the loader?
The project contains 2 makefiles:
* `Makefile` - builds the `libso_loader.so` library from the `loader`
directory. It can be run using:
``` 
make 
```

* `Makefile.example` - builds the `so_exec` and `so_test_prog` binaries from
the `exec` and `test_prog` directories that can be used to test the loader.
It can be run using:
```
make -f Makefile.example
```

* In order to test the code using the testcase present in the `test_prog` directory, use:
```
LD_LIBRARY_PATH=. ./so_exec so_test_prog
```
