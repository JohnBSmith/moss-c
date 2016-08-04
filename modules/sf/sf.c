
// sf: special functions

// == Literature ==
// [1] Richard P. Brent: "Fast Algorithms for High-Precision
// Computation of Elementary Functions", 2006
// [2] Semjon Adlaj: "An eloquent formula for the perimeter
// of an ellipse", Notices of the AMS 59(8) (2012)
// [3] Semjon Adlaj: "An arithmetic-geometric mean of a
// third kind!", 2015

#include <math.h>
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <moss.h>
// #include <gsl/gsl_sf.h>

#define PI M_PI
#define SQRT_PI 1.772453850905516027

#if defined(_WIN32) || defined(__clang__)
  #define CMPLX(x,y) ((x)+(y)*I)
#endif

double mf_float(mt_object* x, int* error);
int mf_complex(mt_object* z, mt_object* x);

static double hzeta(double s, double a);
static double complex chzeta(double complex s, double complex a);
static
double sgngamma(double x){
  if(x>0) return 1;
  return copysign(1,sin(PI*x));
}

static
double binomial_coefficient(double n, double k){
  double s = sgngamma(n+1)*sgngamma(k+1)*sgngamma(n-k+1);
  return s*exp(lgamma(n+1)-lgamma(k+1)-lgamma(n-k+1));
}

// Lanczos approximation
static
double complex cgammapz(double complex z){
  static const double p[]={
    0.99999999999980993, 676.5203681218851, -1259.1392167224028,
    771.32342877765313, -176.61502916214059, 12.507343278686905,
    -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7
  };
  z--;
  double complex y=p[0];
  int i;
  for(i=1; i<9; i++){
    y+=p[i]/(z+i);
  }
  double complex t=z+7.5;
  return sqrt(2*PI)*cpow(t,(z+0.5))*cexp(-t)*y;
}

// Use Euler's reflection formula
double complex mf_cgamma(double complex z){
  if(creal(z)<0.5){
    return PI/(csin(PI*z)*cgammapz(1-z));
  }else{
    return cgammapz(z);
  }
}

static
double Beta(double x, double y){
  return(sgngamma(x)*sgngamma(y)*sgngamma(x+y)
    *exp(lgamma(x)+lgamma(y)-lgamma(x+y))
  );
}

// Use Euler-Maclaurin summation formula applied
// to a partial sum of the harmonic series,
// reflection formula psi(x)=psi(1-x)-pi*cot(pi*x)
// and functional equation psi(x)=psi(x+1)-1/x.
static
double digamma_positive(double x){
  double s=0;
  while(x<10){
    s-=1/x; x++;
  }
  double x2,x4,x8,x10;
  x2=x*x; x4=x2*x2; x8=x4*x4; x10=x8*x2;
  return s+(log(x)-0.5/x-1/(12*x2)
    +1/(120*x4)-1/(252*x4*x2)+1/(240*x8)
    -5/(660*x10)+691/(32760*x10*x2)
  );
}

static
double digamma(double x){
  if(x<0){
    return digamma_positive(1-x)-PI/tan(PI*x);
  }else{
    return digamma_positive(x);
  }
}

static
double complex cdigamma_positive(double complex x){
  double complex s=0;
  while(creal(x)<10){
    s-=1/x; x++;
  }
  double complex x2,x4,x8,x10;
  x2=x*x; x4=x2*x2; x8=x4*x4; x10=x8*x2;
  return s+(clog(x)-0.5/x-1/(12*x2)
    +1/(120*x4)-1/(252*x4*x2)+1/(240*x8)
    -5/(660*x10)+691/(32760*x10*x2)
  );
}

static
double complex cdigamma(double complex x){
  if(creal(x)<0){
    return cdigamma_positive(1-x)-PI/ctan(PI*x);
  }else{
    return cdigamma_positive(x);
  }
}

static
double polygamma(long n, double x){
  if(n>0){
    return cos(PI*(n+1))*tgamma(n+1)*hzeta(n+1,x);
  }else if(n==0){
    return digamma(x);
  }else{
    return NAN;
  }
}

static
double complex cpolygamma(long n, double complex z){
  if(n>0){
    return cos(PI*(n+1))*tgamma(n+1)*chzeta(n+1,z);
  }else if(n==0){
    return cdigamma(z);
  }else{
    return NAN+NAN*I;
  }
}

