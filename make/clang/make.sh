
alias cc=clang-3.5
cd ../..
cc -c modules/str/str.c -o make/clang/bin/str.o -I./include
cc -c modules/bs/bs.c -o make/clang/bin/bs.o -I./include
cc -c modules/tk/tk.c -o make/clang/bin/tk.o -I./include
cc -c modules/vec/vec.c -o make/clang/bin/vec.o -I./include
cc -c objects/complex/complex.c -o make/clang/bin/complexobject.o -I./include
cc -c objects/function/function.c -o make/clang/bin/functionobject.o -I./include
cc -c objects/list/list.c -o make/clang/bin/listobject.o -I./include
cc -c objects/longgmp/long.c -o make/clang/bin/longobject.o -I./include
cc -c objects/map/map.c -o make/clang/bin/mapobject.o -I./include
cc -c objects/string/string.c -o make/clang/bin/stringobject.o -I./include
cc -c objects/set/set.c -o make/clang/bin/setobject.o -I./include
cc -c objects/iterable/iterable.c -o make/clang/bin/iterable.o -I./include
cc -c modules/system/system.c -o make/clang/bin/system.o -I./include
cc -c modules/global/global.c -o make/clang/bin/global.o -I./include
cc -c modules/math/math.c -o make/clang/bin/mmath.o -I./include
cc -c modules/cmath/cmath.c -o make/clang/bin/cmath.o -I./include
cc -c modules/ma/ma.c -o make/clang/bin/ma.o -I./include
cc -c modules/sys/sys.c -o make/clang/bin/sys.o -I./include
cc -c modules/os/os.c -o make/clang/bin/os.o -I./include
cc -c modules/time/time.c -o make/clang/bin/time.o -I./include
cc -c modules/gx/gx.c -o make/clang/bin/gx.o -I./include
cc -c modules/random/random.c -o make/clang/bin/random.o -I./include
cc -c vm/vm.c -o make/clang/bin/vm.o -I./include

cc -c compiler/compiler.c -o make/clang/bin/compiler.o -I./include
cd make/clang/bin
cc -o ../moss -lm -lreadline -lgmp -lSDL2\
  compiler.o vm.o str.o vec.o bs.o\
  tk.o stringobject.o iterable.o\
  listobject.o mapobject.o longobject.o\
  functionobject.o complexobject.o setobject.o\
  system.o global.o cmath.o mmath.o ma.o\
  sys.o os.o time.o gx.o random.o


