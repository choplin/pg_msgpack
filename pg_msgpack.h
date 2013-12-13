#ifndef __PG_MSGPACK_H__
#define __PG_MSGPACK_H__

#include "fmgr.h"

Datum msgpack_in(PG_FUNCTION_ARGS);
Datum msgpack_out(PG_FUNCTION_ARGS);
Datum msgpack_recv(PG_FUNCTION_ARGS);
Datum msgpack_send(PG_FUNCTION_ARGS);

#endif /* __PG_MSGPACK_H__ */
