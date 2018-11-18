/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ikheaders_data contains the inkernel headers as a CPIO archive that is
 * suitable for building kernel modules or eBPF programs to extend the
 * functionality of the kernel.  Developed with ideas from initramfs code.
 *
 * Joel Fernandes <joel@joelfernandes.org>
 */

#include <linux/stringify.h>
#include <asm-generic/vmlinux.lds.h>

.section .ikheaders,"a"
__image_start:
.incbin __stringify(IKH_IMAGE)
__image_end:
.section .ikheaders.info,"a"
.globl __ikheaders_size
__ikheaders_size:
#ifdef CONFIG_64BIT
	.quad __image_end - __image_start
#else
	.long __image_end - __image_start
#endif
