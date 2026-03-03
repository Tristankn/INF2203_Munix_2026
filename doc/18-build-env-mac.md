Building on a Mac
======================================================================

macOS is Unix under the hood (BSD),
so we should be able to use compatible tools, install our cross compiler,
and do our development right in the macOS Terminal,
even on ARM-based Apple Silicon Macs.

- Pro: Homebrew even does the cross compiler build for us.
    So setting up with Homebrew is even easier than the
    [Linux cross compiler setup](10-build-env.md).

- Con: This is new territory for the OS course.
    Compatibility issues are sure to arise up as we go.

If you run into problems, the most reliable thing to do is to install
a virtual machine and install Linux inside of that.
Then you can follow the Linux instructions and your environment inside
the virtual machine will have everything you need.

Working in Your Mac's Terminal (Homebrew)
======================================================================

Xcode Command Line Tools
----------------------------------------------------------------------

First, make sure the Xcode Command Line Tools are installed.

<!-- Markdown note: I am using Python syntax for my shell examples
    because Doxygen's Markdown parser does not support bash/shell scripts,
    but Python has a similar-enough syntax with '#' comments. -->

```py
# Activate Xcode Command Line Tools
xcode-select --install
```

For more information, see
[Apple's instructions](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools).


Installing Homebrew
----------------------------------------------------------------------

First, install homebrew as described on their homepage: <https://brew.sh/>.

If you do not have sudo access, or if you would like to isolate the packages
for this course from your normal Homebrew installation, you can follow the
["Untar anywhere" instructions](https://docs.brew.sh/Installation#untar-anywhere-unsupported)
to install to user directory
(e.g. `~/projects/inf2203/homebrew`).

Installing the Cross Compiler via Homebrew
----------------------------------------------------------------------

Once you have Homebrew set up, you can simply `brew install` the cross
compiler. Note that if you are using a non-standard install directory,
then some of these packages will have to be built from source,
which can take a long time. GCC alone might take 10--20 minutes or more
to build.

```py
# Cross compiler and emulator
brew install make i686-elf-gcc i386-elf-gdb i686-elf-grub qemu

# Optional tools for generating HTML documentation
brew install doxygen graphviz

# Optional tools for configuring your editor
brew install ctags bear
```

### 'make' vs 'gmake'

The Xcode Command Line tools include GNU Make,
but their version is a little out of date (version 3.81 as of February 2026).
Our build system requires version 4.3 or later.

Homebrew will install an up-to-date version of
Make (version 4.4 as of February 2026),
but it will install it as `gmake`.

You can use it as `gmake`, just remember that any time our build
instructions tell you to `make` something, type `gmake` instead.

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

For more details on why we need Make 4.3 or later,
see the [Build FAQ](19-build-faq.md).

Building the OS
----------------------------------------------------------------------

With the cross compiler and QEMU installed,
you should be able to simply build and run the OS.

```py
# Supported Make targets
make dev        # Set up development tooling
make doc        # Generate documentation HTML (Doxygen)
make image      # Build bootable disk image

make run        # Build image and launch it in an emulator
make debug      # Build image and debug it in an emulator

make clean      # Remove most built files
make distclean  # Remove all non-source files
```

For now we can only build the image for the target system.
Test code that compiles and runs on the host machines
does not support macOS/Darwin (yet).
So Make targets that also run tests will fail.

```py
# NOT SUPPORTED ON MAC (yet)
make            # Default: Run tests and build image
make test       # Run tests
make all        # Build all: doc, dev, test, image
```

See the [Build Environment doc](10-build-env.md) for more details.

Debugging with LLDB vs GDB
----------------------------------------------------------------------

For students who have trouble using GDB on macOS, LLDB is a good alternative.
LLDB is the default debugger on macOS and is developed as part of the LLVM
Project. Because Apple’s toolchain is based on LLVM, LLDB is better integrated
with macOS than GDB and usually works out of the box without extra setup or
code-signing issues.

Functionally, LLDB provides the same core features you need for this course:
breakpoints, single-stepping, stack inspection, register and memory inspection,
and remote debugging with QEMU. The commands are different from GDB, but the
concepts are the same, and most workflows translate directly.

If you want more details, command references, or comparisons with GDB, see the
official LLDB documentation at the LLVM website:
<https://lldb.llvm.org/index.html>

Customizing Your Shell
----------------------------------------------------------------------

The default shell in macOS Terminal works, but it can be hard to use
efficiently, especially when working with git, build systems, and large
projects. For this reason, Ghafoor recommends a small customization on top of
the default shell that improves readability and feedback without changing how
commands work. For example, you can use the default macOS shell (zsh) together
with Oh My Zsh and the Powerlevel10k prompt.

More Info on Potential Incompatibilities
----------------------------------------------------------------------

### Command Line Tools: Same but Different

This course code is developed under Linux using the GNU versions of
all the common Unix tools like `find`, `cp`, etc.
macOS is based on BSD
(the [_Berkeley Software Distribution_](https://en.wikipedia.org/wiki/Berkeley_Software_Distribution))
and uses the BSD versions of these same tools.

These tools are mostly compatible, but the course code's build system might
sometimes use GNU extensions or GNU-exclusive behavior in these tools,
which will break compatibility with BSD/Mac.

So be aware that these issues might arise and watch out for them.
Pay close attention to error messages from your build system.
You may have uncovered an incompatibility that we need to work out.
Talk to the staff about what you find.

For more information on BSD and it's differences from GNU/Linux,
you might find this five-minute YouTube video helpful:

_Linux vs BSD: Everything You Need to Know_  
by Make Tech Easer  
<https://www.youtube.com/watch?v=H-xc6c2ovMQ>

### Test Code

In later projects, we introduce test programs that can compile both
on the Linux host system and on the Munix OS.
The idea is that you can see how these programs run on an established OS,
and then go implement the features in your own kernel to support these
programs.

For now, those programs only know how to talk to Linux and Munix.
We have not had time to make them talk to the macOS kernel as well.
So, you will not be able to run these test programs directly on macOS.
If you want to see them running on an established OS,
you will have to install a Linux virtual machine or container.

If you are an ambitious student who would like to work
on macOS compatibility for these test programs,
let us know!

Working in a Virtual Machine
======================================================================

TODO
