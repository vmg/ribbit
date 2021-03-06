#include "ruby.h"
#include <git/common.h>
#include <git/oid.h>
#include <git/odb.h>
#include <git/commit.h>
#include <git/revwalk.h>

static VALUE rb_cRibbit;

/*
 * Ribbit Lib
 */

static VALUE rb_cRibbitLib;

static VALUE rb_git_hex_to_raw(VALUE self, VALUE hex)
{
  Check_Type(hex, T_STRING);
  git_oid oid;
  git_oid_mkstr(&oid, RSTRING_PTR(hex));
  return rb_str_new((&oid)->id, 20);
}

static VALUE rb_git_raw_to_hex(VALUE self, VALUE raw)
{
  Check_Type(raw, T_STRING);
  git_oid oid;
  git_oid_mkraw(&oid, RSTRING_PTR(raw));
  char out[40];
  git_oid_fmt(out, &oid);
  return rb_str_new(out, 40);
}

/*
 * Ribbit Object Database
 */

static VALUE rb_cRibbitOdb;

static VALUE rb_git_odb_init(VALUE self, VALUE path) {
  rb_iv_set(self, "@path", path);

  git_odb *odb;
  git_odb_open(&odb, RSTRING_PTR(path));

  rb_iv_set(self, "odb", (VALUE)odb);
}

static VALUE rb_git_odb_exists(VALUE self, VALUE hex) {
  git_odb *odb;
  odb = (git_odb*)rb_iv_get(self, "odb");

  git_oid oid;
  git_oid_mkstr(&oid, RSTRING_PTR(hex));

  int exists = git_odb_exists(odb, &oid);
  if(exists == 1) {
    return Qtrue;
  }
  return Qfalse;
}

/*
typedef struct {
	void *data;     // Raw, decompressed object data.
	size_t len;     // Total number of bytes in data.
	git_otype type; // Type of this object.
} git_obj;
*/

static VALUE rb_git_odb_read(VALUE self, VALUE hex) {
  git_odb *odb;
  odb = (git_odb*)rb_iv_get(self, "odb");

  git_oid oid;
  git_oid_mkstr(&oid, RSTRING_PTR(hex));

  git_obj obj;

  int read = git_odb_read(&obj, odb, &oid);
  if(read == GIT_SUCCESS) {
    VALUE ret_arr = rb_ary_new();

    unsigned char *data = (&obj)->data;
    rb_ary_store(ret_arr, 0, rb_str_new2(data));

    rb_ary_store(ret_arr, 1, INT2FIX((int)(&obj)->len));

    const char *str_type;
    git_otype git_type = (&obj)->type;
    str_type = git_obj_type_to_string(git_type);
    rb_ary_store(ret_arr, 2, rb_str_new2(str_type));
    return ret_arr;
  }
  return Qfalse;
}

static VALUE rb_git_odb_obj_hash(VALUE self, VALUE content, VALUE type) {
  git_obj obj;
  git_oid oid;
  (&obj)->data = RSTRING_PTR(content);
  (&obj)->len  = RSTRING_LEN(content);
  (&obj)->type = git_obj_string_to_type(RSTRING_PTR(type));
  int result = git_obj_hash(&oid, &obj);
  if(result == GIT_SUCCESS) {
    char out[40];
    git_oid_fmt(out, &oid);
    return rb_str_new(out, 40);
  }
  return Qfalse;
}

static VALUE rb_git_odb_write(VALUE self, VALUE content, VALUE type) {
  git_odb *odb;
  odb = (git_odb*)rb_iv_get(self, "odb");

  git_obj obj;
  git_oid oid;
  (&obj)->data = RSTRING_PTR(content);
  (&obj)->len  = RSTRING_LEN(content);
  (&obj)->type = git_obj_string_to_type(RSTRING_PTR(type));
  git_obj_hash(&oid, &obj);

  int result = git_odb_write(&oid, odb, &obj);
  if(result == GIT_SUCCESS) {
    char out[40];
    git_oid_fmt(out, &oid);
    return rb_str_new(out, 40);
  }
  return Qfalse;
}


static VALUE rb_git_odb_close(VALUE self) {
  git_odb *odb;
  odb = (git_odb*)rb_iv_get(self, "odb");
  git_odb_close(odb);
}

/*
 * Ribbit Revwalking
 */

static VALUE rb_cRibbitWalker;

