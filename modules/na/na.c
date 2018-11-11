
// na: numerical analysis
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <moss.h>

mt_tuple* mf_raw_tuple(long n);
extern mt_function* function_self;
double mf_float(mt_object* x, int* error);
int mf_add(mt_object* x, mt_object* a, mt_object* b);
int mf_sub(mt_object* x, mt_object* a, mt_object* b);
int mf_mpy(mt_object* x, mt_object* a, mt_object* b);
mt_function* mf_new_function(unsigned char* address);
void mf_function_dec_refcount(mt_function* f);

static
mt_object dd_argv[2];

static
double double_double_apply(mt_function* f, double x, int* error){
    dd_argv[1].value.f = x;
    mt_object y;
    if(mf_call(f,&y,1,dd_argv)){
        *error = 1;
        return NAN;
    }
    double u = mf_float(&y,error);
    if(*error){
        mf_type_error1("in float(f(x)): cannot convert f(x) (type: %s) to float.",&y);
        mf_dec_refcount(&y);
    }
    return u;
}

double invab(mt_function* f, double x, double a, double b, int N, int* e){
    double y,ya,yb,m,s,a1,b1;
    a1 = a; b1 = b;

    ya = double_double_apply(f,a,e);
    if(*e) return NAN;

    yb = double_double_apply(f,b,e);
    if(*e) return NAN;

    s = copysign(1,yb-ya);
    if(s==0 || isnan(s)) s=1;

    int k;
    for(k=0; k<N; k++){
        m = (a+b)/2;
        y = double_double_apply(f,m,e);
        if(*e) return NAN;
        if(s*(y-x)<0) a = m;
        else b = m;
    }
    if(fabs(m-a1)<1E-8) return NAN;
    if(fabs(m-b1)<1E-8) return NAN;
    return m;
}

