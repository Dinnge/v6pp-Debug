[BITS 32]
[extern kernelBridge]

global greatstart

%ifdef EARLY_BOOT_GDB
GDB_PORT        equ 0x3F8
GDB_BAUD_LATCH  equ 0x3F9
GDB_LINE_CTRL   equ 0x3FB
GDB_MODEM_CTRL  equ 0x3FC
GDB_LINE_STATUS equ 0x3FD

section .text

greatstart:
    call serial_init
    call gdb_debug_break
    ; 跳转到内核入口点
    jmp kernelBridge
    ud2

serial_init:
    mov dx, GDB_LINE_CTRL
    mov al, 0x80
    out dx, al
    mov dx, GDB_PORT
    mov al, 0x01
    out dx, al
    mov dx, GDB_BAUD_LATCH
    xor al, al
    out dx, al
    mov dx, GDB_LINE_CTRL
    mov al, 0x03
    out dx, al
    mov dx, GDB_MODEM_CTRL
    mov al, 0x0b
    out dx, al
    ret

serial_send_byte:
    mov dx, GDB_LINE_STATUS
.tx:
    in al, dx
    test al, 0x20
    jz .tx
    mov dx, GDB_PORT
    mov al, bl
    out dx, al
    ret

serial_recv_byte:
    mov dx, GDB_LINE_STATUS
.rx:
    in al, dx
    test al, 0x01
    jz .rx
    mov dx, GDB_PORT
    in al, dx
    ret

rsp_recv_packet:     ; 在栈上分配256字节的缓冲区
.ws:
    call serial_recv_byte
    cmp al, '$'
    jne .ws
    mov edi, rsp_buf     ; 使用栈上的缓冲区
.lp:
    call serial_recv_byte
    cmp al, '#'
    je .done
    stosb
    jmp .lp
.done:
    mov byte [edi], 0
    call serial_recv_byte
    call serial_recv_byte
    mov bl, '+'
    call serial_send_byte
    mov eax, rsp_buf     ; 释放栈上的缓冲区
    ret

rsp_begin:
    mov byte [rsp_checksum], 0
    mov bl, '$'
    jmp serial_send_byte

rsp_emit:
    add byte [rsp_checksum], bl
    jmp serial_send_byte

hex_digit:
    add bl, '0'
    cmp bl, '9'
    jbe .ok
    add bl, 7
.ok:
    ret

rsp_end:
    mov bl, '#'
    call serial_send_byte
    mov bl, [rsp_checksum]
    mov bh, bl
    shr bl, 4
    call hex_digit
    call serial_send_byte
    mov bl, bh
    and bl, 0x0f
    call hex_digit
    jmp serial_send_byte

hex_value:
    and bl, 0xdf
    sub bl, '0'
    cmp bl, 9
    jbe .ok
    sub bl, 7
.ok:
    ret

emit_hex_byte:
    mov bh, bl
    shr bl, 4
    call hex_digit
    call rsp_emit
    mov bl, bh
    and bl, 0x0f
    call hex_digit
    jmp rsp_emit

emit_hex_word:
    mov bl, al
    call emit_hex_byte
    mov bl, ah
    jmp emit_hex_byte

emit_hex_dword:
    push eax
    call emit_hex_word
    pop eax
    shr eax, 16
    jmp emit_hex_word

send_empty:
    call rsp_begin
    jmp rsp_end

send_ok:
    call rsp_begin
    mov bl, 'O'
    call rsp_emit
    mov bl, 'K'
    call rsp_emit
    jmp rsp_end

send_s05:
    call rsp_begin
    mov bl, 'S'
    call rsp_emit
    mov bl, '0'
    call rsp_emit
    mov bl, '5'
    call rsp_emit
    jmp rsp_end

parse_hex_u32:
    xor eax, eax
.lp:
    lodsb
    cmp al, dl
    je .done
    mov bl, al
    shl eax, 4
    call hex_value
    or al, bl
    jmp .lp
.done:
    ret