static VALUE rb_git_walker_init(VALUE self, VALUE path) {
  rb_iv_set(self, "@path", path);

  git_odb *odb;
  git_odb_open(&odb, RSTRING_PTR(path));

  git_revpool *pool;
  pool = gitrp_alloc(odb);

  rb_iv_set(self, "pool", (VALUE)pool);
}

static VALUE rb_git_walker_push(VALUE self, VALUE hex) {
  git_revpool *pool;
  pool = (git_revpool*)rb_iv_get(self, "pool");

  git_oid oid;
  git_oid_mkstr(&oid, RSTRING_PTR(hex));

  git_commit *commit;
  commit = git_commit_lookup(pool, &oid);

  gitrp_push(pool, commit);
  return Qnil;
}

static VALUE rb_git_walker_next(VALUE self) {
  git_revpool *pool;
  pool = (git_revpool*)rb_iv_get(self, "pool");

  git_commit *commit;
  commit = gitrp_next(pool);
  if(commit) {
    const git_oid *oid;
    oid = git_commit_id(commit);

    char out[40];
    git_oid_fmt(out, oid);
    return rb_str_new(out, 40);
  }
  return Qfalse;
}

static VALUE rb_git_walker_hide(VALUE self, VALUE hex) {
  git_revpool *pool;
  pool = (git_revpool*)rb_iv_get(self, "pool");

  git_oid oid;
  git_oid_mkstr(&oid, RSTRING_PTR(hex));

  git_commit *commit;
  commit = git_commit_lookup(pool, &oid);

  gitrp_hide(pool, commit);
  return Qnil;
}

static VALUE rb_git_walker_sorting(VALUE self, VALUE ruby_sort_mode) {
  git_revpool *pool;
  pool = (git_revpool*)rb_iv_get(self, "pool");
  unsigned int sort_mode = FIX2INT(ruby_sort_mode);
  gitrp_sorting(pool, sort_mode);
  return Qnil;
}

static VALUE rb_git_walker_reset(VALUE self) {
  git_revpool *pool;
  pool = (git_revpool*)rb_iv_get(self, "pool");
  gitrp_reset(pool);
  return Qnil;
}

//   GIT_EXTERN(void) gitrp_free(git_revpool *pool);

/*
 * Ribbit Init Call
 */

void
Init_ribbit()
{
  rb_cRibbit = rb_define_class("Ribbit", rb_cObject);
  rb_cRibbitLib = rb_define_class_under(rb_cRibbit, "Lib", rb_cObject);

  rb_define_module_function(rb_cRibbitLib, "hex_to_raw", rb_git_hex_to_raw, 1);
  rb_define_module_function(rb_cRibbitLib, "raw_to_hex", rb_git_raw_to_hex, 1);

  rb_cRibbitOdb = rb_define_class_under(rb_cRibbit, "Odb", rb_cObject);
  rb_define_method(rb_cRibbitOdb, "initialize", rb_git_odb_init, 1);
  rb_define_method(rb_cRibbitOdb, "exists", rb_git_odb_exists, 1);
  rb_define_method(rb_cRibbitOdb, "read",   rb_git_odb_read,   1);
  rb_define_method(rb_cRibbitOdb, "close",  rb_git_odb_close,  0);
  rb_define_method(rb_cRibbitOdb, "hash",   rb_git_odb_obj_hash,  2);
  rb_define_method(rb_cRibbitOdb, "write",  rb_git_odb_write,  2);
  //rb_define_method(rb_cRibbitOdb, "get_commit",  rb_git_commit_lookup,  1);

  rb_cRibbitWalker = rb_define_class_under(rb_cRibbit, "Walker", rb_cObject);
  rb_define_method(rb_cRibbitWalker, "initialize", rb_git_walker_init, 1);
  rb_define_method(rb_cRibbitWalker, "push", rb_git_walker_push, 1);
  rb_define_method(rb_cRibbitWalker, "hide", rb_git_walker_hide, 1);
  rb_define_method(rb_cRibbitWalker, "next", rb_git_walker_next, 0);
  rb_define_method(rb_cRibbitWalker, "reset", rb_git_walker_reset, 0);
  rb_define_method(rb_cRibbitWalker, "sorting", rb_git_walker_sorting, 1);

  rb_define_const(rb_cRibbit, "SORT_NONE", INT2FIX(0));
  rb_define_const(rb_cRibbit, "SORT_TOPO", INT2FIX(1));
  rb_define_const(rb_cRibbit, "SORT_DATE", INT2FIX(2));
  rb_define_const(rb_cRibbit, "SORT_REVERSE", INT2FIX(4));
}

