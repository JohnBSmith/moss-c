
#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <moss.h>
#include <modules/str.h>

mt_string* mf_cstr_to_str(const char* a);
double mf_float(mt_object* x, int* error);
mt_list* mf_raw_list(long size);

typedef struct{
    unsigned char r,g,b,a;
} mt_pixel;

typedef struct{
    long refcount;
    mt_object prototype;
    void (*del)(void*);
    SDL_Window* screen;
    SDL_Renderer* rdr;
    TTF_Font* font;
    mt_pixel* buffer;
    int w,h; // TODO: unsigned?
    int size;
    int r,g,b,a;
    int cx,cy;
    int cursor;
    int add;
} mt_canvas;

#include "font.c"
mt_table* canvas_type;

static
void gx_delete(void* p){
    mt_canvas* canvas=p;
    SDL_DestroyWindow(canvas->screen);
    SDL_Quit();
}

static
mt_pixel* new_buffer(int w, int h){
    int n=w*h;
    mt_pixel* a = mf_malloc(sizeof(mt_pixel)*n);
    int i;
    for(i=0; i<n; i++){
        a[i].r=0; a[i].g=0; a[i].b=0; a[i].a=0;
    }
    return a;
}

static
mt_canvas* gx_new(int w, int h, int size){
    if(SDL_Init(SDL_INIT_VIDEO)<0){
        mf_std_exception("Error: cannot construct canvas screen.");
        return NULL;
    }
    mt_canvas* canvas = mf_malloc(sizeof(mt_canvas));
    canvas->refcount=1;
    canvas_type->refcount++;
    canvas->prototype.type = mv_table;
    canvas->prototype.value.p = (mt_basic*)canvas_type;
    canvas->del = gx_delete;
    canvas->h = h;
    canvas->w = w;
    canvas->size = size;
    canvas->cx = 10;
    canvas->cy = 10;
    canvas->add = 1;
    canvas->screen = SDL_CreateWindow("",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        w*size, h*size, SDL_WINDOW_SHOWN);
    if(canvas->screen==NULL){
        mf_free(canvas);
        mf_std_exception("Error: cannot construct canvas screen.");
        return NULL;
    }
    canvas->rdr = SDL_CreateRenderer(canvas->screen,0,SDL_RENDERER_ACCELERATED);
    if(canvas->rdr==NULL){
        mf_free(canvas);
        mf_std_exception("Error: cannot construct canvas screen.");
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(canvas->rdr, SDL_BLENDMODE_BLEND);
    if(TTF_Init()==-1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        abort();
    }
    canvas->font=NULL;
    canvas->buffer = new_buffer(w,h);
    SDL_SetRenderDrawColor(canvas->rdr,0,0,0,255);
    SDL_RenderClear(canvas->rdr);
    SDL_SetRenderDrawColor(canvas->rdr,0xa0,0xa0,0xa0,255);
    canvas->r = 0xa0;
    canvas->g = 0xa0;
    canvas->b = 0xa0;
    canvas->a = 255;
    return canvas;
}

static
void gx_pset(mt_canvas* canvas, unsigned int x, unsigned int y){
    int w = canvas->w;
    int h = canvas->h;
    if(x>=w || y>=h) return;
    int size = canvas->size;
    SDL_Rect r;
    r.x = size*x;
    r.y = size*y;
    r.w = size;
    r.h = size;
    SDL_RenderFillRect(canvas->rdr,&r);
    mt_pixel* p = canvas->buffer+y*w+x;
    p->r = canvas->r;
    p->g = canvas->g;
    p->b = canvas->b;
}

static
int max(int a, int b){
    return a<b? b: a;
}

static
int min(int a, int b){
    return a<b? a: b;
}

static
void gx_pseta(mt_canvas* canvas, unsigned int x, unsigned int y, unsigned char a){
    unsigned int w = canvas->w;
    unsigned int h = canvas->h;
    if(x>=w) return;
    if(y>=w) return;
    mt_pixel* p = canvas->buffer+y*w+x;
    if(canvas->add){
        p->r = max(canvas->r*a/255,p->r);
        p->g = max(canvas->g*a/255,p->g);
        p->b = max(canvas->b*a/255,p->b);
    }else{
        p->r = min((canvas->r-255)*a/255+255,p->r);
        p->g = min((canvas->g-255)*a/255+255,p->g);
        p->b = min((canvas->b-255)*a/255+255,p->b);
    }
    SDL_SetRenderDrawColor(canvas->rdr,p->r,p->g,p->b,255);
    int size = canvas->size;
    SDL_Rect r;
    r.x = size*x;
    r.y = size*y;
    r.w = size;
    r.h = size;
    SDL_RenderFillRect(canvas->rdr,&r);
}

static
void gx_cset(mt_canvas* canvas, int r, int g, int b, int a){
    SDL_SetRenderDrawColor(canvas->rdr,r,g,b,a);
    canvas->r = r;
    canvas->g = g;
    canvas->b = b;
    canvas->a = a;
}

static
void gx_clear(mt_canvas* canvas, int r, int g, int b){
    SDL_SetRenderDrawColor(canvas->rdr,r,g,b,255);
    SDL_RenderClear(canvas->rdr);
    SDL_SetRenderDrawColor(canvas->rdr,
        canvas->r,canvas->g,canvas->b,canvas->a);
    int size = canvas->w*canvas->h;
    int i;
    mt_pixel* a = canvas->buffer;
    for(i=0; i<size; i++){
        a[i].r=r; a[i].g=g; a[i].b=b;
    }
}

static
void gx_flush(mt_canvas* canvas){
    SDL_RenderPresent(canvas->rdr);
}

static
int mf_gx_new(mt_object* x, int argc, mt_object* v){
    if(argc!=3){
        mf_argc_error(argc,3,3,"new");
        return 1;
    }
    if(v[1].type!=mv_int || v[2].type!=mv_int ||
        v[3].type!=mv_int
    ){
        mf_type_error("Type error: new takes only integers.");
        return 1;
    }
    long w = v[1].value.i;
    long h = v[2].value.i;
    long size = v[3].value.i;
    mt_canvas* canvas = gx_new(w,h,size);
    x->type = mv_native;
    x->value.p = (mt_basic*)canvas;
    return 0;
}

static
int isa(mt_object* x, void* prototype){
    if(x->type==mv_native){
        mt_native* p = (mt_native*)x->value.p;
        if(p->prototype.value.p==prototype){
            return 1;
        }
    }
    return 0;
}

static
int mf_gx_pset(mt_object* x, int argc, mt_object* v){
    if(argc!=2){
        mf_argc_error(argc,2,2,"pset");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.pset(x,y): c is not a canvas.");
        return 1;
    }
    if(v[1].type!=mv_int || v[2].type!=mv_int){
        mf_type_error("Type error c.pset(x,y): x,y must be of type integer.");
        return 1;
    }
    gx_pset((mt_canvas*)v[0].value.p,v[1].value.i,v[2].value.i);
    x->type = mv_null;
    return 0;
}

static
int mf_gx_flush(mt_object* x, int argc, mt_object* v){
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.flush(): c is not a canvas.");
        return 1;
    }
    gx_flush((mt_canvas*)v[0].value.p);
    x->type = mv_null;
    return 0;
}

