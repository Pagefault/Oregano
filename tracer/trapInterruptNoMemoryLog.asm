
; Only in MASM
;   TITLE   trap_interrupt.asm
;   .586
;
;   .MODEL flat

; Exportes
GLOBAL  _orgTrapInterrupt
GLOBAL  _orgBreakpointInterrupt
GLOBAL  _targetProcessId
GLOBAL  _target_process
GLOBAL  _stopAddress
GLOBAL  _bottom_log_address
GLOBAL  _top_log_address
GLOBAL  _log_buffer
GLOBAL  _log_buffer_item
GLOBAL  _active_log_buffer
GLOBAL  _used_buffers
GLOBAL  _next_free_log_buffer
GLOBAL  _lastContext
GLOBAL  _threads
GLOBAL  _loggingRanges
GLOBAL  _is_trap_on_branch_set

%ifdef DEBUG
GLOBAL  _DebugVar0
GLOBAL  _DebugVar1
GLOBAL  _DebugVar2
GLOBAL  _DebugVar3
GLOBAL  _DebugVar4
GLOBAL  _DebugVar5
GLOBAL  _DebugVar6
GLOBAL  _DebugVar7
%endif

; Defines
LOG_BUFFER_SIZE              equ 00004000h
SHIFTED_LOG_BUFFER_MAX_SIZE  equ 00000007h
BUFFER_MAX_SIZE_BIT_TO_SHIFT equ 0000000bh
MAX_BUFFER_OFFSET            equ 00003f40h
SIZE_OF_DWORD                equ 04h
SIZE_OF_QWORD                equ 08h
SIZE_OF_POINTER              equ 04h
NUMBER_OF_BUFFERS            equ 1000h
NUMBER_OF_BUFFERS_MASK       equ 0fffh  ; I've got exactly 0x1000 buffers

END_BUFFER_SYMBOL           equ 0ffffffffh
THREAD_CHANGE_SYMBOL        equ 0feh
THREAD_ID_MASK              equ 03ffc0h
THREAD_CONTEXT_SIZE         equ 040h
THREAD_CONTEXT_ARRAY_END    equ 03ffffh
KERNEL_SPACE_MASK           equ 80000000h ; 2GB
TRAP_FLAG_MASK              equ 00000100h
BREAK_SINGLE_STEP_FLAG_MASK equ 00004000h ; AKA BS flag AKA BullShit flag of DR6
BREAK_OTHER_FLAGS           equ 0000a007h
; From Intel Model-Specific Registers PDF
DEBUGCTLMSR_ID              equ 01D9h
BRANCH_TRAP_FLAG            equ 02h
BRANCH_TRAP_FLAG_CLEAR      equ 0
; Get it from PsGetCurrentProcess @ ntoskrnl.exe
CURRENT_KTHREAD_OFFSET      equ 0124h
; Get it from _KiTrap01
FS_VALUE                    equ 30h

; Regs ids:     (Note that this is not the same list as the one found in the disassembler)
REG_ID_EIP                  equ 00h
REG_ID_EDI                  equ 01h
REG_ID_ESI                  equ 02h
REG_ID_EBP                  equ 03h
REG_ID_EBX                  equ 04h
REG_ID_EDX                  equ 05h
REG_ID_ECX                  equ 06h
REG_ID_EAX                  equ 07h
; REG_ID_EIP                equ 08h
REG_ID_ECS                  equ 08h
REG_ID_EFLAGS               equ 09h
REG_ID_ESP                  equ 0Ah
REG_ID_ESS                  equ 0Bh

; Global variables defines
SECTION .data

