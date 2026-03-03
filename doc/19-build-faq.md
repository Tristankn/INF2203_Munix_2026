Build FAQ
======================================================================

[TOC]

Common Errors
--------------------------------------------------

### make: option '--ignore-errors' doesn't allow an argument

When [setting up your build environment](10-build-env.md)
and running this command, you may see this error:

```
$ make -j nproc --ignore=2
make: option '--ignore-errors' doesn't allow an argument
Usage: make [options] [target] ...
Options:
  ...
```

#### Solution: check the command for backticks

The part of the command that says `nproc --ignore=2` is supposed to
be quoted with backticks.
Make sure you are typing / copy-pasting the command exactly as written:

```
make -j `nproc --ignore=2`
```

#### Explanation: backticks are special shell syntax

Backticks in shell syntax tell the shell
"run this as a separate command first,
 and then insert the output back into the command line."

```
make -j `nproc --ignore=2`
        |----------------|
         This part is a separate command that is run first.
```

The command `nproc` is short for "number of processors."
It simply prints the number of processors or CPU cores you have.
The command `nproc --ignore=2` counts all but two processors.
For example, on a 16-core PC, it prints "14":

```
$ nproc --ignore=2
14
```

This then gets plugged into Make's `-j` option,
which tells it how many jobs to run at once.

```
make -j 14
```

This speeds up compilation by using multiple cores,
while leaving two for other tasks.

If you lose the backticks, then `nproc --ignore=2` just get passed as
arguments to Make, and then Make gets confused.
The closest thing it has to an `--ignore=2` option is `--ignore-errors`,
and that does not take an `=2` argument.
Hence, "option '--ignore-errors' doesn't allow an argument."

### undefined reference to '__stack_chk_fail'

You may see this error when building the test processes for your host Linux
system, especially if you are running a different distro with a different
version of GCC.

We seem to see this most often on Arch Linux with GCC 15.

#### Solution: disable GCC's "stack protector" feature

Go into `src/Makefile.template` and, in the "Compiler Flags" section,
add a line like this:

```makefile
CFLAGS += -fno-stack-protector
```

#### Explanation: GCC's stack protector feature

GCC has a "stack protector" feature that attempts to detect _stack smashing,_
buffer overflows that overwrite the stack.
When it compiles a vulnerable function (one that uses an on-stack buffer),
it adds guard values around the buffer, and it adds code that checks the
guard values before returning. If the values are overwritten,
it is a sign that an overflow occurred and that the stack is likely corrupted.
To notify the user, the added code calls `__stack_chk_fail`,
which it expects to be provided by the system's Standard C Library.

On your system, the default C library is most likely glibc, which provides this
function. This is an example of the GNU C compiler and the GNU C library
working together.
However, our code is not using glibc. We have our own mulibc, which does not
implement `__stack_chk_fail`.
So the solution is to use `-fno-stack-protector` to turn off this feature
so that the calls to `__stack_chk_fail` are not emitted in the first place.
Or, alternatively, you could add a `__stack_chk_fail` to mulibc. It should
be easy, just add a new C file in `src/lib/mulibc/` that defines the function.
A quick `fprintf` and then `_exit` should do the trick.

#### More info

- GCC Options:
    [--fstack-protector](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html#index-fstack-protector)

- GCC Internals docs:
    [Stack Smashing Protection](https://gcc.gnu.org/onlinedocs/gccint/Stack-Smashing-Protection.html)

- glibc's implementation of `__stack_chk_fail`:
    [`debug/stack_chk_fail.c`](https://sourceware.org/git/?p=glibc.git;a=blob;f=debug/stack_chk_fail.c)

### cannot find crt0.o / cannot find -lc

You might get these errors when trying to build test processes,
especially on Mac.

```
i686-elf-gcc [...] process/hello.o -o process/hello
i686-elf-ld: cannot find crt0.o: No such file or directory
i686-elf-ld: cannot find -lc: No such file or directory
```

This is most likely a problem with an outdated version of Make.

#### Solution: install a newer version of Make

On Mac, install an up-to-date version of GNU Make via Homebrew:

```py
# Install and up-to-date GNU Make via Homebrew.
brew install make
```

Note that the Homebrew-installed version is installed as `gmake` by default.
So, any time you want to use `make`, use `gmake` instead:

```py
# Use 'gmake' instead of 'make' when building.
gmake image
```

Alternately, you can
[adjust your `PATH`](https://formulae.brew.sh/formula/make)
so that this version is used for `make`, without the `g`:

```py
# Run this each session, or add it to your shell's startup script
# (e.g. .bashrc or .zshenv), after the Homebrew setup.
export PATH="$HOMEBREW_PREFIX/opt/make/libexec/gnubin:$PATH"

# Now you can use 'make'
make image
```

#### Explanation: Our build uses a feature introduced in GNU Make 4.3

The C Runtime (`crt0.o` or `crt1.o`)
and the Standard C Library (`libc` aka `-lc`)
are standard pieces that are linked into each normal user program.
Normally they are provided by the operating system,
and the project's build scripts do not have to worry about them.
However, because we are building an operating system,
we also need to build these components.
We want Make to treat them as dependencies of the program,
even though they are not explicitly listed on the `ld` command line.

GNU Make version 4.3 introduced a feature to do exactly this:
the `.EXTRA_PREREQS` variable.
You can add files to this variable, and Make will treat them as prerequisites
(building/updating them first) without adding them to the list of explicit
prerequisites (that is copied to the `ld` command line).

Our Makefile uses this to make sure the C Runtime and Libc are built/updated
for all of the normal processes:

```make
# Add an implicit Make dependency on the implicity linked crt0 and libc.
$(processes_portable): .EXTRA_PREREQS += \
    lib/sys/$(target_cpu)-$(target_os)/crt0.o \
    lib/sys/$(target_cpu)-$(target_os)/crt1.o \
    lib/sys/$(target_cpu)-$(target_os)/libc.a
```

Earlier versions of Make, which do not have this feature,
will not know that they need to build those files.
So, the files will not be built, and when it comes time to do the linking,
the linker will complain that `crt0.o` and `-lc` are not found.

#### More info

- GNU Make Manual: [Special Variables / `.EXTRA_PREREQS`](https://www.gnu.org/software/make/manual/html_node/Special-Variables.html#index-_002eEXTRA_005fPREREQS-_0028prerequisites-not-added-to-automatic-variables_0029)
- LWN: [GNU Make 4.3 release announcement](https://lwn.net/Articles/810071/)
- Homebrew Formulae: [`make`](https://formulae.brew.sh/formula/make#default)
