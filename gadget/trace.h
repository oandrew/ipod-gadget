#undef TRACE_SYSTEM
#define TRACE_SYSTEM ipod

#if !defined(__IPOD_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __IPOD_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <asm/byteorder.h>


TRACE_EVENT(ipod_req_out_done,

	TP_PROTO(struct usb_request * req),
	TP_ARGS(req),

	TP_STRUCT__entry(
			__field(    			void *,    ptr    )
			__field(    unsigned int,    actual    )
			__field(    unsigned int,    length    )
			__field(    unsigned int,    stream_id    )
			__field(     				 int,    status    )
	),

	TP_fast_assign(
			__entry->ptr 		= req;
			__entry->actual = req->actual;
			__entry->length = req->length;
			__entry->stream_id = req->stream_id;
			__entry->status = req->status;
	),

	TP_printk("out_done: %08x %3d/%3d stream:%d status:%d ", __entry->ptr, __entry->actual, __entry->length, __entry->stream_id, __entry->status)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>