static
int na_inv(mt_object* x, int argc, mt_object* v){
    int N=60;
    if(argc!=4){
        if(argc==5){
            if(v[5].type!=mv_int){
                mf_type_error("Type error in inv(f,x,a,b,n=60): n is not an integer.");
                return 1;
            }else{
                N = v[5].value.i;
            }
        }else{
            mf_argc_error(argc,4,5,"inv");
            return 1;
        }
    }
    if(v[1].type!=mv_function){
        mf_type_error("Type error in inv(f,x,a,b): f is not a function.");
        return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    int e=0;
    double t = mf_float(v+2,&e);
    if(e){
        mf_type_error("Type error in inv(f,x,a,b): cannot convert x to float.");
        return 1;
    }
    double a = mf_float(v+3,&e);
    if(e){
        mf_type_error("Type error in inv(f,x,a,b): cannot convert a to float.");
        return 1;
    }
    double b = mf_float(v+4,&e);
    if(e){
        mf_type_error("Type error in inv(f,x,a,b): cannot convert b to float.");
        return 1;
    }
    double y = invab(f,t,a,b,N,&e);
    if(e){
        mf_type_error("Type error in inv(f,x,a,b): cannot convert return value of f to float.");
        return 1;
    }
    x->type = mv_float;
    x->value.f = y;
    return 0;
}

static
int diff_sum(mt_object* x, mt_function* f, mt_object* r, int size,
    double a[][2], double h, double d
){
    mt_object t,y,wy,s;
    y.type = mv_null;
    wy.type = mv_null;
    t.type = mv_float;
    mt_object argv[2];
    argv[0].type = mv_null;
    argv[1].type = mv_null;
    s.type = mv_null;
    int i;
    for(i=0; i<size; i++){
        t.value.f = h*a[i][0];
        if(mf_add(argv+1,r,&t)) goto error;
        if(mf_call(f,&y,1,argv)) goto error;
        mf_dec_refcount(argv+1);
        t.value.f = a[i][1];
        if(mf_mpy(&wy,&y,&t)) goto error;
        mf_dec_refcount(&y);
        if(s.type==mv_null){
            mf_copy(&s,&wy);
        }else{
            if(mf_add(&s,&s,&wy)) goto error;
            mf_dec_refcount(&wy);
        }
    }
    t.value.f = d;
    if(mf_mpy(&s,&s,&t)) goto error;
    mf_copy(x,&s);
    return 0;
    error:
    mf_dec_refcount(argv+1);
    mf_dec_refcount(&y);
    mf_dec_refcount(&s);
    mf_dec_refcount(&wy);
    return 1;
}

static double stencil1[][2] = {
    {-2,  0.08333333333333333},
    {-1, -0.66666666666666666},
    { 1,  0.66666666666666666},
    { 2, -0.08333333333333333}
};

static double stencil2[][2] = {
    {-2, -0.08333333333333333},
    {-1,  1.33333333333333333},
    { 0, -2.5},
    { 1,  1.33333333333333333},
    { 2, -0.08333333333333333}
};

static double stencil3[][2] = {
    {-3,  0.125},
    {-2, -1},
    {-1,  1.625},
    { 1, -1.625},
    { 2,  1},
    { 3, -0.125}
};

static double stencil4[][2] = {
    {-3, -0.16666666666666666},
    {-2,  2},
    {-1, -6.5},
    { 0,  9.33333333333333333},
    { 1, -6.5},
    { 2,  2},
    { 3, -0.16666666666666666}
};

static double stencil5[][2] = {
    {-3, -0.5},
    {-2,  2},
    {-1, -2.5},
    { 1,  2.5},
    { 2, -2},
    { 3,  0.5}
};

static double stencil6[][2] = {
    {-3,  1},
    {-2, -6},
    {-1, 15},
    { 0,-20},
    { 1, 15},
    { 2, -6},
    { 3,  1}
};

static double stencil7[][2] = {
    {-3.5, -1},
    {-2.5,  7},
    {-1.5,-21},
    {-0.5, 35},
    { 0.5,-35},
    { 1.5, 21},
    { 2.5, -7},
    { 3.5,  1}
};

static double stencil8[][2] = {
    {-4,  1},
    {-3, -8},
    {-2, 28},
    {-1,-56},
    { 0, 70},
    { 1,-56},
    { 2, 28},
    { 3, -8},
    { 4,  1}
};

static double stencil9[][2] = {
    {-4.5,  -1},
    {-3.5,   9},
    {-2.5, -36},
    {-1.5,  84},
    {-0.5,-126},
    { 0.5, 126},
    { 1.5, -84},
    { 2.5,  36},
    { 3.5,  -9},
    { 4.5,   1}
};

static double stencil10[][2] = {
    {-5,   1},
    {-4, -10},
    {-3,  45},
    {-2,-120},
    {-1, 210},
    { 0,-252},
    { 1, 210},
    { 2,-120},
    { 3,  45},
    { 4, -10},
    { 5,   1}
};

static double stencil11[][2] = {
    {-5.5,  -1},
    {-4.5,  11},
    {-3.5, -55},
    {-2.5, 165},
    {-1.5,-330},
    {-0.5, 462},
    { 0.5,-462},
    { 1.5, 330},
    { 2.5,-165},
    { 3.5,  55},
    { 4.5, -11},
    { 5.5,   1}
};

static double stencil12[][2] = {
    {-6,   1},
    {-5, -12},
    {-4,  66},
    {-3,-220},
    {-2, 495},
    {-1,-792},
    { 0, 924},
    { 1,-792},
    { 2, 495},
    { 3,-220},
    { 4,  66},
    { 5, -12},
    { 6,   1}
};

static double stencil13[][2] = {
    {-6.5,   -1},
    {-5.5,   13},
    {-4.5,  -78},
    {-3.5,  286},
    {-2.5, -715},
    {-1.5, 1287},
    {-0.5,-1716},
    { 0.5, 1716},
    { 1.5,-1287},
    { 2.5,  715},
    { 3.5, -286},
    { 4.5,   78},
    { 5.5,  -13},
    { 6.5,    1}
};

static double stencil14[][2] = {
    {-7,    1},
    {-6,  -14},
    {-5,   91},
    {-4, -364},
    {-3, 1001},
    {-2,-2002},
    {-1, 3003},
    { 0,-3432},
    { 1, 3003},
    { 2,-2002},
    { 3, 1001},
    { 4, -364},
    { 5,   91},
    { 6,  -14},
    { 7,    1}
};

static
int diff(mt_object* x, mt_function* f, mt_object* t, long n){
    int e;
    double h;
    switch(n){
    case 0:{
        mt_object argv[2];
        argv[0].type = mv_null;
        mf_copy(argv+1,t);
        e = mf_call(f,x,1,argv);
    } break;
    case 1:
        e = diff_sum(x,f,t,4,stencil1,0.001,1000);
        break;
    case 2:
        e = diff_sum(x,f,t,5,stencil2,0.005,40000);
        break;
    case 3:
        e = diff_sum(x,f,t,6,stencil3,0.01,1E6);
        break;
    case 4:
        e = diff_sum(x,f,t,7,stencil4,0.02,6250000);
        break;
    case 5:
        e = diff_sum(x,f,t,6,stencil5,0.01,1E10);
        break;
    case 6:
        e = diff_sum(x,f,t,7,stencil6,0.02,1.5625E10);
        break;
    case 7:
        e = diff_sum(x,f,t,8,stencil7,0.05,1.28E9);
        break;
    case 8:
        e = diff_sum(x,f,t,9,stencil8,0.05,2.56E10);
        break;
    case 9:
        e = diff_sum(x,f,t,10,stencil9,0.1,1E9);
        break;
    case 10:
        e = diff_sum(x,f,t,11,stencil10,0.1,1E10);
        break;
    case 11:
        h = 0.15;
        e = diff_sum(x,f,t,12,stencil11,h,pow(1/h,11));
        break;
    case 12:
        h = 0.16;
        e = diff_sum(x,f,t,13,stencil12,h,pow(1/h,12));
        break;
    case 13:
        h = 0.2;
        e = diff_sum(x,f,t,14,stencil13,h,pow(1/h,13));
        break;
    case 14:
        h = 0.24;
        e = diff_sum(x,f,t,15,stencil14,h,pow(1/h,14));
        break;
    default:
        mf_value_error("Value error in diff(f,x,n): n is out of range.");
        return 1;
    }
    if(e){
        mf_traceback("diff");
        return 1;
    }
    return 0;
}

static
int na_diff(mt_object* x, int argc, mt_object* v){
    long n;
    if(argc==2){
        n = 1;
    }else if(argc==3){
        if(v[3].type!=mv_int){
            mf_type_error1("in diff(f,x,n): n (type: %s) is not an integer.",v+3);
            return 1;
        }
        n = v[3].value.i;
    }else{
        mf_argc_error(argc,2,3,"diff");
        return 1;
    }
    mt_function* f = (mt_function*)v[1].value.p;
    if(v[1].type!=mv_function){
        mf_type_error1("in diff(f,x): f (type: %s) is not a function.",v+1);
        return 1;
    }
    return diff(x,f,v+2,n);
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
    s = 0;
    for(j=0; j<n; j++){
        aj = a+j*h;
        bj = a+(j+1)*h;
        p = 0.5*(bj-aj);
        q = 0.5*(aj+bj);
        sj = 0;
        for(i=0; i<32; i++){
            y = double_double_apply(f,p*g32[i][1]+q,error);
            if(*error) return NAN;
            sj+=g32[i][0]*y;
        }
        s+=p*sj;
    }
    return s;
}

typedef struct{
    mt_function* f;
    mt_object a,d;
} mt_integral_arguments;

static
mt_integral_arguments* integral_arguments;

static
int real_fp(mt_object* x, int argc, mt_object* v){
    mt_object argv[2];
    argv[0].type = mv_null;
    if(mf_mpy(argv+1,&integral_arguments->d,v+1)){
        mf_traceback("vint");
        return 1;
    }
    if(mf_add(argv+1,argv+1,&integral_arguments->a)){
        mf_traceback("vint");
        return 1;
    }
    if(mf_call(integral_arguments->f,x,1,argv)) return 1;
    if(x->type==mv_complex){
        x->type = mv_float;
        x->value.f = x->value.c.re;
    }
    return 0;
}

static
int imag_fp(mt_object* x, int argc, mt_object* v){
    mt_object argv[2];
    argv[0].type = mv_null;
    if(mf_mpy(argv+1,&integral_arguments->d,v+1)){
        mf_traceback("vint");
        return 1;
    }
    if(mf_add(argv+1,argv+1,&integral_arguments->a)){
        mf_traceback("vint");
        return 1;
    }
    if(mf_call(integral_arguments->f,x,1,argv)) return 1;
    if(x->type==mv_complex){
        x->type = mv_float;
        x->value.f = x->value.c.im;
    }else{
        x->type = mv_float;
        x->value.f = 0;
    }
    return 0;
}

static
int vint_step(mt_object* z, mt_function* f, mt_object a, mt_object b, int n){
    mt_function* ref = mf_new_function(NULL);
    ref->fp = real_fp;
    mt_function* imf = mf_new_function(NULL);
    imf->fp = imag_fp;
    int e=0;
    mt_integral_arguments args;
    mt_integral_arguments* pargs;
    pargs = integral_arguments;
    args.f = f;
    mf_copy(&args.a,&a);
    mf_sub(&args.d,&b,&a);
    integral_arguments = &args;
    double re = gauss(ref,0,1,n,&e);
    if(e) goto error;
    double im = gauss(imf,0,1,n,&e);
    if(e) goto error;
    z->type = mv_complex;
    z->value.c.re = re;
    z->value.c.im = im;
    mf_mpy(z,z,&args.d);
    mf_function_dec_refcount(ref);
    mf_function_dec_refcount(imf);
    integral_arguments = pargs;
    return 0;

    error:
    mf_function_dec_refcount(ref);
    mf_function_dec_refcount(imf);
    integral_arguments = pargs;
    return 1;
}

static
int vint(mt_object* z, mt_function* f, mt_list* list, int n){
    z->type = mv_complex;
    z->value.c.re = 0;
    z->value.c.im = 0;
    mt_object y;
    long m = list->size-1;
    long i;
    for(i=0; i<m; i++){
        if(vint_step(&y,f,list->a[i],list->a[i+1],n)){
            return 1;
        }
        z->value.c.re+=y.value.c.re;
        z->value.c.im+=y.value.c.im;
    }
    return 0;
}

static
int na_integral(mt_object* x, int argc, mt_object* v){
    int n;
    if(argc==3){
        n = 2;
    }else if(argc==4){
        if(v[4].type!=mv_int){
            mf_type_error1("in integral(a,b,f,n): n (type: %s) is not an integer.",v+4);
            return 1;
        }
        n = (int)v[4].value.i;
    }else{
        mf_argc_error(argc,3,4,"integral");
        return 1;
    }
    if(v[3].type!=mv_function){
        mf_type_error1("in integral(a,b,f): f (type: %s) is not a function.",v+3);
        return 1;
    }
    mt_function* f = (mt_function*)v[3].value.p;
    double a,b;
    int e = 0;
    a = mf_float(v+1,&e);
    if(e){
        mf_type_error1("in integral(a,b,f): cannot convert a (type: %s) to float.",v+1);
        return 1;
    }
    b = mf_float(v+2,&e);
    if(e){
        mf_type_error1("in integral(a,b,f): cannot convert b (type: %s) to float.",v+2);
        return 1;
    }
    double y = gauss(f,a,b,n,&e);
    if(e){
        mf_traceback("integral");
        return 1;
    }
    x->type = mv_float;
    x->value.f = y;
    return 0;
}

static
int na_vint(mt_object* x, int argc, mt_object* v){
    int n;
    if(argc==2){
        n = 2;
    }else if(argc==3){
        if(v[3].type!=mv_int){
            mf_type_error1("in vint(c,f,n): n (type: %s) is not an integer.",v+3);
            return 1;
        }
        n = (int)v[3].value.i;
    }else{
        mf_argc_error(argc,2,3,"vint");
        return 1;
    }
    if(v[2].type!=mv_function){
        mf_type_error1("in vint(c,f): f (type: %s) is not a function.",v+1);
        return 1;
    }
    mt_function* f = (mt_function*)v[2].value.p;
    if(v[1].type!=mv_list){
        mf_type_error1("in vint(c,f): c (type: %s) is not a list.",v+2);
        return 1;
    }
    mt_list* list = (mt_list*)v[1].value.p;
    if(vint(x,f,list,n)){
        mf_traceback("vint");
        return 1;
    }
    return 0;
}

typedef struct{
    long refcount;
    mt_object prototype;
    void (*del)(void*);
    long size;
    double* ax;
    double* ay;
} mt_double2_array;

static
void double2_array_delete(mt_double2_array* self){
    mf_free(self->ax);
    mf_free(self->ay);
    mf_free(self);
}

static
double interpolate(double x, long size, double* ax, double* ay){
    if(size==0) return NAN;
    long a = 0;
    long b = size-1;
    long i;
    if(x<ax[a] || x>ax[b]){
        return NAN;
    }
    while(a<=b){
        i = a+(b-a)/2;
        if(x<ax[i]){
            b = i-1;
        }else{
            a = i+1;
        }
    }
    i = a;
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
    int e = 0;
    double t = mf_float(v+1,&e);
    if(e) return 1;
    mt_double2_array* a = (mt_double2_array*)function_self->context->a[0].value.p;
    double y = interpolate(t,a->size,a->ax,a->ay);
    x->type = mv_float;
    x->value.f = y;
    return 0;
}

static
int na_interpolate(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"interpolate");
        return 1;
    }
    if(v[1].type!=mv_list){
        mf_type_error1("in interpolate(a): a (type: %s) is not a list.",v+1);
        return 1;
    }
    mt_list* list = (mt_list*)v[1].value.p;
    long size = list->size;
    long i;
    mt_object* a = list->a;
    mt_list* t;
    for(i=0; i<size; i++){
        if(a[i].type!=mv_list){
            mf_type_error1("in interpolate(a): a[k] (type: %s) is not a pair.",a+i);
            return 1;
        }
        t = (mt_list*)a[i].value.p;
        if(t->size!=2){
            mf_type_error("Type error in interpolate: expected list of pairs.");
            return 1;
        }
    }
    double* ax = mf_malloc(sizeof(double)*size);
    double* ay = mf_malloc(sizeof(double)*size);
    int e=0;
    for(i=0; i<size; i++){
        t = (mt_list*)a[i].value.p;
        ax[i] = mf_float(&t->a[0],&e);
        if(e){
            mf_type_error1("in interpolate(a): cannot convert a[k][0] (type: %s) to float.",&t->a[0]);
            goto error;
        }
        ay[i] = mf_float(&t->a[1],&e);
        if(e){
            mf_type_error1("in interpolate(a): cannot convert a[k][1] (type: %s) to float.",&t->a[1]);
            goto error;
        }
    }
    mt_function* f = mf_new_function(NULL);
    f->argc = 1;
    mt_tuple* context = mf_raw_tuple(1);
    mt_double2_array* p = mf_malloc(sizeof(mt_double2_array));
    p->refcount = 1;
    p->prototype.type = mv_null;
    p->del = (void(*)(void*))double2_array_delete;
    p->size = size;
    p->ax = ax;
    p->ay = ay;
    context->a[0].type = mv_native;
    context->a[0].value.p = (mt_basic*)p;
    f->context = context;
    f->fp = get_value;
    x->type = mv_function;
    x->value.p = (mt_basic*)f;
    return 0;

    error:
    mf_free(ax);
    mf_free(ay);
    return 1;
}

mt_table* mf_na_load(){
    mt_table* na = mf_table(NULL);
    na->m = mf_empty_map();
    mt_map* m = na->m;
    mf_insert_function(m,4,5,"inv",na_inv);
    mf_insert_function(m,2,2,"diff",na_diff);
    mf_insert_function(m,3,4,"integral",na_integral);
    mf_insert_function(m,3,4,"vint",na_vint);
    mf_insert_function(m,1,1,"interpolate",na_interpolate);
    m->frozen = 1;
    dd_argv[0].type = mv_null;
    dd_argv[1].type = mv_float;
    return na;
}