; Original trap interrupt, if needed.
_orgTrapInterrupt           DD 0
; To simulate a breakpoint
_orgBreakpointInterrupt     DD 0
; What process are I debugging?
_targetProcessId            DD 0
_target_process             DD 0
; Where should I stop logging
_stopAddress                DD 0
_bottom_log_address         DD 0
_top_log_address            DD 0
; Output buffer
_log_buffer                 DD 0
; I got 0x1000 log buffers
_log_buffer_item            times NUMBER_OF_BUFFERS DD 0
_active_log_buffer          DD 0
_used_buffers               DD 0
_next_free_log_buffer       DD 0
_lastContext                DD 0
_lastLoggedContext          DD 0
_is_trap_on_branch_set      DD 0
; ThreadContext_t is 0x10 dwords (0x40 bytes) and I got a table of 0x1000 entries
align   16
_threads                    times 65536 DD 0
; LoggingRanges maximum of 0x80 logging ranges
_loggingRanges              times 256 DD 0

; Regs last value offsets in struct:
LAST_THREAD                 equ 00h
LAST_EDI                    equ 04h
LAST_ESI                    equ 08h
LAST_EBP                    equ 0ch
LAST_EBX                    equ 10h
LAST_BH                     equ 11h
LAST_EDX                    equ 14h
LAST_DH                     equ 15h
LAST_ECX                    equ 18h
LAST_CH                     equ 19h
LAST_EAX                    equ 1ch
LAST_AH                     equ 1dh
LAST_EIP                    equ 20h
LAST_ECS                    equ 24h
LAST_EFLAGS                 equ 28h
LAST_ESP                    equ 2ch
BOTTOM_BOUND                equ 34h
TOP_BOUND                   equ 38h

%ifdef DEBUG
_DebugVar0  DD 0
_DebugVar1  DD 0
_DebugVar2  DD 0
_DebugVar3  DD 0
_DebugVar4  DD 0
_DebugVar5  DD 0
_DebugVar6  DD 0
_DebugVar7  DD 0
%endif

; Macros:
%macro CALL_ORIGINAL_INTERRUPT 0
    popad
    pop fs
    ; Call original interrupt handler
    jmp DWORD [_orgTrapInterrupt]
%endmacro

%macro RETURN_FROM_INTERRUPT_WITH_CHECK 0
    ; Do I need to call original interrupt
    mov eax, dr6
    test eax, BREAK_OTHER_FLAGS
    jz %%JUST_IRET
    CALL_ORIGINAL_INTERRUPT
    %%JUST_IRET:
    popad
    pop fs
    iretd
%endmacro

%macro LOG_REGISTER_CHANGE_WITH_STOS 1
    ; Get last %1
    mov eax, DWORD [esp + SAVED_%1]
    cmp eax, [ebp + LAST_%1]
    jz %%DONE_WITH_REG
        mov DWORD [ebp + LAST_%1], eax
        xchg eax, edx
        stosb
        xchg eax, edx
        stosd
    %%DONE_WITH_REG:
    inc edx
%endmacro

SECTION .text
align   16

; Function define
GLOBAL  _trap_interrupt@0
; Inside labels for debugging
GLOBAL  _trap_interrupt_target_process@0
GLOBAL  _trap_interrupt_got_log_buffer@0
GLOBAL  _trap_interrupt_got_thread@0
GLOBAL  _trap_interrupt_in_range@0
GLOBAL  _trap_interrupt_check_log_buffer@0
GLOBAL  _trap_interrupt_save_log_buffer_pos@0
GLOBAL  _trap_interrupt_done@0
GLOBAL  _trap_interrupt_out_of_range@0

;
; trap_interrupt
;
;   Description:
;       This function should replace the original trap interrupt.
;       The purpose of this function is to log the current opcode,
;       and it's affect on memory, registers and anything else that
;       might be in the interest of the programmer.
;       This function shold be as fast, efficient and effective as
;       possible, cause it is called once for every opcode running.
;       It is written in mostly pure x86 assembly.
;
;   Execution plan:
;       0. Check if the interrupt was called from our process and for Single Stepping
;       1. Check if address is in logging range
;       2. Get a pointer to a output buffer
;       3. Check context switch
;       4. Check that there is free space in logging buffer
;       5. Check if I'm in an address that I should log
;       - 6. Write memory changed from last opcode
;       7. Write and update register values
;       8. Write back the last writting point in buffer
;       9. Ret
;
;   Known issues:
;       Because this function replaces the original windows trap interrupt,
;       it is impossable to use debuggers single step function while it is running,
;       an attempt to do so might cause a blue screen.
;
;
    ; NASM assumes nothing
    ; assume DS:NOTHING, SS:NOTHING, ES:NOTHING
    align 8
