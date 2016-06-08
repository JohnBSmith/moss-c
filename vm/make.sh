
gcc -c -g vm.c -Wimplicit-function-declaration\
  -o ../bin/vm.o -I../include
cd ../bin
gcc -o ../vm/moss\
  compiler.o vm.o vec.o str.o system.o bs.o tk.o global.o\
  random.o mmath.o cmath.o sys.o time.o ma.o gx.o os.o\
  listobject.o stringobject.o mapobject.o functionobject.o\
  complexobject.o longobject.o setobject.o iterable.o\
  -lm -lreadline -lgmp -lSDL2









