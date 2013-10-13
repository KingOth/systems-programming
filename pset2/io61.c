#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

// io61.c
//    YOUR CODE HERE!
#define BUFSIZE 8192

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;                /* descriptor for this internal buf */
    int cnt;               /* unread bytes in internal buf */
    int towrite;			/* bytes in buffer that can be written in subsequent order */
    int seeked;				/* if the file has been seeked, the buffer will not work */
    char *bufptr;          /* next unread byte in internal buf */
    char buf[BUFSIZE]; /* internal buffer */
};


// io61_fdopen(fd, mode)
//    Return a new io61_file that reads from and/or writes to the given
//    file descriptor `fd`. `mode` is either O_RDONLY for a read-only file
//    or O_WRONLY for a write-only file. You need not support read/write
//    files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f->fd = fd;
    f->cnt = 0;
    f->towrite = 0;
    f->seeked = 0;
    f->bufptr = f->buf;
    (void) mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f`.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    free(f);
    return r;
}

// reset the buffer if the file has been seeked
void io61_resetbuffer(io61_file* f) {
    f->cnt = 0;
    f->towrite = 0;
    f->bufptr = f->buf;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
	unsigned char buf[1];
	if(io61_read(f, buf, 1) == 1)	/* use the buffer if reading in sequential order */
		return buf[0];
	else
		return EOF;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    unsigned char buf[1];
    buf[0] = ch;
    if (io61_write(f, buf, 1) == 1)	/* use the buffer if writing in sequential order */
        return 0;
    else
        return -1;
}


// io61_flush(f)
//    Forces a write of any `f` buffers that contain data.

int io61_flush(io61_file* f) {
	if(f->towrite > 0)
		write(f->fd, f->bufptr, f->towrite);	/* if in sequential mode */
    return 0;
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count if the file ended before `sz` characters could be read. Returns
//    -1 an error occurred before any characters were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {

    int cnt;

    while (f->cnt <= 0 || f->seeked) {  /* refill if buf is empty or file position updated */
	f->cnt = read(f->fd, f->buf, BUFSIZE);
	if (f->cnt < 0) {
	    if (errno != EINTR) /* interrupted by sig handler return */
		return -1;
	}
	else if (f->cnt == 0)  /* EOF */
	    return 0;
	else {
	    f->bufptr = f->buf; /* reset buffer ptr */
	    f->seeked = 0;
	}
    }

    /* Copy min(sz, f->cnt) bytes from internal buf to user buf */
    cnt = (f->cnt < sz)?f->cnt:sz;
    memcpy(buf, f->bufptr, cnt);
    f->bufptr += cnt;
    f->cnt -= cnt;
    return cnt;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
	int cnt;
	if(f->seeked) { /* buffer content would be outdated */
		io61_resetbuffer(f);
		f->seeked = 0;
		return write(f->fd, buf, sz);
	}
	if(f->bufptr + f->towrite + sz > f->buf + BUFSIZE){ /* if the buffer will be full */
		while (f->cnt <= 0) {
			f->cnt = write(f->fd, f->bufptr, f->towrite);
			if (f->cnt < 0) {
	   			if (errno != EINTR)
					return -1;
			}
			else if (f->cnt == 0)
	  	 		return 0;
			else 
	  	 	f->bufptr += f->cnt; 
		}
		f->towrite -= f->cnt;
		cnt = f->cnt;
		f->cnt = 0;
	}
	if(f->towrite <= 0){ /* resets buffer once it becomes empty */
		f->bufptr = f->buf;
	}
	if(f->bufptr + f->towrite + sz <= f->buf + BUFSIZE){ /* when data can still fit */
		if(sizeof(buf) > 0){
			if(memcpy(f->bufptr + f->towrite, buf, sz)){
				f->towrite += sz;
				cnt = sz;
			}
			else
				return -1;
		}
		else
			return -1;
	}
	return cnt;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, size_t pos) {
    off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
    if (r == (off_t) pos){
    	f->seeked = 1;	/* notifies read() and write() if the file pointer changed */
        return 0;
        }
    else
        return -1;
}


// You should not need to change either of these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename)
        fd = open(filename, mode);
    else if (mode == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode);
}


// io61_filesize(f)
//    Return the number of bytes in `f`. Returns -1 if `f` is not a seekable
//    file (for instance, if it is a pipe).

ssize_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode) && s.st_size <= SSIZE_MAX)
        return s.st_size;
    else
        return -1;
}