_trap_interrupt@0:

; Save all registers
    push fs
    pushad
    ; Set the selector
    mov eax, FS_VALUE
    mov fs, ax

    ; Now the stack looks somthing like this:
SAVED_EDI       equ 000h
SAVED_ESI       equ 004h
SAVED_EBP       equ 008h
SAVED_KERNEL_ESP    equ 00ch
SAVED_EBX       equ 010h
SAVED_EDX       equ 014h
SAVED_ECX       equ 018h
SAVED_EAX       equ 01ch
SAVED_FS        equ 020h
; Interrupt call
SAVED_EIP       equ 024h
SAVED_ECS       equ 028h
SAVED_EFLAGS    equ 02ch
SAVED_ESP       equ 030h
SAVED_ESS       equ 034h
; TODO: Do I need to set the FS to 0x30 and DS, ES to 0x23 as in _KiTrap01

; Interrupt 0x01 can be called for five reasons:
;   1. Single Step
;   2. Hardware breakpoint on execution
;   3. Hardware breakpoint on memory
;   4. Task switch
;   5. General Detect Fault - A use of the debug registers when they are not available
; Check the reason for the Interrupt.
    ; push ebx, pop ebx is faster than mov someGlobal, ebx
    mov eax, dr6
    test eax, BREAK_SINGLE_STEP_FLAG_MASK
    jnz INTERRUPT_COZED_BY_SINGLE_STEP
    ; The interupt was not caused for Single Step issues
    CALL_ORIGINAL_INTERRUPT
INTERRUPT_COZED_BY_SINGLE_STEP:

; Check if this is the traced process
; I use the CR3 (The memory page table pointer) as the process UID
    mov ebx, cr3
    mov ecx, [_target_process]
    cmp ebx, ecx
    je THIS_IS_THE_TARGET_PROCESS
    CALL_ORIGINAL_INTERRUPT
