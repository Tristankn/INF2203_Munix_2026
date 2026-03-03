/**
 * @file
 *
 * Macros for attributes and compiler extensions.
 *
 * This file defines macros for compiler-specific things like function
 * and struct attributes. These are easier to remember/type than the
 * specific `___attribute__((...))` syntax.
 *
 * For now we assume an environment with GCC as the compiler,
 * and maybe clangd providing tooling. So these macros have GCC-specific
 * definitions that clangd should also understand. If we decide to
 * support another compiler in the future, we can add alternate definitions
 * here for that compiler.
 *
 * @see
 *
 * - [GCC Attribute Syntax](
 *      https://gcc.gnu.org/onlinedocs/gcc/Attribute-Syntax.html)
 * - [Clang Attribute Reference](
 *      https://clang.llvm.org/docs/AttributeReference.html)
 */
#ifndef COMPILER_H
#define COMPILER_H

/** @name printf size prefixes */
///@{

/**
 * @def __PRI32_PREFIX
 * Length modifier needed when formatting int32_t and uint32_t with printf
 *
 * This is used to define @ref PRIx32 and friends in @ref inttypes.h .
 * The correct value depends on which type the compiler uses to define
 * `int32_t`: for `long` use `"l"`, for `int` use `""`.
 */
/**
 * @def __PRIPTR_PREFIX
 * Length modifier needed when formatting intptr_t and uintptr_t with printf
 *
 * This is used to define @ref PRIxPTR and friends in @ref inttypes.h .
 * The correct value depends on which type the compiler uses to define
 * `intptr_t`: for `long` use `"l"`, for `int` use `""`.
 */
/**
 * @def __PRI64_PREFIX
 * Length modifier needed when formatting int64_t and uint64_t with printf
 *
 * This is used to define @ref PRIx64 and friends in @ref inttypes.h .
 * The correct value depends on which type the compiler uses to define
 * `int64_t`: for `long` use `"l"`, for `long long` use `"ll"`.
 */
#if __i386__ && __clang__
/* i386 clang uses int for both int32_t and intptr_t */
#define __PRI32_PREFIX  ""
#define __PRIPTR_PREFIX ""
#define __PRI64_PREFIX  "ll"

#elif __i386__ && __GNUC__
/* i386 GCC uses long for both int32_t and intptr_t */
#define __PRI32_PREFIX  "l"
#define __PRIPTR_PREFIX "l"
#define __PRI64_PREFIX  "ll"

#else
/* Likely config for 64-bit CPUs. */
#define __PRI32_PREFIX  ""
#define __PRIPTR_PREFIX "l"
#define __PRI64_PREFIX  "l"

#endif

///@}

/** @name Struct layout. */
///@{

/**
 * Do not add padding between this struct's members
 *
 * @see
 * [GCC's `packed` attribute](
 *      https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-packed-type-attribute)
 */
#define ATTR_PACKED __attribute__((packed))

/**
 * Struct attr: Specify minimum alignment for this type
 *
 * @param   ALIGNMENT   the alignment in bytes. For example if ALIGNMENT is 8
 *                      then the compiler will ensure instances of the struct
 *                      are placed on an 8-byte boundary.
 *
 * @see
 * [GCC's `aligned` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-aligned-type-attribute)
 */
#define ATTR_ALIGNED(ALIGNMENT) __attribute__((aligned(ALIGNMENT)))

///@}

/** @name Interrupt handlers. */
///@{

/**
 * This function is an interrupt handler
 *
 * This macro adds function attributes that make this function suitable
 * for use as an interrupt handler. It changes the function's entry and
 * exit code to expect an interrupt stack and to exit with an interrupt-return
 * instruction, and it places restrictions on the registers that can be used.
 *
 * @see
 *
 * - [GCC's `interrupt` attribute: general](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-interrupt_005fhandler-function-attribute)
 *
 * - [GCC's `interrupt` attribute: details for x86](
        https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-interrupt-function-attribute_002c-x86)
 *
 * - [GCC's `general-regs-only` compilation option for x86](
        https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-target_0028_0022general-regs-only_0022_0029-function-attribute_002c-x86)
 */
#define INTERRUPT_HANDLER \
    __attribute__((interrupt)) __attribute__((target("general-regs-only")))

