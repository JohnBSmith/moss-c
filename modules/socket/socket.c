
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>

#include <moss.h>
#include <objects/string.h>
#include <objects/bstring.h>
#include <objects/list.h>
#include <modules/bs.h>

extern int mode_unsafe;

typedef struct{
  int refcount;
  mt_object prototype;
  void(*del)(void*);
  int id;
  struct hostent* host;
  struct sockaddr_in address;
} mt_socket;

static
mt_table* socket_type;

static
void socket_delete(mt_socket* s){
  mf_dec_refcount(&s->prototype);
  mf_free(s);
}

static
mt_socket* new_socket(void){
  mt_socket* s = mf_malloc(sizeof(mt_socket));
  s->refcount=1;
  socket_type->refcount++;
  s->prototype.type=mv_table;
  s->prototype.value.p=(mt_basic*)socket_type;
  s->del = (void(*)(void*))socket_delete;
  s->host=NULL;
  return s;
}

static
int mf_socket(mt_object* x, int argc, mt_object* v){
  if(0==mode_unsafe){
    mf_std_exception("Exception in socket(): access denied.");
    return 1;
  }
  mt_socket* s = new_socket();
  s->id = socket(AF_INET,SOCK_STREAM,0);
  if(s->id==-1){
    mf_std_exception("Exception in socket().");
    return 1;
  }
  x->type=mv_native;
  x->value.p=(mt_basic*)s;
  return 0;
}

static
int mf_socket_connect(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"s.connect(id)");
    return 1;
  }
  if(!mf_isa(&v[0],socket_type)){
    mf_type_error1("in s.connect(id): s (type: %s) is not a socket.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in s.connect(id): id (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_socket* s = (mt_socket*)v[0].value.p;
  mt_string* id = (mt_string*)v[1].value.p;
  mt_bstr bs;
  mf_encode_utf8(&bs,id->size,id->a);
  s->host = gethostbyname((char*)bs.a);
  mf_free(bs.a);
  if(s->host==NULL){
    mf_std_exception("Exception in s.connect(id): no such host.");
    return 1;
  }
  memset(&s->address,0,sizeof(struct sockaddr_in));
  s->address.sin_family = AF_INET;
  s->address.sin_port = htons(80);
  memcpy(&s->address.sin_addr.s_addr,s->host->h_addr,s->host->h_length);

  if(connect(s->id,(struct sockaddr*)&s->address,sizeof( struct sockaddr_in))<0){
    mf_std_exception("Exception in s.connect(id): could not connect.");
    return 1;
  }

  x->type=mv_null;
  return 0;
}

static
int mf_socket_send(mt_object* x, int argc, mt_object* v){
  if(argc!=1){
    mf_argc_error(argc,1,1,"s.send(text)");
    return 1;
  }
  if(!mf_isa(&v[0],socket_type)){
    mf_type_error1("in s.send(text): s (type: %s) is not a socket.",&v[0]);
    return 1;
  }
  if(v[1].type!=mv_string){
    mf_type_error1("in s.send(text): text (type: %s) is not a string.",&v[1]);
    return 1;
  }
  mt_socket* s = (mt_socket*)v[0].value.p;
  mt_string* text = (mt_string*)v[1].value.p;
  mt_bstr bs;
  mf_encode_utf8(&bs,text->size,text->a);

  long total = bs.size+1;
  long sent=0;
  int bytes;
  while(sent<total){
    bytes = write(s->id,bs.a+sent,total-sent);
    if(bytes<0){
      mf_std_exception("Exception in s.send(text): error while sending.");
      mf_free(bs.a);
      return 1;
    }else if(bytes==0){
      break;
    }
    sent+=bytes;
  }
  mf_free(bs.a);
  x->type=mv_null;
  return 0;
}

static
int mf_socket_receive(mt_object* x, int argc, mt_object* v){
  if(!mf_isa(&v[0],socket_type)){
    mf_type_error1("in s.send(text): s (type: %s) is not a socket.",&v[0]);
    return 1;
  }
  mt_socket* s = (mt_socket*)v[0].value.p;
  mt_bs bs;
  mf_bs_init(&bs);
  char buffer[4096];
  int count;
  while(1){
    count = recv(s->id,buffer,4096,0);
    if(count<0){
      mf_std_exception("s.receive(): error while reveiving message.");
      mf_bs_delete(&bs);
      return 1;
    }else if(count==0){
      break;
    }
    mf_bs_push(&bs,count,buffer);
  }
  mt_bstring* u8 = mf_buffer_to_bstring(bs.size,(unsigned char*)bs.a);
  mf_bs_delete(&bs);
  x->type=mv_bstring;
  x->value.p=(mt_basic*)u8;
  return 0;
}

static
int mf_socket_close(mt_object* x, int argc, mt_object* v){
  if(!mf_isa(&v[0],socket_type)){
    mf_type_error1("in s.send(text): s (type: %s) is not a socket.",&v[0]);
    return 1;
  }
  mt_socket* s = (mt_socket*)v[0].value.p;
  close(s->id);
  x->type=mv_null;
  return 0;
}

mt_table* mf_socket_load(void){
  mt_map* m;
  socket_type = mf_table(NULL);
  socket_type->m = mf_empty_map();
  m = socket_type->m;
  mf_insert_function(m,1,1,"connect",mf_socket_connect);
  mf_insert_function(m,1,1,"send",mf_socket_send);
  mf_insert_function(m,0,0,"receive",mf_socket_receive);
  mf_insert_function(m,0,0,"close",mf_socket_close);
  mt_table* socket = mf_table(NULL);
  socket->m=mf_empty_map();
  m=socket->m;
  mf_insert_function(m,0,0,"socket",mf_socket);
  m->frozen=1;
  return socket;
}

