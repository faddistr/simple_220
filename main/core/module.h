#ifndef MODULE_H
#define MODULE_H

typedef void(*initcall_t)(void);
#define module_init(fn) \
	 static const initcall_t __initcall_##fn  __attribute__((used)) \
	__attribute__((__section__(".initcall_simple"))) = fn

void module_init_all(void);
#endif