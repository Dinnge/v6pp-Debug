[BITS 32]
[extern kernelBridge]

global greatstart

BP_MAX equ 16
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
    call gdb_debug_break
    ; 脤酶脳陋碌陆脛脷潞脣脠毛驴脷碌茫
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

rsp_recv_packet:     ;
    cld
.ws:
    call serial_recv_byte
    cmp al, '$'
    jne .ws
    mov edi, rsp_buf
    mov ecx, 255
.lp:
    call serial_recv_byte
    cmp al, '#'
    je .done
    test ecx, ecx
    jz .lp
    stosb
    dec ecx
    jmp .lp
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

emit_console_char:
    ; GDB "O" packet payload uses hex-encoded bytes.
    jmp emit_hex_byte

emit_ascii_hex_dword:
    push eax
    push ecx
    push edx
    mov ecx, 28
.lp:
    mov edx, eax
    shr edx, cl
    and dl, 0x0f
    mov bl, dl
    call hex_digit
    call emit_console_char
    sub ecx, 4
    jns .lp
    pop edx
    pop ecx
    pop eax
    ret

send_bt_line:
    ; in: cl = frame index (0-9), eax = return eip
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
    ; Frame #0 uses current saved EIP, then follows EBP chain.
    mov cl, 0
    mov eax, [reg_shadow+32]
    call send_bt_line
    mov ebx, [reg_shadow+20]
    mov esi, [reg_shadow+16]
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
    mov eax, [ebx+4]
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
.lp:
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
    jmp .lp
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
    mov eax, 0x18
    mov [reg_shadow+40], eax
    mov eax, 0x20
    mov [reg_shadow+44], eax
    mov [reg_shadow+48], eax
    mov [reg_shadow+52], eax
    xor eax, eax
    mov [reg_shadow+56], eax
    mov [reg_shadow+60], eax
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
    je .efl
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
    jmp .zero
.eax:
    mov eax, [reg_shadow+0]
    ret
.ecx:
    mov eax, [reg_shadow+4]
    ret
.edx:
    mov eax, [reg_shadow+8]
    ret
.ebx:
    mov eax, [reg_shadow+12]
    ret
.esp:
    mov eax, [reg_shadow+16]
    ret
.ebp:
    mov eax, [reg_shadow+20]
    ret
.esi:
    mov eax, [reg_shadow+24]
    ret
.edi:
    mov eax, [reg_shadow+28]
    ret
.eip:
    mov eax, [reg_shadow+32]
    ret
.efl:
    mov eax, [reg_shadow+36]
    ret
.cs:
    mov eax, [reg_shadow+40]
    ret
.ss:
    mov eax, [reg_shadow+44]
    ret
.ds:
    mov eax, [reg_shadow+48]
    ret
.es:
    mov eax, [reg_shadow+52]
    ret
.fs:
    mov eax, [reg_shadow+56]
    ret
.gs:
    mov eax, [reg_shadow+60]
    ret
.zero:
    xor eax, eax
    ret

send_single_reg32:
    lodsb
    cmp al, 'p'
    jne send_empty
    mov esi, rsp_buf
    add esi, 1
    xor ecx, ecx
.parse_reg:
    mov al, [esi]
    test al, al
    jz .reg_ok
    cmp al, ';'
    je .reg_ok
    mov bl, al
    call hex_value
    shl ecx, 4
    movzx edx, bl
    or ecx, edx
    inc esi
    jmp .parse_reg
.reg_ok:
    call rsp_begin
    call get_reg32_value
    call emit_hex_dword
    jmp rsp_end

parse_hex_dword_le:
    push edi                    ; 保护 EDI 寄存器
    call parse_hex_byte
    movzx edi, al               ; 使用 EDI 作为累加器，存入第 1 个字节
    
    call parse_hex_byte
    movzx edx, al
    shl edx, 8
    or edi, edx                 ; 拼接第 2 个字节
    
    call parse_hex_byte
    movzx edx, al
    shl edx, 16
    or edi, edx                 ; 拼接第 3 个字节
    
    call parse_hex_byte
    movzx edx, al
    shl edx, 24
    or edi, edx                 ; 拼接第 4 个字节
    
    mov eax, edi                ; 将完美拼装的值放入 EAX 返回
    pop edi                     ; 恢复 EDI
    ret

parse_nibble_strict:
    ; in:  al = ascii hex char
    ; out: bl = nibble, CF=0 on success, CF=1 on invalid char
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

