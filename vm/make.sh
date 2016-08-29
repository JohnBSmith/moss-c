
gcc -c -g vm.c -Wimplicit-function-declaration\
  -o ../bin/vm.o -I../include
cd ../bin
gcc -o ../vm/moss\
  compiler.o vm.o vec.o str.o system.o bs.o tk.o global.o\
  random.o math.o cmath.o sys.o time.o na.o sf.o nt.o gx.o os.o\
  listobject.o stringobject.o bstringobject.o mapobject.o\
  functionobject.o complexobject.o longobject.o\
  setobject.o iterable.o format.o la.o\
  module.o main.o\
  -lm -lreadline -lgmp -lSDL2 -lSDL2_ttf -lgsl -lblas

# gui.o $(pkg-config --libs gtk+-3.0)

