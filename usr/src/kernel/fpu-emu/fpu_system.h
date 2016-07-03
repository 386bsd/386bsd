/*
 *  fpu_system.h
 *
 *
 * Copyright (C) 1992, 1993  W. Metzenthen, 22 Parker St, Ormond,
 *                           Vic 3163, Australia.
 *                           E-mail apm233m@vaxc.cc.monash.edu.au
 * All rights reserved.
 *
 * This copyright notice covers the redistribution and use of the
 * FPU emulator developed by W. Metzenthen. It covers only its use
 * in the 386BSD operating system. Any other use is not permitted
 * under this copyright.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must include information specifying
 *    that source code for the emulator is freely available and include
 *    either:
 *      a) an offer to provide the source code for a nominal distribution
 *         fee, or
 *      b) list at least two alternative methods whereby the source
 *         can be obtained, e.g. a publically accessible bulletin board
 *         and an anonymous ftp site from which the software can be
 *         downloaded.
 * 3. All advertising materials specifically mentioning features or use of
 *    this emulator must acknowledge that it was developed by W. Metzenthen.
 * 4. The name of W. Metzenthen may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * W. METZENTHEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef _FPU_SYSTEM_H
#define _FPU_SYSTEM_H

/* system dependent definitions */

#ifdef LINUX
#include <linux/sched.h>
#include <linux/kernel.h>

#define I387			(current->tss.i387)
#define FPU_info		(I387.soft.info)

#define FPU_CS			(*(unsigned short *) &(FPU_info->___cs))
#define FPU_DS			(*(unsigned short *) &(FPU_info->___ds))
#define FPU_EAX			(FPU_info->___eax)
#define FPU_EFLAGS		(FPU_info->___eflags)
#define FPU_EIP			(FPU_info->___eip)
#define FPU_ORIG_EIP		(FPU_info->___orig_eip)

#define FPU_lookahead           (I387.soft.lookahead)
#define FPU_entry_eip           (I387.soft.entry_eip)

#define status_word		(I387.soft.swd)
#define control_word		(I387.soft.cwd)
#define regs			(I387.soft.regs)
#define top			(I387.soft.top)

#define ip_offset		(I387.soft.fip)
#define cs_selector		(I387.soft.fcs)
#define data_operand_offset	(I387.soft.foo)
#define operand_selector	(I387.soft.fos)
#endif

#ifdef __386BSD__
/* #include "sys/param.h"
#include "sys/file.h"
#include "sys/time.h"
#include "sys/user.h" */
/*#include "../include/ucred.h"
#include "../include/proc.h"
#include "../include/uio.h"*/
/*#include "machine/npx.h"
#include "machine/pcb.h"*/
#include "machine/reg.h"
/*#include "machine/frame.h"*/

#define printk	printf
/*
struct fpu_reg {
	char sign;
	char tag;
	long exp;
	unsigned sigl;
	unsigned sigh;
};
*/

#define I387			(curproc->p_addr->u_pcb.pcb_savefpu)
#define FPU_info		(curproc->p_md.md_regs)
#define USER_CS		0x1f

#define FPU_CS			(*(unsigned short *) &(curproc->p_md.md_regs[tCS]))
#define FPU_DS			(*(unsigned short *) &(curproc->p_md.md_regs[tDS]))
#define FPU_EAX			(curproc->p_md.md_regs[tEAX])
#define FPU_EFLAGS		(curproc->p_md.md_regs[tEFLAGS])
#define FPU_EIP			(curproc->p_md.md_regs[tEIP])
#define FPU_ORIG_EIP 		(curproc->p_addr->u_pcb.pcb_softfp_orig_eip)

#define FPU_entry_eip           (curproc->p_addr->u_pcb.pcb_softfp_entry_eip)

#define status_word		(I387.sv_env.en_sw)
#define control_word		(I387.sv_env.en_cw)
#define regs			((struct fpu_reg *)curproc->p_addr->u_pcb.pcb_softfp_regs)

#define top			(curproc->p_addr->u_pcb.pcb_softfp_top)

#define ip_offset		(I387.sv_env.en_fip)
#define cs_selector		(I387.sv_env.en_fcs)
#define data_operand_offset	(I387.sv_env.en_foo)
#define operand_selector	(I387.sv_env.en_fos)
#endif

#endif