static
int mf_gx_cset(mt_object* x, int argc, mt_object* v){
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.cset(r,g,b): c is not a canvas.");
        return 1;
    }
    double r,g,b,a;
    int error=0;
    if(argc==1){
        if(v[1].type!=mv_list){
            mf_type_error("Type error in c.cset(a): is not a list.");
            return 1;
        }
        mt_list* a = (mt_list*)v[1].value.p;
        if(a->size!=3){
            mf_value_error("Value error in c.cset(a): size(a)!=3.");
            return 1;
        }
        r = mf_float(a->a+0,&error);
        g = mf_float(a->a+1,&error);
        b = mf_float(a->a+2,&error);
        if(error){
            mf_type_error("Type error in c.cset([r,g,b]): r,g,b must be convertible to float.");
            return 1;
        }
        gx_cset((mt_canvas*)v[0].value.p,255*r,255*g,255*b,255);
    }else if(argc==3){
        r = mf_float(v+1,&error);
        g = mf_float(v+2,&error);
        b = mf_float(v+3,&error);
        if(error){
            mf_type_error("Type error in c.cset(r,g,b): r,g,b must be convertible to float.");
            return 1;
        }
        gx_cset((mt_canvas*)v[0].value.p,255*r,255*g,255*b,255);
    }else if(argc==4){
        r = mf_float(v+1,&error);
        g = mf_float(v+2,&error);
        b = mf_float(v+3,&error);
        a = mf_float(v+4,&error);
        if(error){
            mf_type_error("Type error in c.cset(r,g,b,a): r,g,b,a must be convertible to float.");
            return 1;
        }
        gx_cset((mt_canvas*)v[0].value.p,255*r,255*g,255*b,255*a);
    }else{
        // TODO
        mf_argc_error(argc,1,3,"cset");
        return 1;
    }
    x->type = mv_null;
    return 0;
}

