#include <msgpack.h>

#include "postgres.h"
#include "utils/builtins.h"

#include "pg_msgpack_op.h"
#include "convert_from_msgpack.h"

PG_FUNCTION_INFO_V1(msgpack_object_field);
PG_FUNCTION_INFO_V1(msgpack_array_element);

Datum
msgpack_object_field(PG_FUNCTION_ARGS)
{
	bytea			*data = PG_GETARG_BYTEA_P(0);
	bytea			*result = NULL;
	text			*fname = PG_GETARG_TEXT_P(1);
	const char		*fnamestr = text_to_cstring(fname);

	bool				success;
	msgpack_unpacked	msg;
	msgpack_object_kv	*p;
	msgpack_object_kv	*pend;

	msgpack_unpacked_init(&msg);
	success = msgpack_unpack_next(&msg, VARDATA(data), VARSIZE(data) - VARHDRSZ, NULL);

	if (!success)
		PG_RETURN_NULL();

	if (msg.data.type != MSGPACK_OBJECT_MAP || msg.data.via.map.size == 0)
		PG_RETURN_NULL();

	p = msg.data.via.map.ptr;
	pend = p + msg.data.via.map.size;

	for (; p < pend; ++p) {
		if (p->key.type != MSGPACK_OBJECT_RAW)
			continue;

		if (p->key.via.raw.size == strlen(fnamestr) &&
				memcmp(p->key.via.raw.ptr, fnamestr, p->key.via.raw.size) == 0) {
			result = msgpack_to_bytea(p->val);
			break;
		}
	}

	msgpack_unpacked_destroy(&msg);

	if (result != NULL)
		PG_RETURN_BYTEA_P(result);
	else
		PG_RETURN_NULL();
}


Datum
msgpack_array_element(PG_FUNCTION_ARGS)
{
	bytea	   	*data = PG_GETARG_BYTEA_P(0);
	bytea	   	*result;
	int			element = PG_GETARG_INT32(1);

	bool 				success;
	msgpack_unpacked 	msg;
	msgpack_object 		*target;

	msgpack_unpacked_init(&msg);
	success = msgpack_unpack_next(&msg, VARDATA(data), VARSIZE(data) - VARHDRSZ, NULL);

	if (!success)
		PG_RETURN_NULL();

	if (msg.data.type != MSGPACK_OBJECT_ARRAY ||
			msg.data.via.array.size == 0 ||
			element >= msg.data.via.array.size)
		PG_RETURN_NULL();

	target = msg.data.via.array.ptr + element;
	result = msgpack_to_bytea(*target);

	msgpack_unpacked_destroy(&msg);

	if (result != NULL)
		PG_RETURN_BYTEA_P(result);
	else
		PG_RETURN_NULL();
}