double cfGamma(double s, double z, int n){
  double x=0;
  int k;
  for(k=n; k>0; k--){
    x=k*(s-k)/(2*k+1+z-s+x);
  }
  return exp(-z)/(1+z-s+x);
}

double complex ccfGamma(double complex s, double complex z, int n){
  double complex x=0;
  int k;
  for(k=n; k>0; k--){
    x=k*(s-k)/(2*k+1+z-s+x);
  }
  return cexp(-z)/(1+z-s+x);
}

double sgamma(double s, double z, int n){
  double y=0;
  double p=1/s;
  int k;
  for(k=1; k<=n; k++){
    y+=p;
    p=p*z/(s+k);
  }
  return y*exp(-z);
}

double complex csgamma(double complex s, double complex z, int n){
  double complex y=0;
  double complex p=1/s;
  int k;
  for(k=1; k<=n; k++){
    y+=p;
    p=p*z/(s+k);
  }
  return y*cexp(-z);
}

static
double incGamma(double s, double z){
  if(s+4<z){
    return pow(z,s)*cfGamma(s,z,40);
  }else{
    return tgamma(s)-pow(z,s)*sgamma(s,z,60);
  }
}

static
double incgamma(double s, double z){
  if(s+4<z){
    return tgamma(s)-pow(z,s)*cfGamma(s,z,40);
  }else{
    return pow(z,s)*sgamma(s,z,60);
  }
}

static
double complex cincGamma(double complex s, double complex z){
  if(creal(s)+4<creal(z)){
    return cpow(z,s)*ccfGamma(s,z,40);
  }else{
    return mf_cgamma(s)-cpow(z,s)*csgamma(s,z,60);
  }
}

static
double complex cincgamma(double complex s, double complex z){
  if(creal(s)+4<creal(z)){
    return mf_cgamma(s)-cpow(z,s)*ccfGamma(s,z,40);
  }else{
    return cpow(z,s)*csgamma(s,z,60);
  }
}

// Use Euler-Maclaurin summation formula
static
double hzeta(double s, double a){
  double y=0;
  int N=18;
  double Npa=N+a;
  int k;
  for(k=0; k<N; k++){
    y+=pow(k+a,-s);
  }
  double s2=s*(s+1)*(s+2);
  double s4=s2*(s+3)*(s+4);
  y+=pow(Npa,1-s)/(s-1)+0.5*pow(Npa,-s);
  y+=s*pow(Npa,-s-1)/12;
  y-=s2*pow(Npa,-s-3)/720;
  y+=s4*pow(Npa,-s-5)/30240;
  y-=s4*(s+5)*(s+6)*pow(Npa,-s-7)/1209600;
  return y;
}

// Use reflection formula (Riemann functional equation)
static
double zeta(double s){
  if(s>-1){
    return hzeta(s,1);
  }else{
    double a = 2*pow(2*PI,s-1)*sin(0.5*PI*s);
    return a*tgamma(1-s)*hzeta(1-s,1);
  }
}

static
double complex chzeta(double complex s, double complex a){
  double complex y=0;
  int N=18;
  double complex Npa=N+a;
  int k;
  for(k=0; k<N; k++){
    y+=cpow(k+a,-s);
  }
  double complex s2=s*(s+1)*(s+2);
  double complex s4=s2*(s+3)*(s+4);
  y+=cpow(Npa,1-s)/(s-1)+0.5*cpow(Npa,-s);
  y+=s*cpow(Npa,-s-1)/12;
  y-=s2*cpow(Npa,-s-3)/720;
  y+=s4*cpow(Npa,-s-5)/30240;
  y-=s4*(s+5)*(s+6)*cpow(Npa,-s-7)/1209600;
  return y;
}

static
double complex czeta(double complex s){
  if(creal(s)>-1){
    return chzeta(s,1);
  }else{
    double complex a = 2*cpow(2*PI,s-1)*csin(PI*s/2);
    return a*mf_cgamma(1-s)*chzeta(1-s,1);
  }
}

static
double ALP_iter(long n, long m, double x){
  if(n==m){
    return SQRT_PI/tgamma(0.5-n)*pow(2*sqrt(1-x*x),n);
  }else if(n-1==m){
    return x*(2.0*n-1)*ALP_iter(m,m,x);
  }else{
    double a=ALP_iter(m,m,x);
    double b=ALP_iter(m+1,m,x);
    double h;
    long k;
    for(k=m+2; k<=n; k++){
      h=((2.0*k-1)*x*b-(k-1.0+m)*a)/(k-m);
      a=b; b=h;
    }
    return b;
  }
}

