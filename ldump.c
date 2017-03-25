#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#if (LUA_VERSION_NUM < 502)
# ifndef luaL_newlib
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
# endif
# ifndef lua_rawlen
#  define lua_rawlen(L, l) lua_objlen(L, l)
# endif
# endif

#ifndef LDUMP_MAXNUMBER2STR
# define LDUMP_MAXNUMBER2STR 32L
#endif

#ifndef LUA_INTEGER_FMT
# define LUA_INTEGER_FMT "%ld"
#endif

#ifndef LDUMP_NUMBER_FMT
# define LDUMP_NUMBER_FMT "%.7g"
#endif

#ifdef lua_integer2str
# undef lua_integer2str
#endif
# define lua_integer2str(s, sz, n) sprintf((s), LUA_INTEGER_FMT, (n))

#ifdef lua_number2str
# undef lua_number2str
#endif
# define lua_number2str(s, sz, n) sprintf((s), LDUMP_NUMBER_FMT, (n))

#ifndef RBUF_RATIO_SIZE
# define RBUF_RATIO_SIZE (1024 * 1024)
#endif

#ifndef RBUF_DEFAULT_SIZE
# define RBUF_DEFAULT_SIZE 512
#endif

#ifdef ENABLE_RBUF_DEB
# define DLOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
# define DLOG(fmt, ...) do {} while(0)
#endif

typedef struct {
	char *ptr;
	size_t len;
	size_t cap;
	char data_[RBUF_DEFAULT_SIZE];
} rbuf_t;

static int rbuf_reserve(rbuf_t *self, size_t require)
{
	size_t newlen = self->len + require;
	size_t newcap = self->cap;
	if (self->cap > newlen) {
		return 0;
	}

	do {
		if (newcap < RBUF_RATIO_SIZE) {
			newcap *= 2;
			continue;
		}
		newcap += RBUF_RATIO_SIZE;
	} while (newcap < newlen);

	if (self->ptr != self->data_) {
		self->ptr = realloc(self->ptr, newcap);
		if (self->ptr == NULL) {
			return -1;
		}
	} else {
		self->ptr = malloc(newcap);
		if (self->ptr == NULL) {
			return -1;
		}
		memcpy(self->ptr, self->data_, self->len);
	}
	self->cap = newcap;
	return 1;
}

static void rbuf_init(rbuf_t * self)
{
	self->len = 0;
	self->data_[0] = 0;
	self->cap = sizeof(self->data_);
	self->ptr = self->data_;
}

static void rbuf_free(rbuf_t * self)
{
	if (self->ptr != self->data_) {
		free(self->ptr);
		self->ptr = self->data_;
	}
}

static void rbuf_clear(rbuf_t * self)
{
	self->len = 0;
}

static int rbuf_catlen(rbuf_t * self, const void * data, size_t len)
{
	if (rbuf_reserve(self, len + 1) < 0 ) {
		return -1;
	}
	DLOG("cap=%d,len=%d,dlen=%d", self->cap, self->len, len);
	memcpy(self->ptr + self->len, data, len);
	self->len += len;
	self->ptr[self->len] = '\0';
	return 0;
}

static int rbuf_cat(rbuf_t * self, const char *data)
{
	return rbuf_catlen(self, data, strlen(data));
}

static size_t rbuf_len(rbuf_t * self)
{
	return self->len;
}

static int rbuf_catprintf(rbuf_t * self, const char * fmt, ...)
{
	int n, ret;
	const int max_len = 500 * 1024 * 1024;
	char mybuf[1024];
	va_list ap;
	char *tmp = (char *)&mybuf;
	int tmplen = sizeof(mybuf);
	do {
		va_start(ap, fmt);
		n = vsnprintf(tmp, tmplen, fmt, ap);
		va_end(ap);
		if (n < tmplen) {
			break;
		}
		if (n >= max_len) {
			return -1;
		}
		tmplen *= 2;
		if (tmp != mybuf) {
			tmp = realloc(tmp, tmplen);
		} else {
			tmp = malloc(tmplen);
		}
	} while (1);
	ret = rbuf_catlen(self, tmp, n);
	if (tmp != mybuf) {
		free(tmp);
	}
	return ret;
}

static int rbuf_catrepr(rbuf_t * s, const char *p, size_t len)
{
	rbuf_catlen(s,"\"",1);
	while(len--) {
		switch(*p) {
		case '\\':
		case '"': 
			if (rbuf_catprintf(s,"\\%c",*p) < 0)
				return -1;
			break;
		case '\n': rbuf_catlen(s,"\\n",2); break;
		case '\r': rbuf_catlen(s,"\\r",2); break;
		case '\t': rbuf_catlen(s,"\\t",2); break;
		case '\a': rbuf_catlen(s,"\\a",2); break;
		case '\b': rbuf_catlen(s,"\\b",2); break;
		default:
			   if (isprint(*p)) {
				   if (rbuf_catprintf(s,"%c",*p) < 0)
					   return -1;
			   } else {
				   if (rbuf_catprintf(s,"\\x%02x",(unsigned char)*p) < 0)
					   return -1;
			   }
			   break;
		}    
		p++; 
	}    
	return rbuf_catlen(s,"\"",1);
}

