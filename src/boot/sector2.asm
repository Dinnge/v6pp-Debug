[BITS 32]
[extern kernelBridge]

global greatstart

BP_MAX    equ 16
EFLAGS_TF equ 0x100

%ifdef EARLY_BOOT_GDB
GDB_PORT        equ 0x3F8
GDB_BAUD_LATCH  equ 0x3F9
GDB_LINE_CTRL   equ 0x3FB
GDB_MODEM_CTRL  equ 0x3FC
GDB_LINE_STATUS equ 0x3FD

section .text

greatstart:
    call serial_init
    call gdb_state_init
    call gdb_debug_break
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

gdb_state_init:
    cld
    xor eax, eax
    mov edi, rsp_buf
    mov ecx, (tmp_bp_orig + 1) - rsp_buf
    rep stosb
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

rsp_recv_packet:
    cld
.wait_start:
    call serial_recv_byte
    cmp al, '$'
    jne .wait_start

    mov edi, rsp_buf
    mov ecx, 255
.loop:
    call serial_recv_byte
    cmp al, '#'
    je .done
    test ecx, ecx
    jz .loop
    stosb
    dec ecx
    jmp .loop

.done:
    mov byte [edi], 0
    call serial_recv_byte
    call serial_recv_byte
    mov bl, '+'
    call serial_send_byte
    mov eax, rsp_buf
    ret

rsp_begin:
    mov byte [rsp_checksum], 0
    mov bl, '$'
    jmp serial_send_byte

rsp_emit:
    add byte [rsp_checksum], bl
    jmp serial_send_byte

rsp_emit_cstr:
.loop:
    lodsb
    test al, al
    jz .done
    mov bl, al
    call rsp_emit
    jmp .loop
.done:
    ret

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
    cmp bl, '0'
    jb .bad
    cmp bl, '9'
    jbe .digit
    cmp bl, 'a'
    jb .upper
    cmp bl, 'f'
    jbe .lower
.upper:
    cmp bl, 'A'
    jb .bad
    cmp bl, 'F'
    ja .bad
    sub bl, 'A' - 10
    ret
.lower:
    sub bl, 'a' - 10
    ret
.digit:
    sub bl, '0'
    ret
.bad:
    xor bl, bl
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

emit_console_char:
    jmp emit_hex_byte

emit_ascii_hex_dword:
    push eax
    push ecx
    push edx
    mov ecx, 28
.loop:
    mov edx, eax
    shr edx, cl
    and dl, 0x0f
    mov bl, dl
    call hex_digit
    call emit_console_char
    sub ecx, 4
    jns .loop
    pop edx
    pop ecx
    pop eax
    ret

send_bt_line:
    push eax
    call rsp_begin
    mov bl, 'O'
    call rsp_emit
    mov bl, '#'
    call emit_console_char
    mov bl, cl
    add bl, '0'
    call emit_console_char
    mov bl, ' '
    call emit_console_char
    mov bl, '0'
    call emit_console_char
    mov bl, 'x'
    call emit_console_char
    pop eax
    call emit_ascii_hex_dword
    mov bl, 10
    call emit_console_char
    jmp rsp_end

send_backtrace:
    mov cl, 0
    mov eax, [reg_shadow + 32]
    call send_bt_line
    mov ebx, [reg_shadow + 20]
    mov esi, [reg_shadow + 16]
    mov edi, esi
    add edi, 0x8000
.next:
    cmp cl, 9
    jae .done
    test ebx, ebx
    jz .done
    test ebx, 3
    jne .done
    cmp ebx, esi
    jb .done
    cmp ebx, edi
    jae .done
    mov edx, [ebx]
    mov eax, [ebx + 4]
    inc cl
    call send_bt_line
    cmp edx, ebx
    jbe .done
    mov ebx, edx
    jmp .next
.done:
    ret

parse_hex_u32:
    xor ecx, ecx
.loop:
    lodsb
    test al, al
    jz .done
    cmp al, dl
    je .done
    mov bl, al
    call hex_value
    shl ecx, 4
    movzx eax, bl
    or ecx, eax
    jmp .loop
.done:
    mov bl, al
    mov eax, ecx
    ret

parse_hex_byte:
    lodsb
    mov bl, al
    call hex_value
    movzx eax, bl
    shl eax, 4
    mov dl, al
    lodsb
    mov bl, al
    call hex_value
    movzx eax, bl
    or al, dl
    ret