static
double ALP(long n, long m, double x){
  if(n<0) n=-n-1;
  if(labs(m)>n){
    return 0;
  }else if(m<0){
    return((m%2==0?1:-1)
      *exp(lgamma(n+m+1)-lgamma(n-m+1))
      *ALP_iter(n,-m,x)
    );
  }else{
    return ALP_iter(n,m,x);
  }
}

static
double Hermite(long n, double x){
  if(n==0){
    return 1;
  }else if(n==1){
    return 2*x;
  }else{
    double a,b,h;
    long k;
    a=1; b=2*x;
    for(k=2; k<=n; k++){
      h=2*x*b-2*(n-1)*a;
      a=b; b=h;
    }
    return b;
  }
}

static
double DHermite(long n, long m, double x){
  if(m==0){
    return Hermite(n,x);
  }else if(n<m){
    return 0;
  }else if(m==1){
    return 2*n*Hermite(n-1,x);
  }else{
    return 2*n*DHermite(n-1,m-1,x);
  }
}

static
double PT(long n, double x){
  if(n==0){
    return 1;
  }else if(n==1){
    return x;
  }else{
    double a,b,h;
    a=1; b=x;
    long k;
    for(k=2; k<=n; k++){
      h=2*x*b-a;
      a=b; b=h;
    }
    return b;
  }
}

static
double PU(long n, double x){
  if(n==0){
    return 1;
  }else if(n==1){
    return 2*x;
  }else{
    double a,b,h;
    a=1; b=2*x;
    long k;
    for(k=2; k<=n; k++){
      h=2*x*b-a;
    }
    return b;
  }
}

// Arithmetic-geometric mean
static
double agm(double a, double b){
  double ah,bh;
  int i;
  for(i=0; i<20; i++){
    ah = (a+b)/2;
    bh = sqrt(a*b);
    a=ah; b=bh;
    if(fabs(a-b)<1E-15) break;
  }
  return a;
}

static
double complex cagm(double complex a, double complex b){
  double complex ah,bh;
  int i;
  for(i=0; i<20; i++){
    ah = (a+b)/2;
    bh = csqrt(a*b);
    a=ah; b=bh;
    if(cabs(a-b)<1E-15) break;
  }
  return a;
}

// Modified arithmetic-geometric mean, see
// Semjon Adlaj: "An eloquent formula for the perimeter
// of an ellipse", Notices of the AMS 59(8) (2012), p. 1094-1099
double magm(double x, double y){
  double z=0;
  double xh,yh,zh,r;
  int i;
  for(i=0; i<20; i++){
    xh=0.5*(x+y);
    r=sqrt((x-z)*(y-z));
    yh=z+r; zh=z-r;
    x=xh; y=yh; z=zh;
    if(fabs(x-y)<2E-15) break;
  }
  return x;
}

// Branch cut Re(z)>0, Im(z)=0
double complex csqrta(double complex z){
  return cimag(z)>=0? csqrt(z): -csqrt(z);
}

// Branch cut Re(z)>0, Im(z)=0
double complex csqrtb(double complex z){
  return cimag(z)<=0? csqrt(z): -csqrt(z);
}

double complex cmagm(double complex x, double complex y){
  double complex z=0;
  double complex xh,yh,zh,r;
  double complex (*root)(double complex);
  if(signbit(cimag(y))){
    root = csqrtb;
  }else{
    root = csqrta;
  }
  
  int i;
  for(i=0; i<20; i++){
    xh=0.5*(x+y);
    r=root((x-z)*(y-z));
    yh=z+r; zh=z-r;
    x=xh; y=yh; z=zh;
    if(cabs(x-y)<2E-15) break;
  }
  return x;
}

static
double eiK(double m){
  return 0.5*PI/agm(1,sqrt(1-m));
}

static
double complex ceiK(double complex m){
  return 0.5*PI/cagm(1,csqrt(1-m));
}

double eiE(double m){
  return 0.5*PI*magm(1,1-m)/agm(1,sqrt(1-m));
}

double complex ceiE(double complex m){
  if(m==1) return 1;
  return 0.5*PI*cmagm(1,1-m)/cagm(1,csqrt(1-m));
}