static void rbuf_test()
{
	int i;
	const char *repr = "repr begin hello\t\tworld\n \b repr end";
	rbuf_t buf;
	rbuf_init(&buf);
	for (i=0; i<1000; i++) {
		rbuf_catprintf(&buf, "%d\n", i);
	}
	rbuf_catprintf(&buf, "hello\t\tworld %s\n", "kkk");
	rbuf_catrepr(&buf, repr, strlen(repr));
	printf("%s\n", buf.ptr);
	rbuf_free(&buf);
}

static const size_t TABLE_MAX_DEEP = 16;
static const char* INDENT_MAP[21] = {
	"",
	"  ",
	"    ",
	"      ",
	"        ",
	"          ",
	"            ",
	"              ",
	"                ",
	"                  ",
	"                    ",
	"                      ",
	"                        ",
	"                          ",
	"                            ",
	"                              ",
	"                                ",
	"                                  ",
	"                                    ",
	"                                      ",
	"                                        ",
};

static const size_t INDENT_LEN = 2; /* indent len == 2 */
static int _check_value(lua_State *L);

static int conv_key(lua_State *L, int index, rbuf_t *tb)
{
        int type = lua_type(L, index);
	int ret = 0;
        switch(type)
        {    
                case LUA_TSTRING: {    
                        size_t len=0;
                        const char * key = luaL_checklstring(L, index, &len);
			rbuf_catlen(tb, "[", 1);
			rbuf_catrepr(tb, key, len);
			rbuf_catlen(tb, "]", 1);
                        break;
                }    
                case LUA_TNUMBER: {    
                        char s[LDUMP_MAXNUMBER2STR];
                        size_t len = (size_t)lua_number2str(s, LDUMP_MAXNUMBER2STR, luaL_checknumber(L, index));
                        /* 整数类型才加上[] */
			rbuf_catlen(tb, "[", 1);
			rbuf_catlen(tb, s, len);
			rbuf_catlen(tb, "]", 1);
                        break;
                }    
                default:
			/*
			 * luaL_error(L, "type error:%d", type);
			 */
			lua_pushfstring(L, "type error:%d,type=%s,cannot be key.", type, lua_typename(L, type));
			ret = -1;
                        break;
        }
	return ret;
}

static int conv_simple_type(lua_State *L, int index, rbuf_t *tb)
{
        int type = lua_type(L, index);
	int ret = 0;
        switch(type)
        {
                case LUA_TSTRING: {
                        size_t len=0;
                        const char * key = luaL_checklstring(L, index, &len);
			rbuf_catrepr(tb, key, len);
                        break;
                }
                /* 这儿不能够直接使用lua_tostring，因为这个api会修改栈内容，将导致array的lua_next访问异常。 */
                /* 也不能够强行把lua_Number转换为%d输出。 */
                case LUA_TNUMBER: {
                        char s[LDUMP_MAXNUMBER2STR];
                        size_t len = (size_t)lua_number2str(s, LDUMP_MAXNUMBER2STR, luaL_checknumber(L, index));
			rbuf_catlen(tb, s, len);
                        break;
		}
                case LUA_TBOOLEAN: {
			rbuf_cat(tb, (lua_toboolean(L, index) ? "true" : "false"));
                        break;
		}
                case LUA_TNIL: {
			rbuf_catlen(tb, "nil", 3);
                        break;
		}
                default:
                        /*
			 * luaL_error(L, "type error Type:%d,Name:%s.table can not be key", type, lua_typename(L, type));
			 */
			lua_pushfstring(L, "type error Type:%d,Name:%s can not be dump", type, lua_typename(L, type));
			ret = -1;
                        break;
        }
	return ret;
}

static int hash_conv_line(lua_State *L, int depth, rbuf_t *tb)
{
	int type;
        depth++;
        if (depth > TABLE_MAX_DEEP)
        {
                lua_pushfstring(L, "dump too deep depth=%d,max=%d!", depth, TABLE_MAX_DEEP);
                return -1;
        }
        type = lua_type(L, -1);
        if (type == LUA_TTABLE)
        {
		rbuf_catlen(tb, "{", 1);
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
			rbuf_catlen(tb, "[", 1);
                        if (conv_simple_type(L, -2, tb) != 0)
				return -1;
			rbuf_catlen(tb, "]=", 2);
                        if (hash_conv_line(L, depth, tb) != 0)
				return -1;
			rbuf_catlen(tb, ",", 1);
                        lua_pop(L, 1);
                }
		rbuf_catlen(tb, "}", 1);
        }
        else
        {
		if (conv_simple_type(L, -2, tb) != 0)
			return -1;
        }
	return 0;
}

