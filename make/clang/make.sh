
alias cc=clang-3.5
cd ../..
cc -g -c modules/str/str.c -o make/clang/bin/str.o -I./include
cc -g -c modules/bs/bs.c -o make/clang/bin/bs.o -I./include
cc -g -c modules/tk/tk.c -o make/clang/bin/tk.o -I./include
cc -g -c modules/vec/vec.c -o make/clang/bin/vec.o -I./include
cc -g -c objects/complex/complex.c -o make/clang/bin/complexobject.o -I./include
cc -g -c objects/function/function.c -o make/clang/bin/functionobject.o -I./include
cc -g -c objects/list/list.c -o make/clang/bin/listobject.o -I./include
cc -g -c objects/longgmp/long.c -o make/clang/bin/longobject.o -I./include
cc -g -c objects/map/map.c -o make/clang/bin/mapobject.o -I./include
cc -g -c objects/string/string.c -o make/clang/bin/stringobject.o -I./include
cc -g -c objects/bstring/bstring.c -o make/clang/bin/bstringobject.o -I./include
cc -g -c objects/iterable/iterable.c -o make/clang/bin/iterable.o -I./include
cc -g -c modules/system/system.c -o make/clang/bin/system.o -I./include
cc -g -c modules/global/global.c -o make/clang/bin/global.o -I./include
cc -g -c modules/math/math.c -o make/clang/bin/math.o -I./include
cc -g -c modules/cmath/cmath.c -o make/clang/bin/cmath.o -I./include
cc -g -c modules/sf/sf.c -o make/clang/bin/sf.o -I./include
cc -g -c modules/na/na.c -o make/clang/bin/na.o -I./include
cc -g -c modules/la/la.c -o make/clang/bin/la.o -I./include
cc -g -c modules/nt/nt.c -o make/clang/bin/nt.o -I./include
cc -g -c modules/sys/sys.c -o make/clang/bin/sys.o -I./include
cc -g -c modules/os/os.c -o make/clang/bin/os.o -I./include
cc -g -c modules/time/time.c -o make/clang/bin/time.o -I./include
cc -g -c modules/gx/gx.c -o make/clang/bin/gx.o -I./include
cc -g -c modules/random/random.c -o make/clang/bin/random.o -I./include
cc -g -c modules/module/module.c -o make/clang/bin/module.o -I./include
cc -g -c modules/format/format.c -o make/clang/bin/format.o -I./include
cc -g -c modules/crypto/crypto.c -o make/clang/bin/crypto.o -I./include
cc -g -c main/main.c -o make/clang/bin/main.o -I./include
cc -g -c vm/vm.c -o make/clang/bin/vm.o -I./include

cc -g -c compiler/compiler.c -o make/clang/bin/compiler.o -I./include
cd make/clang/bin
cc -o ../moss -lm -lreadline -lgmp -lblas -lSDL2 -lSDL2_ttf\
  compiler.o vm.o main.o module.o str.o vec.o bs.o\
  tk.o stringobject.o bstringobject.o iterable.o\
  listobject.o mapobject.o longobject.o\
  functionobject.o complexobject.o\
  system.o global.o math.o cmath.o na.o la.o nt.o sf.o\
  sys.o os.o time.o gx.o random.o format.o crypto.o


