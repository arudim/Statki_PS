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

Client clients[100];
ClientData daneodklienta;
int clientCount = 0;
int index_klienta = 0;

int wygrana = 1;

pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

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
  int counts[4] = {0, 0, 0, 1};
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
    printf("odebrano :(%d)%d\n", (int)sizeof(daneodklienta), bytesReceived);
    memcpy(&daneodklienta, &buffer, sizeof(daneodklienta));

    printf("daneodklienta.komenda=%d\n:", daneodklienta.komenda);

    switch (daneodklienta.komenda) {
    case PRZEDSTAWIENIE:
      printf("Klient: %s\n", daneodklienta.username);
      memset(&(clients[clientCount].data), 0,
             sizeof(clients[clientCount].data));
      pthread_mutex_lock(&clientsMutex);
      clients[clientCount].socket = clientSocket;
      memcpy(&(clients[clientCount].data), &daneodklienta,
             sizeof(daneodklienta));
      clients[clientCount].data.id = clientCount + 1;
      set_board(clients[clientCount].data.plansza);
      clients[clientCount].data.komenda = ZAPROSZENIE;
      send(clients[clientCount].socket, &(clients[clientCount].data),
           sizeof(clients[clientCount].data), 0);
      clientCount++;
      for (int i = 0; i < clientCount; i++) {
        printf("send %d\n", i);
        clients[i].data.komenda = PLANSZA;
        send(clients[i].socket, &(clients[i].data), sizeof(clients[i].data), 0);
      }
      if (clientCount == 2) {
        clients[index_klienta].data.komenda = DAJSTRZAL;
        send(clients[index_klienta].socket, &(clients[index_klienta].data),
             sizeof(clients[index_klienta].data), 0);
      }

      pthread_mutex_unlock(&clientsMutex);

      break;
    case STRZAL:
      printf("\n\nindex id: %d(%d), kto %d\n", clients[index_klienta].data.id,
             index_klienta, daneodklienta.id);
      if (clients[index_klienta].data.id == daneodklienta.id) {
        index_klienta = 1 - index_klienta;
        printf("Strzal Klient: %s.%d(%d)\n", daneodklienta.username,
               daneodklienta.id, index_klienta);
        char x = daneodklienta.x;
        char y = daneodklienta.y;
        if (clients[index_klienta].data.plansza[x][y] == 'A' ||
            clients[index_klienta].data.plansza[x][y] == 'B' ||
            clients[index_klienta].data.plansza[x][y] == 'C' ||
            clients[index_klienta].data.plansza[x][y] == 'D') {
          clients[index_klienta].data.plansza[x][y] = 'X';
          wygrana = 1;
          for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
              if (clients[index_klienta].data.plansza[i][j] == 'A' ||
                  clients[index_klienta].data.plansza[i][j] == 'B' ||
                  clients[index_klienta].data.plansza[i][j] == 'C' ||
                  clients[index_klienta].data.plansza[i][j] == 'D') {
                wygrana = 0;
              }
            }
          }
          if (wygrana) {
            for (int k = 0; k < 2; k++) {
              clients[1 - index_klienta].data.komenda = KONIECGRY;
              send(clients[k].socket, &(clients[1 - index_klienta].data),
                   sizeof(clients[1 - index_klienta].data), 0);
            }
          }
        } else {
          clients[index_klienta].data.plansza[x][y] = 'o';
        }
        clients[index_klienta].data.komenda = PLANSZA;

        for (int i = 0; i < 2; i++) {
          printf("send1 %d, %d\n", clients[index_klienta].data.komenda,
                 (int)send(clients[i].socket, &(clients[index_klienta].data),
                           sizeof(clients[index_klienta].data), 0));
        }
        clients[index_klienta].data.komenda = DAJSTRZAL;
        printf("send2 %d, %d\n", clients[index_klienta].data.komenda,
               (int)send(clients[index_klienta].socket,
                         &(clients[index_klienta].data),
                         sizeof(clients[index_klienta].data), 0));
      }
      printf("nowy index id: %d(%d)\n", clients[index_klienta].data.id,
             index_klienta);
      break;
    default:
      printf("Nieznane polecenie: %d\n", daneodklienta.komenda);
      break;
    }

    // Broadcast message to all clients, including the sender
    /*       pthread_mutex_lock(&clientsMutex);
           for (int i = 0; i < clientCount; i++) {
               send(clients[i].socket, buffer, bytesReceived, 0);
           }
           pthread_mutex_unlock(&clientsMutex);
           */
  }

  // Remove client from list
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
  // Set up SIGPIPE handler
  signal(SIGPIPE, handleSigpipe);

  // Creating socket
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  // Specifying the address
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr =
      inet_addr("10.2.10.1"); // Use the correct IP address

  // Binding socket
  bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

  // Listening to the assigned socket
  listen(serverSocket, 5);

  printf("Server is listening on port %d...\n", PORT);

  while (1) {
    // Accepting connection request
    int clientSocket = accept(serverSocket, NULL, NULL);
    if (clientCount > 1) {
      close(clientSocket);
    }
    // Handle client in a new thread
    pthread_t thread;
    pthread_create(&thread, NULL, (void *)handleClient,
                   (void *)(intptr_t)clientSocket);
    pthread_detach(thread);
  }

  // Closing the socket (will never reach here in this example)
  close(serverSocket);

  return 0;
}
