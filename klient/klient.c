#include "../polaczenie.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
ClientData dane;
ClientData przeciwnik;
int ready_for_command = 0;

void Rysowanieplanszy() {
  printf("\033[H\033[J");
  for (int y = 0; y < 10; y++) {
    for (int x = 0; x < 10; x++) {
      switch (dane.plansza[x][y]) {
      case 0:
        printf(".");
        break;
      default:
        printf("%c", dane.plansza[x][y]);
        break;
      }
    }
    printf("  ");
    for (int x = 0; x < 10; x++) {
      switch (przeciwnik.plansza[x][y]) {
      case 0:
      case 'A':
      case 'B':
      case 'C':
      case 'D':
        printf(".");
        break;
      default:
        printf("%c", przeciwnik.plansza[x][y]);
        break;
      }
    }
    printf("\n");
  }
}

void *receiveMessages(void *socket) {
  int clientSocket = *(int *)socket;
  char buffer[1024];
  ClientData *odserwera;
  while (1) {
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    /*if (bytesReceived > 0) {
      printf("Message from server: %s\n", buffer);
    }*/
    odserwera = (ClientData *)buffer;
    switch (odserwera->komenda) {
    case ZAPROSZENIE:
      memcpy(&dane, buffer, sizeof(dane));
      printf("Serwer przyjal: %s(%d)\n", dane.username, dane.id);

      Rysowanieplanszy();
      break;
    case PLANSZA:
      // printf("ja:%d,przyszedl %d\n", dane.id, odserwera->id);
      if (dane.id == odserwera->id) {
        // printf("mojamapa\n");
        memcpy(&(dane.plansza), odserwera->plansza, sizeof(dane.plansza));
      } else {
        printf("mapaprzeciwinika\n");
        memcpy(&(przeciwnik.plansza), odserwera->plansza,
               sizeof(przeciwnik.plansza));
      }
      Rysowanieplanszy();
      break;
    case DAJSTRZAL:
      Rysowanieplanszy();
      ready_for_command = 1;
      break;
    default:
      printf("niznanakomenda %d\n", odserwera->komenda);
      break;
    }
  }
  return NULL;
}
void handleSigpipe(int signal) { printf("Caught SIGPIPE signal!\n"); }

int main() {
  // Set up SIGPIPE handler
  signal(SIGPIPE, handleSigpipe);

  // Creating socket
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Specifying address
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr =
      inet_addr("10.2.10.1"); // Use the correct server IP address

  // Sending connection request
  if (connect(clientSocket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    perror("Connection failed");
    return 1;
  }

  // Send introduction message (username)
  char username[50];
  printf("Enter your username: ");
  fgets(dane.username, (int)sizeof(dane.username), stdin);
  username[strcspn(dane.username, "\n")] = '\0'; // Remove newline character
  dane.komenda = PRZEDSTAWIENIE;
  printf("dane.komenda=%d\n:", dane.komenda);

  /*for (int i = 0; i < sizeof(dane); i++)
    printf("%c(%d),", ((char *)&dane)[i], ((char *)&dane)[i]);
  printf("\n\n");*/

  int wyslane = send(clientSocket, (char *)&dane, sizeof(dane), 0);
  printf("wyslane:(%d) %d\n", (int)sizeof(dane), wyslane);

  // Start thread to receive messages
  pthread_t thread;
  pthread_create(&thread, NULL, receiveMessages, &clientSocket);
  pthread_detach(thread);

  // Communication loop
  while (1) {
    char message[1024];
    // printf("%d\n", ready_for_command);
    if (ready_for_command == 1) {
      while (1) {
        int c = getc(stdin);
        if (c == EOF) {
          break;
        }
      }
      printf("Enter message enter shot: ");
      fgets(message, sizeof(message), stdin);
      if (message[0] >= 'a' && message[0] <= 'j' && message[1] >= '0' &&
          message[1] <= '9') {
        message[strcspn(message, "\n")] = '\0'; // Remove newline character
        char x = message[0] - 'a';
        char y = message[1] - '0';
        dane.x = x;
        dane.y = y;
        dane.komenda = STRZAL;
        ready_for_command = 0;
        send(clientSocket, &dane, sizeof(dane), 0);
      } else {
        continue;
      }
    }
    sleep(1);
  }

  // Closing socket (will never reach here in this example)
  close(clientSocket);

  return 0;
}
