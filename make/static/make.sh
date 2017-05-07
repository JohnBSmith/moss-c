
alias cc="gcc -g -Wpedantic -Wall -Wno-unused-function -Wextra -Wno-unused-parameter -D MOSS_LEVEL0"
cd ../..
cc -c modules/bs/bs.c -o make/static/bin/bs.o -I./include
cc -c modules/str/str.c -o make/static/bin/str.o -I./include
cc -c modules/tk/tk.c -o make/static/bin/tk.o -I./include
cc -c modules/vec/vec.c -o make/static/bin/vec.o -I./include
cc -c objects/complex/complex.c -o make/static/bin/complexobject.o -I./include
cc -c objects/function/function.c -o make/static/bin/functionobject.o -I./include
cc -c objects/list/list.c -o make/static/bin/listobject.o -I./include
cc -c objects/long/long.c -o make/static/bin/longobject.o -I./include
cc -c objects/map/map.c -o make/static/bin/mapobject.o -I./include
cc -c objects/string/string.c -o make/static/bin/stringobject.o -I./include
cc -c objects/bstring/bstring.c -o make/static/bin/bstringobject.o -I./include
cc -c objects/iterable/iterable.c -o make/static/bin/iterable.o -I./include
cc -c modules/system/system.c -o make/static/bin/system.o -I./include
cc -c modules/global/global.c -o make/static/bin/global.o -I./include
cc -c modules/math/math.c -o make/static/bin/math.o -I./include
cc -c modules/cmath/cmath.c -o make/static/bin/cmath.o -I./include
cc -c modules/sf/sf.c -o make/static/bin/sf.o -I./include
cc -c modules/na/na.c -o make/static/bin/na.o -I./include
cc -c modules/nt/nt.c -o make/static/bin/nt.o -I./include
cc -c modules/random/random.c -o make/static/bin/random.o -I./include
cc -c modules/module/module.c -o make/static/bin/module.o -I./include
cc -c modules/sys/sys.c -o make/static/bin/sys.o -I./include
cc -c modules/format/format.c -o make/static/bin/format.o -I./include
cc -c modules/regex/regex.c -o make/static/bin/regex.o -I./include
cc -c modules/la/la.c -o make/static/bin/la.o -I./include
cc -c modules/crypto/crypto.c -o make/static/bin/crypto.o -I./include
cc -c modules/time/time.c -o make/static/bin/time.o -I./include
cc -c modules/crypto/crypto.c -o make/static/bin/crypto.o -I./include
cc -c modules/os/os.c -o make/static/bin/os.o -I./include
cc -c main/main.c -o make/static/bin/main.o -I./include
cc -c vm/vm.c -o make/static/bin/vm.o -I./include


cc -c compiler/compiler.c -o make/static/bin/compiler.o -I./include
cd make/static/bin
cc -static -o ../moss\
  compiler.o vm.o vec.o str.o system.o bs.o tk.o global.o\
  random.o math.o cmath.o sys.o time.o na.o sf.o nt.o os.o\
  listobject.o stringobject.o bstringobject.o mapobject.o\
  functionobject.o complexobject.o longobject.o\
  iterable.o format.o la.o regex.o crypto.o\
  module.o main.o\
  -lm -lreadline