sync_frame_to_shadow:
    mov eax, [ebp + 28]
    mov [reg_shadow + 0], eax
    mov eax, [ebp + 24]
    mov [reg_shadow + 4], eax
    mov eax, [ebp + 20]
    mov [reg_shadow + 8], eax
    mov eax, [ebp + 16]
    mov [reg_shadow + 12], eax
    mov eax, [ebp + 12]
    add eax, 8
    mov [reg_shadow + 16], eax
    mov eax, [ebp + 8]
    mov [reg_shadow + 20], eax
    mov eax, [ebp + 4]
    mov [reg_shadow + 24], eax
    mov eax, [ebp + 0]
    mov [reg_shadow + 28], eax
    mov eax, [ebp + 36]
    mov [reg_shadow + 32], eax
    mov eax, [ebp + 32]
    mov [reg_shadow + 36], eax
    mov eax, 0x18
    mov [reg_shadow + 40], eax
    mov eax, 0x20
    mov [reg_shadow + 44], eax
    mov [reg_shadow + 48], eax
    mov [reg_shadow + 52], eax
    xor eax, eax
    mov [reg_shadow + 56], eax
    mov [reg_shadow + 60], eax
    ret

sync_shadow_to_frame:
    mov eax, [reg_shadow + 0]
    mov [ebp + 28], eax
    mov eax, [reg_shadow + 4]
    mov [ebp + 24], eax
    mov eax, [reg_shadow + 8]
    mov [ebp + 20], eax
    mov eax, [reg_shadow + 12]
    mov [ebp + 16], eax
    mov eax, [reg_shadow + 16]
    sub eax, 8
    mov [ebp + 12], eax
    mov eax, [reg_shadow + 20]
    mov [ebp + 8], eax
    mov eax, [reg_shadow + 24]
    mov [ebp + 4], eax
    mov eax, [reg_shadow + 28]
    mov [ebp + 0], eax
    mov eax, [reg_shadow + 32]
    mov [ebp + 36], eax
    mov eax, [reg_shadow + 36]
    mov [ebp + 32], eax
    ret

send_regs32:
    mov ecx, 16
    mov edi, reg_shadow
    call rsp_begin
.loop:
    mov eax, [edi]
    call emit_hex_dword
    add edi, 4
    loop .loop
    jmp rsp_end

get_reg32_value:
    cmp ecx, 0
    je .eax
    cmp ecx, 1
    je .ecx
    cmp ecx, 2
    je .edx
    cmp ecx, 3
    je .ebx
    cmp ecx, 4
    je .esp
    cmp ecx, 5
    je .ebp
    cmp ecx, 6
    je .esi
    cmp ecx, 7
    je .edi
    cmp ecx, 8
    je .eip
    cmp ecx, 9
    je .eflags
    cmp ecx, 10
    je .cs
    cmp ecx, 11
    je .ss
    cmp ecx, 12
    je .ds
    cmp ecx, 13
    je .es
    cmp ecx, 14
    je .fs
    cmp ecx, 15
    je .gs
    xor eax, eax
    ret
.eax:
    mov eax, [reg_shadow + 0]
    ret
.ecx:
    mov eax, [reg_shadow + 4]
    ret
.edx:
    mov eax, [reg_shadow + 8]
    ret
.ebx:
    mov eax, [reg_shadow + 12]
    ret
.esp:
    mov eax, [reg_shadow + 16]
    ret
.ebp:
    mov eax, [reg_shadow + 20]
    ret
.esi:
    mov eax, [reg_shadow + 24]
    ret
.edi:
    mov eax, [reg_shadow + 28]
    ret
.eip:
    mov eax, [reg_shadow + 32]
    ret
.eflags:
    mov eax, [reg_shadow + 36]
    ret
.cs:
    mov eax, [reg_shadow + 40]
    ret
.ss:
    mov eax, [reg_shadow + 44]
    ret
.ds:
    mov eax, [reg_shadow + 48]
    ret
.es:
    mov eax, [reg_shadow + 52]
    ret
.fs:
    mov eax, [reg_shadow + 56]
    ret
.gs:
    mov eax, [reg_shadow + 60]
    ret

send_single_reg32:
    lodsb
    cmp al, 'p'
    jne send_empty
    mov esi, rsp_buf
    add esi, 1
    xor ecx, ecx
