; user.asm  -- simple ET_DYN program that writes to VGA memory and returns
; assemble: nasm -f elf64 user.asm -o user.o
; link: ld -shared -o user.elf user.o

global _start
section .text
_start:
    ; VGA base: 0xB8000 (text-mode), write string at row 10 column 0
    mov rdi, 0xB8000
    mov rcx, 0          ; index
    mov rsi, msg
.write_loop:
    mov al, [rsi + rcx]
    cmp al, 0
    je .done
    ; each VGA cell is 2 bytes: char + attr
    mov rdx, rcx
    shl rdx, 1
    mov word [rdi + rdx], 0x0700        ; write attribute 0x07 initially (we'll overwrite low byte)
    mov byte [rdi + rdx], al
    inc rcx
    jmp .write_loop
.done:
    ret

section .data
msg: db "Hello from user!", 0
