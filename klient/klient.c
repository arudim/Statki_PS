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
ClientData client_data;
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
      switch (client_data.board[x][y]) {
      case 0:
        printf(".");
        break;
      default:
        printf("%c", client_data.board[x][y]);
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
      /*Jeżeli klient przyjmie strukturę z flagą wyryowuje plansze i wyświetla wiadomość o oczekiwaniu na drugiego gracza*/
    case INVITE:
      memcpy(&client_data, buffer, sizeof(client_data));
      Rysowanieplanszy();
      printf("Serwer przyjal: %sOczekiwanie na drugoego gracza.\n", client_data.username);
      break;
    /*Jeżeli klient przyjmie strukturę board kopiuje on obie struktury i następnie rysuje obie plansze*/
    case BOARD:
      // printf("ja:%d,przyszedl %d\n", client_data.id, from_server->id);
      if (client_data.id == from_server->id) {
        memcpy(&(client_data.board), from_server->board, sizeof(client_data.board));
      } else {
        printf("mapaprzeciwinika\n");
        memcpy(&(enemy.board), from_server->board, sizeof(enemy.board));
      }
      Rysowanieplanszy();
      break;
      /*Flaga GIVESHOT przygotowuej zmienna do wysłanie wiadomości do serwera z strzałęm*/
    case GIVESHOT:
      Rysowanieplanszy();
      // debugowanie
      // printf("flaga 5\n");
      ready_for_command = 1;
      break;
      /*Wyświetla wiadomość ze zwycięscą i zamyka socket wraz z programem*/
    case GAMEOVER:
      Rysowanieplanszy();
      printf("ZWYCIESCA: %s", from_server->username);
      close(clientSocket);
      exit(0);
      break;
      /*W wypadku utraty połączenia z serweram zamyka socket oraz program*/
    case CONECTIONLOST:
      printf("Utrata polaczenia z serweram\n");
      close(clientSocket);
      exit(-1);
      break;
      /*W wypadku nadprogramowego połączenia zamyka socket i program*/
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

int main(int argc, char *argv[]) {
  // Ustawienie hendlera SIGPIPE
  signal(SIGPIPE, handleSigpipe);

  // Tworzenie socketu
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Specyfikacja adresacji klienta
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  printf("adress: %s\n", argv[1]);
  serverAddress.sin_addr.s_addr =
      inet_addr(argv[1]); // Staly addres IP serwera

  // Wyslanie prozby o polaczenie
  if (connect(clientSocket, (struct sockaddr *)&serverAddress,
              sizeof(serverAddress)) < 0) {
    perror("Connection failed");
    return 1;
  }

  // Wysłąnie wiadomości do serwera z flagą INRODUCTION wraz z nazwa uzytkownika
  char username[50];
  printf("Wprowadz swoja nazwe uzytkownika: ");
  fgets(client_data.username, (int)sizeof(client_data.username), stdin);
  username[strcspn(client_data.username, "\n")] = '\0'; // Remove newline character
  client_data.comand = INTRODUCTION;
  // debugowanie kodu
  //  printf("client_data.comand=%d\n:", client_data.comand);
  int wyslane = send(clientSocket, (char *)&client_data, sizeof(client_data), 0);
  // debugowanie kodu
  //  printf("wyslane:(%d) %d\n", (int)sizeof(client_data), wyslane);

  // Rozpoczecie nowego wadku do przechwytywania wiadomosci
  pthread_t thread;
  pthread_create(&thread, NULL, receiveMessages, &clientSocket);
  pthread_detach(thread);

  // Glowna petla
  while (1) {
    char message[1024];
    /*Jeżeli przyszła glaga GIVESHOT wysyłana jest struktura do serwera z miejscem oddania strzalu */
    if (ready_for_command == 1) {
      printf("Wprowadz miejsce strzalu: ");
      fgets(message, sizeof(message), stdin);
      if (message[0] >= 'a' && message[0] <= 'j' && message[1] >= '0' &&
          message[1] <= '9') {
        message[strcspn(message, "\n")] = '\0'; // Usuniecie znaku nowej lini
        char x = message[0] - 'a';
        char y = message[1] - '0';
        client_data.x = x;
        client_data.y = y;
        client_data.comand = SHOT;
        ready_for_command = 0;
        send(clientSocket, &client_data, sizeof(client_data), 0);
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
