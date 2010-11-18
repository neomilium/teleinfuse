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
int teleinfo_read (int serial_fd, char * buffer, size_t buflen)
{
  static char current_chr[2];
  int res;
  tcflush(serial_fd, TCIFLUSH) ;                     // Efface les données non lus en entrée.

  do {
    current_chr[1] = current_chr[0] ;
    res = read(serial_fd, current_chr, 1) ;
    if (!res) {
      syslog(LOG_ERR, "Erreur pas de réception début données Téléinfo !\n") ;
      return 0;
    }
  } while ( ! (current_chr[0] == 0x02 && current_chr[1] == 0x03) ) ;       // Attend code fin suivi de début trame téléinfo .

  do {
    res = read(serial_fd, current_chr, 1) ;
    if (! res) {
      syslog(LOG_ERR, "Erreur pas de réception fin données Téléinfo !\n") ;
      return 0;
    }
    *buffer = current_chr[0];
    buffer++;
    buflen--;
  } while (buflen && (current_chr[0] != 0x03)) ;                    // Attend code fin trame téléinfo.
  *(buffer-1) = '\0';

  return buflen ? 1 : 0;
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
      message++; // On passe le LN de début de ligne

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
        return 0;
       }
    }
    message = message_oel; // On se place sur la fin de ligne
    message++; // On passe le CR de fin de ligne
  }
  *datasetlen = data_count;
  return 1;
}