.parse:
    mov al, [esi]
    test al, al
    jz .done
    cmp al, ';'
    je .done
    mov bl, al
    call hex_value
    shl ecx, 4
    movzx edx, bl
    or ecx, edx
    inc esi
    jmp .parse
.done:
    call rsp_begin
    call get_reg32_value
    call emit_hex_dword
    jmp rsp_end

parse_nibble_strict:
    cmp al, '0'
    jb .bad
    cmp al, '9'
    jbe .digit
    cmp al, 'a'
    jb .upper
    cmp al, 'f'
    jbe .lower
.upper:
    cmp al, 'A'
    jb .bad
    cmp al, 'F'
    ja .bad
    mov bl, al
    sub bl, 'A' - 10
    clc
    ret
.lower:
    mov bl, al
    sub bl, 'a' - 10
    clc
    ret
.digit:
    mov bl, al
    sub bl, '0'
    clc
    ret
.bad:
    stc
    ret

parse_hex_byte_strict:
    mov al, [esi]
    test al, al
    jz .bad
    call parse_nibble_strict
    jc .bad
    mov dl, bl
    shl dl, 4

    mov al, [esi + 1]
    test al, al
    jz .bad
    call parse_nibble_strict
    jc .bad

    mov al, dl
    or al, bl
    add esi, 2
    clc
    ret
.bad:
    stc
    ret

parse_hex_dword_le_strict:
    push edi
    call parse_hex_byte_strict
    jc .bad
    movzx edi, al

    call parse_hex_byte_strict
    jc .bad
    movzx edx, al
    shl edx, 8
    or edi, edx

    call parse_hex_byte_strict
    jc .bad
    movzx edx, al
    shl edx, 16
    or edi, edx

    call parse_hex_byte_strict
    jc .bad
    movzx edx, al
    shl edx, 24
    or edi, edx

    mov eax, edi
    pop edi
    clc
    ret
.bad:
    pop edi
    stc
    ret

write_single_reg32:
    lodsb
    cmp al, 'P'
    jne .bad
    mov esi, rsp_buf
    add esi, 1
    xor ecx, ecx
.find_eq:
    mov al, [esi]
    test al, al
    jz .bad
    cmp al, '='
    je .value
    mov bl, al
    call hex_value
    shl ecx, 4
    movzx edx, bl
    or ecx, edx
    inc esi
    jmp .find_eq
.value:
    inc esi
    call parse_hex_dword_le_strict
    jc .bad
    cmp ecx, 0
    je .eax
    cmp ecx, 1
    je .ecx
    cmp ecx, 2
    je .edx
    cmp ecx, 3
    je .ebx
    cmp ecx, 4
    je .esp
    cmp ecx, 5
    je .ebp
    cmp ecx, 6
    je .esi
    cmp ecx, 7
    je .edi
    cmp ecx, 8
    je .eip
    cmp ecx, 9
    je .eflags
    cmp ecx, 10
    je .cs
    cmp ecx, 11
    je .ss
    cmp ecx, 12
    je .ds
    cmp ecx, 13
    je .es
    cmp ecx, 14
    je .fs
    cmp ecx, 15
    je .gs
    jmp send_ok
.eax:
    mov [reg_shadow + 0], eax
    jmp send_ok
.ecx:
    mov [reg_shadow + 4], eax
    jmp send_ok
.edx:
    mov [reg_shadow + 8], eax
    jmp send_ok
.ebx:
    mov [reg_shadow + 12], eax
    jmp send_ok
.esp:
    mov [reg_shadow + 16], eax
    jmp send_ok
.ebp:
    mov [reg_shadow + 20], eax
    jmp send_ok
.esi:
    mov [reg_shadow + 24], eax
    jmp send_ok
.edi:
    mov [reg_shadow + 28], eax
    jmp send_ok
.eip:
    mov [reg_shadow + 32], eax
    jmp send_ok
.eflags:
    mov [reg_shadow + 36], eax
    jmp send_ok
.cs:
    mov [reg_shadow + 40], eax
    jmp send_ok
.ss:
    mov [reg_shadow + 44], eax
    jmp send_ok
.ds:
    mov [reg_shadow + 48], eax
    jmp send_ok
.es:
    mov [reg_shadow + 52], eax
    jmp send_ok
.fs:
    mov [reg_shadow + 56], eax
    jmp send_ok
.gs:
    mov [reg_shadow + 60], eax
    jmp send_ok
