#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "compat.h"
#include "rbuf.h"
#define LIBNAME "ldump"

#define LUA_INTEGER_FMT "%d"
#define LUAI_MAXNUMBER2STR 32
#define integer2str(s,n) sprintf((s), LUA_INTEGER_FMT, (n))
#define DEFAULT_TINY_BUFF_SIZE 1024 * 512

static const size_t TableMaxDeep = 20;
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

static const size_t INDENT_LEN = 2; // indent len == 2
static int _check_value(lua_State *L);


static void ConvKey(lua_State *L, int index, rbuf_t *tb)
{
        int type = lua_type(L, index);
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
                        char s[LUAI_MAXNUMBER2STR];
                        size_t len = lua_number2str(s, luaL_checknumber(L, index));
                        //整数类型才加上[]
			rbuf_catlen(tb, "[", 1);
			rbuf_catlen(tb, s, len);
			rbuf_catlen(tb, "]", 1);
                        break;
                }    
                default:
                        luaL_error(L, "type error:%d", type);
                        break;
        }    

}

static void ConvSimpleType(lua_State *L, int index, rbuf_t *tb)
{
        int type = lua_type(L, index);
        switch(type)
        {
                case LUA_TSTRING: {
                        size_t len=0;
                        const char * key = luaL_checklstring(L, index, &len);
			rbuf_catrepr(tb, key, len);
                        break;
                }
                //这儿不能够直接使用lua_tostring，因为这个api会修改栈内容，将导致array的lua_next访问异常。
                //也不能够强行把lua_Number转换为%d输出。
                case LUA_TNUMBER: {
                        char s[LUAI_MAXNUMBER2STR];
                        size_t len = lua_number2str(s, luaL_checknumber(L, index));
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
                        luaL_error(L, "type error Type:%d,Name:%s.table can not be key", type, lua_typename(L, type));
                        break;
        }
}

static void HashConvLine(lua_State *L, int Depth, rbuf_t *tb)
{
        Depth++;
        if (Depth > 20)
        {
                luaL_error(L, "dump too deep!");
                return;
        }
        int type = lua_type(L, -1);
        if (type == LUA_TTABLE)
        {
		rbuf_catlen(tb, "{", 1);
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
			rbuf_catlen(tb, "[", 1);
                        ConvSimpleType(L, -2, tb);
			rbuf_catlen(tb, "]=", 2);
                        HashConvLine(L, Depth, tb);
			rbuf_catlen(tb, ",", 1);
                        lua_pop(L, 1);
                }
		rbuf_catlen(tb, "}", 1);
        }
        else
        {
                ConvSimpleType(L, -1, tb);
        }
}

// hash and array may be included in one table at the same time
// keep array in order
static int ConvMixData(lua_State *L, int Depth, rbuf_t *tb, int isShowCommentIdx, int isInLine)
{
	Depth++;
	if (Depth > TableMaxDeep) {
		return 0;
	}
        int type = lua_type(L, -1);
        if (type == LUA_TTABLE) {
		rbuf_catlen(tb, "{", 1);
		size_t arrayLen = (size_t)lua_objlen(L, -1); // L : ..., tbl
		if (arrayLen > 0) {
			// array include
			int *visitedMap = NULL;
			int i;
			visitedMap = (int *)malloc(sizeof(int) * (arrayLen + 1));
			if (visitedMap == NULL) {
				return 0;
			}
			bzero((void *)visitedMap, sizeof(int) * (arrayLen + 1));
			for(i=1; i<=(int)arrayLen; i++){
				visitedMap[i] = 1;
				if (!isInLine) {
					rbuf_catlen(tb, "\n", 1);
					const char *space = INDENT_MAP[Depth];
					rbuf_catlen(tb, space, Depth * INDENT_LEN);
				}

				lua_rawgeti(L, -1, i); // push table value to stack, L : ..., tbl, value
				if (!ConvMixData(L, Depth, tb, isShowCommentIdx, isInLine)) {
					free(visitedMap);
					return 0;
				}
				if (!isInLine && isShowCommentIdx) {
					rbuf_catlen(tb, ", --", 4);
					char s[LUAI_MAXNUMBER2STR];
					integer2str(s, i);
					rbuf_cat(tb, s);
				} else {
					rbuf_catlen(tb, ",", 1);
				}
				lua_pop(L,1); // repair stack
			}
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				int type = lua_type(L, -2);
				if (type == LUA_TNUMBER) {
					// assume every number key is not float
					size_t idx = (size_t) lua_tointeger(L, -2);
					if (idx <= arrayLen && visitedMap[idx]) {
						lua_pop(L, 1); 		// L : ..., tbl, key(integer only)
						continue;
					}
				}
				if  (!isInLine) {
					rbuf_catlen(tb, "\n", 1);
					const char *space = INDENT_MAP[Depth];
					rbuf_catlen(tb, space, Depth * INDENT_LEN);
				}
				ConvKey(L, -2, tb);	// L : ..., tbl, key(integer or string), value
				rbuf_catlen(tb, "=", 1);
				if(!ConvMixData(L, Depth, tb, isShowCommentIdx, isInLine)) {
					free(visitedMap);
					return 0;
				}
				rbuf_catlen(tb, ",", 1);
				lua_pop(L, 1);
			}
			free(visitedMap);
		} else {
			lua_pushnil(L);
                	while (lua_next(L, -2) != 0) {
				if (!isInLine) {
					rbuf_catlen(tb, "\n", 1);
					const char *space = INDENT_MAP[Depth];
					rbuf_catlen(tb, space, Depth * INDENT_LEN);
				}
				ConvKey(L, -2, tb);	// L : ..., tbl, key(integer or string), value
				rbuf_catlen(tb, "=", 1);
				if (!ConvMixData(L, Depth, tb, isShowCommentIdx, isInLine)) {
					return 0;
				}
				rbuf_catlen(tb, ",", 1);
				lua_pop(L, 1);
			}
		}
		if (!isInLine) {
			rbuf_catlen(tb, "\n", 1);
			const char *space = INDENT_MAP[Depth-1];
			rbuf_catlen(tb, space, (Depth-1) * INDENT_LEN);
		}
		rbuf_catlen(tb, "}", 1);
	} else {
                ConvSimpleType(L, -1, tb);
	}
	return 1;
}

