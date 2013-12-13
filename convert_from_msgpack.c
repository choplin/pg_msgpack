#include <inttypes.h>

#include "postgres.h"
#include "lib/stringinfo.h"

#include "convert_from_msgpack.h"

/*
 * private functions for msgpack_to_json_string
 */
static void msgpack_object_to_string(msgpack_object o, StringInfo out);
static inline void raw_to_string(msgpack_object o, StringInfo out);
static void array_to_string(msgpack_object o, StringInfo out);
static void map_to_string(msgpack_object o, StringInfo out);


char *
msgpack_to_json_string(msgpack_object o)
{
	StringInfoData out;
	initStringInfo(&out);

	msgpack_object_to_string(o, &out);

	return out.data;
}

bytea *
msgpack_to_bytea(msgpack_object o)
{
	bytea			*out;
	msgpack_sbuffer	*sbuf;
	msgpack_packer	*pk;

	sbuf = msgpack_sbuffer_new();

	pk = msgpack_packer_new(sbuf, msgpack_sbuffer_write);
	msgpack_pack_object(pk, o);

	out = (bytea *) palloc(sbuf->size + VARHDRSZ);
	SET_VARSIZE(out, sbuf->size + VARHDRSZ);
	memcpy(VARDATA(out), sbuf->data, sbuf->size);

	msgpack_packer_free(pk);
	msgpack_sbuffer_free(sbuf);

	return out;
}

/*
 * private functions
 */
static inline void
msgpack_object_to_string(msgpack_object o, StringInfo out)
{
	switch(o.type) {
	case MSGPACK_OBJECT_NIL:
		appendStringInfoString(out, "null");
		break;

	case MSGPACK_OBJECT_BOOLEAN:
		appendStringInfoString(out, (o.via.boolean ? "true" : "false"));
		break;

	case MSGPACK_OBJECT_POSITIVE_INTEGER:
		appendStringInfo(out, "%"PRIu64, o.via.u64);
		break;

	case MSGPACK_OBJECT_NEGATIVE_INTEGER:
		appendStringInfo(out, "%"PRIi64, o.via.i64);
		break;

	case MSGPACK_OBJECT_DOUBLE:
		appendStringInfo(out, "%f", o.via.dec);
		break;

	case MSGPACK_OBJECT_RAW:
		raw_to_string(o, out);
		break;

	case MSGPACK_OBJECT_ARRAY:
		array_to_string(o, out);
		break;

	case MSGPACK_OBJECT_MAP:
		map_to_string(o, out);
		break;

	default:
		appendStringInfo(out, "#<UNKNOWN %i %"PRIu64">", o.type, o.via.u64);
	}
}

static inline void
raw_to_string(msgpack_object o, StringInfo out)
{
	char *buf;
	
	buf = palloc(o.via.raw.size + 1);
	memcpy(buf, o.via.raw.ptr, o.via.raw.size);
	buf[o.via.raw.size] = '\0';

	appendStringInfoChar(out, '"');
	appendStringInfoString(out, buf);
	appendStringInfoChar(out, '"');

	pfree(buf);
}

static void
array_to_string(msgpack_object o, StringInfo out)
{
	msgpack_object *p;
	msgpack_object *pend;

	appendStringInfoChar(out, '[');

	if (o.via.array.size != 0) {
		p = o.via.array.ptr;
		pend =  p + o.via.array.size;

		msgpack_object_to_string(*p, out);

		++p;

		for(; p < pend; ++p) {
			appendStringInfoString(out, ", ");
			msgpack_object_to_string(*p, out);
		}
	}

	appendStringInfoChar(out, ']');
}

static void
map_to_string(msgpack_object o, StringInfo out)
{
	msgpack_object_kv *p;
	msgpack_object_kv *pend;

	appendStringInfoChar(out, '{');
	if (o.via.map.size != 0) {
		p = o.via.map.ptr;
		pend = p + o.via.map.size;

		msgpack_object_to_string(p->key, out);
		appendStringInfoChar(out, ':');
		msgpack_object_to_string(p->val, out);

		++p;

		for (; p < pend; ++p) {
			appendStringInfoString(out, ", ");

			msgpack_object_to_string(p->key, out);
			appendStringInfoChar(out, ':');
			msgpack_object_to_string(p->val, out);
		}
	}
	appendStringInfoChar(out, '}');
}