parse_hex_byte:
    lodsb
    mov bl, al
    call hex_value
    shl bl, 4
    mov ah, bl
    lodsb
    mov bl, al
    call hex_value
    mov al, bl
    or al, ah
    ret

parse_hex_word_le:
    call parse_hex_byte
    mov dl, al
    call parse_hex_byte
    mov ah, al
    mov al, dl
    ret

sync_frame_to_shadow:
    mov eax, [ebp+28]
    mov [reg_shadow+0], eax
    mov eax, [ebp+24]
    mov [reg_shadow+4], eax
    mov eax, [ebp+20]
    mov [reg_shadow+8], eax
    mov eax, [ebp+16]
    mov [reg_shadow+12], eax
    mov eax, [ebp+12]
    add eax, 8
    mov [reg_shadow+16], eax
    mov eax, [ebp+8]
    mov [reg_shadow+20], eax
    mov eax, [ebp+4]
    mov [reg_shadow+24], eax
    mov eax, [ebp]
    mov [reg_shadow+28], eax
    mov eax, [ebp+36]
    mov [reg_shadow+32], eax
    mov eax, [ebp+32]
    mov [reg_shadow+36], eax
    ret

sync_shadow_to_frame:
    mov eax, [reg_shadow+0]
    mov [ebp+28], eax
    mov eax, [reg_shadow+4]
    mov [ebp+24], eax
    mov eax, [reg_shadow+8]
    mov [ebp+20], eax
    mov eax, [reg_shadow+12]
    mov [ebp+16], eax
    mov eax, [reg_shadow+16]
    sub eax, 8
    mov [ebp+12], eax
    mov eax, [reg_shadow+20]
    mov [ebp+8], eax
    mov eax, [reg_shadow+24]
    mov [ebp+4], eax
    mov eax, [reg_shadow+28]
    mov [ebp], eax
    mov eax, [reg_shadow+32]
    mov [ebp+36], eax
    mov eax, [reg_shadow+36]
    mov [ebp+32], eax
    ret

send_regs32:
    call rsp_begin
    mov eax, [reg_shadow+0]
    call emit_hex_dword
    mov eax, [reg_shadow+4]
    call emit_hex_dword
    mov eax, [reg_shadow+8]
    call emit_hex_dword
    mov eax, [reg_shadow+12]
    call emit_hex_dword
    mov eax, [reg_shadow+16]
    call emit_hex_dword
    mov eax, [reg_shadow+20]
    call emit_hex_dword
    mov eax, [reg_shadow+24]
    call emit_hex_dword
    mov eax, [reg_shadow+28]
    call emit_hex_dword
    mov eax, [reg_shadow+32]
    call emit_hex_dword
    jmp rsp_end

get_reg32_value:
    cmp ecx, 0
    jb .zero
    cmp ecx, 8
    jbe .shadow
    cmp ecx, 9
    je .efl
    cmp ecx, 10
    je .cs
    cmp ecx, 11
    je .ss
    cmp ecx, 12
    je .ds
    cmp ecx, 13
    je .es
    jmp .zero
.shadow:
    lea edx, [reg_shadow + ecx*4]
    mov eax, [edx]
    ret
.efl:
    mov eax, [reg_shadow+36]
    ret
.cs:
    mov eax, 0x18
    ret
.ss:
    mov eax, 0x20
    ret
.ds:
    mov eax, 0x20
    ret
.es:
    mov eax, 0x20
    ret
.zero:
    xor eax, eax
    ret

send_single_reg32:
    lodsb
    cmp al, 'p'
    jne send_empty
    mov dl, 0
    call parse_hex_u32
    mov ecx, eax
    call rsp_begin
    call get_reg32_value
    call emit_hex_dword
    jmp rsp_end

parse_hex_dword_le:
    call parse_hex_byte
    movzx eax, al
    call parse_hex_byte
    movzx edx, al
    shl edx, 8
    or eax, edx
    call parse_hex_byte
    movzx edx, al
    shl edx, 16
    or eax, edx
    call parse_hex_byte
    movzx edx, al
    shl edx, 24
    or eax, edx
    ret

