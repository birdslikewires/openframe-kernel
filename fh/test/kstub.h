/* Just enough of the kernel for test_guards.c to compile fh.c in user space. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#ifndef __linux__
typedef long long loff_t;
#endif
#define __user
#define __init
#define __exit
#define ERESTARTSYS 512
#define GFP_KERNEL 0
#define FMODE_WRITE 2
#define THIS_MODULE NULL
#define MISC_DYNAMIC_MINOR 255
#define printk printf
#define module_param(n, t, p)
#define MODULE_PARM_DESC(n, d)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)
#define module_init(f)
#define module_exit(f)
struct semaphore { int n; };
struct inode { int unused; };
struct file { unsigned int f_mode; };
struct class;
struct file_operations {
	void *owner;
	loff_t (*llseek)(struct file *, loff_t, int);
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	ssize_t (*write)(struct file *, const char *, size_t, loff_t *);
	ssize_t (*read)(struct file *, char *, size_t, loff_t *);
};
struct miscdevice { int minor; const char *name; const struct file_operations *fops; };
static inline void sema_init(struct semaphore *s, int n) { s->n = n; }
static inline int down_interruptible(struct semaphore *s) { s->n--; return 0; }
static inline void up(struct semaphore *s) { s->n++; }
static inline void *kmalloc(size_t n, int f) { return malloc(n); }
static inline void kfree(void *p) { free(p); }
static inline void msleep(int x) {}
static inline void mdelay(int x) {}
static inline void udelay(int x) {}
static inline void *ioremap(unsigned long a, unsigned long n) { return calloc(1, n); }
static inline void iounmap(void *p) {}
static inline int misc_register(struct miscdevice *m) { return 0; }
static inline void misc_deregister(struct miscdevice *m) {}
static inline loff_t fixed_size_llseek(struct file *f, loff_t o, int w, loff_t s) { return o; }
unsigned long test_copy_from_user(void *d, const void *s, unsigned long n);
#define copy_from_user(d, s, n) test_copy_from_user(d, s, n)
static inline unsigned long copy_to_user(void *d, const void *s, unsigned long n) { memcpy(d, s, n); return 0; }
