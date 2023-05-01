#include"inotify_aux.h"

#include<sys/inotify.h>
#include<sys/poll.h>
#include<unistd.h>

#include<stdlib.h>

// gcc ./inotify_rsync.c `pkg-config --cflags --libs glib-2.0 lua5.4`

static inline struct inotify_event *
inotify_event_iter (struct inotify_event * p)
{
  void * ptr = p;
  return ptr + p->len + sizeof (struct inotify_event);
}

struct inotify_event *
read_inotify (int _fd, int read_latency_ms)
{
  #define BUFFERSIZE (4096 * 16)
  static void * buff;
  static struct inotify_event * ptr;
  static size_t sz;
  static struct pollfd pfd;

  if (!buff)
    buff = malloc (BUFFERSIZE);
  if (!pfd.fd)
  {
    pfd.fd = _fd;
    pfd.events = POLLIN;
  }

  if (ptr)
    ptr = inotify_event_iter (ptr);
  if (((void*)ptr - buff) >= sz)
  {
	poll (&pfd, 1, -1);
	poll (NULL, 0, read_latency_ms);
    sz = read (pfd.fd, buff, BUFFERSIZE);
	if (!sz)
	  return NULL;
    ptr = buff;
  }
  return ptr;
  #undef BUFFERSIZE
}
