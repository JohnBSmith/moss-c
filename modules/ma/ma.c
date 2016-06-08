
// ma: mathematical analysis
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <moss.h>

mt_tuple* mf_raw_tuple(long n);
extern mt_function* function_self;
double mf_float(mt_object* x, int* error);

static
mt_object dd_argv[2];

static
double double_double_apply(mt_function* f, double x, int* error){
  dd_argv[1].value.f=x;
  mt_object y;
  if(mf_call(f,&y,1,dd_argv)){
    *error=1;
    return NAN;
  }
  double u=mf_float(&y,error);
  if(error) mf_dec_refcount(&y);
  return u;
}

double invab(mt_function* f, double x, double a, double b, int* e){
  double y,ya,yb,m,s,a1=a,b1=b;
  ya=double_double_apply(f,a,e);
  if(*e) return NAN;
  yb=double_double_apply(f,b,e);
  if(*e) return NAN;
  s = copysign(1,yb-ya);
  if(s==0 || isnan(s)) s=1;
  int k;
  for(k=0; k<60; k++){
    m=(a+b)/2;
    y=double_double_apply(f,m,e);
    if(*e) return NAN;
    if(s*(y-x)<0) a=m;
    else b=m;
  }
  if(fabs(m-a1)<1E-8) return NAN;
  if(fabs(m-b1)<1E-8) return NAN;
  return m;
}

