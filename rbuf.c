#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include "rbuf.h"

// #define ENABLE_BUF_TEST 1
// #define ENABLE_RBUF_DEB

#ifdef ENABLE_RBUF_DEB
# define DLOG(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
# define DLOG(fmt, ...) do {} while(0)
#endif


static int rbuf_upsize(rbuf_t *self, size_t require)
{
	size_t newlen = self->len + require;
	size_t newcap = self->cap;
	if (self->cap > newlen) {
		return 0;
	}

	do {
		if (newcap < RBUF_RATIO_SIZE) {
			newcap *= 2;
			continue;
		}
		newcap += RBUF_RATIO_SIZE;
	} while (newcap < newlen);

	if (self->ptr != self->data_) {
		self->ptr = realloc(self->ptr, newcap);
		if (self->ptr == NULL) {
			return -1;
		}
	} else {
		self->ptr = malloc(newcap);
		if (self->ptr == NULL) {
			return -1;
		}
		memcpy(self->ptr, self->data_, self->len);
	}
	self->cap = newcap;
	return 1;
}

void rbuf_init(rbuf_t * self)
{
	self->len = 0;
	self->data_[0] = 0;
	self->cap = sizeof(self->data_);
	self->ptr = self->data_;
}

void rbuf_free(rbuf_t * self)
{
	if (self->ptr != self->data_) {
		free(self->ptr);
		self->ptr = self->data_;
	}
}

void rbuf_clear(rbuf_t * self)
{
	self->len = 0;
}

int rbuf_catlen(rbuf_t * self, const void * data, size_t len)
{
	if (rbuf_upsize(self, len + 1) < 0 ) {
		return -1;
	}
	DLOG("cap=%d,len=%d,dlen=%d", self->cap, self->len, len);
	memcpy(self->ptr + self->len, data, len);
	self->len += len;
	self->ptr[self->len] = '\0';
	return 0;
}

int rbuf_cat(rbuf_t * self, const char *data)
{
	return rbuf_catlen(self, data, strlen(data));
}

size_t rbuf_len(rbuf_t * self)
{
	return self->len;
}

int rbuf_catprintf(rbuf_t * self, const char * fmt, ...)
{
	int n, ret;
	const int max_len = 500 * 1024 * 1024;
	char mybuf[1024];
	va_list ap;
	char *tmp = (char *)&mybuf;
	int tmplen = sizeof(mybuf);
	do {
		va_start(ap, fmt);
		n = vsnprintf(tmp, tmplen, fmt, ap);
		va_end(ap);
		if (n < tmplen) {
			break;
		}
		if (n >= max_len) {
			return -1;
		}
		tmplen *= 2;
		if (tmp != mybuf) {
			tmp = realloc(tmp, tmplen);
		} else {
			tmp = malloc(tmplen);
		}
	} while (1);
	ret = rbuf_catlen(self, tmp, n);
	if (tmp != mybuf) {
		free(tmp);
	}
	return ret;
}

int rbuf_catrepr(rbuf_t * s, const char *p, size_t len)
{
	rbuf_catlen(s,"\"",1);
	while(len--) {
		switch(*p) {
		case '\\':
		case '"': 
			rbuf_catprintf(s,"\\%c",*p);
			break;
		case '\n': rbuf_catlen(s,"\\n",2); break;
		case '\r': rbuf_catlen(s,"\\r",2); break;
		case '\t': rbuf_catlen(s,"\\t",2); break;
		case '\a': rbuf_catlen(s,"\\a",2); break;
		case '\b': rbuf_catlen(s,"\\b",2); break;
		default:
			   if (isprint(*p))
				   rbuf_catprintf(s,"%c",*p);
			   else 
				   rbuf_catprintf(s,"\\x%02x",(unsigned char)*p);
			   break;
		}    
		p++; 
	}    
	return rbuf_catlen(s,"\"",1);
}

static void test()
{
	int i;
	const char *repr = "repr begin hello\t\tworld\n \b repr end";
	rbuf_t buf;
	rbuf_init(&buf);
	for (i=0; i<1000; i++) {
		rbuf_catprintf(&buf, "%d\n", i);
	}
	rbuf_catprintf(&buf, "hello\t\tworld %s\n", "kkk");
	rbuf_catrepr(&buf, repr, strlen(repr));
	printf("%s\n", buf.ptr);
	rbuf_free(&buf);
}


#ifdef ENABLE_BUF_TEST
int main(int argc, const char *argv[])
{
	test();
	return 0;
}
#endif  // ENABLE_BUF_TEST

