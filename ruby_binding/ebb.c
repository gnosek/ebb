#include <ruby.h>
#include "../ebb.h"

void Init_ebb()
{
  VALUE mEbb = rb_define_module("Ebb");
  VALUE cServer = rb_define_class_under(mEbb, "Server", rb_cObject);
  
  rb_define_singleton_method(cServer, "new", rb_ebb_new, 0);
  rb_define_method(cServer, "start", rb_ebb_start, 2);
  rb_define_method(cServer, "start", rb_ebb_start, 2);
  
}