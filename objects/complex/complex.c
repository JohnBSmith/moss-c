
#include <objects/object.h>
#include <math.h>

#ifndef M_E
  #define M_E 2.718281828459045
#endif
#ifndef M_PI
  #define M_PI 3.141592653589793
#endif

static
double sgn(double x){
  if(x<0) return -1;
  else if(x==0) return 0;
  else return 1;
}

static
double arg(double re, double im, double r){
  if(r==0) return 0;
  if(im==0 && re<0) return M_PI;
  return sgn(im)*acos(re/r);
}

void mf_complex_powcr(mt_object* w, double re, double im, double n){
  double r = hypot(re,im);
  double p = pow(r,n);
  double phi = arg(re,im,r);
  w->value.c.re=p*cos(n*phi);
  w->value.c.im=p*sin(n*phi);
}

void mf_complex_powrc(mt_object* w, double x, double re, double im){
  double p = pow(x,re);
  double phi = im*log(x);
  w->value.c.re=p*cos(phi);
  w->value.c.im=p*sin(phi);
}

void mf_complex_powcc(mt_object* w, double rea, double ima, double ren, double imn){
  double r = hypot(rea,ima);
  double phi = arg(rea,ima,r);
  double r2 = pow(r,ren)*exp(-phi*imn);
  double phi2 = log(r)*imn+phi*ren;
  w->value.c.re=r2*cos(phi2);
  w->value.c.im=r2*sin(phi2);
}

void mf_complex_powrr(mt_object* w, double x, double y){
  if(x<0){
    double r=pow(-x,y);
    w->value.c.re=r*cos(M_PI*y);
    w->value.c.im=r*sin(M_PI*y);
  }else{
    w->value.c.re=pow(x,y);
    w->value.c.im=0;
  }
}