static
double floor_mod(double x, double m){
    return x-m*floor(x/m);
}

void HSL_to_RGB(double H, double S, double L,
    double* R, double* G, double* B
){
    S = S<0?0:S>1?1:S;
    L = L<0?0:L>1?1:L;
    double R1,G1,B1;
    double C = (1-fabs(2*L-1))*S;
    double Hp = 3*floor_mod(H,2*M_PI)/M_PI;
    double X = C*(1-fabs(fmod(Hp,2)-1));
    if(0<=Hp && Hp<=1){
        R1=C; G1=X; B1=0;
    }else if(1<=Hp && Hp<2){
        R1=X; G1=C; B1=0;
    }else if(2<=Hp && Hp<3){
        R1=0; G1=C; B1=X;
    }else if(3<=Hp && Hp<4){
        R1=0; G1=X; B1=C;
    }else if(4<=Hp && Hp<5){
        R1=X; G1=0; B1=C;
    }else if(5<=Hp && Hp<=6){
        R1=C; G1=0; B1=X;
    }else{
        R1=0; G1=0; B1=0;
    }
    double m = L-C/2;
    *R = R1+m;
    *G = G1+m;
    *B = B1+m;
}

static
int canvas_HSL(mt_object* x, int argc, mt_object* v){
    if(argc!=3){
        mf_argc_error(argc,3,3,"cset");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.cset(H,S,L): c is not a canvas.");
        return 1;
    }
    double H,S,L,r,g,b;
    int error = 0;
    H = mf_float(v+1,&error);
    S = mf_float(v+2,&error);
    L = mf_float(v+3,&error);
    if(error){
        mf_type_error("Type error in c.cset(H,S,L): H,S,L must be convertible to float.");
        return 1;
    }
    HSL_to_RGB(H,S,L,&r,&g,&b);
    gx_cset((mt_canvas*)v[0].value.p,255*r,255*g,255*b,255);
    x->type = mv_null;
    return 0;
}

static
int gx_HSL(mt_object* x, int argc, mt_object* v){
    if(argc!=3){
        mf_argc_error(argc,3,3,"cset");
        return 1;
    }
    double H,S,L,r,g,b;
    int error = 0;
    H = mf_float(v+1,&error);
    S = mf_float(v+2,&error);
    L = mf_float(v+3,&error);
    if(error){
        mf_type_error("Type error in gx.cset(H,S,L): H,S,L must be convertible to float.");
        return 1;
    }
    HSL_to_RGB(H,S,L,&r,&g,&b);
    mt_list* list = mf_raw_list(3);
    mt_object* a = list->a;
    a[0].type = mv_float; a[0].value.f=r;
    a[1].type = mv_float; a[1].value.f=g;
    a[2].type = mv_float; a[2].value.f=b;
    x->type = mv_list;
    x->value.p = (mt_basic*)list;
    return 0;
}