/* hash and array may be included in one table at the same time */
/* keep array in order */
static int conv_mix_data(lua_State *L, int depth, rbuf_t *tb, int enable_comment, int is_in_line)
{
	int type;
	depth++;
	if (depth > TABLE_MAX_DEEP) {
                lua_pushfstring(L, "dump too deep depth=%d,max=%d!", depth, TABLE_MAX_DEEP);
		return -1;
	}
        type = lua_type(L, -1);
        if (type == LUA_TTABLE) {
		size_t arrayLen = (size_t)lua_rawlen(L, -1); /* L : ..., tbl */
		rbuf_catlen(tb, "{", 1);
		if (arrayLen > 0) {
			/* array include */
			int *visited_map = NULL;
			int i;
			visited_map = (int *)malloc(sizeof(int) * (arrayLen + 1));
			if (visited_map == NULL) {
				lua_pushliteral(L, "nomem");
				return -1;
			}
			bzero((void *)visited_map, sizeof(int) * (arrayLen + 1));
			for(i=1; i<=(int)arrayLen; i++){
				visited_map[i] = 1;
				if (!is_in_line) {
					const char *space = INDENT_MAP[depth];
					rbuf_catlen(tb, "\n", 1);
					rbuf_catlen(tb, space, depth * INDENT_LEN);
				}

				lua_rawgeti(L, -1, i); /* push table value to stack, L : ..., tbl, value */
				if (conv_mix_data(L, depth, tb, enable_comment, is_in_line) != 0) {
					free(visited_map);
					return -1;
				}
				if (!is_in_line && enable_comment) {
					char s[LDUMP_MAXNUMBER2STR];
					rbuf_catlen(tb, ", --", 4);
					lua_integer2str(s, LDUMP_MAXNUMBER2STR, (lua_Integer)i);
					rbuf_cat(tb, s);
				} else {
					rbuf_catlen(tb, ",", 1);
				}
				lua_pop(L,1); /* repair stack */
			}
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				int type = lua_type(L, -2);
				if (type == LUA_TNUMBER) {
					/* assume every number key is not float */
					size_t idx = (size_t) lua_tointeger(L, -2);
					if (idx <= arrayLen && visited_map[idx]) {
						lua_pop(L, 1); 		/* L : ..., tbl, key(integer only) */
						continue;
					}
				}
				if  (!is_in_line) {
					const char *space = INDENT_MAP[depth];
					rbuf_catlen(tb, "\n", 1);
					rbuf_catlen(tb, space, depth * INDENT_LEN);
				}

				/* L : ..., tbl, key(integer or string), value */
				if (conv_key(L, -2, tb) != 0) {
					free(visited_map);
					return -1;
				}
				rbuf_catlen(tb, "=", 1);
				if(conv_mix_data(L, depth, tb, enable_comment, is_in_line) != 0) {
					free(visited_map);
					return -1;
				}
				rbuf_catlen(tb, ",", 1);
				lua_pop(L, 1);
			}
			free(visited_map);
		} else {
			lua_pushnil(L);
                	while (lua_next(L, -2) != 0) {
				if (!is_in_line) {
					const char *space = INDENT_MAP[depth];
					rbuf_catlen(tb, "\n", 1);
					rbuf_catlen(tb, space, depth * INDENT_LEN);
				}
				/* L : ..., tbl, key(integer or string), value */
				if (conv_key(L, -2, tb) != 0)
					return -1;
				rbuf_catlen(tb, "=", 1);
				if (conv_mix_data(L, depth, tb, enable_comment, is_in_line) != 0) {
					return -1;
				}
				rbuf_catlen(tb, ",", 1);
				lua_pop(L, 1);
			}
		}
		if (!is_in_line) {
			const char *space = INDENT_MAP[depth-1];
			rbuf_catlen(tb, "\n", 1);
			rbuf_catlen(tb, space, (depth-1) * INDENT_LEN);
		}
		rbuf_catlen(tb, "}", 1);
	} else {
                if (conv_simple_type(L, -1, tb) != 0)
			return -1;
	}
	return 0;
}

