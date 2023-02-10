



;----------------------------------------------------
; Calls the real mode callback function
; rdi - the callback function pointer
[bits 64]
real_mode_callback:
    
    sub rdi, real_mode_start
    add rdi, REAL_MODE_BASE_ADDRESS
    
    ; far jump
    mov rax, REAL_MODE_RELOCATION(call_real_mode_function)
    call rax
    ret
;----------------------------------------------------

; copy real mode code to the lower memory
real_mode_start:
%include "src/bios/switch_modes.asm"
%include "src/bios/vga.asm"
real_mode_end: