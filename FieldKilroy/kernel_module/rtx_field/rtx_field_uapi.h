/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_RTX_FIELD_H
#define _UAPI_RTX_FIELD_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define RTX_FIELD_MAGIC 0x52545846u /* 'RTXF' */
#define RTX_FIELD_ABI   "kilroy-7.1.1-field-2026.3"
#define RTX_FIELD_PHI_MILLI 618

/* Mirrors AMOURANTHRTX ThermoAccountant + analog FCC (data_bus[16-28]). */
struct rtx_field_thermo {
	__u32 entropy_milli;
	__u32 boundary_thermo_milli;
	__u32 prev_maint_milli;
	__u32 free_energy_milli;
	__u32 steps;
	__u32 host_heat_milli;
	__u32 phi_milli;
	__u32 entropy_floor_milli;
};

struct rtx_field_fcc {
	__u32 time_scale_milli;
	__u32 thermo_alpha_milli;
	__u32 wave_speed_milli;
	__u32 gate_fidelity_milli;
	__u32 entropy_floor_milli;
	__u32 inject_strength_milli;
	__u32 propalactic_milli;
	__u32 field_coupling_milli;
	__u32 tesla_bias_milli;
};

struct rtx_field_state {
	__u32 magic;
	__u32 active;
	__u32 pid;
	__u64 syscall_count;
	__u64 fabric_scheduled;
	__u64 host_passthrough;
	struct rtx_field_thermo thermo;
	struct rtx_field_fcc fcc;
	char abi[32];
};

#define RTX_IOC_MAGIC 'R'
#define RTX_IOC_ACTIVATE    _IO(RTX_IOC_MAGIC, 1)
#define RTX_IOC_DEACTIVATE  _IO(RTX_IOC_MAGIC, 2)
#define RTX_IOC_GET_ABI     _IOR(RTX_IOC_MAGIC, 3, char[32])
#define RTX_IOC_GET_STATE   _IOR(RTX_IOC_MAGIC, 4, struct rtx_field_state)
#define RTX_IOC_SET_THERMO  _IOW(RTX_IOC_MAGIC, 5, struct rtx_field_thermo)
#define RTX_IOC_SET_FCC     _IOW(RTX_IOC_MAGIC, 6, struct rtx_field_fcc)

#endif /* _UAPI_RTX_FIELD_H */