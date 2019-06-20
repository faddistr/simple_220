#ifndef PARTCL_LIB
#define PARTCL_LIB
typedef char tcl_value_t;

struct tcl {
  struct tcl_env *env;
  struct tcl_cmd *cmds;
  tcl_value_t *result;
};

enum { FERROR, FNORMAL, FRETURN, FBREAK, FAGAIN };

void tcl_init(struct tcl *tcl);
void tcl_destroy(struct tcl *tcl);
int tcl_eval(struct tcl *tcl, const char *s, size_t len);

const char *tcl_string(tcl_value_t *v);
int tcl_int(tcl_value_t *v);
int tcl_length(tcl_value_t *v);
#endif