static int conv_hash_data(lua_State *L, int depth, rbuf_t *tb)
{
	int type;
        depth++;
        if (depth > TABLE_MAX_DEEP) {
                lua_pushfstring(L, "dump too deep depth=%d,max=%d!", depth, TABLE_MAX_DEEP);
                return -1;
        }

        type = lua_type(L, -1);
        if (type == LUA_TTABLE) {
                int index = 0;
                int count = 0; /* 每行的变量数目有限。 */
		const char *space = INDENT_MAP[depth-1];
		rbuf_catlen(tb, "{", 1);
                lua_pushnil(L);

                while (lua_next(L, -2) != 0) {
                        /* 增加一些方便阅读的格式符 */
                        if  (depth<=sizeof(INDENT_MAP)/sizeof(INDENT_MAP[0])) {
				const char *space = INDENT_MAP[depth];
				rbuf_catlen(tb, "\n", 1);
				rbuf_catlen(tb, space, depth * INDENT_LEN);
                        }

                        index ++;
                        count ++;

                        if (conv_key(L, -2, tb) != 0)
				return -1;
			rbuf_catlen(tb, "=", 1);
                        if (conv_hash_data(L, depth, tb) != 0) {
				return -1;
			}
			rbuf_catlen(tb, ",", 1);
                        lua_pop(L, 1);
                }
		rbuf_catlen(tb, "\n", 1);
		rbuf_catlen(tb, space, (depth-1) * INDENT_LEN);
		rbuf_catlen(tb, "}", 1);
		return 0;
        }
	return conv_simple_type(L, -1, tb);
}

static int lua__dump_in_line(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	rbuf_init(pbuf);

        if (hash_conv_line(L, 0, pbuf) == 0) {
		lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
		lua_pushinteger(L, rbuf_len(pbuf));
	} else {
		lua_insert(L, 1);
		lua_pushnil(L);
		lua_insert(L, 1);
		lua_settop(L, 2);
	}

	rbuf_free(pbuf);
        return 2;
}

/* hash and array may be included in one table at the same time */
/* keep array in order */
/* if an array contains nil value, such as {1,nil, nil, 2}, the result keeps that */
static int lua__dump_mix(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	int enable_comment = 0;
	int is_in_line = 1;
	int argc = lua_gettop(L);
	rbuf_init(pbuf);
	if (argc > 3) {
		luaL_error(L, "lua__dump_mix args:(value, enable_comment, is_in_line)");
	}
	if (argc >= 2) {
		if ( argc == 3) {
			/* L : value, is_in_line, enable_comment */
			enable_comment = (int)lua_toboolean(L, -1);
			lua_pop(L, 1);
		}
		is_in_line = (int)lua_toboolean(L, -1); /* L : value, is_in_line */
		if (is_in_line) {
			enable_comment = 0;
		}
		lua_pop(L, 1);
	}
	if (conv_mix_data(L, 0, pbuf, enable_comment, is_in_line) != 0) {
		lua_insert(L, 1);
		lua_pushnil(L);
		lua_insert(L, 1);
		lua_settop(L, 2);
		rbuf_free(pbuf);
		return 2;
	}
	lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
	lua_pushinteger(L, rbuf_len(pbuf));
	rbuf_free(pbuf);
	return 2;
}

/* convert every array to hash */
static int lua__dump(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	rbuf_init(pbuf);
	if (conv_hash_data(L, 0, pbuf) == 0) {
		lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
		lua_pushinteger(L, rbuf_len(pbuf));
	} else {
		lua_insert(L, 1);
		lua_pushnil(L);
		lua_insert(L, 1);
		lua_settop(L, 2);
	}
	rbuf_free(pbuf);
	return 2;
}

static int _check_value(lua_State *L)
{
	int type = lua_type(L, -1);
	int top = lua_gettop(L);

	switch(type) {
	case LUA_TNIL:
	case LUA_TBOOLEAN:
	case LUA_TNUMBER:
	case LUA_TSTRING:
		return 1;
	default:
		break;
	}

	if (type != LUA_TTABLE) {
		return 0;
	}

	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		int ktype = lua_type(L, -2);
		if ((ktype != LUA_TNUMBER && ktype != LUA_TSTRING) || 
			!_check_value(L)) {
			lua_pop(L, 2);
			return 0;
		}
		lua_pop(L, 1);
	}
	assert(top == lua_gettop(L));
	return 1;
}

static int lua__check(lua_State *L)
{
	int argc = lua_gettop(L);
	int r;
	if (argc != 1) {
		luaL_error(L, "only 1 arg required in %s", __FUNCTION__);
		return 0;
	}
	r = _check_value(L);
	lua_pushboolean(L, r);
	return 1;
}

int luaopen_ldump(lua_State* L)
{
	luaL_Reg lfuncs[] = {
		{"check", lua__check},
		{"dump_in_line", lua__dump_in_line},
		{"dump_mix", lua__dump_mix},
		{"dump", lua__dump},
		{NULL, NULL},
	};
	luaL_newlib(L, lfuncs);
	return 1;
}

