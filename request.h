#ifndef __REQUEST_H__

//void requestHandle(int fd);
void requestHandle(buff_t* node);
void requestError(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void requestReadhdrs(rio_t *rp);
int requestParseURI(char *uri, char *filename, char *cgiargs);
void requestGetFiletype(char *filename, char *filetype);
void requestServeDynamic(int fd, char *filename, char *cgiargs);
void requestServeStatic(int fd, char *filename, int filesize);
#endif
