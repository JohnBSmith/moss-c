
gcc -c gui.c -g -Wimplicit-function-declaration\
  -o ../../bin/gui.o -I../../include\
  $(pkg-config --cflags gtk+-3.0)
