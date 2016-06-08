
alias cc=i586-mingw32msvc-gcc
cd ../..
cc -c modules/bs/bs.c -o make/win/bin/bs.o -I./include
cc -c modules/str/str.c -o make/win/bin/str.o -I./include
cc -c modules/tk/tk.c -o make/win/bin/tk.o -I./include
cc -c modules/vec/vec.c -o make/win/bin/vec.o -I./include
cc -c objects/complex/complex.c -o make/win/bin/complexobject.o -I./include
cc -c objects/function/function.c -o make/win/bin/functionobject.o -I./include
cc -c objects/list/list.c -o make/win/bin/listobject.o -I./include
cc -c objects/long/long.c -o make/win/bin/longobject.o -I./include
cc -c objects/map/map.c -o make/win/bin/mapobject.o -I./include
cc -c objects/string/string.c -o make/win/bin/stringobject.o -I./include
cc -c objects/set/set.c -o make/win/bin/setobject.o -I./include
cc -c objects/iterable/iterable.c -o make/win/bin/iterable.o -I./include
cc -c modules/system/system.c -o make/win/bin/system.o -I./include
cc -c modules/global/global.c -o make/win/bin/global.o -I./include
cc -c modules/math/math.c -o make/win/bin/math.o -I./include
cc -c modules/cmath/cmath.c -o make/win/bin/cmath.o -I./include
cc -c modules/ma/ma.c -o make/win/bin/ma.o -I./include
cc -c modules/random/random.c -o make/win/bin/random.o -I./include
cc -c vm/vm.c -o make/win/bin/vm.o -I./include

cc -c compiler/compiler.c -o make/win/bin/compiler.o -I./include
cd make/win/bin
cc -o ../moss.exe -lm\
  compiler.o vm.o str.o vec.o bs.o random.o\
  tk.o iterable.o stringobject.o\
  listobject.o mapobject.o longobject.o\
  functionobject.o complexobject.o setobject.o\
  system.o global.o cmath.o math.o ma.o


