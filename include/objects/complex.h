#ifndef COMPLEX_H
#define COMPLEX_H

void mf_complex_powcr(mt_object* w, double re, double im, double n);
void mf_complex_powrc(mt_object* w, double x, double re, double im);
void mf_complex_powcc(mt_object* w, double rea, double ima,
  double ren, double imn);
void mf_complex_powrr(mt_object* w, double x, double y);

#endif

