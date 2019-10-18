/* policyproc_inst.h -- preprocess/postprocess policy as binary image
 *
 * Copyright 2017-2018 ZTE Corp.
 * All Rights Reserved.
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
 *
 * Written by Jia Jia <jia.jia@zte.com.cn>
 */
#ifndef _SS_POLICYPROC_INST_H_
#define _SS_POLICYPROC_INST_H_

/*
 * Macro Definition
 */
#define PP_DATUMS_NUM 8  /* datums number */


/*
 * Type Definition
 */
/*
 * Policy Description Definition
 * NOTICE: Update system/sepolicy/tools/sepolicy-parser/sepolicy-appendix.json required
 */
typedef enum {
	PERM_SU = 0,
} pp_perm_desc_item_t;

typedef enum {
	/* Policy Group 1 */
	AVC_ADBD_SELF = 0,
	AVC_ADBD_SU,
	/* Policy Group 2 */
	AVC_ADBD_KERNEL,
	AVC_ADBD_SELINUXFS,
	/* Policy Group 5 */
	AVC_SU_TEE_DEV_CHR_FILE,
	/* Policy Group 6 */
	AVC_TRACE_CAP_PTRACE,
	AVC_TRACE_SYSFILE_ENT,
	AVC_TRACE_DEBUG_FILE,
	/* Policy Group 7 */
	AVC_VOLD_KERNEL,
	AVC_VOLD_SELINUXFS,
} pp_avc_desc_item_t;

typedef struct {
	const char *name;
} pp_perm_desc_t;

typedef struct {
	const char *sname;
	const char *tname;
	const char *cname;
	uint16_t specified;
	const char *pname;
	uint16_t sdatums;
	uint16_t tdatums;
	uint16_t cdatums;
	uint32_t pdatums[PP_DATUMS_NUM];
} pp_avc_desc_t;


/*
 * Global Variable Definition
 */
/*
 * Policy Instruction Definition
 * NOTICE: Update system/sepolicy/tools/sepolicy-parser/sepolicy-appendix.json required
 */
static pp_perm_desc_t pp_perm_desc_list[] = {
	/* permissive policy: permissive su */
	[PERM_SU] = {
		.name = "su",
	},
};

static pp_avc_desc_t pp_avc_desc_list[] = {
	/*
	 * Policy Group 1
	 * Purpose: allow adbd to root device
	 */
	/* avc policy: allow adbd self:process setcurrent */
	[AVC_ADBD_SELF] = {
		.sname     = "adbd",
		.tname     = "adbd",
		.cname     = "process",
		.specified = AVTAB_ALLOWED,
		.pname     = "setcurrent",
	},

	/* avc policy: allow adbd su:process dyntransition */
	[AVC_ADBD_SU] = {
		.sname     = "adbd",
		.tname     = "su",
		.cname     = "process",
		.specified = AVTAB_ALLOWED,
		.pname     = "dyntransition",
	},

	/*
	 * Policy Group 2
	 * Purpose: allow adbd to setenforce
	 * Running: adb shell "echo 0 > /sys/fs/selinux/enforce"
	 */
	/* avc policy: allow adbd kernel:security setenforce */
	[AVC_ADBD_KERNEL] = {
		.sname     = "adbd",
		.tname     = "kernel",
		.cname     = "security",
		.specified = AVTAB_ALLOWED,
		.pname     = "setenforce",
	},

	/* avc policy: allow adbd selinuxfs:file write */
	[AVC_ADBD_SELINUXFS] = {
		.sname     = "adbd",
		.tname     = "selinuxfs",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "write",
	},
	/*
	 * Policy Group 5
	 * Purpose: allow su to access tee device
	 */
	/* avc policy: allow su tee_device:chr_file { ioctl open read write } */
	[AVC_SU_TEE_DEV_CHR_FILE] = {
		.sname     = "su",
		.tname     = "tee_device",
		.cname     = "chr_file",
		.specified = AVTAB_ALLOWED,
		.pname     = "ioctl open read write",
	},

	/*
	 * Policy Group 6
	 * Purpose: allow tracer to execute prop file
	 */
	/* avc policy: allow tracer self:capability sys_ptrace */
	[AVC_TRACE_CAP_PTRACE] = {
		.sname     = "tracer",
		.tname     = "tracer",
		.cname     = "capability",
		.specified = AVTAB_ALLOWED,
		.pname     = "sys_ptrace dac_override",
	},

	/* avc policy: allow tracer system_file:file entrypoint */
	[AVC_TRACE_SYSFILE_ENT] = {
		.sname     = "tracer",
		.tname     = "system_file",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "entrypoint",
	},

    /* avc policy: allow tracer debugfs_tracing:file { open read write };*/
	[AVC_TRACE_DEBUG_FILE] = {
		.sname     = "tracer",
		.tname     = "debugfs_tracing",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "open read write",
	},
	/*
	 * Policy Group 7
	 * Purpose: allow vold to setenforce
	 */
	/* avc policy: allow vold kernel:security setenforce */
	[AVC_VOLD_KERNEL] = {
		.sname     = "vold",
		.tname     = "kernel",
		.cname     = "security",
		.specified = AVTAB_ALLOWED,
		.pname     = "setenforce",
	},

	/* avc policy: allow vold selinuxfs:file write */
	[AVC_VOLD_SELINUXFS] = {
		.sname     = "vold",
		.tname     = "selinuxfs",
		.cname     = "file",
		.specified = AVTAB_ALLOWED,
		.pname     = "open write",
	},
};

#endif /* _SS_POLICYPROC_INST_H_ */
