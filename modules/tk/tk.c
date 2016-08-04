
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <moss.h>
#include <modules/tk.h>

void mf_file_read(FILE* f, mt_bstr* data){
  long size;
  fseek(f,0,SEEK_END);
  size = ftell(f);
  data->a = mf_malloc(size+1);
  fseek(f,0,SEEK_SET);
  fread(data->a,1,size,f);
  data->a[size]=0;
  data->size=size;
}

void mf_file_load(const char* id, mt_bstr* data){
  FILE* file = fopen(id,"rb");
  if(file==NULL){
    printf("File %s could not be opened.\n",id);
    exit(1);
  }
  mf_file_read(file,data);
  fclose(file);
}

void mf_file_write(FILE* f, mt_bstr* data){
  fwrite(data->a,1,data->size,f);
}

char mv_file_save_error[200];
int mf_file_save(const char* id, mt_bstr* data){
  FILE* file = fopen(id,"wb");
  if(file==0){
    snprintf(mv_file_save_error,200,"File '%s' could not be opened to write.",id);
    return 1;
  }
  mf_file_write(file,data);
  fclose(file);
  return 0;
}

// deprecated
void file_read(FILE* f, bstring* data){
  int size;
  fseek(f,0,SEEK_END);
  size = ftell(f);
  data->a = mf_malloc(size+1);
  data->allocated=1;
  fseek(f,0,SEEK_SET);
  fread(data->a,1,size,f);
  data->a[size]=0;
  data->size = size;
}

void file_load(const char* id, bstring* data){
  FILE* file = fopen(id,"rb");
  if(file==0){
    printf("File %s could not be opened.\n",id);
    exit(1);
  }
  file_read(file,data);
  fclose(file);
}

