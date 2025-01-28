#include "../polaczenie.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
//Maksymalna przewidziana ilosc klientow jest rowna 2
Client clients[2];
//Inicjalizacja zmiennych i struktury
ClientData data_from_client;
int clientCount = 0;
int index_klienta = 0;
int max_id = 0;

int wygrana = 1;

pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

/*Zestaw trzech funkcji generujacych losowo plansze z ustaiwniem statkow*/
int is_valid_position(char board[10][10], int x, int y, int size,
                      char orientation) {
  if (orientation == 'h') { // horizontal
    if (y + size > 10)
      return 0; // out of bounds
    for (int i = y; i < y + size; i++) {
      if (board[x][i] != 0)
        return 0; // spot already taken
    }
  } else { // vertical
    if (x + size > 10)
      return 0; // out of bounds
    for (int i = x; i < x + size; i++) {
      if (board[x][i] != 0)
        return 0; // spot already taken
    }
  }
  return 1; // valid position
}

void place_ship(char board[10][10], int x, int y, int size, char orientation,
                char ship_char) {
  if (orientation == 'h') { // horizontal
    for (int i = y; i < y + size; i++) {
      board[x][i] = ship_char;
    }
  } else { // vertical
    for (int i = x; i < x + size; i++) {
      board[i][y] = ship_char;
    }
  }
}

void set_board(char board[10][10]) {
  char ships[4] = {'A', 'B', 'C', 'D'};
  int sizes[4] = {4, 3, 2, 1};
  int counts[4] = {1, 2, 3, 4};
  int x, y, size, ship_index;
  char orientation;

  srand(time(0)); // Seed random number generator

  for (ship_index = 0; ship_index < 4; ship_index++) {
    size = sizes[ship_index];
    for (int i = 0; i < counts[ship_index]; i++) {
      int placed = 0;
      while (!placed) {
        x = rand() % 10;
        y = rand() % 10;
        orientation = (rand() % 2 == 0) ? 'h' : 'v';

        if (is_valid_position(board, x, y, size, orientation)) {
          place_ship(board, x, y, size, orientation, ships[ship_index]);
          printf("umieszczony:%c\n", ships[ship_index]);
          placed = 1;
        } /* else {
           printf("not valid %d %d\n", x, y);
         }*/
      }
    }
  }
}

