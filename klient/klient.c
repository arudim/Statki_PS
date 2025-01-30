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

// Inicjalizacja struktur i zmiennej
ClientData client_data;
ClientData enemy;
int ready_for_command = 0;

/* Funkcja rysująca planszę */
void Drawboard() {
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

/* Obsługa wiadomości od serwera */
void *receiveMessages(void *socket) {
  int clientSocket = *(int *)socket;
  char buffer[1024];
  ClientData *from_server;
  while (1) {
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
      printf("Utrata połączenia z serwerem\n");
      close(clientSocket);
      exit(-1);
    }

    from_server = (ClientData *)buffer;
    switch (from_server->comand) {
    case INVITE:
      memcpy(&client_data, buffer, sizeof(client_data));
      Drawboard();
      printf("Serwer przyjął: %s. Oczekiwanie na drugiego gracza.\n", client_data.username);
      break;
    case BOARD:
      if (client_data.id == from_server->id) {
        memcpy(&(client_data.board), from_server->board, sizeof(client_data.board));
      } else {
        memcpy(&(enemy.board), from_server->board, sizeof(enemy.board));
      }
      Drawboard();
      break;
    case GIVESHOT:
      Drawboard();
      ready_for_command = 1;
      break;
    case GAMEOVER:
      Drawboard();
      printf("ZWYCIĘZCA: %s\n", from_server->username);
      close(clientSocket);
      exit(0);
      break;
    case CONECTIONLOST:
      printf("Utrata połączenia z serwerem\n");
      close(clientSocket);
      exit(-1);
      break;
    case TOOMANY:
      printf("Już jest 2 graczy\n");
      close(clientSocket);
      exit(0);
    default:
      printf("Nieznana komenda: %d\n", from_server->comand);
      break;
    }
  }
  return NULL;
}

void handleSigpipe(int signal) {
  printf("Caught SIGPIPE signal!\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Użycie: %s <adres IP serwera>\n", argv[0]);
    return 1;
  }

  // Ustawienie handlera SIGPIPE
  signal(SIGPIPE, handleSigpipe);

  // Tworzenie socketu
  int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0) {
    perror("Błąd tworzenia socketu");
    return 1;
  }

  // Specyfikacja adresacji klienta
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr = inet_addr(argv[1]);

  // Wysłanie prośby o połączenie
  if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
    perror("Połączenie nieudane");
    close(clientSocket);
    return 1;
  }

  // Wysłanie wiadomości do serwera z flagą INTRODUCTION wraz z nazwą użytkownika
  printf("Wprowadź swoją nazwę użytkownika: ");
  fgets(client_data.username, sizeof(client_data.username), stdin);
  client_data.username[strcspn(client_data.username, "\n")] = '\0'; // Usunięcie znaku nowej linii
  client_data.comand = INTRODUCTION;
  if (send(clientSocket, &client_data, sizeof(client_data), 0) < 0) {
    perror("Błąd wysyłania wiadomości");
    close(clientSocket);
    return 1;
  }

  // Rozpoczęcie nowego wątku do przechwytywania wiadomości
  pthread_t thread;
  if (pthread_create(&thread, NULL, receiveMessages, &clientSocket) != 0) {
    perror("Błąd tworzenia wątku");
    close(clientSocket);
    return 1;
  }
  pthread_detach(thread);

  // Główna pętla
  while (1) {
    if (ready_for_command == 1) {
      char message[1024];
      printf("Wprowadź miejsce strzału: ");
      fgets(message, sizeof(message), stdin);
      if (message[0] >= 'a' && message[0] <= 'j' && message[1] >= '0' && message[1] <= '9') {
        message[strcspn(message, "\n")] = '\0'; // Usunięcie znaku nowej linii
        char x = message[0] - 'a';
        char y = message[1] - '0';
        client_data.x = x;
        client_data.y = y;
        client_data.comand = SHOT;
        ready_for_command = 0;
        if (send(clientSocket, &client_data, sizeof(client_data), 0) < 0) {
          perror("Błąd wysyłania strzału");
          close(clientSocket);
          return 1;
        }
      } else {
        printf("Nieprawidłowe współrzędne. Wprowadź np. 'a0'.\n");
      }
    }
    sleep(1);
  }

  // Zamknięcie socketu klienta
  close(clientSocket);

  return 0;
}