THIS_IS_THE_TARGET_PROCESS:
_trap_interrupt_target_process@0:

    ; Clear the single step flag from DR6 (http://en.wikipedia.org/wiki/X86_debug_register)
    and eax, ~BREAK_SINGLE_STEP_FLAG_MASK
    mov dr6, eax
%ifdef DEBUG
    mov eax, 1
    mov [_DebugVar0], eax
%endif

    ; Is in range
    mov ebx, [esp + SAVED_EIP]
    test ebx, KERNEL_SPACE_MASK
    jz NOT_IN_KERNEL
        RETURN_FROM_INTERRUPT_WITH_CHECK
    NOT_IN_KERNEL:
    ; Check logging range
    cmp DWORD [_top_log_address], ebx
    jbe OUT_OF_LOGGING_BOUNDS
    TOP_BOUND_OK:
    cmp DWORD [_bottom_log_address], ebx
    jbe IN_RANGE
    OUT_OF_LOGGING_BOUNDS:
        ; Ok I am now out of the logging range
        ; Go to check other logging ranges
    _trap_interrupt_out_of_range@0:
        ; I got EIP in ebx at this point
        mov ecx, _loggingRanges
        cmp [ecx], ebx
        jbe BOTTOM_RANGE_FOUND
        ; I assume that the ranges are sorted by BOTTOM
        ; Find the first range that starts before EIP
        NEXT_RANGE:
            add ecx, 8  ; sizeof(LoggingRange_t)
            cmp [ecx], ebx
            ja NEXT_RANGE
        BOTTOM_RANGE_FOUND:
        ; Check the TOP bound
        cmp [ecx + 4], ebx
        jbe NOT_IN_ANY_RANGE
        ; Range found
        mov eax, [ecx]
        mov ecx, [ecx + 4]
        mov [_bottom_log_address], eax
        mov [_top_log_address], ecx
        ; Clear the trap on branch flag if needed
        mov ecx, [_is_trap_on_branch_set]
        test ecx, ecx
        jz IN_RANGE
        ; Clear the trap on branch flag
        mov ecx, DEBUGCTLMSR_ID
        xor eax, eax
        wrmsr
        ; Continue as if nothing happend
        jmp IN_RANGE

        NOT_IN_ANY_RANGE:
        mov edx, [_is_trap_on_branch_set]
        test edx, edx
        jnz TRAP_ON_BRANCH_IS_SET
        ; Anything that is not zero
        mov [_is_trap_on_branch_set], ecx
        xor edx, edx
        ; Switch to trap on branch, because returning into the logging range,
        ; is more likley to happen on branch, and this way I hope to get better
        ; pref'
        mov ecx, DEBUGCTLMSR_ID
        mov eax, BRANCH_TRAP_FLAG
        ; xor edx, edx - Edx is supposed to be set to zero, but I think it would
        ; be ok to leave it as is, because DEBUGCTLMSR has only 32 relevent bits.
        ; If not, I would also have to change the code for clearing the flag.
        wrmsr
        TRAP_ON_BRANCH_IS_SET:
        ; Return for now
        ; Note that I didn't write the last writting offset to the log,
        ; because I want the trace to overwrite this cycle
        RETURN_FROM_INTERRUPT_WITH_CHECK
    IN_RANGE:

_trap_interrupt_in_range@0:
%ifdef DEBUG
    push eax
    mov eax, 2
    mov [_DebugVar0], eax
    pop eax
%endif

    ; Get output buffer pointer
    ; I try to keep the buffer_ptr in eax most of the time coz it's used the most.
    ; The next place to write is _log_bufer + _log_buffer_pos (Saved at offset zero of the log_buffer).
_trap_interrupt_check_log_buffer@0:
    ; Check for output buffer end
    mov eax, DWORD [_log_buffer]
    ; Last writting offset is the firest DWORD of each buffer
    mov edx, DWORD [eax]
    cmp edx, MAX_BUFFER_OFFSET
    jb LOG_BUFFER_HAS_SPACE
        ; Move to the next buffer
        ; Inc the number of buffers used
        loc inc DWORD [_used_buffers]
        ; Get the next buffer from the buffers array
        mov edx, DWORD [_active_log_buffer]
        inc edx
        and edx, NUMBER_OF_BUFFERS_MASK
        ; TODO: comper with next_free_log_buffer
        mov DWORD [_active_log_buffer], edx
        mov eax, DWORD [_log_buffer_item + edx * SIZE_OF_POINTER]
        mov DWORD [_log_buffer], eax
        ; 4 first bytes are used to save the last pos of the buffer
        mov DWORD [eax], SIZE_OF_DWORD
        add eax, SIZE_OF_DWORD
        jmp GOT_WRITTING_PTR
LOG_BUFFER_HAS_SPACE:
    ; Make Eax point to the next writting point
    add eax, edx
GOT_WRITTING_PTR:

_trap_interrupt_got_log_buffer@0:

    ; Let Ebp hold the logging context
    ; This might not be the current one, as thread might have changed
    mov ebp, DWORD [_lastContext]
    ; So check for thread change
    ; Ecx would keep the KTHREAD
    mov ecx, [fs:CURRENT_KTHREAD_OFFSET]
    cmp ecx, DWORD [ebp]
    je NO_THREAD_CHANGE
        ; Write thread change

        ; Load context
        ; Should never fail to find thread context.
        mov edx, ecx
        ; Address is usually DWORD aligned, so for better hashing I shift 2 bits out
        and edx, THREAD_ID_MASK
        ; Find item in hash table
        ; Every entry is 0x40 bytes long
        ; But I use only bits 6 to 12, so I'm already aligend with the index
        IS_THIS_THE_RIGHT_CONTEXT:
            lea ebp, [edx + _threads]
            cmp ecx, [ebp] ; .ID
            je CONTEXT_FOUND
        TRY_NEXT_THREAD_CONTEXT:
            add edx, THREAD_CONTEXT_SIZE
            and edx, THREAD_CONTEXT_ARRAY_END
            lea ebp, [edx + _threads]
            cmp ecx, [ebp] ; .ID
            jne TRY_NEXT_THREAD_CONTEXT
    CONTEXT_FOUND:
        ; Need to write thread chnage to log
        mov BYTE [eax], THREAD_CHANGE_SYMBOL
        inc eax
        mov DWORD [eax], ecx
        add eax, 4
        ; Update the last logged context
        mov [_lastContext], ebp
    NO_THREAD_CHANGE:

%ifdef DEBUG
    push eax
    mov eax, 5
    mov [_DebugVar0], eax
    pop eax
%endif

_trap_interrupt_got_thread@0:
    ; Eax supposed to point to the next writting point
    ; Ebx supposed to have saved EIP now
    ; Ecx is the current thread id
    ; Ebp supposed to point to a struct containg last cycle regs values

    ; Write the EIP reg code
    mov BYTE [eax], 0
    inc eax
    ; Write EIP
    mov DWORD [eax], ebx
    ; pos += sizeof( EIP )
    add eax, 4

    ; I'll use edx to hold the regs log ids
    ; and ebp to iterate over them
    ; Ids start from 0 which is EIP
    xor edx, edx
    ; But I already logged EIP
    inc edx

    ; Get last edi
    mov ecx, DWORD [ebp + LAST_EDI]
    cmp ecx, edi
    jz DONE_WITH_EDI
        mov DWORD [ebp + LAST_EDI], edi
        mov BYTE [eax], dl
        inc eax
        mov DWORD [eax], edi
        add eax, SIZE_OF_DWORD
DONE_WITH_EDI:
    ; Inc reg id
    inc edx ; Next is ESI (0x02)

    ; From now on I shall write them using edi which just got free for use
    mov edi, eax

    ; Get last esi
    mov eax, DWORD [ebp + LAST_ESI]
    cmp eax, esi
    jz DONE_WITH_ESI
        mov DWORD [ebp + LAST_ESI], esi
        mov eax, edx
        stosb
        mov eax, esi
        stosd
DONE_WITH_ESI:
    inc edx ; Next is EBP (0x03)

    LOG_REGISTER_CHANGE_WITH_STOS EBP
    ; Here comes the kernel esp on the stack, but I don't care about it.
    LOG_REGISTER_CHANGE_WITH_STOS EBX
    LOG_REGISTER_CHANGE_WITH_STOS EDX
    LOG_REGISTER_CHANGE_WITH_STOS ECX
    LOG_REGISTER_CHANGE_WITH_STOS EAX
    inc edx
    ;LOG_REGISTER_CHANGE_WITH_STOS EFLAGS
    inc edx ; Skip EFlags
    LOG_REGISTER_CHANGE_WITH_STOS ESP

_trap_interrupt_save_log_buffer_pos@0:
    ; Save log buffer pos (Relatived to the buffer)
    mov eax, DWORD[_log_buffer]
    sub edi, eax
    mov DWORD [eax], edi

_trap_interrupt_done@0:
%ifdef DEBUG
    push eax
    mov eax, 8
    mov [_DebugVar0], eax
    pop eax
%endif
ALL_DONE:
    RETURN_FROM_INTERRUPT_WITH_CHECK

CLEAR_TRAP_FLAG_AND_RET:
    ; Save log buffer pos (Relatived to the buffer)
    mov edi, DWORD[_log_buffer]
    sub eax, edi
    mov DWORD [edi], eax
    ; Clear the trap flag
    mov eax, DWORD [esp + SAVED_EFLAGS]
    and eax, ~TRAP_FLAG_MASK
    mov DWORD [esp + SAVED_EFLAGS], eax
    RETURN_FROM_INTERRUPT_WITH_CHECK

;_trap_interrupt@0 ENDP

;END
