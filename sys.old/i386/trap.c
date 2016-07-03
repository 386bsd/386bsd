/*
 * 386 Trap and System call handleing
 */

#include "../i386/psl.h"
#include "../i386/reg.h"
#include "../i386/pte.h"
#include "../i386/segments.h"
#include "../i386/frame.h"

#include "param.h"
#include "systm.h"
#include "user.h"
#include "proc.h"
#include "seg.h"
#include "acct.h"
#include "kernel.h"
#include "vm.h"
#include "cmap.h"
#include "syslog.h"

#include "../i386/trap.h"

#define	USER	0x40		/* user-mode flag added to type */
#define	FRMTRAP	0x100		/* distinguish trap from syscall */

struct	sysent sysent[];
int	nsysent;
#include "dbg.h"
/*
 * Called from the trap handler when a processor trap occurs.
 */
unsigned rcr2(), Sysbase;
/*ARGSUSED*/
trap(frame)
	struct trapframe frame;
#define type frame.tf_trapno
#define code frame.tf_err
#define pc frame.tf_eip
{
	register int i;
	register struct proc *p;
	struct timeval syst;
	extern int nofault;
	int ucode;

p=u.u_procp;
#define DEBUG
#ifdef DEBUG
dprintf(DALLTRAPS, "\n%d. trap",u.u_procp->p_pid);
dprintf(DALLTRAPS, " pc:%x cs:%x ds:%x eflags:%x isp %x\n",
		frame.tf_eip, frame.tf_cs, frame.tf_ds, frame.tf_eflags,
		frame.tf_isp);
dprintf(DALLTRAPS, "edi %x esi %x ebp %x ebx %x esp %x\n",
		frame.tf_edi, frame.tf_esi, frame.tf_ebp,
		frame.tf_ebx, frame.tf_esp);
dprintf(DALLTRAPS, "edx %x ecx %x eax %x\n",
		frame.tf_edx, frame.tf_ecx, frame.tf_eax);
dprintf(DALLTRAPS, " ec %x type %x ",
		frame.tf_err&0xffff, frame.tf_trapno);
#endif

	frame.tf_eflags &= ~PSL_NT;	/* clear nested trap XXX */
/*if(nofault && type != T_PAGEFLT)
	{ frame.tf_eip = nofault; return;}*/

	syst = u.u_ru.ru_stime;
	if (ISPL(frame.tf_cs) == SEL_UPL /* && nofault == 0 */) {
		type |= USER;
		u.u_ar0 = &frame.tf_es;
	}
	ucode=0;
	switch (type) {

	default:
bit_sucker:
#ifdef KDB
		if (kdb_trap(&psl))
			return;
#endif

splhigh();
printf("cr2 %x ", rcr2());
		printf("trap type %d, code = %x, pc = %x cs = %x, eflags = %x\n", type, code, pc, frame.tf_cs, frame.tf_eflags);
		type &= ~USER;
pg("panic");
		panic("trap");
		/*NOTREACHED*/

	case T_SEGNPFLT + USER:
log(LOG_DEBUG,"segnp %x %d (%s)", code,p->p_pid, p->p_comm);
goto sh1;
	case T_PROTFLT + USER:		/* protection fault */
log(LOG_DEBUG,"protflt %x %d (%s)", code,p->p_pid, p->p_comm);
sh1:
		ucode = code + BUS_SEGM_FAULT ;
		i = SIGBUS;
		break;

	case T_PRIVINFLT + USER:	/* privileged instruction fault */
	case T_RESADFLT + USER:		/* reserved addressing fault */
	case T_RESOPFLT + USER:		/* reserved operand fault */
	case T_FPOPFLT + USER:		/* coprocessor operand fault */
		ucode = type &~ USER;
		i = SIGILL;
		break;

	case T_ASTFLT + USER:		/* Allow process switch */
	case T_ASTFLT:
		astoff();
		if ((u.u_procp->p_flag & SOWEUPC) && u.u_prof.pr_scale) {
			addupc(pc, &u.u_prof, 1);
			u.u_procp->p_flag &= ~SOWEUPC;
		}
		goto out;

	case T_DNA + USER:
#ifdef	NPX
		if (npxdna()) return;
#endif
		ucode = FPE_FPU_NP_TRAP;
		i = SIGFPE;
		break;

	case T_BOUND + USER:
		ucode = FPE_SUBRNG_TRAP;
		i = SIGFPE;
		break;

	case T_OFLOW + USER:
		ucode = FPE_INTOVF_TRAP;
		i = SIGFPE;
		break;

	case T_DIVIDE + USER:
		ucode = FPE_INTDIV_TRAP;
		i = SIGFPE;
		break;

	case T_ARITHTRAP + USER:
		ucode = code;
		i = SIGFPE;
		break;

#ifdef notdef
	/*
	 * If the user SP is above the stack segment,
	 * grow the stack automatically.
	 */
	case T_STKFLT + USER:
	case T_SEGFLT + USER:
		if (grow((unsigned)locr0[tESP]) /*|| grow(code)*/)
			goto out;
		ucode = code;
		i = SIGSEGV;
		break;

	case T_TABLEFLT:		/* allow page table faults in kernel */
	case T_TABLEFLT + USER:		/* page table fault */
		panic("ptable fault");
#endif

	case T_PAGEFLT:			/* allow page faults in kernel mode */
		if (code & PGEX_P) goto bit_sucker;
		/* fall into */
	case T_PAGEFLT + USER:		/* page fault */
		{	register u_int vp;
			u_int ea;

#ifdef DEBUG
dprintf(DPAGIN|DALLTRAPS, "pf code %x pc %x usp %x cr2 %x nof %x |",
		code, frame.tf_eip, frame.tf_esp, rcr2(), nofault);
#endif
			ea = (u_int)rcr2();

			/* out of bounds reference */
			if (/*ea >= (u_int)&Sysbase ||*/
				(code & (PGEX_P|PGEX_U)) == (PGEX_P|PGEX_U)) {
log(LOG_DEBUG,"oob %x %d (%s) ref %x pc %x:%x nof %x\n",
	code, p->p_pid, p->p_comm, ea, frame.tf_cs, frame.tf_eip, nofault);
				ucode = code + BUS_PAGE_FAULT;
				i = SIGBUS;
				break;
			}
			if ((code & (PGEX_P|PGEX_U)) == PGEX_P) {
				frame.tf_eip = nofault;
				return;
			}

			/* stack reference to the running process? */
			vp = btop(ea);
			if (vp >= dptov(u.u_procp, u.u_procp->p_dsize)
			&& vp < sptov(u.u_procp, u.u_procp->p_ssize-1)){
if (type != T_PAGEFLT && nofault) {
					frame.tf_eip = nofault;
					return;
}
				/* attempt to grow stack */
				if (grow((unsigned)u.u_ar0[tESP]) || grow(ea))
					goto out;
				/*else	if (nofault) {
					frame.tf_eip = nofault;
					return;
				}*/
if(p->p_comm[0] != 's' || p->p_comm[1] != 'h' || p->p_comm[2] != '\0')
log(LOG_DEBUG,"brkstk %x %d (%s)@%x ", code, p->p_pid, p->p_comm, ea);
				i = SIGSEGV;
				ucode = code + BUS_PAGE_FAULT;
				break;
			}

			pagein(ea, 0, code, frame.tf_eip, frame.tf_cs);
			if (type == T_PAGEFLT) return;
			goto out;
		}

	case T_TRCTRAP:	 /* trace trap -- someone single stepping lcall's */
		frame.tf_eflags &= ~PSL_T;
			/* Q: how do we turn it on again? */
		return;
	
	case T_BPTFLT + USER:		/* bpt instruction fault */
	case T_TRCTRAP + USER:		/* trace trap */
		frame.tf_eflags &= ~PSL_T;
		i = SIGTRAP;
		break;

#ifdef notdef
	/*
	 * For T_KSPNOTVAL and T_BUSERR, can not allow spl to
	 * drop to 0 as clock could go off and we would end up
	 * doing an rei to the interrupt stack at ipl 0 (a
	 * reserved operand fault).  Instead, we allow psignal
	 * to post an ast, then return to user mode where we
	 * will reenter the kernel on the kernel's stack and
	 * can then service the signal.
	 */
	case T_KSPNOTVAL:
		if (noproc)
			panic("ksp not valid");
		/* fall thru... */
	case T_KSPNOTVAL + USER:
		printf("pid %d: ksp not valid\n", u.u_procp->p_pid);
		/* must insure valid kernel stack pointer? */
		trapsignal(SIGKILL,0|FRMTRAP);
		return;

	case T_BUSERR + USER:
		trapsignal(SIGBUS, code|FRMTRAP);
		return;
#endif

#include "isa.h"
#if	NISA > 0
	case T_NMI:
	case T_NMI + USER:
		if(isa_nmi(code) == 0) return;
		else goto bit_sucker;
#endif
	}
	trapsignal(i, ucode|FRMTRAP);
	if ((type & USER) == 0)
		return;
out:
	p = u.u_procp;
	if (i = CURSIG(p))
		psig(i,FRMTRAP);
	p->p_pri = p->p_usrpri;
	if (runrun) {
		/*
		 * Since we are u.u_procp, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		(void) splclock();
		setrq(p);
		u.u_ru.ru_nivcsw++;
		swtch();
		if (i = CURSIG(p))
			psig(i,FRMTRAP);
		spl0();
	}
	if (u.u_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &u.u_ru.ru_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks)
			addupc(pc, &u.u_prof, ticks);
	}
	curpri = p->p_pri;
#undef type
#undef code
#undef pc
}

/*
 * Called from locore when a system call occurs
 */
/*ARGSUSED*/
syscall(frame)
	struct syscframe frame;
#define code frame.sf_eax	/* note: written over! */
#define pc frame.sf_eip
{
	register caddr_t params;
	register int i;
	register struct sysent *callp;
	register struct proc *p;
	struct timeval syst;
	int error, opc;
	int args[8], rval[2];

#ifdef lint
	r0 = 0; r0 = r0; r1 = 0; r1 = r1;
#endif
	syst = u.u_ru.ru_stime;
	if (ISPL(frame.sf_cs) != SEL_UPL)
{
printf("\npc:%x cs:%x eflags:%x\n",
		frame.sf_eip, frame.sf_cs, frame.sf_eflags);
printf("edi %x esi %x ebp %x ebx %x esp %x\n",
		frame.sf_edi, frame.sf_esi, frame.sf_ebp,
		frame.sf_ebx, frame.sf_esp);
printf("edx %x ecx %x eax %x\n", frame.sf_edx, frame.sf_ecx, frame.sf_eax);
printf("cr0 %x cr2 %x\n", rcr0(), rcr2());
		panic("syscall");
}
	u.u_ar0 = &frame;
	params = (caddr_t)frame.sf_esp + NBPW ;

	callp = ((unsigned)frame.sf_eax >= nsysent) ? sysent+63
		: sysent+frame.sf_eax;
	if (callp == sysent) {
		i = fuword(params);
		params += NBPW;
		callp = ((unsigned) i >= nsysent) ? sysent+63 : sysent+i;
	}
dprintf(DALLSYSC,"%d. call %d ", u.u_procp->p_pid, code);
	if ((i = callp->sy_narg * sizeof (int)) &&
	    (error = copyin(params, (caddr_t)args, (u_int)i))) {
		frame.sf_eax = (u_char) error;
		frame.sf_eflags |= PSL_C;	/* carry bit */
		goto done;
	}
	rval[0] = 0;
	rval[1] = frame.sf_edx;
	error = (*callp->sy_call)(u.u_procp, args, rval);
	/*
	 * Reconstruct pc, assuming lcall $X,y is 7 bytes, as it is always.
	 */
	if (error == ERESTART)
		pc -= 7;
	else if (error != EJUSTRETURN) {
		if (error) {
			frame.sf_eax = (u_char) error;
			frame.sf_eflags |= PSL_C;	/* carry bit */
			/* u.u_ar0[sEAX] = (u_char) error;
			u.u_ar0[sEFLAGS] |= PSL_C;	/* carry bit */
		} else {
			frame.sf_eax = rval[0];
			frame.sf_edx = rval[1];
			frame.sf_eflags &= ~PSL_C;	/* carry bit */
			/*u.u_ar0[sEAX] = rval[0];
			u.u_ar0[sEDX] = rval[1];
			u.u_ar0[sEFLAGS] &= ~PSL_C;	/* carry bit */
		}
	}
	/* else if (error == EJUSTRETURN) */
		/* nothing to do */
done:
	/*
	 * Reinitialize proc pointer `p' as it may be different
	 * if this is a child returning from fork syscall.
	 */
	p = u.u_procp;
	/*
	 * XXX the check for sigreturn ensures that we don't
	 * attempt to set up a call to a signal handler (sendsig) before
	 * we have cleaned up the stack from the last call (sigreturn).
	 * Allowing this seems to lock up the machine in certain scenarios.
	 * What should really be done is to clean up the signal handling
	 * so that this is not a problem.
	 */
#include "syscall.h"
	if (i = CURSIG(p))
		psig(i,0);
	p->p_pri = p->p_usrpri;
	if (runrun) {
		/*
		 * Since we are u.u_procp, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		(void) splclock();
		setrq(p);
		u.u_ru.ru_nivcsw++;
		swtch();
		if (i = CURSIG(p))
			psig(i,0);
		spl0();
	}
	if (u.u_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &u.u_ru.ru_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(pc, &u.u_prof, ticks * profscale);
#else
			addupc(pc, &u.u_prof, ticks);
#endif
		}
	}
	curpri = p->p_pri;
#ifdef KTRACEx
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

#ifdef notdef
/*
 * nonexistent system call-- signal process (may want to handle it)
 * flag error if process won't see signal immediately
 * Q: should we do that all the time ??
 */
nosys()
{

	if (u.u_signal[SIGSYS] == SIG_IGN || u.u_signal[SIGSYS] == SIG_HOLD)
		u.u_error = EINVAL;
	psignal(u.u_procp, SIGSYS);
}
#endif

/*
 * Ignored system call
 */
nullsys()
{

}