/**
 * This function is called from an interrupt handler
 *
 * This is a case where there is a subtle difference between the GCC and clang
 * compilers, and it has to do with calling conventions inside of interrupt
 * handlers. An ISR must make sure all registers are preserved when it
 * returns to the interrupted code. But what does that mean when calling
 * a function from an ISR?
 *
 * - GCC follows the typical calling convention: the ISR saves all caller-saved
 *      registers (on x86: AX, CX, and DX) just in case the callee clobbers
 *      them.
 *
 * - clang prioritizes efficiency: the ISR does not preemptively save any
 *      registers. Instead, functions called from an ISR are expected to save
 *      all of the registers they use (and only the registers they use). This
 *      way, no registers are ever saved unnecessarily.
 *
 *      Therefore, clang requires that all functions that are called from an
 *      ISR have the `no_caller_saved_registers` attribute and use general
 *      registers only (i.e. no floating-point or SIMD registers).
 *
 * This macro handles both cases. It expands to nothing for GCC and to the
 * necessary attributes for clang.
 *
 * @see
 *
 * - [clang's `interrupt` attribute (x86)](
        https://clang.llvm.org/docs/AttributeReference.html#interrupt-x86)
 *
 * - [clang's `no_caller_saved_registers` attribute](
        https://clang.llvm.org/docs/AttributeReference.html#no-caller-saved-registers)
 *
 * - [clang's `target` attribute](
        https://clang.llvm.org/docs/AttributeReference.html#target)
 *      and
 *      [`general-regs-only` compilation option](
            https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-interrupt-function-attribute_002c-x86)
 */
#ifdef __clang__
#define ATTR_CALLED_FROM_ISR \
    __attribute__((no_caller_saved_registers)) \
    __attribute__((target("general-regs-only")))
#else
#define ATTR_CALLED_FROM_ISR
#endif

/**
 * Make this function easier to call from hand-written asm.
 *
 * This macro expands to function attributes that make the function easy
 * to call from hand-written assembly. It will save all registers that it
 * uses and it will use general purpose registers only.
 *
 * @see
 *
 * - [GCC's `no_caller_saved_registers` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-no_005fcaller_005fsaved_005fregisters-function-attribute_002c-x86)
 *
 * - [GCC's `general-regs-only` compilation option](
        https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-target_0028_0022general-regs-only_0022_0029-function-attribute_002c-x86)
 */
#define ATTR_EASY_ASM_CALL \
    __attribute__((no_caller_saved_registers)) \
    __attribute__((target("general-regs-only")))

///@}

/** @name Other function attributes */
///@{

/**
 * This function is a save point that code might resume from
 *
 * This attribute is useful for functions like `setjmp`, which saves a state
 * that is later restored. When the program calls the matching `longjmp`,
 * control does not return from `longjmp`. Instead, it returns from the saved
 * state in `setjmp` for a second time. Hence, `longjmp` is `noreturn` and
 * `setjmp` is `returns_twice`.
 *
 * @see
 *
 * - [GCC's `returns_twice` attribute]( https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-returns_005ftwice-function-attribute)
 */
#define ATTR_RETURNS_TWICE __attribute__((returns_twice))

///@}

/** @name Compile-time checks and warnings. */
///@{

/**
 * This function may be intentionally unused
 *
 * The compiler should not give an "unused" warning for this function.
 *
 * @see
 *
 * - [GCC's `unused` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-unused-function-attribute)
 *
 * - Our @ref UNUSED macro
 */
#define ATTR_UNUSED __attribute__((unused))

/**
 * This function takes a printf pattern and matching arguments.
 *
 * This attribute turns on printf pattern checking for calls of this function.
 * The compiler will check the arguments against the format pattern and issue
 * a warning if there are any mismatches. For example, it checks that an `"%s"`
 * pattern has a matching `char*` argument.
 *
 * @param   PAT_ARG is the index in the argument list of the format pattern
 *                  argument.
 *
 * @param   VA_ARG is the index in the argument list where the variable
 *                  arguments (`...`) start.
 *
 * Both of these indexes start at `1` for the first argument.
 *
 * For example, when adding this attribute to `fprintf`, where the first
 * argument is the file, the second is the pattern, and the third is the start
 * of the variable arguments, the signature would look like this:
 *
 * ```c
    ATTR_PRINTFLIKE(2, 3)
    int fprintf(FILE *stream, const char *format, ...);
 * ```
 *
 * @see
 *
 * - [GCC's `format` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-format-function-attribute)
 */
#define ATTR_PRINTFLIKE(PAT_ARG, VA_ARG) \
    __attribute__((format(printf, PAT_ARG, VA_ARG)))