static
const char* get_key_name(SDL_Keycode sym){
    switch(sym){
    case SDLK_0: return "0";
    case SDLK_1: return "1";
    case SDLK_2: return "2";
    case SDLK_3: return "3";
    case SDLK_4: return "4";
    case SDLK_5: return "5";
    case SDLK_6: return "6";
    case SDLK_7: return "7";
    case SDLK_8: return "8";
    case SDLK_9: return "9";
    case SDLK_a: return "a";
    case SDLK_b: return "b";
    case SDLK_c: return "c";
    case SDLK_d: return "d";
    case SDLK_e: return "e";
    case SDLK_f: return "f";
    case SDLK_g: return "g";
    case SDLK_h: return "h";
    case SDLK_i: return "i";
    case SDLK_j: return "j";
    case SDLK_k: return "k";
    case SDLK_l: return "l";
    case SDLK_m: return "m";
    case SDLK_n: return "n";
    case SDLK_o: return "o";
    case SDLK_p: return "p";
    case SDLK_q: return "q";
    case SDLK_r: return "r";
    case SDLK_s: return "s";
    case SDLK_t: return "t";
    case SDLK_u: return "u";
    case SDLK_v: return "v";
    case SDLK_w: return "w";
    case SDLK_x: return "x";
    case SDLK_y: return "y";
    case SDLK_z: return "z";
    case SDLK_ESCAPE: return "ESC";
    case SDLK_F1: return "F1";
    case SDLK_F2: return "F2";
    case SDLK_F3: return "F3";
    case SDLK_F4: return "F4";
    case SDLK_F5: return "F5";
    case SDLK_F6: return "F6";
    case SDLK_F7: return "F7";
    case SDLK_F8: return "F8";
    case SDLK_F9: return "F9";
    case SDLK_F10: return "F10";
    case SDLK_F11: return "F11";
    case SDLK_F12: return "F12";
    case SDLK_UP: return "up";
    case SDLK_DOWN: return "down";
    case SDLK_LEFT: return "left";
    case SDLK_RIGHT: return "right";
    case SDLK_LCTRL: return "LCTRL";
    case SDLK_RCTRL: return "RCTRL";
    case SDLK_LALT: return "LALT";
    case SDLK_RALT: return "RALT";
    case SDLK_LSHIFT: return "LSHIFT";
    case SDLK_RSHIFT: return "RSHIFT";
    case SDLK_SPACE: return "space";
    case SDLK_PLUS: return "+";
    case SDLK_MINUS: return "-";
    case SDLK_CARET: return "^";
    case SDLK_PERIOD: return ".";
    case SDLK_COMMA: return ",";
    case SDLK_HASH: return "#";
    case SDLK_ASTERISK: return "*";
    default:
        return "??";
    }
}

static
int mf_gx_key(mt_object* x, int argc, mt_object* v){
    SDL_Event event;
    while (SDL_PollEvent(&event)){
        if(event.type==SDL_KEYDOWN){
            const char* p = get_key_name(event.key.keysym.sym);
            mt_string* s = mf_cstr_to_str(p);
            x->type = mv_string;
            x->value.p = (mt_basic*)s;
            return 0;
        }
    }
    x->type = mv_null;
    return 0;
}

static
int mf_gx_clear(mt_object* x, int argc, mt_object* v){
    if(argc!=3){
        mf_argc_error(argc,3,3,"clear");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.clear(r,g,b): c is not a canvas.");
        return 1;
    }
    double r,g,b;
    int error = 0;
    r = mf_float(v+1,&error);
    g = mf_float(v+2,&error);
    b = mf_float(v+3,&error);
    if(error){
        mf_type_error("Type error in c.clear(r,g,b): r,g,b must be convertible to float.");
        return 1;
    }
    gx_clear((mt_canvas*)v[0].value.p,255*r,255*g,255*b);
    x->type = mv_null;
    return 0;
}

