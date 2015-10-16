
#ifndef RBUF_RATIO_SIZE
# define RBUF_RATIO_SIZE (1024 * 1024)
#endif

#ifndef RBUF_DEFAULT_SIZE
# define RBUF_DEFAULT_SIZE 512
#endif

typedef struct {
	char *ptr;
	size_t len;
	size_t cap;
	char data_[RBUF_DEFAULT_SIZE];
} rbuf_t;

void rbuf_init(rbuf_t * self);
void rbuf_free(rbuf_t * self);
void rbuf_clear(rbuf_t * self);
int rbuf_catlen(rbuf_t * self, const void * data, size_t len);
int rbuf_cat(rbuf_t * self, const char *data);
size_t rbuf_len(rbuf_t * self);
int rbuf_catprintf(rbuf_t * self, const char * fmt, ...);
int rbuf_catrepr(rbuf_t * s, const char *p, size_t len);

