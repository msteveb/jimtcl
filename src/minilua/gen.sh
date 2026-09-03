LUAVER=5.5.0
LUADIR=lua-$LUAVER
LUAPKG=lua-$LUAVER.tar.gz
LUAURL=https://www.lua.org/ftp/$LUAPKG

OUTFILE=minilua.h

if [ ! -d "$LUADIR" ]; then
    rm -rf $LUAPKG
    wget $LUAURL
    tar xzf $LUAPKG
fi

rm -f $OUTFILE

cat <<EOF >> $OUTFILE
/*
  minilua.h -- Lua in a single header
  Project URL: https://github.com/edubart/minilua

  This is Lua $LUAVER contained in a single header to be bundled in C/C++ applications with ease.
  Lua is a powerful, efficient, lightweight, embeddable scripting language.

  Do the following in *one* C file to create the implementation:
    #define LUA_IMPL

  By default it detects the system platform to use, however you could explicitly define one.

  Note that almost no modification was made in the Lua implementation code,
  thus there are some C variable names that may collide with your code,
  therefore it is best to declare the Lua implementation in dedicated C file.

  Optionally provide the following defines:
    LUA_MAKE_LUA     - implement the Lua command line REPL

  LICENSE
    MIT License, same as Lua, see end of file.
*/

/* detect system platform */
#if !defined(LUA_USE_WINDOWS) && !defined(LUA_USE_LINUX) && !defined(LUA_USE_MACOSX) && !defined(LUA_USE_POSIX) && !defined(LUA_USE_C89) && !defined(LUA_USE_IOS)
#if defined(_WIN32)
#define LUA_USE_WINDOWS
#elif defined(__linux__)
#define LUA_USE_LINUX
#elif defined(__APPLE__)
#define LUA_USE_MACOSX
#else /* probably a POSIX system */
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN
#endif
#endif

EOF

echo "#ifdef LUA_IMPL" >> $OUTFILE
  echo "#define LUA_CORE" >> $OUTFILE
  cat $LUADIR/src/lprefix.h >> $OUTFILE
echo "#endif /* LUA_IMPL */" >> $OUTFILE

cat <<EOF >> $OUTFILE
#ifdef __cplusplus
extern "C" {
#endif
EOF

cat $LUADIR/src/luaconf.h >> $OUTFILE
cat $LUADIR/src/lua.h >> $OUTFILE
cat $LUADIR/src/lauxlib.h >> $OUTFILE
cat $LUADIR/src/lualib.h >> $OUTFILE

echo "#ifdef LUA_IMPL" >> $OUTFILE
  # C headers
  echo "typedef struct CallInfo CallInfo;" >> $OUTFILE
  cat $LUADIR/src/llimits.h >> $OUTFILE
  cat $LUADIR/src/lobject.h >> $OUTFILE
  cat $LUADIR/src/lmem.h >> $OUTFILE
  cat $LUADIR/src/ltm.h >> $OUTFILE
  cat $LUADIR/src/lstate.h >> $OUTFILE
  cat $LUADIR/src/lzio.h >> $OUTFILE
  cat $LUADIR/src/lopcodes.h >> $OUTFILE
  cat $LUADIR/src/ldebug.h >> $OUTFILE
  cat $LUADIR/src/ldo.h >> $OUTFILE
  cat $LUADIR/src/lgc.h >> $OUTFILE
  cat $LUADIR/src/lfunc.h >> $OUTFILE
  cat $LUADIR/src/lstring.h >> $OUTFILE
  cat $LUADIR/src/lundump.h >> $OUTFILE
  cat $LUADIR/src/lapi.h >> $OUTFILE
  cat $LUADIR/src/llex.h >> $OUTFILE
  cat $LUADIR/src/ltable.h >> $OUTFILE
  cat $LUADIR/src/lparser.h >> $OUTFILE
  cat $LUADIR/src/lcode.h >> $OUTFILE
  cat $LUADIR/src/lvm.h >> $OUTFILE
  cat $LUADIR/src/lctype.h >> $OUTFILE

  # C sources
  cat $LUADIR/src/lzio.c >> $OUTFILE
  cat $LUADIR/src/lctype.c >> $OUTFILE
  cat $LUADIR/src/lopcodes.c >> $OUTFILE
  cat $LUADIR/src/lmem.c >> $OUTFILE
  cat $LUADIR/src/lundump.c >> $OUTFILE
  cat $LUADIR/src/ldump.c >> $OUTFILE
  cat $LUADIR/src/lstate.c >> $OUTFILE
  cat $LUADIR/src/lgc.c >> $OUTFILE
  cat $LUADIR/src/llex.c >> $OUTFILE
  cat $LUADIR/src/lcode.c >> $OUTFILE
  cat $LUADIR/src/lparser.c >> $OUTFILE
  cat $LUADIR/src/ldebug.c >> $OUTFILE
  cat $LUADIR/src/lfunc.c >> $OUTFILE
  cat $LUADIR/src/lobject.c >> $OUTFILE
  cat $LUADIR/src/ltm.c >> $OUTFILE
  cat $LUADIR/src/lstring.c >> $OUTFILE
  cat $LUADIR/src/ltable.c >> $OUTFILE
  cat $LUADIR/src/ldo.c >> $OUTFILE
  cat $LUADIR/src/lvm.c >> $OUTFILE
  sed -i "/#include \"ljumptab.h\"/r $LUADIR/src/ljumptab.h" $OUTFILE
  cat $LUADIR/src/lapi.c >> $OUTFILE
  cat $LUADIR/src/lauxlib.c >> $OUTFILE
  cat $LUADIR/src/lbaselib.c >> $OUTFILE
  cat $LUADIR/src/lcorolib.c >> $OUTFILE
  cat $LUADIR/src/ldblib.c >> $OUTFILE
  cat $LUADIR/src/liolib.c >> $OUTFILE
  cat $LUADIR/src/lmathlib.c >> $OUTFILE
  cat $LUADIR/src/loadlib.c >> $OUTFILE
  cat $LUADIR/src/loslib.c >> $OUTFILE
  cat $LUADIR/src/lstrlib.c >> $OUTFILE
  cat $LUADIR/src/ltablib.c >> $OUTFILE
  cat $LUADIR/src/lutf8lib.c >> $OUTFILE
  cat $LUADIR/src/linit.c >> $OUTFILE
echo "#endif /* LUA_IMPL */" >> $OUTFILE

cat <<EOF >> $OUTFILE
#ifdef __cplusplus
}
#endif
EOF

# Implement Lua command line utility when LUA_MAKE_LUA is defined
echo "#ifdef LUA_MAKE_LUA" >> $OUTFILE
cat $LUADIR/src/lua.c >> $OUTFILE
echo "#endif /* LUA_MAKE_LUA */" >> $OUTFILE

# Comment all include headers
sed -i 's/#include "\([^"]*\)"/\/\*#include "\1"\*\//' $OUTFILE

cat <<EOF >> $OUTFILE

/*
  MIT License

  Copyright (c) 1994–2026 Lua.org, PUC-Rio.
  Copyright (c) 2020-2026 Eduardo Bart (https://github.com/edubart).

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
EOF
