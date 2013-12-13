#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "postgres.h"
#include "utils/jsonapi.h"
#include "utils/builtins.h"

#include "convert_to_msgpack.h"

/*
 * Types for a scalar
 */
typedef enum {
	JSON_VALUE_SCALAR,
	MSGPACK_BINARY
} JsonValueType;

typedef struct {
	JsonTokenType	token_type;
	char			*token;
} JsonValueScalar;

typedef struct {
	size_t	size;
	char	*data;
} MsgpackBinary;

typedef union {
	JsonValueScalar	scalar;
	MsgpackBinary	msg;
} JsonValueUnion;

typedef struct {
	JsonValueType	type;
	JsonValueUnion	via;
} JsonValueData, *JsonValue;

/*
 * Types for a container
 */
typedef enum {
	JSON_ARRAY,
	JSON_OBJECT,
	JSON_TOP_LEVEL
} JsonContainerType;

typedef struct _JsonObjectField {
	struct _JsonObjectField	*next;
	char					*key;
	JsonValue				value;
} JsonObjectFieldData, *JsonObjectField;

typedef struct {
	size_t			length;
	JsonObjectField	start;
	JsonObjectField	end;
} JsonObject;

typedef struct _JsonArrayElement {
	struct _JsonArrayElement	*next;
	JsonValue			value;
} JsonArrayElementData, *JsonArrayElement;

typedef struct {
	size_t				length;
	JsonArrayElement	start;
	JsonArrayElement	end;
} JsonArray;

typedef struct {
	JsonValue	value;
} JsonTopLevel;

typedef union {
	JsonObject		object;
	JsonArray		array;
	JsonTopLevel	top_level;
} JsonContainerUnion;

typedef struct _JsonContainer{
	struct _JsonContainer		*parent;
	JsonContainerType			type;
	JsonContainerUnion			via;
} JsonContainerData, *JsonContainer;

/*
 * State for json_string_to_msgpack
 */
typedef struct {
	JsonLexContext	*lex;
	JsonContainer	current_container;
	msgpack_packer	*pk;
	msgpack_sbuffer	*buf;
} PackStateData, *PackState;

/*
 * Semantic action functions for json_string_to_msgpack
 */
static void sem_object_start(void *state);
static void sem_object_end(void *state);
static void sem_array_start(void *state);
static void sem_array_end(void *state);
static void sem_object_field_start(void *state, char *fname, bool isnull);
static void sem_array_element_start(void *state, bool isnull);
static void sem_scalar(void *state, char *token, JsonTokenType tokentype);

/*
 * Destroy function
 */

static inline void destroy_container(JsonContainer container);
static inline void destroy_object(JsonObject object);
static inline void destroy_array(JsonArray array);
static inline void destroy_value(JsonValue value);

/*
 * Pack functions
 */
static inline void pack_value(msgpack_packer *pk, const JsonValue value);
static inline void pack_scalar(msgpack_packer *pk, const JsonValueScalar scalar);
static inline void pack_string(msgpack_packer *pk, const char *str);
static inline void pack_number(msgpack_packer *pk, const char *number_token);
static inline void pack_unsigned_integer(msgpack_packer *pk, const char *unsigned_integer_token);
static inline void pack_integer(msgpack_packer *pk, const char *integer_token);
static inline void pack_real(msgpack_packer *pk, const char *real_token);

/*
 * Utility functions
 */
static inline JsonValue value_to_be_set(const JsonContainer container);

void
json_string_to_msgpack(const char *json_str, msgpack_sbuffer *buf)
{
	PackState		state;
	JsonLexContext	*lex = makeJsonLexContext(cstring_to_text(json_str), true);
	JsonSemAction	*sem;
	msgpack_packer	*pk;
	JsonContainer	top_level;
	JsonValue		top_level_value;

	/* initialize packer */
	pk = msgpack_packer_new(buf, msgpack_sbuffer_write);

	/* initialize top level object */
	top_level_value = palloc(sizeof(JsonValueData));

	top_level = palloc(sizeof(JsonContainerData));
	top_level->type = JSON_TOP_LEVEL;
	top_level->parent = NULL;
	top_level->via.top_level.value = top_level_value;

	/* initialize state object */
	state = palloc(sizeof(PackStateData));
	state->lex = lex;
	state->current_container = top_level;
	state->pk = pk;
	state->buf = buf;

	/* initialize sem action */
	sem = palloc(sizeof(JsonSemAction));
	sem->semstate 				= (void *) state;
	sem->object_start 			= sem_object_start;
	sem->object_end 			= sem_object_end;
	sem->array_start 			= sem_array_start;
	sem->array_end 				= sem_array_end;
	sem->object_field_start 	= sem_object_field_start;
	sem->object_field_end 		= NULL;
	sem->array_element_start 	= sem_array_element_start;
	sem->array_element_end 		= NULL;
	sem->scalar 				= sem_scalar;

	/* run parser */
	pg_parse_json(lex, sem);

	/* pack top level value */
	pack_value(pk, top_level_value);

	/* destroy packer */
	msgpack_packer_free(pk);

	/* destroy top level object */
	destroy_container(top_level);
}

