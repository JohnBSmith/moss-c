
#include <gtk/gtk.h>
#include <moss.h>
#include <objects/list.h>
#include <modules/str.h>

mt_table* gui_type;

typedef struct{
  long refcount;
  mt_object prototype;
  void (*del)(void*);
  GtkApplication* app;
} mt_gui;

void gui_delete(mt_gui* gui){
  mf_dec_refcount(&gui->prototype);
  g_object_unref (gui->app);
}

static
void activate (GtkApplication* app, gpointer user_data){
  GtkWidget *window;
  window = gtk_application_window_new (app);
  gtk_window_set_title(GTK_WINDOW (window), "");
  gtk_window_set_default_size(GTK_WINDOW (window), 720, 480);

  GtkWidget* button_box;
  GtkWidget* button;

  button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add (GTK_CONTAINER (window), button_box);

  button = gtk_button_new_with_label ("Hello World");
  gtk_container_add (GTK_CONTAINER (button_box), button);
  button = gtk_button_new_with_label ("lala");
  gtk_container_add (GTK_CONTAINER (button_box), button);
  button = gtk_button_new_with_label ("lila");
  gtk_container_add (GTK_CONTAINER (button_box), button);

  gtk_widget_show_all(window);
}

int gui_new(mt_object* x, int argc, mt_object* v){
  mt_gui* gui = mf_malloc(sizeof(mt_gui));
  gui->refcount=1;
  gui_type->refcount++;
  gui->prototype.type=mv_table;
  gui->prototype.value.p=(mt_basic*)gui_type;
  gui->del = (void(*)(void*))gui_delete;
  gui->app = gtk_application_new("moss.app",G_APPLICATION_FLAGS_NONE);
  x->type=mv_native;
  x->value.p=(mt_basic*)gui;
  return 0;
}

int gui_run(mt_object* x, int argc, mt_object* v){
  if(!mf_isa(&v[0],gui_type)){
    mf_type_error1("in gui.run(): gui (type: %s) is not a gui.",&v[0]);
    return 1;
  }
  mt_gui* gui = (mt_gui*)v[0].value.p;
  g_signal_connect (gui->app, "activate", G_CALLBACK (activate), NULL);
  int status = g_application_run (G_APPLICATION (gui->app),0,NULL);
  x->type=mv_int;
  x->value.i=status;
  return 0;
}

mt_table* mf_gui_load(){
  mt_table* gui = mf_table(NULL);
  gui_type = mf_table(NULL);
  gui_type->m = mf_empty_map();
  mt_map* m = gui_type->m;
  mf_insert_function(m,0,0,"run",gui_run);

  gui->m = mf_empty_map();
  m = gui->m;
  mf_insert_function(m,0,0,"new",gui_new);
  return gui;
}


