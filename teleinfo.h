/*-
* teleinfuse is a FUSE module to access to the "Téléinformation"
* "Téléinfo" data are transmitted by french electric meters (EDF/ERDF)
*
* Copyright (C) 2010 Romuald Conty
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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

// returns file descriptor if succeed otherwise 0
int teleinfo_open (const char * port);

// returns 0 if succeed otherwise negative
int teleinfo_read_frame ( const int fd, char *const buffer, const size_t buflen);

// returns 0 if succeed otherwise negative
int teleinfo_decode(const char * frame, teleinfo_data dataset[], size_t * datasetlen);

void teleinfo_close (int fd);

#endif