static void
sem_object_start(void *state)
{
	PackState		_state = (PackState) state;
	JsonContainer	current_container, new_container;

	current_container = _state->current_container;

	/* create and set new container */
	new_container = palloc(sizeof(JsonContainerData));
	new_container->type = JSON_OBJECT;
	new_container->parent = current_container;
	new_container->via.object.length = 0;
	new_container->via.object.start = NULL;
	new_container->via.object.end = NULL;

	/* step forward to new container */
	_state->current_container = new_container;
}

static void
sem_object_end(void *state)
{
	PackState		_state = (PackState) state;
	msgpack_sbuffer	*buf;
	JsonContainer	current_container, parent_container;
	msgpack_packer 	*pk;
	JsonObjectField	field;
	size_t			packed_size;
	char			*packed_data;
	JsonValue		value;

	current_container = _state->current_container;
	pk = _state->pk;

	/* start to pack map */
	msgpack_pack_map(pk, current_container->via.object.length);

	/* pack each field */
	field = current_container->via.object.start;
	while (field != NULL) {
		/* pack key */
		pack_string(pk, field->key);
		/* pack value */
		pack_value(pk, field->value);
		/* next field*/
		field = field->next;
	}

	/* retrieve packed binary and reset buffer of packer */
	buf = _state->buf;
	packed_size = buf->size;
	packed_data = msgpack_sbuffer_release(buf);

	/* set packed binary to the last value of parent conationer */
	parent_container = current_container->parent;
	value = value_to_be_set(parent_container);
	value->type = MSGPACK_BINARY;
	value->via.msg.size = packed_size;
	value->via.msg.data = packed_data;

	/* step back to parent level */
	_state->current_container = parent_container;
	destroy_container(current_container);
}

static void
sem_array_start(void *state)
{
	PackState		_state = (PackState) state;
	JsonContainer	current_container, new_container;

	current_container = _state->current_container;

	/* create and set new container */
	new_container = palloc(sizeof(JsonContainerData));
	new_container->type = JSON_ARRAY;
	new_container->parent = current_container;
	new_container->via.array.length = 0;
	new_container->via.array.start = NULL;
	new_container->via.array.end = NULL;

	/* step forward to new container */
	_state->current_container = new_container;
}

static void
sem_array_end(void *state)
{
	PackState			_state = (PackState) state;
	msgpack_sbuffer		*buf;
	JsonContainer		current_container, parent_container;
	msgpack_packer 		*pk;
	JsonArrayElement	element;
	size_t				packed_size;
	char				*packed_data;
	JsonValue			value;

	current_container = _state->current_container;
	pk = _state->pk;

	/* start to pack array */
	msgpack_pack_array(pk, current_container->via.array.length);

	/* pack each element */
	element = current_container->via.array.start;
	while (element != NULL) {
		/* pack value */
		pack_value(pk, element->value);
		/* next element */
		element = element->next;
	}

	/* retrieve packed binary and reset buffer of packer */
	buf = _state->buf;
	packed_size = buf->size;
	packed_data = msgpack_sbuffer_release(buf);

	/* set packed binary to the last value of parent conationer */
	parent_container = current_container->parent;
	value = value_to_be_set(parent_container);
	value->type = MSGPACK_BINARY;
	value->via.msg.size = packed_size;
	value->via.msg.data = packed_data;

	/* step back to parent level */
	_state->current_container = parent_container;
	destroy_container(current_container);
}

static void
sem_object_field_start(void *state, char *fname, bool isnull)
{
	PackState		_state = (PackState) state;
	JsonContainer	container;
	JsonObjectField	new_field;
	JsonValue		new_value;

	container = _state->current_container;

	new_value = palloc(sizeof(JsonValueData));

	new_field = palloc(sizeof(JsonObjectFieldData));
	new_field->next = NULL;
	new_field->key = pstrdup(fname);
	new_field->value = new_value;

	container->via.object.length += 1;

	if (container->via.object.end == NULL) {
		container->via.object.start = new_field;
		container->via.object.end = new_field;
	} else {
		container->via.object.end->next = new_field;
		container->via.object.end = new_field;
	}
}

