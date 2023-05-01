#include<sys/inotify.h>
#include<sys/poll.h>
#include<sys/wait.h>
#include<unistd.h>

#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include<glib.h>

#include"read_config.h"
#include"inotify_aux.h"

// gcc ./inotify_rsync.c `pkg-config --cflags --libs glib-2.0 lua5.4`

struct cmdv {
  size_t count;
  const char * argv [];
} const rsync_cmd_template = {
  7, 
  {
    "/usr/bin/rsync", "--temp-dir=/dev/shm/", "--delete", "--dirs", "--relative", "-lptgoD", NULL
  }
};

struct node {
  int wd;
  const char * pname;
  const char * pdest;
};

int
exec_rsync_cmd (const char * src, const char * dst);

/*
int
recursive_inotify_rm (int fd, int wd, GHashTable * w2p, GHashTable * w2dst)
{
  #define BUFFSIZE (4096*4)
  int wd = inotify_rm_watch (fd, wd);
  int cpid = exec_rsync_cmd (pathname_storage, dst);

  struct node * node = g_hash_table_lookup (w2p, &wd);
  g_hash_table_remove (w2p, &wd);
  g_hash_table_remove (w2dst, &node->wd);

  char * pathname = node->pname;

  if (g_file_test (pathname, G_FILE_TEST_IS_DIR))
  {
    GDir * dir = g_dir_open (pathname, 0, NULL);
    char * buff = malloc (BUFFSIZE);
    int pathlen = strlen (pathname);
    assert (pathlen+1 < BUFFSIZE);
    memcpy (buff, pathname, pathlen);
    buff[pathlen++] = '/';
    for (const char * name; name = g_dir_read_name (dir); )
    {
      assert (pathlen + 1 + strlen (name) + 2 < BUFFSIZE);
      strcpy (buff+pathlen, name);
      if (g_file_test (buff, G_FILE_TEST_IS_DIR))
      {
	    (buff+pathlen)[strlen (name)] = '/';
	    (buff+pathlen)[strlen (name)+1] = 0;
        recursive_inotify_remove (fd, buff, dst, mask, w2p, w2dst);
		waitpid (cpid, NULL, 0);
      }    
    }
    free (buff);
  }

  free (node);
  #undef BUFFSIZE
  return 1;
}
*/


int
recursive_inotify_add (int fd, const char * pathname, const char * dst, uint32_t mask, GHashTable * w2p)
{
  #define BUFFSIZE (4096*4)
  _Bool cur_is_dir = g_file_test (pathname, G_FILE_TEST_IS_DIR);
  char * pathname_storage;

  if (cur_is_dir)
    pathname_storage = g_build_filename (pathname, "/", NULL);
  else
  {
    char * pathname_storage = malloc (strlen (pathname) + 1);
    strcpy (pathname_storage, pathname);
  }

  int wd = inotify_add_watch (fd, pathname_storage, mask);
  int cpid = exec_rsync_cmd (pathname_storage, dst);

  struct node * node = malloc (sizeof (node));
  node->wd = wd, node->pname = pathname_storage, node->pdest = dst;
  g_hash_table_insert (w2p, &node->wd, node);

  if (cur_is_dir)
  {
    GDir * dir = g_dir_open (pathname, 0, NULL);
    char * buff = malloc (BUFFSIZE);
    int pathlen = strlen (pathname);
    assert (pathlen+1 < BUFFSIZE);
    memcpy (buff, pathname, pathlen);
    buff[pathlen++] = '/';
    for (const char * name; name = g_dir_read_name (dir); )
    {
      assert (pathlen + 1 + strlen (name) + 2 < BUFFSIZE);
      strcpy (buff+pathlen, name);
      if (g_file_test (buff, G_FILE_TEST_IS_DIR))
      {
	    (buff+pathlen)[strlen (name)] = '/';
	    (buff+pathlen)[strlen (name)+1] = 0;
        int cwd = recursive_inotify_add (fd, buff, dst, mask, w2p); //max rsync process count is the depth of directory
		if (cpid != -1)
		{
          waitpid (cpid, NULL, 0);
          cpid = -1;
        }
      }    
    }
    free (buff);
  }
  #undef BUFFSIZE
  return wd;
}

static inline int
forkexecv (const char * name, char * const argv [])
{
  int pid;
  if (pid = fork ())
    return pid;

  printf ("[pid=%d] exec : \"", (int)getpid ());
  for (int i = 0; argv[i]; i++)
    printf ("%s ", argv[i]);
  printf ("\"\n");
  _exit (execv (name, argv));
}

