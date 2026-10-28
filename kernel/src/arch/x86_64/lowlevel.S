.text

.global astral_cli
astral_cli:
    cli
    ret

.global astral_sti
astral_sti:
    sti
    ret

.global astral_hlt
astral_hlt:
    hlt
    ret

.global astral_pause
astral_pause:
    pause
    ret

.global astral_read_cr0
astral_read_cr0:
    mov %cr0, %rax
    ret

.global astral_read_cr2
astral_read_cr2:
    mov %cr2, %rax
    ret

.global astral_read_cr3
astral_read_cr3:
    mov %cr3, %rax
    ret

.global astral_invalidate_page
astral_invalidate_page:
    invlpg (%rdi)
    ret

.global astral_in8
astral_in8:
    mov %di, %dx
    xor %eax, %eax
    in %dx, %al
    ret

.global astral_out8
astral_out8:
    mov %di, %dx
    mov %sil, %al
    out %al, %dx
    ret

.section .note.GNU-stack,"",@progbits