static
double Fmn(long m, long n, double* a, double* b, double x){
  long i,k;
  double s=1,p=1;
  for(k=0; k<100; k++){
    p=p*x/(k+1);
    for(i=0; i<m; i++) p=p*(a[i]+k);
    for(i=0; i<n; i++) p=p/(b[i]+k);
    s+=p;
  }
  return s;
}

static
double complex cerf_series(double complex z, int N){
  double complex y=z;
  double p=1;
  double m;
  int n;
  for(n=1; n<=N; n++){
    p=-p*n; m=2*n+1;
    y+=cpow(z,m)/(m*p);
  }
  return y*2/SQRT_PI;
}

static
double complex cerf_cf(double complex z, int N){
  double complex t=0;
  int k;
  for(k=N; k>=1; k--){
    t=k/((k%2==0?z:2*z)+t);
  }
  return 1-cexp(-z*z)/(SQRT_PI*(z+t));
}

static
double complex cerf_cf2(double complex z, int N){
  double complex t=1, z2=z*z;
  int k;
  for(k=N; k>=1; k--){
    t=(2*k-1)+(k%2==0?2:-2)*k*z2/t;
  }
  return 2*z/SQRT_PI*cexp(-z2)/t;
}

static
double complex mf_cerf(double complex z){
  double s;
  if(creal(z)<0){
    z=-z; s=-1;
  }else{
    s=1;
  }
  if(cabs(z)<2){
    return s*cerf_series(z,30);
  }else if(creal(z)<2 && fabs(cimag(z))<6){
    return s*cerf_cf2(z,60);
  }else{
    return s*cerf_cf(z,40);
  }
}


static
int sf_bc(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"bc");
    return 1;
  }
  int e=0;
  double n = mf_float(v+1,&e);
  if(e){
    mf_type_error1("in bc(n,k): cannot convert n (type: %s) to float.",v+1);
    return 1;
  }
  double k = mf_float(v+2,&e);
  if(e){
    mf_type_error1("in bc(n,k): cannot convert k (type: %s) to float.",v+2);
    return 1;
  }
  x->type=mv_float;
  x->value.f = binomial_coefficient(n,k);
  return 0;
}

static
int sf_gamma(mt_object* x, int argc, mt_object* v){
  if(argc==1){
    if(v[1].type==mv_complex){
      double complex w = mf_cgamma(CMPLX(v[1].value.c.re,v[1].value.c.im));
      x->type=mv_complex;
      x->value.c.re=creal(w);
      x->value.c.im=cimag(w);
      return 0;
    }else{
      int e=0;
      double t = mf_float(v+1,&e);
      if(e){
        mf_type_error1("in sf.gamma(z): cannot convert z (type: %s) to complex.",v+1);
        return 1;
      }
      x->type=mv_float;
      x->value.f = tgamma(t);
      return 0;
    }
  }else if(argc==2){
    mt_object s,z;
    if(v[1].type==mv_complex || v[2].type==mv_complex){
      mf_complex(&s,v+1);
      mf_complex(&z,v+2);
      double complex w = cincgamma(
        CMPLX(s.value.c.re,s.value.c.im),
        CMPLX(z.value.c.re,z.value.c.im)
      );
      x->type=mv_complex;
      x->value.c.re=creal(w);
      x->value.c.im=cimag(w);
      return 0;
    }else{
      int e=0;
      s.value.f=mf_float(v+1,&e);
      z.value.f=mf_float(v+2,&e);
      x->type=mv_float;
      x->value.f=incgamma(s.value.f,z.value.f);
      return 0;
    }
  }else{
    mf_argc_error(argc,1,2,"sf.gamma");
    return 1;
  }
}

static
int sf_Gamma(mt_object* x, int argc, mt_object* v){
  if(argc==2){
    mt_object s,z;
    if(v[1].type==mv_complex || v[2].type==mv_complex){
      mf_complex(&s,v+1);
      mf_complex(&z,v+2);
      double complex w = cincGamma(
        CMPLX(s.value.c.re,s.value.c.im),
        CMPLX(z.value.c.re,z.value.c.im)
      );
      x->type=mv_complex;
      x->value.c.re=creal(w);
      x->value.c.im=cimag(w);
      return 0;
    }else{
      int e=0;
      s.value.f=mf_float(v+1,&e);
      z.value.f=mf_float(v+2,&e);
      x->type=mv_float;
      x->value.f=incGamma(s.value.f,z.value.f);
      return 0;
    }
  }else{
    mf_argc_error(argc,2,2,"sf.Gamma");
    return 1;
  }
}

