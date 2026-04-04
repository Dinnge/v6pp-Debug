[BITS 32]
[extern kernelBridge]

global greatstart

%ifdef EARLY_BOOT_GDB
GDB_PORT        equ 0x3F8
GDB_LINE_STATUS equ 0x3FD

section .text

greatstart:
    call gdb_debug_break
    jmp kernelBridge
    ud2

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

rsp_recv_packet:
.ws:
    call serial_recv_byte
    cmp al, '$'
    jne .ws
    mov edi, rsp_buf
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
    jmp serial_send_byte

rsp_begin:
    mov byte [rsp_sum], 0
    mov bl, '$'
    jmp serial_send_byte

rsp_emit:
    add byte [rsp_sum], bl
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
    mov bl, [rsp_sum]
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

send_regs16:
    call rsp_begin
    mov ax, [esp+28]
    call emit_hex_word
    mov ax, [esp+24]
    call emit_hex_word
    mov ax, [esp+20]
    call emit_hex_word
    mov ax, [esp+16]
    call emit_hex_word
    mov ax, [esp+12]
    call emit_hex_word
    mov ax, [esp+8]
    call emit_hex_word
    mov ax, [esp+4]
    call emit_hex_word
    mov ax, [esp+0]
    call emit_hex_word
    mov ax, [esp+36]
    call emit_hex_word
    mov ax, [esp+32]
    call emit_hex_word
    mov ax, 0x18
    call emit_hex_word
    mov ax, 0x20
    call emit_hex_word
    mov ax, 0x20
    call emit_hex_word
    mov ax, 0x20
    call emit_hex_word
    xor ax, ax
    call emit_hex_word
    call emit_hex_word
    call emit_hex_word
    call emit_hex_word
    jmp rsp_end

write_single_reg16:
    lodsb
    cmp al, 'P'
    jne .bad
    mov dl, '='
    call parse_hex_u32
    mov ebx, eax
    call parse_hex_word_le
    cmp ebx, 0
    je .eax
    cmp ebx, 1
    je .ecx
    cmp ebx, 2
    je .edx
    cmp ebx, 3
    je .ebx
    cmp ebx, 4
    je .esp
    cmp ebx, 5
    je .ebp
    cmp ebx, 6
    je .esi
    cmp ebx, 7
    je .edi
    cmp ebx, 8
    je .eip
    cmp ebx, 9
    je .efl
    jmp send_ok
.eax:
    mov [esp+28], ax
    jmp send_ok
.ecx:
    mov [esp+24], ax
    jmp send_ok
.edx:
    mov [esp+20], ax
    jmp send_ok
.ebx:
    mov [esp+16], ax
    jmp send_ok
.esp:
    mov [esp+12], ax
    jmp send_ok
.ebp:
    mov [esp+8], ax
    jmp send_ok
.esi:
    mov [esp+4], ax
    jmp send_ok
.edi:
    mov [esp+0], ax
    jmp send_ok
.eip:
    mov [esp+36], ax
    jmp send_ok
.efl:
    mov [esp+32], ax
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
    call send_s05
.loop:
    call rsp_recv_packet
    mov esi, rsp_buf
    mov al, [esi]
    cmp al, 'c'
    je .cont
    cmp al, 's'
    je .cont
    cmp al, '?'
    je .sig
    cmp al, 'g'
    je .regs
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
    call send_regs16
    jmp .loop
.memr:
    call handle_read_mem
    jmp .loop
.memw:
    call handle_write_mem
    jmp .loop
.setr:
    call write_single_reg16
    jmp .loop
.ok:
    call send_ok
    jmp .loop
.empty:
    call send_empty
    jmp .loop
.cont:
    popad
    popfd
    ret

section .bss
rsp_buf resb 256
section .data
rsp_sum db 0

%else
section .text
greatstart:
    jmp kernelBridge
    ud2
%endif