static void
sem_array_element_start(void *state, bool isnull)
{
	PackState			_state = (PackState) state;
	JsonContainer		container;
	JsonArrayElement	new_element;
	JsonValue			new_value;

	container = _state->current_container;

	new_value = palloc(sizeof(JsonValueData));

	new_element = palloc(sizeof(JsonArrayElementData));
	new_element->next = NULL;
	new_element->value = new_value;

	container->via.array.length += 1;

	if (container->via.array.end == NULL) {
		container->via.array.start = new_element;
		container->via.array.end = new_element;
	} else {
		container->via.array.end->next = new_element;
		container->via.array.end = new_element;
	}
}

static void
sem_scalar(void *state, char *token, JsonTokenType tokentype)
{
	PackState	_state = (PackState) state;
	JsonContainer		container;
	JsonValue			value;

	container = _state->current_container;
	value = value_to_be_set(container);
	value->type = JSON_VALUE_SCALAR;
	value->via.scalar.token_type = tokentype;
	value->via.scalar.token = token;
}


static inline void
destroy_container(JsonContainer container)
{
	switch (container->type) {
		case JSON_OBJECT:
			destroy_object(container->via.object);
			break;
		case JSON_ARRAY:
			destroy_array(container->via.array);
			break;
		case JSON_TOP_LEVEL:
			destroy_value(container->via.top_level.value);
			break;
	}
	pfree(container);
}

static inline void
destroy_object(JsonObject object)
{
	JsonObjectField	field;

	field = object.start;
	while (field != NULL) {
		pfree(field->key);
		destroy_value(field->value);
		field = field->next;
	}
}

static inline void
destroy_array(JsonArray array)
{
	JsonArrayElement	element;

	element = array.start;
	while (element != NULL) {
		destroy_value(element->value);
		element = element->next;
	}
}

static inline void
destroy_value(JsonValue value)
{
	switch (value->type) {
		case JSON_VALUE_SCALAR:
			pfree(value->via.scalar.token);
			break;
		case MSGPACK_BINARY:
			free(value->via.msg.data);
			break;
	}
	pfree(value);
}

static inline void
pack_value(msgpack_packer *pk, const JsonValue value)
{
	switch (value->type) {
		case JSON_VALUE_SCALAR:
			pack_scalar(pk, value->via.scalar);
			break;
		case MSGPACK_BINARY:
			(* pk->callback)(pk->data, value->via.msg.data, value->via.msg.size);
			break;
	}
}

static inline void
pack_scalar(msgpack_packer *pk, const JsonValueScalar scalar)
{
	switch (scalar.token_type) {
		case JSON_TOKEN_STRING:
			pack_string(pk, scalar.token);
			break;
		case JSON_TOKEN_NUMBER:
			pack_number(pk, scalar.token);
			break;
		case JSON_TOKEN_TRUE:
			msgpack_pack_true(pk);
			break;
		case JSON_TOKEN_FALSE:
			msgpack_pack_false(pk);
			break;
		case JSON_TOKEN_NULL:
			msgpack_pack_nil(pk);
			break;
		default:
			/* TODO: parse error */
			break;
	}
}

static inline void
pack_string(msgpack_packer *pk, const char *str)
{
	size_t len = strlen(str);
	msgpack_pack_raw(pk, len);
	msgpack_pack_raw_body(pk, str, len);
}

static inline void
pack_number(msgpack_packer *pk, const char *number_token)
{
	if (strchr(number_token, '.') == NULL) {
		if (number_token[0] == '-')
			pack_integer(pk, number_token);
		else
			pack_unsigned_integer(pk, number_token);
	} else
		pack_real(pk, number_token);
}

static inline void
pack_unsigned_integer(msgpack_packer *pk, const char *uint_token)
{
	char *end;
	unsigned long value;
	
	errno = 0;
	value = strtoul(uint_token, &end, 10);
	/* TODO: error handling */

	/* TODO */
	msgpack_pack_unsigned_long(pk, value);
}

static inline void
pack_integer(msgpack_packer *pk, const char *int_token)
{
	char *end;
	long value;
	
	errno = 0;
	value = strtol(int_token, &end, 10);
	/* TODO: error handling */

	/* TODO */
	msgpack_pack_long(pk, value);
}

static inline void
pack_real(msgpack_packer *pk, const char *real_token)
{
	char *end;
	double value;

	errno = 0;
	value = strtod(real_token, &end);
	/* TODO: error handling */

	/* TODO */
	msgpack_pack_double(pk, value);
}

static inline JsonValue
value_to_be_set(const JsonContainer container)
{
	JsonValue value;
	switch (container->type) {
		case JSON_OBJECT:
			value = container->via.object.end->value;
			break;
		case JSON_ARRAY:
			value = container->via.array.end->value;
			break;
		case JSON_TOP_LEVEL:
			value = container->via.top_level.value;
			break;
	}
	return value;
};
