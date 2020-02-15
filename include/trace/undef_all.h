#undef TRACE_EVENT
#undef TRACE_DEFINE_ENUM
#undef TRACE_DEFINE_SIZEOF
#undef __field
#undef __field_ext
#undef __field_struct
#undef __field_struct_ext
#undef __array
#undef __dynamic_array
#undef __bitmask
#undef __string
#undef TP_STRUCT__entry
#undef DEFINE_EVENT
#undef DEFINE_EVENT_FN
#undef DEFINE_EVENT_PRINT
#undef TRACE_EVENT_FN
#undef TRACE_EVENT_FN_COND
#undef TRACE_EVENT_FLAGS
#undef TRACE_EVENT_PERF_PERM
#undef DECLARE_EVENT_CLASS

#undef __entry
#undef TP_printk
#undef __get_dynamic_array
#undef __get_dynamic_array_len
#undef __get_str
#undef __get_bitmask
#undef __print_flags
#undef __print_symbolic
#undef __print_flags_u64
#undef __print_symbolic_u64
#undef __print_hex
#undef __print_hex_str
#undef __print_array
#undef __print_hex_dump
/*
 * DECLARE_EVENT_CLASS can be used to add a generic function
 * handlers for events. That is, if all events have the same
 * parameters and just have distinct trace points.
 * Each tracepoint can be defined with DEFINE_EVENT and that
 * will map the DECLARE_EVENT_CLASS to the tracepoint.
 *
 * TRACE_EVENT is a one to one mapping between tracepoint and template.
 */
#define TRACE_EVENT(name, proto, args, tstruct, assign, print) \
	DECLARE_EVENT_CLASS(name,			       \
			     PARAMS(proto),		       \
			     PARAMS(args),		       \
			     PARAMS(tstruct),		       \
			     PARAMS(assign),		       \
			     PARAMS(print));		       \
	DEFINE_EVENT(name, name, PARAMS(proto), PARAMS(args));

#define TP_STRUCT__entry(args...) args


#define __get_dynamic_array(field)	\
		((void *)__entry + (__entry->__data_loc_##field & 0xffff))

#define __get_dynamic_array_len(field)	\
		((__entry->__data_loc_##field >> 16) & 0xffff)

#define __get_str(field) ((char *)__get_dynamic_array(field))