int sf_Beta(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"Beta");
    return 1; 
  }
  int e=0;
  double a=mf_float(v+1,&e);
  if(e){
    mf_type_error1("in Beta(x,y): cannot convert x (type: %s) to flaot.",v+1);
    return 1;
  }
  double b=mf_float(v+2,&e);
  if(e){
    mf_type_error1("in Beta(x,y): cannot convert y (type: %s) to flaot.",v+2);
    return 1;
  }
  x->type=mv_float;
  x->value.f=Beta(a,b);
  return 0;
}

static
int sf_digamma(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"digamma");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = cdigamma(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t=mf_float(v+1,&e);
    if(e){
      mf_type_error1("in digamma(x): cannot convert x (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=digamma(t);
    return 0;
  }
}

static
int sf_polygamma(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"polygamma");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in polygamma(n,z): n (type: %s) is not an integer.",v+1);
    return 1;
  }
  long n = v[1].value.i;
  if(v[2].type==mv_complex){
    double complex w = cpolygamma(n,CMPLX(v[2].value.c.re,v[2].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+2,&e);
    if(e){
      mf_type_error1("in polygamma(n,z): z (type: %s) is not a complex number.",v+2);
      return 1;
    }
    x->type=mv_float;
    x->value.f=polygamma(n,t);
    return 0;
  }
}

static
int sf_zeta(mt_object* x, int argc, mt_object* v){
  int e;
  if(argc==1){
    if(v[1].type==mv_complex){
      double complex w = czeta(CMPLX(v[1].value.c.re,v[1].value.c.im));
      x->type=mv_complex;
      x->value.c.re = creal(w);
      x->value.c.im = cimag(w);
      return 0;
    }else{
      e=0;
      double t = mf_float(v+1,&e);
      if(e){
        mf_type_error1("in zeta(s): cannot convert s (type: %s) to complex.",v+1);
        return 1;
      }
      x->type=mv_float;
      x->value.f = zeta(t);
      return 0;
    }
  }else if(argc==2){
    mt_object z,a;
    if(v[1].type==mv_complex || v[2].type==mv_complex){
      mf_complex(&z,v+1);
      mf_complex(&a,v+2);
      double complex w = chzeta(
        CMPLX(z.value.c.re,z.value.c.im),
        CMPLX(a.value.c.re,a.value.c.im)
      );
      x->type=mv_complex;
      x->value.c.re=creal(w);
      x->value.c.im=cimag(w);
      return 0;
    }else{
      e=0;
      z.value.f=mf_float(v+1,&e);
      a.value.f=mf_float(v+2,&e);
      x->type=mv_float;
      x->value.f=hzeta(z.value.f,a.value.f);
      return 0;
    }
  }else{
    mf_argc_error(argc,1,2,"zeta");
    return 1;
  }
}

static
int sf_BernoulliB(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sf.B");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e){
    mf_type_error1("in sf.B(n): cannot convert n (type: %s) to float.",v+1);
    return 1;
  }
  x->type=mv_float;
  if(t==0){
    x->value.f=1;
  }else{
    x->value.f = -t*zeta(1-t);
  }
  return 0;
}

static
int sf_PP(mt_object* x, int argc, mt_object* v){
  if(argc==2){
    if(v[1].type!=mv_int){
      mf_type_error1("in sf.PP(n,x): n (type: %s) is not an integer.",v+1);
      return 1;
    }
    int n=(int)v[1].value.i;
    int e=0;
    double t=mf_float(v+2,&e);
    if(e){
      mf_type_error1("in sf.PP(n,x): cannot convert x (type: %s) to float.",v+2);
      return 1;
    }
    x->type=mv_float;
    x->value.f=ALP(n,0,t);
    return 0;
  }else if(argc==3){
    if(v[1].type!=mv_int){
      mf_type_error1("in sf.PP(n,m,x): n (type: %s) is not an integer.",v+1);
      return 1;
    }
    if(v[2].type!=mv_int){
      mf_type_error1("in sf.PP(n,m,x): m (type: %s) is not an integer.",v+2);
      return 1;
    }
    long n = v[1].value.i;
    long m = v[2].value.i;
    int e=0;
    double t=mf_float(v+3,&e);
    if(e){
      mf_type_error1("in sf.PP(n,m,x): cannot convert x (type: %s) to float.",v+3);
      return 1;
    }
    x->type=mv_float;
    x->value.f=ALP(n,m,t);
    return 0;
  }else{
    mf_argc_error(argc,2,3,"sf.PP");
    return 1;
  }
}

