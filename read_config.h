#ifndef __READ_CONFIG_H__
#define __READ_CONFIG_H__

#include<stddef.h>

struct src_dst {
  const char * src, * dst;
};

struct config {
  size_t count;
  struct src_dst v [];
};

struct config *
read_config (char * path);

#endif
