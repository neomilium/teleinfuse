#include "teleinfo.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <termios.h>

int teleinfo_open (const char* port)
    // Mode Non-Canonical Input Processing, Attend 1 caractère ou time-out(avec VMIN et VTIME).
{
    // Déclaration pour le port série.
    struct termios  teleinfo_serial_attr ;
    int fd ;

    // Ouverture de la liaison serie (Nouvelle version de config.)
    if ( (fd = open (port, O_RDWR | O_NOCTTY)) == -1 ) {
      syslog(LOG_ERR, "Erreur ouverture du port serie %s !", port);
      return 0;
    }
    
    tcgetattr(fd,&teleinfo_serial_attr) ;                            // Lecture des parametres courants.

    cfsetispeed(&teleinfo_serial_attr, B1200) ;                       // Configure le débit en entrée/sortie.
    cfsetospeed(&teleinfo_serial_attr, B1200) ;

    teleinfo_serial_attr.c_cflag |= (CLOCAL | CREAD) ;                   // Active réception et mode local.

    // Format série "7E1"
    teleinfo_serial_attr.c_cflag |= PARENB  ;                            // Active 7 bits de donnees avec parite pair.
    teleinfo_serial_attr.c_cflag &= ~PARODD ;
    teleinfo_serial_attr.c_cflag &= ~CSTOPB ;
    teleinfo_serial_attr.c_cflag &= ~CSIZE ;
    teleinfo_serial_attr.c_cflag |= CS7 ;
//     teleinfo_serial_attr.c_cflag &= ~CRTSCTS ;                           // Désactive control de flux matériel. (pas compatible POSIX)

    teleinfo_serial_attr.c_iflag |= (INPCK | ISTRIP) ;                   // Mode de control de parité.
    teleinfo_serial_attr.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL) ;    // Désactive control de flux logiciel, conversion 0xOD en 0x0A.

    teleinfo_serial_attr.c_oflag &= ~OPOST ;                             // Pas de mode de sortie particulier (mode raw).

    teleinfo_serial_attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;    // Mode non-canonique (mode raw) sans echo.

    teleinfo_serial_attr.c_cc[VTIME] = 80 ;                              // time-out à ~8s.
    teleinfo_serial_attr.c_cc[VMIN]  = 0 ;                               // 1 car. attendu.

    tcflush (fd, TCIFLUSH) ;                                     // Efface les données reçues mais non lues.
    tcsetattr (fd,TCSANOW,&teleinfo_serial_attr) ;                    // Sauvegarde des nouveaux parametres
    return fd ;
}

void teleinfo_close (int fd) 
{
  close (fd);
}

