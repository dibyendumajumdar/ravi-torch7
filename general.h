#ifndef TORCH_GENERAL_INC
#define TORCH_GENERAL_INC

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "luaT.h"
#include "TH.h"

#if (defined(_MSC_VER) || defined(__MINGW32__))

#define snprintf _snprintf
#define popen _popen
#define pclose _pclose

#endif

#if LUA_VERSION_NUM >= 503
/* one can simply enable LUA_COMPAT_5_2 to be backward compatible.
However, this does not work when we are trying to use system-installed lua,
hence these redefines
*/
#ifndef luaL_optlong
#define luaL_optlong(L,n,d)     ((int64_t)luaL_optinteger(L, (n), (d)))
#endif
#ifndef luaL_checklong
#define luaL_checklong(L,n)     ((int64_t)luaL_checkinteger(L, (n)))
#endif
#define luaL_checkint(L,n)      ((int)luaL_checkinteger(L, (n)))
#endif

#endif