.bad:
    jmp send_empty

write_all_regs32:
    lodsb
    cmp al, 'G'
    jne .bad
    mov ecx, 16
    mov edi, reg_shadow
.loop:
    call parse_hex_dword_le_strict
    jc .bad
    mov [edi], eax
    add edi, 4
    loop .loop
    jmp send_ok
.bad:
    jmp send_empty

handle_read_mem:
    lodsb
    cmp al, 'm'
    jne .bad
    mov dl, ','
    call parse_hex_u32
    cmp bl, ','
    jne .bad
    mov edi, eax
    mov dl, 0
    call parse_hex_u32
    mov ecx, eax
    call rsp_begin
.loop:
    test ecx, ecx
    jz .done
    mov bl, [edi]
    call emit_hex_byte
    inc edi
    dec ecx
    jmp .loop
.done:
    jmp rsp_end
.bad:
    jmp send_empty

handle_write_mem:
    lodsb
    cmp al, 'M'
    jne .bad
    mov dl, ','
    call parse_hex_u32
    cmp bl, ','
    jne .bad
    mov edi, eax
    mov dl, ':'
    call parse_hex_u32
    cmp bl, ':'
    jne .bad
    mov ecx, eax
.loop:
    test ecx, ecx
    jz .ok
    mov al, [esi]
    test al, al
    jz .bad
    mov al, [esi + 1]
    test al, al
    jz .bad
    call parse_hex_byte
    mov [edi], al
    inc edi
    dec ecx
    jmp .loop
.ok:
    jmp send_ok
.bad:
    jmp send_empty

handle_query:
    lodsb
    cmp al, 'q'
    jne .bad
    mov esi, rsp_buf
    add esi, 1
    cmp byte [esi], 'S'
    je .supported
    cmp byte [esi], 'C'
    je .current_thread
    cmp byte [esi], 'A'
    je .attached
    cmp byte [esi], 'f'
    je .thread_first
    cmp byte [esi], 's'
    je .thread_next
    cmp byte [esi], 'R'
    je .rcmd
    jmp .bad
.supported:
    cmp byte [esi + 1], 'u'
    jne .bad
    cmp byte [esi + 2], 'p'
    jne .bad
    cmp byte [esi + 3], 'p'
    jne .bad
    cmp byte [esi + 4], 'o'
    jne .bad
    cmp byte [esi + 5], 'r'
    jne .bad
    cmp byte [esi + 6], 't'
    jne .bad
    cmp byte [esi + 7], 'e'
    jne .bad
    cmp byte [esi + 8], 'd'
    jne .bad
    call rsp_begin
    mov esi, gdb_qsupported_features
    call rsp_emit_cstr
    jmp rsp_end
.current_thread:
    cmp byte [esi + 1], 0
    jne .bad
    call rsp_begin
    mov esi, gdb_qc_reply
    call rsp_emit_cstr
    jmp rsp_end
.attached:
    cmp byte [esi + 1], 't'
    jne .bad
    cmp byte [esi + 2], 't'
    jne .bad
    cmp byte [esi + 3], 'a'
    jne .bad
    cmp byte [esi + 4], 'c'
    jne .bad
    cmp byte [esi + 5], 'h'
    jne .bad
    cmp byte [esi + 6], 'e'
    jne .bad
    cmp byte [esi + 7], 'd'
    jne .bad
    call rsp_begin
    mov esi, gdb_attached_reply
    call rsp_emit_cstr
    jmp rsp_end
.thread_first:
    cmp byte [esi + 1], 'T'
    jne .bad
    cmp byte [esi + 2], 'h'
    jne .bad
    cmp byte [esi + 3], 'r'
    jne .bad
    cmp byte [esi + 4], 'e'
    jne .bad
    cmp byte [esi + 5], 'a'
    jne .bad
    cmp byte [esi + 6], 'd'
    jne .bad
    cmp byte [esi + 7], 'I'
    jne .bad
    cmp byte [esi + 8], 'n'
    jne .bad
    cmp byte [esi + 9], 'f'
    jne .bad
    cmp byte [esi + 10], 'o'
    jne .bad
    call rsp_begin
    mov esi, gdb_thread_first_reply
    call rsp_emit_cstr
    jmp rsp_end
