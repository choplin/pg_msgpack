MODULE_big = pg_msgpack
OBJS = pg_msgpack.o pg_msgpack_op.o convert_from_msgpack.o convert_to_msgpack.o

EXTENSION = pg_msgpack
EXTVERSION = 0.0.1
EXTSQL = $(EXTENSION)--$(EXTVERSION).sql
SHLIB_LINK += -lmsgpack

DATA = $(EXTSQL)
DOCS = doc/$(EXTENSION).rst
REGRESS = $(EXTENSION)

PG_CPPFLAGS = -std=c99 -Werror

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
