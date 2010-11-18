#ifndef _TELEINFO_H_
#define _TELEINFO_H_

#include <sys/types.h>
typedef struct {
  char label[9];
  char value[13];
} teleinfo_data;

// Message Teleinfo 
// LF 1 char (0x0a)
// [ ETIQUETTE ] 4 à 8 char
// SP 1 char (0x20)
// [ DONNEES ] 1 à 12 char
// SP 1 char (0x20)
// [ CHECKSUM ] 1 char
// CR 1 char (0x0d)
// (1 + 8 + 1 + 12 + 1 + 1 + 1) = 25
#define TI_MESSAGE_LENGTH_MAX 26
#define TI_MESSAGE_COUNT_MAX 32
#define TI_FRAME_LENGTH_MAX (TI_MESSAGE_LENGTH_MAX * TI_MESSAGE_COUNT_MAX)

int teleinfo_open (const char * port);
int teleinfo_read (int fd, char * buffer, size_t buflen);
int teleinfo_decode(const char * frame, teleinfo_data dataset[], size_t * datasetlen);
void teleinfo_close (int fd);

#endif