.thread_next:
    cmp byte [esi + 1], 'T'
    jne .bad
    cmp byte [esi + 2], 'h'
    jne .bad
    cmp byte [esi + 3], 'r'
    jne .bad
    cmp byte [esi + 4], 'e'
    jne .bad
    cmp byte [esi + 5], 'a'
    jne .bad
    cmp byte [esi + 6], 'd'
    jne .bad
    cmp byte [esi + 7], 'I'
    jne .bad
    cmp byte [esi + 8], 'n'
    jne .bad
    cmp byte [esi + 9], 'f'
    jne .bad
    cmp byte [esi + 10], 'o'
    jne .bad
    call rsp_begin
    mov esi, gdb_thread_next_reply
    call rsp_emit_cstr
    jmp rsp_end
.rcmd:
    cmp byte [esi + 1], 'c'
    jne .bad
    cmp byte [esi + 2], 'm'
    jne .bad
    cmp byte [esi + 3], 'd'
    jne .bad
    cmp byte [esi + 4], ','
    jne .bad
    add esi, 5
    call parse_hex_byte
    cmp al, 'b'
    jne .bad
    call parse_hex_byte
    cmp al, 't'
    jne .bad
    call send_backtrace
    jmp send_ok
.bad:
    jmp send_empty

handle_set_sw_break:
    lodsb
    cmp al, 'Z'
    jne .bad
    lodsb
    cmp al, '0'
    je .type_ok
    cmp al, '1'
    jne .bad
.type_ok:
    lodsb
    cmp al, ','
    jne .bad
    mov dl, ','
    call parse_hex_u32
    cmp bl, ','
    jne .bad
    mov edi, eax
    mov dl, 0
    call parse_hex_u32
    mov ebx, -1
    xor ecx, ecx
.find:
    cmp ecx, BP_MAX
    jae .alloc
    mov al, [bp_used + ecx]
    test al, al
    jz .free
    mov eax, [bp_addr + ecx * 4]
    cmp eax, edi
    je .ok
    inc ecx
    jmp .find
.free:
    cmp ebx, -1
    jne .next
    mov ebx, ecx
.next:
    inc ecx
    jmp .find
.alloc:
    cmp ebx, -1
    je .bad
    mov ecx, ebx
    mov al, [edi]
    mov [bp_orig + ecx], al
    mov byte [edi], 0xCC
    mov [bp_addr + ecx * 4], edi
    mov byte [bp_used + ecx], 1
.ok:
    jmp send_ok
.bad:
    jmp send_empty

handle_clear_sw_break:
    lodsb
    cmp al, 'z'
    jne .bad
    lodsb
    cmp al, '0'
    je .type_ok
    cmp al, '1'
    jne .bad
.type_ok:
    lodsb
    cmp al, ','
    jne .bad
    mov dl, ','
    call parse_hex_u32
    cmp bl, ','
    jne .bad
    mov edi, eax
    mov dl, 0
    call parse_hex_u32
    xor ecx, ecx
.find:
    cmp ecx, BP_MAX
    jae .bad
    mov al, [bp_used + ecx]
    test al, al
    jz .next
    mov eax, [bp_addr + ecx * 4]
    cmp eax, edi
    je .clear
.next:
    inc ecx
    jmp .find
.clear:
    mov al, [bp_orig + ecx]
    mov [edi], al
    mov byte [bp_used + ecx], 0
    jmp send_ok
.bad:
    jmp send_empty

clear_temp_breakpoint:
    cmp byte [tmp_bp_active], 0
    je .done
    mov edi, [tmp_bp_addr]
    mov al, [tmp_bp_orig]
    mov [edi], al
    mov byte [tmp_bp_active], 0
.done:
    ret

handle_temp_break_hit:
    cmp byte [tmp_bp_active], 0
    je .done
    mov eax, [reg_shadow + 32]
    mov edx, [tmp_bp_addr]
    cmp eax, edx
    je .clear_only
    lea ecx, [edx + 1]
    cmp eax, ecx
    jne .done
    mov [reg_shadow + 32], edx
.clear_only:
    call clear_temp_breakpoint
.done:
    ret

compute_step_target:
    push ebx
    push ecx
    push edx
    mov bl, [eax]
    cmp bl, 0xE8
    je .rel32_target
    cmp bl, 0xE9
    je .rel32_target
    cmp bl, 0xEB
    je .jmp_short
    cmp bl, 0x0F
    je .op_0f
    cmp bl, 0x66
    je .op_66
    cmp bl, 0xB0
    je .len2
    cmp bl, 0xA8
    je .len2
    cmp bl, 0x74
    je .len2
    cmp bl, 0x30
    je .op_30
    cmp bl, 0xEE
    je .len1
    cmp bl, 0xEC
    je .len1
    cmp bl, 0xC3
    je .len1
    jmp .len1