static
void gx_fill(mt_canvas* canvas, long x, long y, long w, long h){
    int i,j;
    for(i=x+w-1; i>=x; i--){
        for(j=y+h-1; j>=y; j--){
            gx_pset(canvas,i,j);
        }
    }
}

static
int mf_gx_fill(mt_object* x, int argc, mt_object* v){
    if(argc!=4){
        mf_argc_error(argc,4,4,"fill");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.fill(x,y,w,h): c is not a canvas.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    if(v[1].type!=mv_int || v[2].type!=mv_int ||
         v[3].type!=mv_int || v[4].type!=mv_int
    ){
        mf_type_error("Type error in c.fill(x,y,w,h): fill takes only integers.");
        return 1;
    }
    long px = v[1].value.i;
    long py = v[2].value.i;
    long w = v[3].value.i;
    long h = v[4].value.i;
    gx_fill(canvas,px,py,w,h);
    
    x->type = mv_null;
    return 0;
}

static
double fade(double x){
    return exp(-0.5*x*x);
}

static
void psetdiff(mt_canvas* canvas, double rx, double ry, int px, int py){
    double d = hypot(px-rx,py-ry);
    gx_pseta(canvas,px,py,255*fade(d));
}

static
void point(mt_canvas* canvas, double x, double y){
    double rx = (canvas->w+canvas->w*x)/2.0;
    double ry = (canvas->h-canvas->w*y)/2.0;
    unsigned int px = rx;
    unsigned int py = ry;
    int i,j;
    double d,a;
    for(i=-2; i<=2; i++){
        for(j=-2; j<=2; j++){
            psetdiff(canvas,rx,ry,px+i,py+j);
        }
    }
}

static
double scatter_fade(double x){
    double a = 3.4;
    return x<a? 1: exp(-1.0*(a-x)*(a-x));
}

static
void scatter_psetdiff(mt_canvas* canvas, double rx, double ry, int px, int py){
    double d = hypot(px-rx,py-ry);
    gx_pseta(canvas,px,py,255*scatter_fade(d));
}

static
void scatter_point(mt_canvas* canvas, double x, double y){
    double rx = (canvas->w+canvas->w*x)/2.0;
    double ry = (canvas->h-canvas->w*y)/2.0;
    unsigned int px = rx;
    unsigned int py = ry;
    int i,j;
    for(i=-6; i<=6; i++){
        for(j=-6; j<=6; j++){
            scatter_psetdiff(canvas,rx,ry,px+i,py+j);
        }
    }
}

static
void box(mt_canvas* canvas, double x, double y){
    double rx = (canvas->w+canvas->w*x)/2.0;
    double ry = (canvas->h-canvas->w*y)/2.0;
    unsigned int px = rx;
    unsigned int py = ry;
    gx_fill(canvas,px-4,py-4,10,2);
    gx_fill(canvas,px-4,py+4,10,2);
    gx_fill(canvas,px-4,py-2,2,6);
    gx_fill(canvas,px+4,py-2,2,6);
}

static
int mf_gx_point(mt_object* x, int argc, mt_object* v){
    if(argc!=2){
        mf_argc_error(argc,2,2,"point");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.point(x,y): c is not a canvas.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    double rx,ry;
    int error = 0;
    rx = mf_float(v+1,&error);
    ry = mf_float(v+2,&error);
    if(error){
        mf_type_error("Type error in c.point(x,y): x,y must be convertible to float.");
        return 1;
    }
    if(!isnan(rx) && !isnan(ry)){
        // int px,py;
        // px=(canvas->w+canvas->w*rx)/2.0;
        // py=(canvas->h-canvas->w*ry)/2.0;
        // gx_pset(canvas,px,py);
        point(canvas,rx,ry);
    }
    x->type = mv_null;
    return 0;
}

static
int mf_gx_scatter(mt_object* x, int argc, mt_object* v){
    if(argc!=2){
        mf_argc_error(argc,2,2,"scatter");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.scatter(x,y): c is not a canvas.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    double rx,ry;
    int error = 0;
    rx = mf_float(v+1,&error);
    ry = mf_float(v+2,&error);
    if(error){
        mf_type_error("Type error in c.scatter(x,y): x,y must be convertible to float.");
        return 1;
    }
    if(!isnan(rx) && !isnan(ry)){
        scatter_point(canvas,rx,ry);
    }
    x->type = mv_null;
    return 0;
}

static
int canvas_box(mt_object* x, int argc, mt_object* v){
    if(argc!=2){
        mf_argc_error(argc,2,2,"box");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.box(x,y): c is not a canvas.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    double rx,ry;
    int error = 0;
    rx = mf_float(v+1,&error);
    ry = mf_float(v+2,&error);
    if(error){
        mf_type_error("Type error in c.box(x,y): x,y must be convertible to float.");
        return 1;
    }
    if(!isnan(rx) && !isnan(ry)){
        box(canvas,rx,ry);
    }
    x->type = mv_null;
    return 0;
}

static
void line(mt_canvas* canvas, double x1, double y1, double x2, double y2){
    double dx = x2-x1;
    double dy = y2-y1;
    double d = hypot(dx,dy);
    double h = 1/(canvas->w*d);
    double t;
    for(t=0; t<1; t+=h){
        point(canvas,x1+t*dx,y1+t*dy);
    }
}

static
int gx_line(mt_object* x, int argc, mt_object* v){
 if(argc!=4){
        mf_argc_error(argc,4,4,"line");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.line(x1,y1,x2,y2): c is not a canvas.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    double x1,y1,x2,y2;
    int error = 0;
    x1 = mf_float(v+1,&error);
    y1 = mf_float(v+2,&error);
    x2 = mf_float(v+3,&error);
    y2 = mf_float(v+4,&error);
    if(error){
        mf_type_error("Type error in c.line(x1,y1,x2,y2): x1,y2,x2,y2 must be convertible to float.");
        return 1;
    }
    if(!isnan(x1) && !isnan(y1) && !isnan(x2) && !isnan(y2)){
        line(canvas,x1,y1,x2,y2);
    }
    x->type = mv_null;
    return 0;
}

static
int gx_mix(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"mix");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.mix(n): c is not a canvas.");
        return 1;
    }
    if(v[1].type!=mv_int){
        mf_type_error("Type error in c.mix(n): n is not an integer.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    long n = v[1].value.i;
    canvas->add = n;
    x->type = mv_null;
    return 0;
}

static
void gx_pset_str(const char* a, mt_canvas* canvas,
    unsigned int x, unsigned int y
){
    const char* p = a;
    char c;
    int i = 0;
    int j = 0;
    while(*p!=0){
        c = *p;
        if(c=='x'){
            gx_fill(canvas,x+i,y+j,1,1);
            i++;
        }else if(c=='n'){
            i=0; j++;
        }else{
            i++;
        }
        p++;
    }
}

static
void gx_putchar(mt_canvas* canvas, uint32_t c){
    if(c=='\n'){
        canvas->cy+=LINE_HEIGHT;
        canvas->cursor = canvas->cx;
    }else{
        const char* p = font_get_char(c);
        gx_pset_str(p,canvas,canvas->cursor,canvas->cy);
        canvas->cursor+=LETTER_WIDTH;
    }
}

static
void gx_print(mt_canvas* canvas, long size, uint32_t* a){
    int i;
    canvas->cursor = canvas->cx;
    for(i=0; i<size; i++){
        gx_putchar(canvas,a[i]);
    }
    canvas->cy+=LINE_HEIGHT;
}

static
void gx_print_ttf(mt_canvas* canvas, const char* a){
    SDL_Color color;
    color.r = canvas->r;
    color.g = canvas->g;
    color.b = canvas->b;
    SDL_Surface* s = TTF_RenderUTF8_Blended(canvas->font,a,color);
    SDL_Texture* t = SDL_CreateTextureFromSurface(canvas->rdr,s);
    SDL_Rect rect;
    rect.x = canvas->cx;
    rect.y = canvas->cy;
    rect.w = s->w;
    rect.h = s->h;
    canvas->cy+=s->h;
    SDL_RenderCopy(canvas->rdr, t, NULL, &rect);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

int mf_gx_print(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"print");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.print(s): c is not a canvas.");
        return 1;
    }
    if(v[1].type!=mv_string){
        mf_type_error("Type error in c.print(s): s is not a string.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    if(canvas->font==NULL){
        mf_std_exception("Error in c.print(s): font not specified.");
        return 1;
    }
    mt_string* s = (mt_string*)v[1].value.p;
    mt_bstr bs;
    mf_encode_utf8(&bs,s->size,s->a);
    gx_print_ttf(canvas,(char*)bs.a);
    // gx_print(canvas,s->size,s->a);
    x->type=mv_null;
    return 0;
}

static
int canvas_font(mt_object* x, int argc, mt_object* v){
    if(argc!=1){
        mf_argc_error(argc,1,1,"font");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.print(s): c is not a canvas.");
        return 1;
    }
    if(v[1].type!=mv_int){
        mf_type_error1("in canvas.font(n): n (type: %s) is not an integer.",v+1);
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    canvas->font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", v[1].value.i);
    if(canvas->font==NULL){
        char a[400];
        snprintf(a,400,"Error in TTF_OpenFont: %s.", TTF_GetError());
        mf_std_exception(a);
        return 1;
    }
    x->type = mv_null;
    return 0;
}

int mf_gx_pos(mt_object* x, int argc, mt_object* v){
    if(argc!=2){
        mf_argc_error(argc,2,2,"pos");
        return 1;
    }
    if(!isa(v,canvas_type)){
        mf_type_error("Type error in c.pos(x,y): c is not a canvas.");
        return 1;
    }
    if(v[1].type!=mv_int || v[2].type!=mv_int){
        mf_type_error("Type error in c.pos(x,y): x or y is not an integer.");
        return 1;
    }
    mt_canvas* canvas = (mt_canvas*)v[0].value.p;
    long px = v[1].value.i;
    long py = v[2].value.i;
    canvas->cx = px;
    canvas->cy = py;
    x->type = mv_null;
    return 0;
}

mt_table* mf_gx_load(){
    mt_table* gx = mf_table(NULL);
    canvas_type = mf_table(NULL);
    canvas_type->m = mf_empty_map();
    mt_map* m = canvas_type->m;
    mf_insert_function(m,2,2,"pset",mf_gx_pset);
    mf_insert_function(m,0,0,"flush",mf_gx_flush);
    mf_insert_function(m,3,3,"cset",mf_gx_cset);
    mf_insert_function(m,3,3,"HSL",canvas_HSL);
    mf_insert_function(m,3,3,"clear",mf_gx_clear);
    mf_insert_function(m,0,0,"key",mf_gx_key);
    mf_insert_function(m,2,2,"point",mf_gx_point);
    mf_insert_function(m,2,2,"scatter",mf_gx_scatter);
    mf_insert_function(m,2,2,"box",canvas_box);
    mf_insert_function(m,4,4,"line",gx_line);
    mf_insert_function(m,4,4,"fill",mf_gx_fill);
    mf_insert_function(m,1,1,"print",mf_gx_print);
    mf_insert_function(m,2,2,"pos",mf_gx_pos);
    mf_insert_function(m,1,1,"mix",gx_mix);
    mf_insert_function(m,1,1,"font",canvas_font);

    gx->m = mf_empty_map();
    m = gx->m;
    mf_insert_function(m,3,3,"new",mf_gx_new);
    mf_insert_function(m,3,3,"HSL",gx_HSL);
    m->frozen = 1;
    return gx;
}