static void ConvHashData(lua_State *L, int Depth, rbuf_t *tb)
{
        Depth++;
        if (Depth > TableMaxDeep) {
                luaL_error(L, "dump too deep!");
                return;
        }

        int type = lua_type(L, -1);
        if (type == LUA_TTABLE) {
		rbuf_catlen(tb, "{", 1);
                lua_pushnil(L);

                int index = 0;
                int count = 0; //每行的变量数目有限。
                while (lua_next(L, -2) != 0) {
                        //增加一些方便阅读的格式符
                        if  (Depth<=sizeof(INDENT_MAP)/sizeof(INDENT_MAP[0])) {
				rbuf_catlen(tb, "\n", 1);
				const char *space = INDENT_MAP[Depth];
				rbuf_catlen(tb, space, Depth * INDENT_LEN);
                        }

                        index ++;
                        count ++;

                        ConvKey(L, -2, tb);
			rbuf_catlen(tb, "=", 1);
                        ConvHashData(L, Depth, tb);
			rbuf_catlen(tb, ",", 1);
                        lua_pop(L, 1);
                }
		rbuf_catlen(tb, "\n", 1);
		const char *space = INDENT_MAP[Depth-1];
		rbuf_catlen(tb, space, (Depth-1) * INDENT_LEN);
		rbuf_catlen(tb, "}", 1);
        } else {
                ConvSimpleType(L, -1, tb);
        }
}

static int SerializeHashDataInLine(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	rbuf_init(pbuf);

        HashConvLine(L, 0, pbuf);
        lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
	lua_pushinteger(L, rbuf_len(pbuf));

	rbuf_free(pbuf);
        return 2;
}

// hash and array may be included in one table at the same time
// keep array in order
// if an array contains nil value, such as {1,nil, nil, 2}, the result keeps that
static int SerializeMixData(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	rbuf_init(pbuf);
	int isShowCommentIdx = 0;
	int isInLine = 1;
	int argc = lua_gettop(L);
	if (argc > 3) {
		luaL_error(L, "SerializeMixData args:(value, isShowComment, InLine)");
	}
	if (argc >= 2) {
		if ( argc == 3) {
			isShowCommentIdx = (int)lua_toboolean(L, -1); // L : value, isInLine, isShowCommentIdx
			lua_pop(L, 1);
		}
		isInLine = (int)lua_toboolean(L, -1); // L : value, isInLine
		if (isInLine) {
			isShowCommentIdx = 0;
		}
		lua_pop(L, 1);
	}
	if (!ConvMixData(L, 0, pbuf, isShowCommentIdx, isInLine)) {
		luaL_error(L, "dump too deep or alloc memory failed!");
	}
	lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
	lua_pushinteger(L, rbuf_len(pbuf));
	rbuf_free(pbuf);
	return 2;
}

// convert every array to hash
static int SerializeHashDataIndent(lua_State *L)
{
	rbuf_t buf;
	rbuf_t *pbuf = &buf;
	rbuf_init(pbuf);
	ConvHashData(L, 0, pbuf);
	lua_pushlstring(L, (const char *)(pbuf->ptr), rbuf_len(pbuf));
	lua_pushinteger(L, rbuf_len(pbuf));
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

static int check_value(lua_State *L)
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
		{"check", check_value},
		{"dump_in_line", SerializeHashDataInLine},
		{"dump_mix", SerializeMixData},
		{"dump", SerializeHashDataIndent},
		{NULL, NULL},
	};
	luaL_newlib(L, lfuncs);
	return 1;
}

