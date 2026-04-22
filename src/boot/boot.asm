%ifidn __OUTPUT_FORMAT__,bin
org 0x7c00
%endif

%ifdef GDB_DEBUG
GDB_PORT        equ 0x3F8
GDB_BAUD_LATCH  equ 0x3F9
GDB_LINE_CTRL   equ 0x3FB
GDB_MODEM_CTRL  equ 0x3FC
GDB_LINE_STATUS equ 0x3FD

%macro BOOT_DEBUG_HOOK 0
    call gdb_debug_break
%endmacro
%endif

%ifdef USE_VESA
vesa_video_mode      equ 0x143
vesa_video_mode_code equ (vesa_video_mode | 0x4000)
%endif

global start

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7a00
    cld

%ifdef USE_VESA
    xor ax, ax
    mov es, ax
    mov di, 0x7e00
    mov ax, 0x4f01
    mov cx, vesa_video_mode
    int 0x10

    mov bx, vesa_video_mode_code
    mov ax, 0x4f02
    int 0x10
%endif

    lgdt [gdtr]

    in al, 92h
    or al, 02h
    out 92h, al

    mov eax, cr0
    or al, 01h
    mov cr0, eax

    mov eax, cr4
    or al, 10h
    mov cr4, eax

    jmp dword 0x8:_startup

[BITS 32]
_startup:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

%ifdef GDB_DEBUG
    call gdb_real_init
    BOOT_DEBUG_HOOK
%endif

    mov ecx, KERNEL_SIZE
    mov eax, 1
    mov ebx, 0x100000

_load_kernel:
    push eax
    push ebx
    add ebx, 512
    inc eax
    call _load_sector
    loop _load_kernel

    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    or esp, 0xc0000000
    jmp 0x18:0xc0100000

_load_sector:
    push ebp
    mov ebp, esp
    pushad

    mov dx, 0x1f2
    mov al, 1
    out dx, al

    mov eax, [ebp + 12]
    inc dx
    out dx, al

    inc dx
    mov al, ah
    out dx, al

    shr eax, 16
    inc dx
    out dx, al

    mov al, ah
    and al, 0x0f
    or al, 0xe0
    inc dx
    out dx, al

    mov al, 0x20
    inc dx
    out dx, al

.wait:
    in al, dx
    test al, 0x80
    jnz .wait

    mov ecx, 128
    mov dx, 0x1f0
    mov edi, [ebp + 8]
    rep insd

    popad
    leave
    retn 8

KERNEL_SIZE equ 398

gdt:
    dq 0
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
    dq 0x40cf9a000000ffff
    dq 0x40cf92000000ffff

gdtr:
    dw $ - gdt - 1
    dd gdt

%ifdef GDB_DEBUG
gdb_real_init:
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

    inc dx
    mov al, 0x0b
    out dx, al
    ret

gdb_send_byte:
    mov dx, GDB_LINE_STATUS
.tx:
    in al, dx
    test al, 0x20
    jz .tx
    mov dx, GDB_PORT
    mov al, bl
    out dx, al
    ret

gdb_recv_byte:
    mov dx, GDB_LINE_STATUS
.rx:
    in al, dx
    test al, 0x01
    jz .rx
    mov dx, GDB_PORT
    in al, dx
    mov bl, al
    ret

gdb_recv_command:
.wait_start:
    call gdb_recv_byte
    cmp bl, '$'
    jne .wait_start

    call gdb_recv_byte
    mov bh, bl

.skip_payload:
    call gdb_recv_byte
    cmp bl, '#'
    jne .skip_payload

    call gdb_recv_byte
    call gdb_recv_byte

    mov bl, '+'
    call gdb_send_byte
    mov bl, bh
    ret

gdb_debug_break:
    pushad
    mov esi, gdb_pkt_s05
    call .send_zstr

.command_loop:
    call gdb_recv_command
    cmp bl, 'c'
    je .resume
    cmp bl, 's'
    je .resume
    cmp bl, 'g'
    je .send_regs
    cmp bl, '?'
    je .send_sig

    mov esi, gdb_pkt_empty
    call .send_zstr
    jmp .command_loop

.send_sig:
    mov esi, gdb_pkt_s05
    call .send_zstr
    jmp .command_loop

.send_regs:
    mov bl, '$'
    call gdb_send_byte
    mov ecx, 128
.zero_regs:
    mov bl, '0'
    call gdb_send_byte
    loop .zero_regs
    mov bl, '#'
    call gdb_send_byte
    mov bl, '0'
    call gdb_send_byte
    mov bl, '0'
    call gdb_send_byte
    jmp .command_loop

.resume:
    popad
    ret

.send_zstr:
    lodsb
    test al, al
    jz .done
    mov bl, al
    call gdb_send_byte
    jmp .send_zstr
.done:
    ret

gdb_pkt_empty db '$', '#', '0', '0', 0
gdb_pkt_s05   db '$', 'S', '0', '5', '#', 'b', '8', 0
%endif

%ifidn __OUTPUT_FORMAT__,bin
times 510 - ($ - $$) db 0
dw 0xAA55
%endif
