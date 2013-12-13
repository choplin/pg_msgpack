#ifndef __PG_MSGPACK_OP_H__
#define __PG_MSGPACK_OP_H__

#include "fmgr.h"

Datum msgpack_object_field(PG_FUNCTION_ARGS);
Datum msgpack_array_element(PG_FUNCTION_ARGS);

#endif /* __PG_MSGPACK_OP_H__ */
