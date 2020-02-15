/* SPDX-License-Identifier: GPL-2.0 */
/* This header needs to be included before a stage starts */
#undef __entry
#undef __field
#undef __field_ext
#undef __field_struct
#undef __field_struct_ext
#undef __array
#undef __dynamic_array
#undef __bitmask
#undef __string

#undef TRACE_DEFINE_ENUM
#undef TRACE_DEFINE_SIZEOF
#undef DEFINE_EVENT
#undef DEFINE_EVENT_FN
#undef DEFINE_EVENT_PRINT
#undef DECLARE_EVENT_CLASS
#undef TRACE_EVENT_FN
#undef TRACE_EVENT_FN_COND
#undef TRACE_EVENT_FLAGS
#undef TRACE_EVENT_PERF_PERM

#undef TP_printk
#undef TP_STRUCT__entry
#undef TP_fast_assign
#define TP_STRUCT__entry(args...) args

#undef __get_dynamic_array
#undef __get_dynamic_array_len
#undef __get_str
#undef __get_bitmask
#define __get_dynamic_array(field)	\
		((void *)__entry + (__entry->__data_loc_##field & 0xffff))

#define __get_dynamic_array_len(field)	\
		((__entry->__data_loc_##field >> 16) & 0xffff)

#define __get_str(field) ((char *)__get_dynamic_array(field))

#undef __print_flags
#undef __print_symbolic
#undef __print_flags_u64
#undef __print_symbolic_u64
#undef __print_hex
#undef __print_hex_str
#undef __print_array
#undef __print_hex_dump