; 写寄存器解析函数
parse_hex_byte_strict:
    ; in:  esi -> two hex chars
    ; out: al = parsed byte, esi += 2, CF=0 success / CF=1 fail
    mov al, [esi]
    test al, al
    jz .bad
    call parse_nibble_strict
    jc .bad
    mov dl, bl
    shl dl, 4

    mov al, [esi+1]
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
    push edi                    ; 保护 EDI 寄存器
    
    call parse_hex_byte_strict
    jc .bad_pop
    movzx edi, al               ; 使用 EDI 作为累加器
    
    call parse_hex_byte_strict
    jc .bad_pop
    movzx edx, al
    shl edx, 8
    or edi, edx
    
    call parse_hex_byte_strict
    jc .bad_pop
    movzx edx, al
    shl edx, 16
    or edi, edx
    
    call parse_hex_byte_strict
    jc .bad_pop
    movzx edx, al
    shl edx, 24
    or edi, edx
    
    mov eax, edi                ; 将完美拼装的值放入 EAX 返回
    pop edi
    clc
    ret
.bad_pop:
    pop edi                     ; 如果出错，也要记得恢复堆栈
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
    je .efl
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
    mov [reg_shadow+0], eax
    jmp send_ok
.ecx:
    mov [reg_shadow+4], eax
    jmp send_ok
.edx:
    mov [reg_shadow+8], eax
    jmp send_ok
.ebx:
    mov [reg_shadow+12], eax
    jmp send_ok
.esp:
    mov [reg_shadow+16], eax
    jmp send_ok
.ebp:
    mov [reg_shadow+20], eax
    jmp send_ok
.esi:
    mov [reg_shadow+24], eax
    jmp send_ok
.edi:
    mov [reg_shadow+28], eax
    jmp send_ok
.eip:
    mov [reg_shadow+32], eax
    jmp send_ok
.efl:
    mov [reg_shadow+36], eax
    jmp send_ok
.cs:
    mov [reg_shadow+40], eax
    jmp send_ok
.ss:
    mov [reg_shadow+44], eax
    jmp send_ok
.ds:
    mov [reg_shadow+48], eax
    jmp send_ok
.es:
    mov [reg_shadow+52], eax
    jmp send_ok
.fs:
    mov [reg_shadow+56], eax
    jmp send_ok
.gs:
    mov [reg_shadow+60], eax
    jmp send_ok
.bad:
    jmp send_empty

write_all_regs32:
    lodsb
    cmp al, 'G'
    jne .bad
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+0], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+4], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+8], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+12], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+16], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+20], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+24], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+28], eax
    call parse_hex_dword_le_strict
    jc .bad
    mov [reg_shadow+32], eax
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
    mov al, [esi+1]
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
    cmp byte [esi], 'R'
    je .rcmd
    jmp .bad
.supported:
    ; qSupported: advertise software breakpoint capability.
    ; Keep response minimal for early-boot stub compatibility.
    cmp byte [esi+1], 'u'
    jne .bad
    cmp byte [esi+2], 'p'
    jne .bad
    cmp byte [esi+3], 'p'
    jne .bad
    cmp byte [esi+4], 'o'
    jne .bad
    cmp byte [esi+5], 'r'
    jne .bad
    cmp byte [esi+6], 't'
    jne .bad
    cmp byte [esi+7], 'e'
    jne .bad
    cmp byte [esi+8], 'd'
    jne .bad
    call rsp_begin
    mov bl, 's'
    call rsp_emit
    mov bl, 'w'
    call rsp_emit
    mov bl, 'b'
    call rsp_emit
    mov bl, 'r'
    call rsp_emit
    mov bl, 'e'
    call rsp_emit
    mov bl, 'a'
    call rsp_emit
    mov bl, 'k'
    call rsp_emit
    mov bl, '+'
    call rsp_emit
    jmp rsp_end
.rcmd:
    cmp byte [esi+1], 'c'
    jne .bad
    cmp byte [esi+2], 'm'
    jne .bad
    cmp byte [esi+3], 'd'
    jne .bad
    cmp byte [esi+4], ','
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
    jne .bad
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
    mov al, [bp_used+ecx]
    test al, al
    jz .free
    mov eax, [bp_addr+ecx*4]
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
    mov [bp_orig+ecx], al
    mov byte [edi], 0xCC
    mov [bp_addr+ecx*4], edi
    mov byte [bp_used+ecx], 1
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
    jne .bad
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
    mov al, [bp_used+ecx]
    test al, al
    jz .next
    mov eax, [bp_addr+ecx*4]
    cmp eax, edi
    je .clear
.next:
    inc ecx
    jmp .find
.clear:
    mov al, [bp_orig+ecx]
    mov [edi], al
    mov byte [bp_used+ecx], 0
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

set_temp_breakpoint:
    ; in: edi = address to patch with int3
    call clear_temp_breakpoint
    mov al, [edi]
    mov [tmp_bp_orig], al
    mov [tmp_bp_addr], edi
    mov byte [edi], 0xCC
    mov byte [tmp_bp_active], 1
    ret

