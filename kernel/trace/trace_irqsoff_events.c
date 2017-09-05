/*
 * trace irqs off common code.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2008 Ingo Molnar <mingo@redhat.com>
 *
 * From code in the latency_tracer, that is:
 *
 *  Copyright (C) 2004-2006 Ingo Molnar
 *  Copyright (C) 2004 Nadia Yvette Chambers
 */
#include "trace.h"

void start_critical_timings(void)
{
	start_critical_timings_tracer();
}
EXPORT_SYMBOL_GPL(start_critical_timings);

void stop_critical_timings(void)
{
	stop_critical_timings_tracer();
}
EXPORT_SYMBOL_GPL(stop_critical_timings);

void trace_hardirqs_on(void)
{
	trace_hardirqs_on_tracer();
}
EXPORT_SYMBOL(trace_hardirqs_on);

void trace_hardirqs_off(void)
{
	trace_hardirqs_off_tracer();
}
EXPORT_SYMBOL(trace_hardirqs_off);

__visible void trace_hardirqs_on_caller(unsigned long caller_addr)
{
	trace_hardirqs_on_caller_tracer(caller_addr);
}
EXPORT_SYMBOL(trace_hardirqs_on_caller);

__visible void trace_hardirqs_off_caller(unsigned long caller_addr)
{
	trace_hardirqs_off_caller_tracer(caller_addr);
}
EXPORT_SYMBOL(trace_hardirqs_off_caller);

/*
 * This define will be changed in another patch in this series
 */
#if defined(CONFIG_DEBUG_PREEMPT) || defined(CONFIG_PREEMPT_TRACER)
void trace_preempt_on(unsigned long a0, unsigned long a1)
{
	trace_preempt_on_tracer(a0, a1);
}

void trace_preempt_off(unsigned long a0, unsigned long a1)
{
	trace_preempt_off_tracer(a0, a1);
}
#endif