.rel32_target:
    mov edx, [eax + 1]
    lea eax, [eax + 5]
    add eax, edx
    jmp .done
.jmp_short:
    movsx edx, byte [eax + 1]
    lea eax, [eax + 2]
    add eax, edx
    jmp .done
.op_0f:
    cmp byte [eax + 1], 0x0B
    jne .len1
    add eax, 2
    jmp .done
.op_66:
    cmp byte [eax + 1], 0xBA
    jne .len1
    add eax, 4
    jmp .done
.op_30:
    cmp byte [eax + 1], 0xC0
    jne .len1
.len2:
    add eax, 2
    jmp .done
.len1:
    add eax, 1
.done:
    pop edx
    pop ecx
    pop ebx
    ret

handle_continue_packet:
    mov esi, rsp_buf
    add esi, 1
    mov al, [esi]
    test al, al
    jz .clear_tf
    mov dl, 0
    call parse_hex_u32
    mov [reg_shadow + 32], eax
.clear_tf:
    and dword [reg_shadow + 36], ~EFLAGS_TF
    call clear_temp_breakpoint
    ret

handle_step_over_packet:
    and dword [reg_shadow + 36], ~EFLAGS_TF
    mov eax, [reg_shadow + 32]
    mov bl, [eax]
    cmp bl, 0xE8
    jne .fallback
    add eax, 5
    mov [reg_shadow + 32], eax
    ret
.fallback:
    call compute_step_target
    mov [reg_shadow + 32], eax
    ret

handle_step_packet:
    mov esi, rsp_buf
    add esi, 1
    mov al, [esi]
    test al, al
    jz .step_now
    mov dl, 0
    call parse_hex_u32
    mov [reg_shadow + 32], eax
.step_now:
    and dword [reg_shadow + 36], ~EFLAGS_TF
    mov eax, [reg_shadow + 32]
    call compute_step_target
    mov [reg_shadow + 32], eax
    ret

arm_temp_breakpoint:
    call clear_temp_breakpoint
    mov edi, eax
    mov dl, [edi]
    mov [tmp_bp_addr], edi
    mov [tmp_bp_orig], dl
    mov byte [edi], 0xCC
    mov byte [tmp_bp_active], 1
    ret

handle_range_step_packet:
    mov esi, rsp_buf
    add esi, 7
    mov dl, ','
    call parse_hex_u32
    cmp bl, ','
    jne .bad
    mov edx, eax
    mov dl, ':'
    call parse_hex_u32
    cmp bl, 0
    je .parsed
    cmp bl, ':'
    jne .bad
.parsed:
    mov ecx, eax
    and dword [reg_shadow + 36], ~EFLAGS_TF
    call clear_temp_breakpoint
    mov eax, [reg_shadow + 32]
    cmp eax, edx
    jb .single_step
    cmp eax, ecx
    jae .single_step

    mov bl, [eax]
    cmp bl, 0xE8
    je .continue_to_end
    cmp bl, 0x9A
    je .continue_to_end
    cmp bl, 0xFF
    jne .compute_target
    mov bl, [eax + 1]
    shr bl, 3
    and bl, 0x07
    cmp bl, 2
    je .continue_to_end
    cmp bl, 3
    je .continue_to_end

.compute_target:
    mov eax, [reg_shadow + 32]
    call compute_step_target
    cmp eax, edx
    jb .step_now
    cmp eax, ecx
    jae .step_now

.continue_to_end:
    mov eax, ecx
    call arm_temp_breakpoint
    mov eax, 1
    ret

.single_step:
    mov eax, [reg_shadow + 32]
    call compute_step_target

.step_now:
    mov [reg_shadow + 32], eax
    mov eax, 2
    ret
.bad:
    xor eax, eax
    ret

handle_v_packet:
    lodsb
    cmp al, 'v'
    jne .bad
    mov esi, rsp_buf
    add esi, 1
    cmp byte [esi], 'C'
    jne .bad
    cmp byte [esi + 1], 'o'
    jne .bad
    cmp byte [esi + 2], 'n'
    jne .bad
    cmp byte [esi + 3], 't'
    jne .bad
    mov al, [esi + 4]
    cmp al, '?'
    je .caps
    cmp al, ';'
    je .run
    jmp .bad
