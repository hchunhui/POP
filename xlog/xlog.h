#ifndef _XLOG_H_
#define _XLOG_H_

#define XLOG_DEBUG 0
#define XLOG_INFO  16
#define XLOG_ERROR 32

void xlog_set_verbose(int level);
void xlog(int level, char *fmt, ...);
int xlog_get_verbose(void);

#define xdebug(...) xlog(XLOG_DEBUG, __VA_ARGS__)
#define xinfo(...) xlog(XLOG_INFO, __VA_ARGS__)
#define xerror(...) xlog(XLOG_ERROR, __VA_ARGS__)

#endif /* _XLOG_H_ */