write_single_reg32:
    lodsb
    cmp al, 'P'
    jne .bad
    mov dl, '='
    call parse_hex_u32
    mov ecx, eax
    call parse_hex_dword_le
    cmp ecx, 0
    jb .bad
    cmp ecx, 8
    jbe .shadow
    cmp ecx, 9
    je .efl
    jmp send_ok
.shadow:
    lea edx, [reg_shadow + ecx*4]
    mov [edx], eax
    jmp send_ok
.efl:
    mov [reg_shadow+36], eax
    jmp send_ok
.bad:
    jmp send_empty

write_all_regs32:
    lodsb
    cmp al, 'G'
    jne .bad
    call parse_hex_dword_le
    mov [reg_shadow+0], eax
    call parse_hex_dword_le
    mov [reg_shadow+4], eax
    call parse_hex_dword_le
    mov [reg_shadow+8], eax
    call parse_hex_dword_le
    mov [reg_shadow+12], eax
    call parse_hex_dword_le
    mov [reg_shadow+16], eax
    call parse_hex_dword_le
    mov [reg_shadow+20], eax
    call parse_hex_dword_le
    mov [reg_shadow+24], eax
    call parse_hex_dword_le
    mov [reg_shadow+28], eax
    call parse_hex_dword_le
    mov [reg_shadow+32], eax
    jmp send_ok
.bad:
    jmp send_empty

handle_read_mem:
    lodsb
    cmp al, 'm'
    jne send_empty
    mov dl, ','
    call parse_hex_u32
    mov edi, eax
    mov dl, 0
    call parse_hex_u32
    mov ecx, eax
    call rsp_begin
.lp:
    test ecx, ecx
    jz .done
    mov bl, [edi]
    inc edi
    call emit_hex_byte
    dec ecx
    jmp .lp
.done:
    jmp rsp_end

handle_write_mem:
    lodsb
    cmp al, 'M'
    jne send_empty
    mov dl, ','
    call parse_hex_u32
    mov edi, eax
    mov dl, ':'
    call parse_hex_u32
    mov ecx, eax
.lp:
    test ecx, ecx
    jz .done
    call parse_hex_byte
    mov [edi], al
    inc edi
    dec ecx
    jmp .lp
.done:
    jmp send_ok

gdb_debug_break:
    pushfd
    pushad
    mov ebp, esp
    call sync_frame_to_shadow
    call send_s05
.loop:
    call rsp_recv_packet  ; 调用后，eax 指向栈上的缓冲区
    mov esi, eax
    mov al, [esi]
    cmp al, 'c'
    je .cont
    cmp al, 's'
    je .cont
    cmp al, '?'
    je .sig
    cmp al, 'g'
    je .regs
    cmp al, 'p'
    je .regr
    cmp al, 'G'
    je .setrs
    cmp al, 'm'
    je .memr
    cmp al, 'M'
    je .memw
    cmp al, 'P'
    je .setr
    cmp al, 'H'
    je .ok
    cmp al, 'q'
    je .empty
    cmp al, 'v'
    je .empty
    jmp .empty
.sig:
    call send_s05
    jmp .loop
.regs:
    call send_regs32
    jmp .loop
.regr:
    call send_single_reg32
    jmp .loop
.setrs:
    call write_all_regs32
    jmp .loop
.memr:
    call handle_read_mem
    jmp .loop
.memw:
    call handle_write_mem
    jmp .loop
.setr:
    call write_single_reg32
    jmp .loop
.ok:
    call send_ok
    jmp .loop
.empty:
    call send_empty
    jmp .loop
.cont:
    call sync_shadow_to_frame
    popad
    popfd
    ret

; 使用局部变量替代全局变量
section .bss
rsp_buf resb 256
rsp_checksum resb 1
reg_shadow resd 10

%else
section .text
greatstart:
    ; 跳转到内核入口点
    jmp 0x18:0xc0100000
    ud2
%endif