#include "../polaczenie.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
// Inicjalizaja struktur i zmiennej
ClientData _data;
ClientData enemy;
int ready_for_command = 0;

/*Funkcja rysujaca plansze
funkcja czysci terminal nastepnie rysuje plansze gracza ze statkami i oddanymi w
nia strzalami oraz plansze przeciwnika z uktytymi statkami*/
void Rysowanieplanszy() {
  printf("\033[H\033[J");
  printf("   ");
  for (char c = 'a'; c <= 'j'; c++) {
    printf("%c", c);
  }
  printf("     ");
  for (char c = 'a'; c <= 'j'; c++) {
    printf("%c", c);
  }
  printf("\n");

  for (int y = 0; y < 10; y++) {
    printf("%2d ", y);
    for (int x = 0; x < 10; x++) {
      switch (_data.board[x][y]) {
      case 0:
        printf(".");
        break;
      default:
        printf("%c", _data.board[x][y]);
        break;
      }
    }

    printf("  %2d ", y);
    for (int x = 0; x < 10; x++) {
      switch (enemy.board[x][y]) {
      case 0:
      case 'A':
      case 'B':
      case 'C':
      case 'D':
        printf(".");
        break;
      default:
        printf("%c", enemy.board[x][y]);
        break;
      }
    }
    printf("\n");
  }
}

/*Obsluga wiadomosci od serwera*/
void *receiveMessages(void *socket) {
  int clientSocket = *(int *)socket;
  char buffer[1024];
  ClientData *from_server;
  while (1) {
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    /*if (bytesReceived > 0) {
      printf("Message from server: %s\n", buffer);
    }*/
    from_server = (ClientData *)buffer;
    /*Switch case obslugujacy widomosci od serwera
    pierwszym bajtem w strukturze jest flaga obslugiwana przez klienta*/
    switch (from_server->comand) {
    case INVITE:
      memcpy(&_data, buffer, sizeof(_data));
      printf("Serwer przyjal: %s(%d)\n", _data.username, _data.id);
      Rysowanieplanszy();
      break;
    case BOARD:
      // printf("ja:%d,przyszedl %d\n", _data.id, from_server->id);
      if (_data.id == from_server->id) {
        // printf("mojamapa\n");
        memcpy(&(_data.board), from_server->board, sizeof(_data.board));
      } else {
        printf("mapaprzeciwinika\n");
        memcpy(&(enemy.board), from_server->board, sizeof(enemy.board));
      }
      Rysowanieplanszy();
      break;
    case GIVESHOT:
      Rysowanieplanszy();
      printf("flaga 5\n");
      ready_for_command = 1;
      break;
    case GAMEOVER:
      Rysowanieplanszy();
      printf("ZWYCIESCA: %s", from_server->username);
      close(clientSocket);
      exit(0);
      break;
    case CONECTIONLOST:
      printf("Utrata polaczenia z serweram\n");
      close(clientSocket);
      exit(-1);
      break;
    case TOOMANY:
      printf("Już jest 2 graczy");
      close(clientSocket);
      exit(0);
    default:
      printf("niznanakomenda %d\n", from_server->comand);
      break;
    }
  }
  return NULL;
}
void handleSigpipe(int signal) { printf("Caught SIGPIPE signal!\n"); }

int main() {
  // Ustawienie hendlera SIGPIPE
  signal(SIGPIPE, handleSigpipe);

  // Tworzenie socketu
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Specyfikacja adresacji klienta
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr =
      inet_addr("10.2.10.1"); // Staly addres IP serwera

  // Wyslanie prozby o polaczenie
  if (connect(clientSocket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    perror("Connection failed");
    return 1;
  }

  // Wysanie wiadomosci do klienta zprozba o przedstawienie
  char username[50];
  printf("Wprowadz swoja nazwe uzytkownika: ");
  fgets(_data.username, (int)sizeof(_data.username), stdin);
  username[strcspn(_data.username, "\n")] = '\0'; // Remove newline character
  _data.comand = INTRODUCTION;
  printf("_data.comand=%d\n:", _data.comand);

  /*for (int i = 0; i < sizeof(_data); i++)
    printf("%c(%d),", ((char *)&_data)[i], ((char *)&_data)[i]);
  printf("\n\n");*/

  int wyslane = send(clientSocket, (char *)&_data, sizeof(_data), 0);
  printf("wyslane:(%d) %d\n", (int)sizeof(_data), wyslane);

  // Rozpoczecie nowego wadku do przechwytywania wiadomosci
  pthread_t thread;
  pthread_create(&thread, NULL, receiveMessages, &clientSocket);
  pthread_detach(thread);

  // Glowna petla
  while (1) {
    char message[1024];
    // printf("%d\n", ready_for_command);
    if (ready_for_command == 1) {
      printf("Wprowadz miejsce strzalu: ");
      fgets(message, sizeof(message), stdin);
      if (message[0] >= 'a' && message[0] <= 'j' && message[1] >= '0' &&
          message[1] <= '9') {
        message[strcspn(message, "\n")] = '\0'; // Usuniecie znaku nowej lini
        char x = message[0] - 'a';
        char y = message[1] - '0';
        _data.x = x;
        _data.y = y;
        _data.comand = SHOT;
        ready_for_command = 0;
        send(clientSocket, &_data, sizeof(_data), 0);
      } else {
        continue;
      }
    }
    sleep(1);
  }

  // Zamkniecie socketu Klienta
  close(clientSocket);

  return 0;
}