int
exec_rsync_cmd (const char * src, const char * dst)
{
  static size_t sizezz;
  static struct cmdv * rsync_cmd;
  static const char ** charpp;

  if (!sizezz)
    sizezz = sizeof (rsync_cmd_template) + sizeof (char*) * rsync_cmd_template.count;
  if (!rsync_cmd)
  {
    rsync_cmd = malloc (sizezz + sizeof (char*) * 2);
    memcpy (rsync_cmd, &rsync_cmd_template, sizezz);
    rsync_cmd->count += 2;
    charpp = rsync_cmd->argv;
    while (*++charpp)
      ;
  }

  *charpp = src;
  *(charpp+1) = dst;
  *(charpp+2) = NULL;
  return forkexecv (rsync_cmd->argv[0], (char**)rsync_cmd->argv);
}
 
int
main (int argc, char ** argv)
{
  char * conffile = "./config.lua";
  if (argc >= 2)
    conffile = argv[1];
  else
    fprintf (stderr, "no configuration file specified, use default : %s\n", conffile);

  struct config * tobackup = read_config (conffile);

  int notifyfd = inotify_init ();

  GHashTable * wd_to_path;
  GHashTable * wd_to_dst;
  wd_to_path = g_hash_table_new (g_int_hash, g_int_equal);

  for (int i = 0; i < tobackup->count; i++)
  {
    printf ("%s -> %s\n", tobackup->v[i].src, tobackup->v[i].dst);
    recursive_inotify_add (notifyfd, tobackup->v[i].src, tobackup->v[i].dst, (IN_ALL_EVENTS | IN_IGNORED | IN_EXCL_UNLINK) & ~(IN_CLOSE_NOWRITE | IN_ACCESS | IN_OPEN), wd_to_path);
  }
  printf ("build tree OK\n");

  GHashTable * ht;
  {
    guint h (gconstpointer key)
    {
      struct inotify_event * ptr = (struct inotify_event*)key;
      return g_int_hash (&ptr->wd);
    }
    gboolean e (gconstpointer a, gconstpointer b)
    {
      struct inotify_event * pa, * pb;
	  pa = (struct inotify_event*)a, pb = (struct inotify_event*)b;
      return (pa->wd == pb->wd);
    }
    ht = g_hash_table_new (h, e);
  }

  for (struct inotify_event * ptr, * oldptr = ptr; ptr = read_inotify (notifyfd, 100); oldptr = ptr)
  {
    if (oldptr >= ptr)
      g_hash_table_remove_all (ht);
    if (ptr->mask & IN_IGNORED)
	  printf ("IN_IGNORED\n");
    if (ptr->mask & (IN_CREATE|IN_MOVED_TO) && ptr->len)
    {
      printf ("got IN_CREATE | IN_MOVED_TO\n");
      struct node * node = g_hash_table_lookup (wd_to_path, &ptr->wd);
      const char * path = node->pname;
      const char * dst = node->pdest;
      size_t sz = strlen (path) + strlen (dst) + 2;
      char * buff = malloc (sz);
      snprintf (buff, sz, "%s/%s", path, ptr->name);
      if (g_file_test (buff, G_FILE_TEST_IS_DIR))
        printf ("isdir\n"),recursive_inotify_add (notifyfd, buff, dst, (IN_ALL_EVENTS | IN_IGNORED | IN_EXCL_UNLINK) & ~(IN_CLOSE_NOWRITE | IN_ACCESS | IN_OPEN), wd_to_path);
      free (buff);
    }
	if (ptr->mask & (IN_DELETE_SELF | IN_MOVED_FROM))
	  printf ("got IN_DELETE_SELF | IN_MOVED_FROM\n");
	if (g_hash_table_contains (ht, ptr))
	  continue;
    g_hash_table_insert (ht, ptr, NULL);
    struct node * node = g_hash_table_lookup (wd_to_path, &ptr->wd);
    const char * path = node->pname;
    const char * dst = node->pdest;
	puts (path);
	puts (dst);

    exec_rsync_cmd (path, dst);

#define print_test(X,Y)  if ((X) & (Y)) printf(#X " & " #Y)
#define print_cmp(X,Y)  if ((X) == (Y)) printf(#X " == " #Y)
/*    print_test (ptr->mask, IN_CREATE);
    print_test (ptr->mask, IN_DELETE);
    print_test (ptr->mask, IN_ACCESS);
    print_test (ptr->mask, IN_ATTRIB);
    print_test (ptr->mask, IN_CLOSE_WRITE);
    print_test (ptr->mask, IN_CLOSE_NOWRITE);
    print_test (ptr->mask, IN_DELETE_SELF);
    print_test (ptr->mask, IN_MODIFY);
    print_test (ptr->mask, IN_MOVE_SELF);
    print_test (ptr->mask, IN_MOVED_FROM);
    print_test (ptr->mask, IN_MOVED_TO);
    print_test (ptr->mask, IN_OPEN);
    printf (" 0x%x\n", ptr->mask); */
#undef print_test
#undef print_cmp
  }

  free (tobackup);

  return 0;
}