static
int sf_PH(mt_object* x, int argc, mt_object* v){
  if(argc==2){
    if(v[1].type!=mv_int){
      mf_type_error1("in sf.PH(n,x): n (type: %s) is not an integer.",v+1);
      return 1;
    }
    int n=(int)v[1].value.i;
    int e=0;
    double t=mf_float(v+2,&e);
    if(e){
      mf_type_error1("in sf.PH(n,x): cannot convert x (type: %s) to float.",v+2);
      return 1;
    }
    x->type=mv_float;
    x->value.f=Hermite(n,t);
    return 0;
  }else if(argc==3){
    if(v[1].type!=mv_int){
      mf_type_error1("in sf.PH(n,m,x): n (type: %s) is not an integer.",v+1);
      return 1;
    }
    if(v[2].type!=mv_int){
      mf_type_error1("in sf.PH(n,m,x): m (type: %s) is not an integer.",v+2);
      return 1;
    }
    long n = v[1].value.i;
    long m = v[2].value.i;
    int e=0;
    double t=mf_float(v+3,&e);
    if(e){
      mf_type_error1("in sf.PH(n,m,x): cannot convert x (type: %s) to float.",v+3);
      return 1;
    }
    x->type=mv_float;
    x->value.f=DHermite(n,m,t);
    return 0;
  }else{
    mf_argc_error(argc,2,3,"sf.PH");
    return 1;
  }
}

static
int sf_PT(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"sf.PT");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in sf.PT(n,x): n (type: %s) is not an integer.",v+1);
    return 1;
  }
  long n=v[1].value.i;
  int e=0;
  double t=mf_float(v+2,&e);
  if(e){
    mf_type_error1("in sf.PT(n,x): cannot convert x (type: %s) to float.",v+2);
    return 1;
  }
  x->type=mv_float;
  x->value.f=PT(n,t);
  return 0;
}

static
int sf_PU(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"sf.PU");
    return 1;
  }
  if(v[1].type!=mv_int){
    mf_type_error1("in sf.PU(n,x): n (type: %s) is not an integer.",v+1);
    return 1;
  }
  long n=v[1].value.i;
  int e=0;
  double t=mf_float(v+2,&e);
  if(e){
    mf_type_error1("in sf.PU(n,x): cannot convert x (type: %s) to float.",v+2);
    return 1;
  }
  x->type=mv_float;
  x->value.f=PU(n,t);
  return 0;
}

static
int sf_eiK(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sf.eiK");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ceiK(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sf.eiK(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=eiK(t);
    return 0;
  }
}

static
int sf_eiE(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sf.eiE");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = ceiE(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sf.eiE(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=eiE(t);
    return 0;
  }
}

static
int sf_erf(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"sf.erf");
    return 1;
  }
  if(v[1].type==mv_complex){
    double complex w = mf_cerf(CMPLX(v[1].value.c.re,v[1].value.c.im));
    x->type=mv_complex;
    x->value.c.re=creal(w);
    x->value.c.im=cimag(w);
    return 0;
  }else{
    int e=0;
    double t = mf_float(v+1,&e);
    if(e){
      mf_type_error1("in sf.erf(z): cannot convert z (type: %s) to complex.",v+1);
      return 1;
    }
    x->type=mv_float;
    x->value.f=erf(t);
    return 0;
  }
}

static
int sf_Fmn(mt_object* x, int argc, mt_object* v){
  if(argc!=3){
    mf_argc_error(argc,3,3,"sf.F");
    return 1;
  }
  if(v[1].type!=mv_list){
    mf_type_error1("in sf.F(a,b,x): a (type: %s) is not a list.",v+1);
    return 1;
  }
  if(v[2].type!=mv_list){
    mf_type_error1("in sf.F(a,b,x): b (type: %s) is not a list.",v+2);
    return 1;
  }
  mt_list* La=(mt_list*)v[1].value.p;
  mt_list* Lb=(mt_list*)v[2].value.p;
  int e=0;
  double t=mf_float(v+3,&e);
  if(e){
    mf_type_error1("in sf.F(a,b,x): cannot convert x (type: %s) to float.",v+3);
    return 1;
  }
  double* a=mf_malloc(sizeof(double)*La->size);
  double* b=mf_malloc(sizeof(double)*Lb->size);
  long i;
  for(i=0; i<La->size; i++){
    a[i]=mf_float(La->a+i,&e);
    if(e){
      mf_type_error1("in sf.F(a,b,x): cannot convert a[k] (type: %s) to float.",La->a+i);
      mf_free(a);
      return 1;
    }
  }
  for(i=0; i<Lb->size; i++){
    b[i]=mf_float(Lb->a+i,&e);
    if(e){
      mf_type_error1("in sf.F(a,b,x): cannot convert b[k] (type: %s) to float.",Lb->a+i);
      mf_free(a);
      mf_free(b);
      return 1;
    }
  }
  x->type=mv_float;
  x->value.f=Fmn(La->size,Lb->size,a,b,t);
  mf_free(a);
  mf_free(b);
  return 0;
}