// Trame Teleinfo
// STX 1 char (0x02)
// [ MESSAGE_0 ] 25 char max
// ... [ MESSAGE_N ]
// ETX 1 char (0x03)
#define STX '\x02'
#define ETX '\x03' 
#define EOT '\x04'
#define LF  '\x0a'
#define CR  '\x0d'
int teleinfo_read_frame ( const int fd, char *const buffer, const size_t buflen)
{
  char *p;
  size_t s;
  char c;
  enum state { INIT, FRAME_BEGIN, FRAME_END, MSG_BEGIN, MSG_END };
  enum state current_state;
  int error_count = 0;

  do {
    int res = read(fd, &c, 1) ;
    if (!res) {
      syslog(LOG_ERR, "unable to read from source\n") ;
      return -2;
    }
//     syslog(LOG_INFO, "c = %02x", c) ;
    switch(c) {
      case STX:
        if (current_state != INIT) {
          #ifdef DEBUG
          syslog(LOG_INFO, "new STX detected but not expected, resetting frame begin") ;
          #endif
          error_count++;
        }
        current_state = FRAME_BEGIN;
        p = buffer;
        s = 0;
//         if (s<buflen) { *p++ = c; s++; } else { return -1; }
        break;
      case LF:
        if (current_state != INIT) {
          if ((current_state != FRAME_BEGIN) && (current_state != MSG_END)) {
            #ifdef DEBUG
            syslog(LOG_INFO, "LF detected but not expected, frame is invalid") ;
            #endif
            error_count++;
            current_state = INIT;
          } else {
            current_state = MSG_BEGIN;
            if (s<buflen) { *p++ = c; s++; } else { return -1; }
          }
        } // else do nothing: simply skip the char
        break;
      case CR:
        if (current_state != INIT) {
          if (current_state != MSG_BEGIN) {
            #ifdef DEBUG
            syslog(LOG_INFO, "CR detected but not expected, frame is invalid") ;
            #endif
            error_count++;
            current_state = INIT;
          } else {
            current_state = MSG_END;
            if (s<buflen) { *p++ = c; s++; } else { return -1; }
          }
        } // else do nothing: simply skip the char
        break;
      case ETX:
        if (current_state != INIT) {
          if (current_state != MSG_END) {
            #ifdef DEBUG
            syslog(LOG_INFO, "ETX detected but not expected, frame is invalid") ;
            #endif
            error_count++;
            current_state = INIT;
          } else {
            current_state = FRAME_END;
//             if (s<buflen) { *p++ = c; s++; } else { return -1; }
          }
        } // else do nothing: simply skip the char
        break;
      case EOT:
        syslog(LOG_INFO, "frame have been interrupted by EOT, resetting frame");
        current_state = INIT;
        break;
      default:
        switch(current_state) {
          case INIT:
            // STX have not been detected yet, so we skip char
            break;
          case FRAME_BEGIN:
            #ifdef DEBUG
            syslog(LOG_INFO, "STX should be followed by LF, frame is invalid") ;
            #endif
            current_state = INIT;
            error_count++;
            break;
          case FRAME_END:
            // We should not be here !
            break;
          case MSG_BEGIN:
            // Message content
            if (s<buflen) { *p++ = c; s++; } else { return -1; }
            break;
          case MSG_END:
            #ifdef DEBUG
            syslog(LOG_INFO, "CR should be followed by ETX or LF, frame is invalid") ;
            #endif
            current_state = INIT;
            error_count++;
            break;
        }
    }
  } while ((current_state != FRAME_END) && (error_count<10));
  if (current_state == FRAME_END) {
    return 0;
  } else {
    syslog(LOG_INFO, "too many error while reading, giving up");
    return -3;
  }
}

int teleinfo_checksum(char *message)
{
  const char * message_oel = strchr(message, 0x0d);             // Mémorise le pointer de fin de ligne
  unsigned char sum = 0 ;                 // Somme des codes ASCII du message

  message++;
  while ( (*message != '\0') && (message != (message_oel-2) ) ) { // Tant qu'on est pas au checksum (avec SP precédent)
    sum += *message;
    message++;
  }
  sum = (sum & 0x3F) + 0x20 ;
  message++; // On passe le SP
  if ( sum == *message) {
    return 1 ;        // Return 1 si checkum ok.*
  }
#ifdef DEBUG
  syslog(LOG_INFO, "Checksum lu:%02x   calculé:%02x", *message, sum) ;
#endif
  return 0;
}

int teleinfo_decode(const char * frame, teleinfo_data dataset[], size_t * datasetlen)
{
  char * message_oel;
  char * message = (char*)frame;
  char label[20];
  char value[20];
  int wrong_checksum_count = 0;
  size_t data_count = 0;
  *datasetlen = 0;

  while ( (message_oel = strchr(message, 0x0d)) ) {
    if (1 == teleinfo_checksum(message)) {
      message++; // On passe le LF de début de ligne

      sscanf( message, "%s %s *", label, value );
      // TODO: Check if lenght(label) > 8
      strncpy(dataset[data_count].label, label, 8); dataset[data_count].label[8] = '\0';
      // TODO: Check  if lenght(value) > 12
      strncpy(dataset[data_count].value, value, 12); dataset[data_count].value[12] = '\0';
      data_count++;

    } else {
      // Erreur de checksum
      wrong_checksum_count++;
      if (wrong_checksum_count>=3) {
        return -1;
       }
    }
    message = message_oel; // On se place sur la fin de ligne
    message++; // On passe le CR de fin de ligne
  }
  *datasetlen = data_count;
  return 0;
}