handle_temp_break_hit:
    cmp byte [tmp_bp_active], 0
    je .done
    mov eax, [reg_shadow+32]
    mov edx, [tmp_bp_addr]
    cmp eax, edx
    je .clear_only
.check_after:
    lea ecx, [edx+1]
    cmp eax, ecx
    jne .done
    mov [reg_shadow+32], edx
.clear_only:
    call clear_temp_breakpoint
.done:
    ret

handle_continue_packet:
    ; supports c[addr]
    mov esi, rsp_buf
    add esi, 1
    mov al, [esi]
    test al, al
    jz .clear_tf
    mov dl, 0
    call parse_hex_u32
    mov [reg_shadow+32], eax
.clear_tf:
    and dword [reg_shadow+36], ~EFLAGS_TF
    call clear_temp_breakpoint
    ret

handle_step_over_packet:
    ; Early-boot safe step-over: update EIP in-place, do not resume target.
    and dword [reg_shadow+36], ~EFLAGS_TF
    mov eax, [reg_shadow+32]
    mov bl, [eax]
    cmp bl, 0xE8
    jne .fallback_step
    add eax, 5
    mov [reg_shadow+32], eax
    ret
.fallback_step:
    call compute_step_target
    mov [reg_shadow+32], eax
    ret

compute_step_target:
    ; in:  eax = current eip
    ; out: eax = best-effort next executed eip
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
    mov edx, [eax+1]
    lea eax, [eax+5]
    add eax, edx
    jmp .out
.jmp_short:
    movsx edx, byte [eax+1]
    lea eax, [eax+2]
    add eax, edx
    jmp .out
.op_0f:
    cmp byte [eax+1], 0x0B
    jne .len1
    add eax, 2
    jmp .out
.op_66:
    cmp byte [eax+1], 0xBA
    jne .len1
    add eax, 4
    jmp .out
.op_30:
    cmp byte [eax+1], 0xC0
    jne .len1
.len2:
    add eax, 2
    jmp .out
.len1:
    add eax, 1
.out:
    pop edx
    pop ecx
    pop ebx
    ret

handle_step_packet:
    ; supports s[addr], early-boot safe single-step by EIP advance.
    mov esi, rsp_buf
    add esi, 1
    mov al, [esi]
    test al, al
    jz .step_now
    mov dl, 0
    call parse_hex_u32
    mov [reg_shadow+32], eax
.step_now:
    and dword [reg_shadow+36], ~EFLAGS_TF
    mov eax, [reg_shadow+32]
    call compute_step_target
    mov [reg_shadow+32], eax
    ret

handle_v_packet:
    lodsb
    cmp al, 'v'
    jne .bad
    mov esi, rsp_buf
    add esi, 1
    cmp byte [esi], 'C'
    jne .bad
    cmp byte [esi+1], 'o'
    jne .bad
    cmp byte [esi+2], 'n'
    jne .bad
    cmp byte [esi+3], 't'
    jne .bad
    mov al, [esi+4]
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
    mov bl, 'n'
    call rsp_emit
    jmp rsp_end
.run:
    mov al, [esi+5]
    cmp al, 's'
    je .step
    cmp al, 'S'
    je .step
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
.cont:
    and dword [reg_shadow+36], ~EFLAGS_TF
    call clear_temp_breakpoint
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
    call rsp_recv_packet  ; 碌梅脫脙潞贸拢卢eax 脰赂脧貌脮禄脡脧碌脛禄潞鲁氓脟酶
    mov esi, eax
    mov al, [esi]
    cmp al, 'c'
    je .cmd_cont
    cmp al, 's'
    je .step
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
    cmp al, 'Z'
    je .brkset
    cmp al, 'z'
    je .brkclr
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
.brkset:
    call handle_set_sw_break
    jmp .loop
.brkclr:
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
.cmd_cont:
    call handle_continue_packet
.resume:
    call sync_shadow_to_frame
    popad
    popfd
    ret

; 脢鹿脫脙戮脰虏驴卤盲脕驴脤忙麓煤脠芦戮脰卤盲脕驴
section .bss
rsp_buf resb 256
rsp_checksum resb 1
alignb 4
reg_shadow resd 16
bp_used resb BP_MAX
alignb 4
bp_addr resd BP_MAX
bp_orig resb BP_MAX
tmp_bp_active resb 1
alignb 4
tmp_bp_addr resd 1
tmp_bp_orig resb 1

%else
section .text
greatstart:
    ; 脤酶脳陋碌陆脛脷潞脣脠毛驴脷碌茫
    jmp 0x18:0xc0100000
    ud2
%endif
