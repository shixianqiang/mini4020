/*
 *  linux/include/asm-arm/arch-clps711x/system.h
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <asm/io.h>
#include <asm/arch/hardware.h>
//#include <asm/hardware/clps7111.h>

static void arch_idle(void)
{
//	clps_writel(1, HALT);
	__asm__ __volatile__(
	"mov	r0, r0\n\
	mov	r0, r0");
}

static inline void arch_reset(char mode)
{
	if (mode == 's') {
		cpu_reset(0);
	}

	printk("arch_reset: attempting watchdog reset\n");

	*(volatile unsigned long*)RTC_WD_CNT_V = 0xee;
	*(volatile unsigned long*)RTC_CTR_V	|= (1<<24) | (1<<1);
	*(volatile unsigned long*)RTC_INT_EN_V |= (1<<5);	


		/* wait for reset to assert... */
	mdelay(5000);

	printk(KERN_ERR "Watchdog reset failed to assert reset\n");

	/* we'll take a jump through zero as a poor second */
	cpu_reset(0);
}

#endif
