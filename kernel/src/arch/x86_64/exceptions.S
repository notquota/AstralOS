
.section .text
.code64

.extern astral_exception_dispatch

# Exceptions without CPU-generated error codes.
.macro EXCEPTION_NOERR number
    .global astral_exception_\number
    .type astral_exception_\number, @function

astral_exception_\number:
    cli
    pushq $0
    pushq $\number
    jmp astral_exception_common
.endm

# Exceptions with CPU-generated error codes.
.macro EXCEPTION_ERR number
    .global astral_exception_\number
    .type astral_exception_\number, @function

astral_exception_\number:
    cli
    pushq $\number
    jmp astral_exception_common
.endm

# CPU exception vectors 0-31.

EXCEPTION_NOERR 0
EXCEPTION_NOERR 1
EXCEPTION_NOERR 2
EXCEPTION_NOERR 3
EXCEPTION_NOERR 4
EXCEPTION_NOERR 5
EXCEPTION_NOERR 6
EXCEPTION_NOERR 7

EXCEPTION_ERR   8

EXCEPTION_NOERR 9

EXCEPTION_ERR   10
EXCEPTION_ERR   11
EXCEPTION_ERR   12
EXCEPTION_ERR   13
EXCEPTION_ERR   14

EXCEPTION_NOERR 15
EXCEPTION_NOERR 16

EXCEPTION_ERR   17

EXCEPTION_NOERR 18
EXCEPTION_NOERR 19
EXCEPTION_NOERR 20

EXCEPTION_ERR   21

EXCEPTION_NOERR 22
EXCEPTION_NOERR 23
EXCEPTION_NOERR 24
EXCEPTION_NOERR 25
EXCEPTION_NOERR 26
EXCEPTION_NOERR 27
EXCEPTION_NOERR 28

EXCEPTION_ERR   29
EXCEPTION_ERR   30

EXCEPTION_NOERR 31

# Shared exception entry point.
.global astral_exception_common
.type astral_exception_common, @function

astral_exception_common:
    # Clear the direction flag for C++ code.
    cld

    # RSP points to Astral::CPU::ExceptionFrame.
    # Pass that address as the first argument.
    movq %rsp, %rdi

    # Align the stack to a 16-byte boundary.
    andq $-16, %rsp

    # Call the C++ exception dispatcher.
    call astral_exception_dispatch

    # Exception dispatcher must not return.
    # If it does, disable interrupts and halt.
    cli

.Lfatal_halt:
    hlt
    jmp .Lfatal_halt

.size astral_exception_common, .-astral_exception_common

.section .note.GNU-stack,"",@progbits
