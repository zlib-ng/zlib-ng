#ifndef __ASSEMBLER_H__
#define __ASSEMBLER_H__

#ifndef __ALIGN
#define __ALIGN     .align 4,0x90
#define __ALIGN_STR ".align 4,0x90"
#endif

//#ifdef __ASSEMBLY__
#ifndef ASM_NL
#define ASM_NL       ;
#endif

#define ALIGN __ALIGN
#define ALIGN_STR __ALIGN_STR

#ifndef GLOBAL
#define GLOBAL(name) \
    .globl name ASM_NL \
    name:
#endif

#ifndef ENTRY
#define ENTRY(name) \
    .globl name ASM_NL \
    ALIGN ASM_NL \
    name:
#endif

#ifndef WEAK
#define WEAK(name)     \
    .weak name ASM_NL   \
    ALIGN ASM_NL \
    name:
#endif

#ifndef END
#define END(name) \
    .size name, .-name
#endif

/* If symbol 'name' is treated as a subroutine (gets called, and returns)
 * then please use ENDPROC to mark 'name' as STT_FUNC for the benefit of
 * static analysis tools such as stack depth analyzer.
 */
#ifndef ENDPROC
#define ENDPROC(name) \
    .type name, @function ASM_NL \
    END(name)
#endif

/*
 * Select code when configured for BE.
 */
#ifdef CONFIG_BIG_ENDIAN
#define CPU_BE(code...) code
#else
#define CPU_BE(code...)
#endif

/*
 * Select code when configured for LE.
 */
#ifdef CONFIG_BIG_ENDIAN
#define CPU_LE(code...)
#else
#define CPU_LE(code...) code
#endif

//#endif   /* __ASSEMBLY__ */

#endif  /* __ASM_ASSEMBLER_H */
