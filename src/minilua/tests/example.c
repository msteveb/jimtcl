#define LUA_IMPL
#include "../minilua.h"

int main() {
  lua_State *L = luaL_newstate();
  if(L == NULL)
    return -1;
  luaL_openlibs(L);
  luaL_loadstring(L, "print 'hello world'");
  lua_call(L, 0, 0);
  lua_close(L);
  return 0;
}