static
int ma_inv(mt_object* x, int argc, mt_object* v){
  if(argc!=4){
    mf_argc_error(argc,4,4,"inv");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in inv(f,x,a,b): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[1].value.p;
  int e=0;
  double t=mf_float(v+2,&e);
  if(e){
    mf_type_error("Type error in inv(f,x,a,b): cannot convert x to float.");
    return 1;
  }
  double a=mf_float(v+3,&e);
  if(e){
    mf_type_error("Type error in inv(f,x,a,b): cannot convert a to float.");
    return 1;
  }
  double b=mf_float(v+4,&e);
  if(e){
    mf_type_error("Type error in inv(f,x,a,b): cannot convert b to float.");
    return 1;
  }
  double y=invab(f,t,a,b,&e);
  if(e){
    mf_type_error("Type error in inv(f,x,a,b): cannot convert return value of f to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=y;
  return 0;
}

static
int ma_diff(mt_object* x, int argc, mt_object* v){
  if(argc!=2){
    mf_argc_error(argc,2,2,"diff");
    return 1;
  }
  if(v[1].type!=mv_function){
    mf_type_error("Type error in diff(f,x): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[1].value.p;
  int e;
  double t=mf_float(v+2,&e);
  if(e){
    mf_type_error("Type error in diff(f,x): cannot convert x to float.");
    return 1;
  }
  double y1=double_double_apply(f,t+0.001,&e);
  if(e){
    mf_type_error("Type error in diff(f,x): cannot convert return value of f to float.");
    return 1;
  }
  double y2=double_double_apply(f,t-0.001,&e);
  if(e){
    mf_type_error("Type error in diff(f,x): cannot convert return value of f to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=(y1-y2)/0.002;
  return 0;
}

static
double g32[32][2]={
  {0.0965400885147278, -0.0483076656877383},
  {0.0965400885147278,  0.0483076656877383},
  {0.0956387200792749, -0.1444719615827965},
  {0.0956387200792749,  0.1444719615827965},

  {0.0938443990808046, -0.2392873622521371},
  {0.0938443990808046,  0.2392873622521371},
  {0.0911738786957639, -0.3318686022821277},
  {0.0911738786957639,  0.3318686022821277},

  {0.0876520930044038, -0.4213512761306353},
  {0.0876520930044038,  0.4213512761306353},
  {0.0833119242269467, -0.5068999089322294},
  {0.0833119242269467,  0.5068999089322294},

  {0.0781938957870703, -0.5877157572407623},
  {0.0781938957870703,  0.5877157572407623},
  {0.0723457941088485, -0.6630442669302152},
  {0.0723457941088485,  0.6630442669302152},

  {0.0658222227763618, -0.7321821187402897},
  {0.0658222227763618,  0.7321821187402897},
  {0.0586840934785355, -0.7944837959679424},
  {0.0586840934785355,  0.7944837959679424},

  {0.0509980592623762, -0.8493676137325700},
  {0.0509980592623762,  0.8493676137325700},
  {0.0428358980222267, -0.8963211557660521},
  {0.0428358980222267,  0.8963211557660521},

  {0.0342738629130214, -0.9349060759377397},
  {0.0342738629130214,  0.9349060759377397},
  {0.0253920653092621, -0.9647622555875064},
  {0.0253920653092621,  0.9647622555875064},

  {0.0162743947309057, -0.9856115115452684},
  {0.0162743947309057,  0.9856115115452684},
  {0.0070186100094701, -0.9972638618494816},
  {0.0070186100094701,  0.9972638618494816}
};

static
double gauss(mt_function* f, double a, double b, int n, int* error){
  double y,s,sj,h,aj,bj,p,q;
  int i,j;
  h = (b-a)/n;
  s=0;
  for(j=0; j<n; j++){
    aj=a+j*h;
    bj=a+(j+1)*h;
    p=0.5*(bj-aj);
    q=0.5*(aj+bj);
    sj=0;
    for(i=0; i<32; i++){
      y = double_double_apply(f,p*g32[i][1]+q,error);
      if(*error) return NAN;
      sj+=g32[i][0]*y;
    }
    s+=p*sj;
  }
  return s;
}

static
int ma_int(mt_object* x, int argc, mt_object* v){
  if(argc!=3){
    mf_argc_error(argc,3,3,"int");
    return 1;
  }
  if(v[3].type!=mv_function){
    mf_type_error("Type error in int(a,b,f): f is not a function.");
    return 1;
  }
  mt_function* f = (mt_function*)v[3].value.p;
  double a,b;
  int e=0;
  a = mf_float(v+1,&e);
  if(e){
    mf_type_error("Type error in int(a,b,f): cannot convert a to float.");
    return 1;
  }
  b = mf_float(v+2,&e);
  if(e){
    mf_type_error("Type error in int(a,b,f): cannot convert b to float.");
    return 1;
  }
  double y=gauss(f,a,b,2,&e);
  if(e){
    mf_type_error("Type error in int(a,b,f): cannot convert return value of f to float.");
    return 1;
  }
  x->type=mv_float;
  x->value.f=y;
  return 0;
}

typedef struct{
  long refcount;
  // TODO: object_head
  void (*del)(void*);
  long size;
  double* ax;
  double* ay;
} mt_double2_array;

static
void double2_array_delete(void* p){
  mt_double2_array* a=p;
  free(a->ax);
  free(a->ay);
}

static
double interpolate(double x, long size, double* ax, double* ay){
  if(size==0) return NAN;
  long a=0;
  long b=size-1;
  long i;
  if(x<ax[a] || x>ax[b]){
    return NAN;
  }
  while(a<=b){
    i = a+(b-a)/2;
    if(x<ax[i]){
      b=i-1;
    }else{
      a=i+1;
    }
  }
  i=a;
  if(i>0){
    return (ay[i]-ay[i-1])/(ax[i]-ax[i-1])*(x-ax[i])+ay[i];
  }else{
    return NAN;
  }
}

static
int get_value(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"function returned by interpolate.");
    return 1;
  }
  int e=0;
  double t = mf_float(v+1,&e);
  if(e) return 1;
  // mt_double2_array* a = (mt_double2_array*)function_self->context->a[0];
  // double y = interpolate(t,a->size,a->ax,a->ay);
  // return new_float(y);
  x->type=mv_float;
  x->value.f=NAN; // TODO
  return 0;
}

/*
static
Object* ma_interpolate(int argc, Object** v){
  if(argc!=1){
    mf_argc_error(argc,1,"interpolate");
    return NULL;
  }
  if(v[1]->type!=mv_list){
    mf_type_error("Type error in interpolate(a): a is not a list.");
    return NULL;
  }
  List* list = (List*)v[1];
  long size = list->size;
  long i;
  Object** a=list->a;
  List* t;
  for(i=0; i<size; i++){
    if(a[i]->type!=mv_list){
      mf_type_error("Type error in interpolate: excepted list of pairs.");
      return NULL;
    }
    t = (List*)a[i];
    if(t->size!=2){
      mf_type_error("Type error in interpolate: expected list of pairs.");
      return NULL;
    }
    if(t->a[0]->type!=mv_float && t->a[0]->type!=mv_int){
      mf_type_error("Type error in interpolate: cannot convert x in [x,y] to float.");
      return NULL;
    }
    if(t->a[1]->type!=mv_float && t->a[1]->type!=mv_int){
      mf_type_error("Type error in interpolate: cannot convert y in [x,y] to float.");
      return NULL; 
    }
  }
  double* ax = mf_malloc(sizeof(double)*size);
  double* ay = mf_malloc(sizeof(double)*size);
  int e=0;
  for(i=0; i<size; i++){
    t = (List*)a[i];
    ax[i] = get_double(t->a[0],&e);
    ay[i] = get_double(t->a[1],&e);    
  }
  Function* f = (Function*)new_function(NULL);
  f->argc=1;
  mt_tuple* context = mf_tuple_raw(1);
  mt_double2_array* p = mf_malloc(sizeof(mt_double2_array));
  p->refcount=1;
  p->del=double2_array_delete;
  p->size=size;
  p->ax=ax;
  p->ay=ay;
  context->a[0]=(Object*)p;
  f->context = context;
  f->fp=get_value;
}
*/

mt_table* mf_ma_load(){
  mt_table* ma = mf_table(NULL);
  ma->m=mf_empty_map();
  mt_map* m=ma->m;
  mf_insert_function(m,4,4,"inv",ma_inv);
  mf_insert_function(m,2,2,"diff",ma_diff);
  mf_insert_function(m,3,3,"int",ma_int);
  // mf_insert_function(m,"interpolate",ma_interpolate);
  m->frozen=1;
  dd_argv[0].type=mv_null;
  dd_argv[1].type=mv_float;
  return ma;
}