/**
 * This switch case is may fall through to the next case.
 *
 * In C's `switch` statement, execution of one `case` will fall through to the
 * next `case` unless the programmer addds a `break` statement. This `break` is
 * very easy to forget though. And so the compiler will issue a warning if it
 * thinks that a `break` statement is missing.
 *
 * This macro suppresses the fallthrough warning by explicitly marking the
 * fallthrough as deliberate.
 *
 * To use, add it as a statement at the end of the `case`, where the `break`
 * would normally go:
 *
 * ```c
 *      switch(x) {
 *      case 0:
 *          do_stuff();
 *          FALLTHROUGH;
 *      case 1:
 *          do_more_stuff();
 *          break;
 *      }
 * ```
 *
 * @see
 *
 * - [GCC's `fallthrough` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/Statement-Attributes.html#index-fallthrough-statement-attribute)
 *
 */
#define FALLTHROUGH __attribute__((fallthrough))

/**
 * Check if two expressions have the same type.
 *
 * @see
 *
 * - [GCC's `__builtin_types_compatible_p` builtin](
        https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#index-_005f_005fbuiltin_005ftypes_005fcompatible_005fp)
 *
 * - [GCC's `typeof` extension](
            https://gcc.gnu.org/onlinedocs/gcc/Typeof.html),
 *      which was standardized in C23 as the
 *      [`typeof` operator](
            https://en.cppreference.com/w/c/language/typeof.html)
 */
#define TYPES_COMPATIBLE(a, b) \
    __builtin_types_compatible_p(typeof(a), typeof(b))

///@}

/** @name Linking */
///@{

/**
 * Function/variable attribute: Place in given ELF section.
 *
 * ELF binaries are divided into _sections_ such as `.text` for executable code
 * (the "text" of the program) and `.data` for data. This attribute overrides
 * the default section for the function or variable it is attached to. This is
 * typically used to give the function or variable special treatment when
 * linking or when loading the executable.
 *
 * @see
 *
 * - [GCC's `section` attribute for functions](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-section-function-attribute)
 * - [GCC's `section` attribute for variables](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-section-variable-attribute)
 */
#define ATTR_SECTION(SECTION) __attribute__((section(SECTION)))

/**
 * Function/variable attr: Mark this symbol as _weak_.
 *
 * Weak symbols are a feature of ELF files. Normally, during linking, it
 * is an error for two symbols to have the same name. But if one of them
 * is marked as _weak_, then the linker ignores the weak symbol and uses
 * the other one.
 *
 * This is useful for libraries that want to provide a default implementation
 * of a function that programs can override.
 *
 * @see
 *
 * - [GCC's `weak` attribute](
        https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-weak-function-attribute)
 */
#define ATTR_WEAK __attribute__((weak))

/**
 * Issue a linker warning if the current file is included when linking.
 *
 * This is useful for libraries that have stub implementations of dependency
 * functions. Note that the warning works at the file level, so the stub
 * should be placed in its own file.
 *
 * Example:
 *
 * ```c
 * #include <compiler.h>
 *
 * LINK_WARNING("using stub implementation of log_write");
 *
 * int log_write(const char *buf, size_t nbyte)
 * {
 *     return -1;
 * }
 * ```
 *
 * If the linking executable does not define its own `log_write`,
 * then the warning will be shown during linking.
 *
 * ```
 * || i386-elf-gcc -L lib/ -nolibc -nostartfiles kernel/kernel.o \
 *      lib/libdrivers.a lib/drivers.a -o kernel/kernel
 * || i386-elf/bin/ld: lib/drivers.a(log_write_stub.o): warning: \
 *      using stub implementation of log_write
 * ```
 *
 * This works by adding a special section to the ELF file called
 * `.gnu.warning`. When the GNU linker sees this section during linking, it
 * will issue the given warning. The GNU documentation also says that it can
 * issue a warning only if a given symbol is referenced, but this file-level
 * warning seems to be more reliable.
 *
 * @see
 *
 * - The GNU linker's ducmentation on [Special Sections](
        https://sourceware.org/binutils/docs/ld/Special-Sections.html).
 */
/* clang-format off */
#define LINK_WARNING(msg) \
    asm("	.pushsection	.gnu.warning, \"\", @note\n" \
        "	.string	\"" msg "\"\n" \
        "	.popsection\n")
/* clang-format on */

///@}

#endif /* COMPILER_H */
