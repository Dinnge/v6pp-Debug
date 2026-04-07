%ifidn __OUTPUT_FORMAT__,bin
org 0x7c00
%endif

%ifdef GDB_DEBUG
GDB_PORT           equ 0x3F8
GDB_BAUD_LATCH     equ 0x3F9
GDB_LINE_CTRL      equ 0x3FB
GDB_MODEM_CTRL     equ 0x3FC
GDB_LINE_STATUS    equ 0x3FD

%macro BOOT_DEBUG_HOOK 0
    call gdb_debug_break
%endmacro
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

    ; --- 进入保护模式 ---
    lgdt [gdtr]
    in al, 92h
    or al, 02h
    out 92h, al     ; 启用 A20
    
    mov eax, cr0
    or al, 01h
    mov cr0, eax    ; 启用 PE
    
    mov eax, cr4
    or al, 10h
    mov cr4, eax    ; 启用 PSE
    
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

    ; --- 加载内核 ---
    mov ecx, 398    ; KERNEL_SIZE
    mov eax, 1      ; Start LBA
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

; --- 加载磁盘扇区 ---
_load_sector:
    push ebp
    mov ebp, esp
    pushad
    mov dx, 0x1f2
    mov al, 1
    out dx, al      ; 设置扇区数
    mov eax, [ebp+12]
    inc dx
    out dx, al      ; LBA Low
    inc dx
    mov al, ah
    out dx, al      ; LBA Mid
    shr eax, 16
    inc dx
    out dx, al      ; LBA High
    mov al, ah
    and al, 0x0f
    or al, 0xe0
    inc dx
    out dx, al      ; Drive head
    mov al, 0x20
    inc dx
    out dx, al      ; Command: Read
.test:
    in al, dx
    test al, 0x80
    jnz .test
    mov ecx, 128
    mov dx, 0x1f0
    mov edi, [ebp+8]
    rep insd
    popad
    leave
    retn 8

; --- GDT 全局描述符表 ---
gdt:
    dq 0
    dq 0x00cf9a000000ffff  ; low-base code
    dq 0x00cf92000000ffff  ; low-base data
    dq 0x40cf9a000000ffff  ; high-base code
    dq 0x40cf92000000ffff  ; high-base data
gdtr:
    dw $-gdt-1
    dd gdt

; --- GDB Stub 远程调试桩代码 ---
%ifdef GDB_DEBUG
gdb_real_init:
    mov dx, GDB_LINE_CTRL
    mov al, 0x80
    out dx, al      ; DLAB = 1
    mov dx, GDB_PORT
    mov al, 0x01    ; 115200 baud
    out dx, al
    mov dx, GDB_BAUD_LATCH
    xor al, al
    out dx, al
    mov dx, GDB_LINE_CTRL
    mov al, 0x03    ; 8N1
    out dx, al
    inc dx
    mov al, 0x0b
    out dx, al
    ret

gdb_send_byte:
    mov dx, GDB_LINE_STATUS
.l: in al, dx
    test al, 0x20
    jz .l
    mov dx, GDB_PORT
    mov al, bl
    out dx, al
    ret

gdb_recv_byte:
    mov dx, GDB_LINE_STATUS
.w: in al, dx
    test al, 0x01
    jz .w
    mov dx, GDB_PORT
    in al, dx
    mov bl, al
    ret

; 接收完整的GDB数据包
gdb_recv_command:
.ws:call gdb_recv_byte
    cmp bl, '$'
    jne .ws
    call gdb_recv_byte
    mov bh, bl
.sk:call gdb_recv_byte
    cmp bl, '#'
    jne .sk
    call gdb_recv_byte
    mov bl, '+'
    call gdb_send_byte
    mov bl, bh
    ret

gdb_debug_break:
    pushad              ; 保存所有32位寄存器
    mov esi, gdb_pkt_s05
    call .sz
.cl:call gdb_recv_command
    cmp bl, 'c'
    je .ex
    cmp bl, 's'
    je .ex
    cmp bl, 'g'
    je .sg
    mov esi, gdb_pkt_empty
    cmp bl, '?'
    jne .sn
    mov esi, gdb_pkt_s05
.sn:call .sz
    jmp .cl
.sg:call .rg
    jmp .cl
.ex:popad
    ret
.rg:mov bl, '$'
    call gdb_send_byte
    mov ecx, 72
.rz:mov bl, '0'
    call gdb_send_byte
    loop .rz
    mov bl, '#'
    call gdb_send_byte
    mov bl, '8'
    call gdb_send_byte
    mov bl, '0'
    call gdb_send_byte
    ret
.sz:lodsb               ; 从内存中加载字节并发送
    test al, al
    jz .sd
    mov bl, al
    call gdb_send_byte
    jmp .sz
.sd:ret

gdb_pkt_empty db '$','#','0','0',0
gdb_pkt_s05   db '$','S','0','5','#','b','8',0
%endif

%ifidn __OUTPUT_FORMAT__,bin
times 510 - ($ - $$) db 0
dw 0xAA55
%endif