.caps:
    call rsp_begin
    mov bl, 'v'
    call rsp_emit
    mov bl, 'C'
    call rsp_emit
    mov bl, 'o'
    call rsp_emit
    mov bl, 'n'
    call rsp_emit
    mov bl, 't'
    call rsp_emit
    mov bl, ';'
    call rsp_emit
    mov bl, 'c'
    call rsp_emit
    mov bl, ';'
    call rsp_emit
    mov bl, 's'
    call rsp_emit
    mov bl, ';'
    call rsp_emit
    mov bl, 'S'
    call rsp_emit
    mov bl, ';'
    call rsp_emit
    mov bl, 'r'
    call rsp_emit
    jmp rsp_end
.run:
    mov al, [esi + 5]
    cmp al, 's'
    je .step
    cmp al, 'S'
    je .step
    cmp al, 'r'
    je .range
    cmp al, 'n'
    je .next
    cmp al, 'N'
    je .next
    cmp al, 'c'
    je .cont
    cmp al, 'C'
    je .cont
    jmp .bad
.step:
    call handle_step_packet
    mov eax, 2
    ret
.next:
    call handle_step_over_packet
    mov eax, 2
    ret
.range:
    call handle_range_step_packet
    ret
.cont:
    call handle_continue_packet
    mov eax, 1
    ret
.bad:
    xor eax, eax
    ret

gdb_debug_break:
    cld
    pushfd
    pushad
    mov ebp, esp
    call sync_frame_to_shadow
    call handle_temp_break_hit
    call send_s05

.loop:
    call rsp_recv_packet
    mov esi, eax
    mov al, [esi]
    cmp al, 'c'
    je .continue
    cmp al, 's'
    je .step
    cmp al, '?'
    je .sig
    cmp al, 'g'
    je .regs
    cmp al, 'p'
    je .single_reg
    cmp al, 'G'
    je .set_regs
    cmp al, 'm'
    je .read_mem
    cmp al, 'M'
    je .write_mem
    cmp al, 'P'
    je .set_reg
    cmp al, 'Z'
    je .set_bp
    cmp al, 'z'
    je .clear_bp
    cmp al, 'H'
    je .ok
    cmp al, 'q'
    je .query
    cmp al, 'v'
    je .vpacket
    jmp .empty

.sig:
    call send_s05
    jmp .loop
.regs:
    call send_regs32
    jmp .loop
.single_reg:
    call send_single_reg32
    jmp .loop
.set_regs:
    call write_all_regs32
    jmp .loop
.read_mem:
    call handle_read_mem
    jmp .loop
.write_mem:
    call handle_write_mem
    jmp .loop
.set_reg:
    call write_single_reg32
    jmp .loop
.set_bp:
    call handle_set_sw_break
    jmp .loop
.clear_bp:
    call handle_clear_sw_break
    jmp .loop
.ok:
    call send_ok
    jmp .loop
.query:
    call handle_query
    jmp .loop
.empty:
    call send_empty
    jmp .loop
.vpacket:
    call handle_v_packet
    test eax, eax
    jz .empty
    cmp eax, 2
    je .sig
    jmp .resume
.step:
    call handle_step_packet
    jmp .sig
.continue:
    call handle_continue_packet
.resume:
    call sync_shadow_to_frame
    popad
    popfd
    ret

section .rodata
gdb_qsupported_features db "PacketSize=1000;qRelocInsn+;multiprocess+;vContSupported+;qRcmd+;QStartNoAckMode-;swbreak+;hwbreak+", 0
gdb_qc_reply db "QC1", 0
gdb_attached_reply db "1", 0
gdb_thread_first_reply db "m1", 0
gdb_thread_next_reply db "l", 0

section .bss
rsp_buf        resb 256
rsp_checksum   resb 1
alignb 4
reg_shadow     resd 16
bp_used        resb BP_MAX
alignb 4
bp_addr        resd BP_MAX
bp_orig        resb BP_MAX
tmp_bp_active  resb 1
alignb 4
tmp_bp_addr    resd 1
tmp_bp_orig    resb 1

%else
section .text
greatstart:
    jmp kernelBridge
    ud2
%endif