void handleClient(int clientSocket) {
  char buffer[1024];
  char username[50];
  //  printf("Client %s connected\n", username);
  while (1) {
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
      break;
    }
    printf("odebrano :(%d)%d\n", (int)sizeof(data_from_client), bytesReceived);
    memcpy(&data_from_client, &buffer, sizeof(data_from_client));

    printf("data_from_client.comand=%d\n", data_from_client.comand);
    for (int i = 0; i < 2; i++) {
      if (clients[i].data.id >= max_id) {
        max_id = clients[i].data.id;
      }
     // printf("klient %s id %d\n", clients[i].data.username, clients[i].data.id);
    }
    /*Switch case obslugujacy flagi w strukturze*/
    switch (data_from_client.comand) {
    case INTRODUCTION:
      printf("Klient: %s\n", data_from_client.username);
      memset(&(clients[clientCount].data), 0,
             sizeof(clients[clientCount].data));
      pthread_mutex_lock(&clientsMutex);
      clients[clientCount].socket = clientSocket;
      memcpy(&(clients[clientCount].data), &data_from_client,
             sizeof(data_from_client));
      clients[clientCount].data.id = max_id + 1;
      set_board(clients[clientCount].data.board);
      clients[clientCount].data.comand = INVITE;
      send(clients[clientCount].socket, &(clients[clientCount].data),
           sizeof(clients[clientCount].data), 0);
      clientCount++;
      for (int i = 0; i < clientCount; i++) {
        printf("send %d\n", i);
        clients[i].data.comand = BOARD;
        send(clients[i].socket, &(clients[i].data), sizeof(clients[i].data), 0);
      }
      if (clientCount == 2) {
        clients[index_klienta].data.comand = GIVESHOT;
        send(clients[index_klienta].socket, &(clients[index_klienta].data),
             sizeof(clients[index_klienta].data), 0);
      }

      pthread_mutex_unlock(&clientsMutex);

      break;
    case SHOT:
      printf("\n\nindex id: %d(%d), kto %d\n", clients[index_klienta].data.id,
             index_klienta, data_from_client.id);
      if (clients[index_klienta].data.id == data_from_client.id) {
        index_klienta = 1 - index_klienta;
        printf("Strzal Klient: %s.%d(%d)\n", data_from_client.username,
               data_from_client.id, index_klienta);
        char x = data_from_client.x;
        char y = data_from_client.y;
        if (clients[index_klienta].data.board[x][y] == 'A' ||
            clients[index_klienta].data.board[x][y] == 'B' ||
            clients[index_klienta].data.board[x][y] == 'C' ||
            clients[index_klienta].data.board[x][y] == 'D') {
          clients[index_klienta].data.board[x][y] = 'X';
          wygrana = 1;
          for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
              if (clients[index_klienta].data.board[i][j] == 'A' ||
                  clients[index_klienta].data.board[i][j] == 'B' ||
                  clients[index_klienta].data.board[i][j] == 'C' ||
                  clients[index_klienta].data.board[i][j] == 'D') {
                wygrana = 0;
              }
            }
          }
          if (wygrana) {
            for (int k = 0; k < 2; k++) {
              clients[1 - index_klienta].data.comand = GAMEOVER;
              send(clients[k].socket, &(clients[1 - index_klienta].data),
                   sizeof(clients[1 - index_klienta].data), 0);
            }
          }
        } else {
          clients[index_klienta].data.board[x][y] = 'o';
        }
        clients[index_klienta].data.comand = BOARD;

        for (int i = 0; i < 2; i++) {
          printf("send1 %d, %d\n", clients[index_klienta].data.comand,
                 (int)send(clients[i].socket, &(clients[index_klienta].data),
                           sizeof(clients[index_klienta].data), 0));
        }
        clients[index_klienta].data.comand = GIVESHOT;
        printf("send2 %d, %d\n", clients[index_klienta].data.comand,
               (int)send(clients[index_klienta].socket,
                         &(clients[index_klienta].data),
                         sizeof(clients[index_klienta].data), 0));
      }
      printf("nowy index id: %d(%d)\n", clients[index_klienta].data.id,
             index_klienta);
      break;
    default:
      printf("Nieznane polecenie: %d\n", data_from_client.comand);
      break;
    }
  }

  // Usuniecie klienta z vektora
  pthread_mutex_lock(&clientsMutex);
  for (int i = 0; i < clientCount; i++) {
    if (clients[i].socket == clientSocket) {
      clients[i] = clients[clientCount - 1];
      clientCount--;
      break;
    }
  }
  pthread_mutex_unlock(&clientsMutex);
  close(clientSocket);
}

void handleSigpipe(int signal) { printf("Caught SIGPIPE signal!\n"); }

int main() {
  // Ustawienie hendlera SIGPYPE
  signal(SIGPIPE, handleSigpipe);

  // Utworzenie socketu
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Specyfikacja adresacji serwera
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr =
      inet_addr("10.2.10.1"); // Statyczny adress IP

  // Bindowanie socketu serwera
  bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

  // Ustawienie nasuchiwania serwera na porcie
  listen(serverSocket, 5);

  printf("Server is listening on port %d...\n", PORT);

  while (1) {
    // Akceptowanie paczen
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientCount > 1) {
      close(clientSocket);
    }
    // Obsluga klientow w wadkach
    pthread_t thread;
    pthread_create(&thread, NULL, (void *)handleClient,
                   (void *)(intptr_t)clientSocket);
    pthread_detach(thread);
  }

  close(serverSocket);

  return 0;
}
