/*
TeleinFUSE: Teleinfo as Filesystem in Userspace

  Copyright (C) 2010  Romuald Conty <conty.romuald@free.fr>

based on hellofuse with following copyright
   Copyright (C) 2001-2005  Miklos Szeredi <miklos@szeredi.hu>

This program can be distributed under the terms of the GNU GPL.
See the file COPYING.
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <syslog.h>
#include <unistd.h>
#include <time.h>

// Define port serie
#include "teleinfo.h"

#include <time.h>

typedef struct {
  char filename[20];
  char content[30];
  time_t time;
  // time_t td;
} teleinfuse_file;

static pthread_mutex_t teleinfuse_files_mutex = PTHREAD_MUTEX_INITIALIZER;
static teleinfuse_file teleinfuse_files[32];
static size_t teleinfuse_files_count = 0;

teleinfuse_file* teleinfuse_find_file(const char* label)
{
  teleinfuse_file * file = NULL;
  for (size_t n=0; n<teleinfuse_files_count; n++) {
    if (0==strcmp(label, teleinfuse_files[n].filename)) {
      file = &(teleinfuse_files[n]);
      break;
    }
  }
  return file;
}

void teleinfuse_update (const teleinfo_data dataset[], size_t datasetlen)
{
  time_t now = time(NULL);
  teleinfuse_file * file;
  
  pthread_mutex_lock( &teleinfuse_files_mutex );
  for (int n=0; n<datasetlen; n++) {
    if ( (file=teleinfuse_find_file (dataset[n].label)) ) {
      if (0!=strcmp(dataset[n].value, file->content)) {
        strcpy (file->content, dataset[n].value);
        file->time = now;
//         printf ("update: file->filename = %s, file->content = %s;\n", file->filename, file->content);
      } // else do nothing
    } else {
      // New file
      file = &(teleinfuse_files[teleinfuse_files_count]);
      strcpy(file->filename, dataset[n].label);
      strcpy(file->content,  dataset[n].value);
      teleinfuse_files[teleinfuse_files_count].time = now;
//       printf ("new: file->filename = %s, file->content = %s;\n", file->filename, file->content);
      teleinfuse_files_count++;
    }
  }
  pthread_mutex_unlock( &teleinfuse_files_mutex );
}

enum status { ONLINE, OFFLINE, DISCONNECTED, ERROR };
const char * status_str(enum status s)
{
  switch (s) {
    case ONLINE:
      return "online";
      break;
    case OFFLINE:
      return "offline";
      break;
    case DISCONNECTED:
      return "disconnected";
      break;
    case ERROR:
      return "error";
      break;
  }
  return "";
}

void* teleinfuse_process(void * userdata)
{
  char* port = (char*)userdata;
  int     res ;
  int teleinfo_serial_fd ;
  char teleinfo_buffer[TI_FRAME_LENGTH_MAX];
  enum status current_status = DISCONNECTED;
  enum status previous_status = DISCONNECTED;

  for(;;) {
    teleinfo_data teleinfo_dataset[TI_MESSAGE_COUNT_MAX];
    size_t teleinfo_data_count = 0;

    teleinfo_serial_fd = teleinfo_open(port);
    if (teleinfo_serial_fd) {
      res = teleinfo_read_frame ( teleinfo_serial_fd, teleinfo_buffer, sizeof(teleinfo_buffer));
      teleinfo_close (teleinfo_serial_fd);
      if (!res) {
        res = teleinfo_decode (teleinfo_buffer, teleinfo_dataset, &teleinfo_data_count);
      }
      if (!res) {
        current_status = ONLINE;
      } else {
        current_status = OFFLINE;
      }
    } else {
      current_status = DISCONNECTED;
    }
    // Add a fake teleinfo file to show status
    strcpy(teleinfo_dataset[teleinfo_data_count].label, "status");
    strcpy(teleinfo_dataset[teleinfo_data_count].value, status_str(current_status));
    if (current_status != previous_status) {
      syslog(LOG_INFO, "status changed: was \"%s\", now \"%s\"", status_str(previous_status), status_str(current_status));
      previous_status = current_status;
    }
    teleinfo_data_count++;

    teleinfuse_update (teleinfo_dataset, teleinfo_data_count);
    pthread_testcancel();
#ifdef DEBUG
    sleep (3);
#else
    sleep (10);
#endif
    pthread_testcancel();
  }
}

static int teleinfuse_getattr(const char *path, struct stat *stbuf)
{
  int res = -ENOENT;
  
  memset(stbuf, 0, sizeof(struct stat));
  if(strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    res = 0;
  } else {
    pthread_mutex_lock( &teleinfuse_files_mutex );
    for (size_t n=0; n<teleinfuse_files_count; n++) {
      if(strcmp(path+1, teleinfuse_files[n].filename) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(teleinfuse_files[n].content);
        stbuf->st_mtime = teleinfuse_files[n].time;
        res = 0;
        break;
      }
    }
    pthread_mutex_unlock( &teleinfuse_files_mutex );
  }
  return res;
}

static int teleinfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;

  if(strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  
  pthread_mutex_lock( &teleinfuse_files_mutex );
  for (size_t n=0; n<teleinfuse_files_count; n++) {
    filler(buf, teleinfuse_files[n].filename, NULL, 0);
  }
  pthread_mutex_unlock( &teleinfuse_files_mutex );
  
  return 0;
}

static int teleinfuse_open(const char *path, struct fuse_file_info *fi)
{
  int res = -ENOENT;
  pthread_mutex_lock( &teleinfuse_files_mutex );
  for (size_t n=0; n<teleinfuse_files_count; n++) {
    if(strcmp(path+1, teleinfuse_files[n].filename) == 0) {
      res = 0;
      break;
    }
  }
  pthread_mutex_unlock( &teleinfuse_files_mutex );

  if (res== -ENOENT) return res;

  if((fi->flags & 3) != O_RDONLY)
    res = -EACCES;
  
  return res;
}

static int teleinfuse_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi)
{
  size_t len;
  (void) fi;
  int res = -ENOENT;
  size_t n;
  
  pthread_mutex_lock( &teleinfuse_files_mutex );
  for (n=0; n<teleinfuse_files_count; n++) {
    if(strcmp(path+1, teleinfuse_files[n].filename) == 0) {
      res = 0;
      break;
    }
  }
  pthread_mutex_unlock( &teleinfuse_files_mutex );
  if (res== -ENOENT) return res;

  pthread_mutex_lock( &teleinfuse_files_mutex );
  len = strlen(teleinfuse_files[n].content);

  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, teleinfuse_files[n].content + offset, size);
  } else
    size = 0;
  
  pthread_mutex_unlock( &teleinfuse_files_mutex );
  
  return size;
}

static struct fuse_operations teleinfuse_oper = {
  .getattr    = teleinfuse_getattr,
  .readdir    = teleinfuse_readdir,
  .open       = teleinfuse_open,
  .read       = teleinfuse_read,
};

static int foreground = 0;
int main(int argc, char *argv[])
{
  // Args parssing
  if (argc<3) {
    printf ("Usage: %s DEV MOUNTPOINT\nExample: %s /dev/ttyUSB0 /house/electric_meter\n", argv[0], argv[0]);
    exit(EXIT_FAILURE);
  }

  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
  for(int i = 0; i < argc; i++) {
    if (0 == strcmp("-f", argv[i])) {
      foreground = 1;
    }
    if (i == 1) {
      // We skip the first arg: it a device link to serial port
    } else {
      fuse_opt_add_arg(&args, argv[i]);
    }
  }

  fuse_opt_add_arg(&args, "-f"); // Force FUSE to run in foreground

  // Daemonize
  if(!foreground) {
    daemon(1,1);
  }

  openlog("teleinfuse", LOG_PID, LOG_USER) ;

  // FIXME: Test if argv[1] is a reacheable.
  pthread_t teleinfuse_thread;
  int res;
  res = pthread_create( &teleinfuse_thread, NULL, teleinfuse_process, (void*) argv[1]);

  fuse_main (args.argc, args.argv, &teleinfuse_oper, NULL);
  pthread_cancel (teleinfuse_thread);
  pthread_join (teleinfuse_thread, NULL);

  closelog() ;
  exit(EXIT_SUCCESS) ;
}
