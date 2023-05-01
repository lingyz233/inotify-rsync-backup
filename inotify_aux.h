#ifndef __INOTIFY_RSYNC_H__
#define __INOTIFY_RSYNC_H__

#include<sys/inotify.h>

struct inotify_event *
read_inotify (int _fd, int read_latency_ms);

#endif