static
int sf_test(mt_object* x, int argc, mt_object* v){
  int e;
  if(argc==1){
    if(v[1].type==mv_complex){
      double complex w = czeta(CMPLX(v[1].value.c.re,v[1].value.c.im));
      x->type=mv_complex;
      x->value.c.re = creal(w);
      x->value.c.im = cimag(w);
      return 0;
    }else{
      e=0;
      double t = mf_float(v+1,&e);
      if(e){
        mf_type_error1("in test(s): cannot convert s (type: %s) to complex.",v+1);
        return 1;
      }
      x->type=mv_float;
      // x->value.f = gsl_sf_psi(t);
      return 0;
    }
  }else if(argc==2){
    if(v[1].type==mv_complex || v[2].type==mv_complex){
      mt_object z,a;
      if(mf_complex(&z,v+1)){
        mf_type_error1("in test(x,y): cannot convert x (type: %s) to complex.",v+1);
        return 1;
      }
      if(mf_complex(&a,v+2)){
        mf_type_error1("in test(x,y): cannot convert y (type: %s) to complex.",v+2);
        return 1;
      }
      double complex w = chzeta(
        CMPLX(z.value.c.re,z.value.c.im),
        CMPLX(a.value.c.re,a.value.c.im)
      );
      x->type=mv_complex;
      x->value.c.re=creal(w);
      x->value.c.im=cimag(w);
      return 0;
    }else{
      double rz,ra;
      e=0;
      rz=mf_float(v+1,&e);
      if(e){
        mf_type_error1("in test(x,y): cannot convert x (type: %s) to float.",v+1);
        return 1;
      }
      ra=mf_float(v+2,&e);
      if(e){
        mf_type_error1("in test(x,y): cannot convert y (type: %s) to float.",v+2);
        return 1;
      }
      x->type=mv_float;
      if(ra>0 && rz>0){
        // x->value.f=tgamma(rz)*gsl_sf_gamma_inc_P(rz,ra);
      }else if(ra>0){
        // x->value.f=tgamma(rz)-gsl_sf_gamma_inc(rz,ra);
      }else{
        x->value.f=NAN;
      }
      return 0;
    }
  }else{
    mf_argc_error(argc,1,2,"test");
    return 1;
  }
}

mt_table* mf_sf_load(){
  mt_table* sf = mf_table(NULL);
  sf->m=mf_empty_map();
  mt_map* m=sf->m;
  mf_insert_function(m,2,2,"bc",sf_bc);
  mf_insert_function(m,1,2,"gamma",sf_gamma);
  mf_insert_function(m,2,2,"Gamma",sf_Gamma);
  mf_insert_function(m,2,2,"Beta",sf_Beta);
  mf_insert_function(m,1,1,"digamma",sf_digamma);
  mf_insert_function(m,2,2,"polygamma",sf_polygamma);
  mf_insert_function(m,1,2,"zeta",sf_zeta);
  mf_insert_function(m,1,1,"B",sf_BernoulliB);
  mf_insert_function(m,2,3,"PP",sf_PP);
  mf_insert_function(m,2,3,"PH",sf_PH);
  mf_insert_function(m,2,2,"PT",sf_PT);
  mf_insert_function(m,2,2,"PU",sf_PU);
  mf_insert_function(m,1,1,"eiK",sf_eiK);
  mf_insert_function(m,1,1,"eiE",sf_eiE);
  mf_insert_function(m,1,1,"erf",sf_erf);
  mf_insert_function(m,3,3,"F",sf_Fmn);
  mf_insert_function(m,1,2,"test",sf_test);
  m->frozen=1;
  return sf;
}
