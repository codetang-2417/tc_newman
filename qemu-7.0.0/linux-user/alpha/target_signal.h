#ifndef ALPHA_TARGET_SIGNAL_H
#define ALPHA_TARGET_SIGNAL_H

#define TARGET_SIGHUP            1
#define TARGET_SIGINT            2
#define TARGET_SIGQUIT           3
#define TARGET_SIGILL            4
#define TARGET_SIGTRAP           5
#define TARGET_SIGABRT           6
#define TARGET_SIGSTKFLT         7 /* actually SIGEMT */
#define TARGET_SIGFPE            8
#define TARGET_SIGKILL           9
#define TARGET_SIGBUS           10
#define TARGET_SIGSEGV          11
#define TARGET_SIGSYS           12
#define TARGET_SIGPIPE          13
#define TARGET_SIGALRM          14
#define TARGET_SIGTERM          15
#define TARGET_SIGURG           16
#define TARGET_SIGSTOP          17
#define TARGET_SIGTSTP          18
#define TARGET_SIGCONT          19
#define TARGET_SIGCHLD          20
#define TARGET_SIGTTIN          21
#define TARGET_SIGTTOU          22
#define TARGET_SIGIO            23
#define TARGET_SIGXCPU          24
#define TARGET_SIGXFSZ          25
#define TARGET_SIGVTALRM        26
#define TARGET_SIGPROF          27
#define TARGET_SIGWINCH         28
#define TARGET_SIGPWR           29 /* actually SIGINFO */
#define TARGET_SIGUSR1          30
#define TARGET_SIGUSR2          31
#define TARGET_SIGRTMIN         32

#define TARGET_SIG_BLOCK         1
#define TARGET_SIG_UNBLOCK       2
#define TARGET_SIG_SETMASK       3

/* this struct defines a stack used during syscall handling */

typedef struct target_sigaltstack {
    abi_ulong ss_sp;
    abi_int ss_flags;
    abi_ulong ss_size;
} target_stack_t;


/*
 * sigaltstack controls
 */
#define TARGET_SS_ONSTACK	1
#define TARGET_SS_DISABLE	2

#define TARGET_SA_ONSTACK       0x00000001
#define TARGET_SA_RESTART       0x00000002
#define TARGET_SA_NOCLDSTOP     0x00000004
#define TARGET_SA_NODEFER       0x00000008
#define TARGET_SA_RESETHAND     0x00000010
#define TARGET_SA_NOCLDWAIT     0x00000020 /* not supported yet */
#define TARGET_SA_SIGINFO       0x00000040

#define TARGET_MINSIGSTKSZ	4096

/* From <asm/gentrap.h>.  */
#define TARGET_GEN_INTOVF      -1      /* integer overflow */
#define TARGET_GEN_INTDIV      -2      /* integer division by zero */
#define TARGET_GEN_FLTOVF      -3      /* fp overflow */
#define TARGET_GEN_FLTDIV      -4      /* fp division by zero */
#define TARGET_GEN_FLTUND      -5      /* fp underflow */
#define TARGET_GEN_FLTINV      -6      /* invalid fp operand */
#define TARGET_GEN_FLTINE      -7      /* inexact fp operand */
#define TARGET_GEN_DECOVF      -8      /* decimal overflow (for COBOL??) */
#define TARGET_GEN_DECDIV      -9      /* decimal division by zero */
#define TARGET_GEN_DECINV      -10     /* invalid decimal operand */
#define TARGET_GEN_ROPRAND     -11     /* reserved operand */
#define TARGET_GEN_ASSERTERR   -12     /* assertion error */
#define TARGET_GEN_NULPTRERR   -13     /* null pointer error */
#define TARGET_GEN_STKOVF      -14     /* stack overflow */
#define TARGET_GEN_STRLENERR   -15     /* string length error */
#define TARGET_GEN_SUBSTRERR   -16     /* substring error */
#define TARGET_GEN_RANGERR     -17     /* range error */
#define TARGET_GEN_SUBRNG      -18
#define TARGET_GEN_SUBRNG1     -19
#define TARGET_GEN_SUBRNG2     -20
#define TARGET_GEN_SUBRNG3     -21
#define TARGET_GEN_SUBRNG4     -22
#define TARGET_GEN_SUBRNG5     -23
#define TARGET_GEN_SUBRNG6     -24
#define TARGET_GEN_SUBRNG7     -25

#define TARGET_ARCH_HAS_SETUP_FRAME
#define TARGET_ARCH_HAS_KA_RESTORER
#define TARGET_ARCH_HAS_SIGTRAMP_PAGE 1

/* bit-flags */
#define TARGET_SS_AUTODISARM (1U << 31) /* disable sas during sighandling */
/* mask for all SS_xxx flags */
#define TARGET_SS_FLAG_BITS  TARGET_SS_AUTODISARM

#endif /* ALPHA_TARGET_SIGNAL_H */
