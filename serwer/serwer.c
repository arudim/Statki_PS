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
// Maksymalna przewidziana ilosc klientow jest rowna 2
Client clients[2];
ClientData dummyClient;
// Inicjalizacja zmiennych i struktury
ClientData data_from_client;
int clientCount = 0;   // Liczba aktualnie podłączonych klientów
int index_klienta = 0; // Indeks aktualnego gracza (0 lub 1)
int max_id = 0;
time_t referencetime;
int gamestarted = 0;
int wygrana = 1;
int trafienie = 0;

pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex dla synchronizacji wątków

/*Zestaw trzech funkcji generujacych losowo plansze z ustaiwniem statkow*/
int is_valid_position(char board[10][10], int x, int y, int size, char orientation) {
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

void place_ship(char board[10][10], int x, int y, int size, char orientation, char ship_char) {
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
        }
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
    }
    /*Switch case obslugujacy flagi w strukturze.
    Serwer sprawdza typ flaig jaką otrzymał*/
    switch (data_from_client.comand) {
      /*Flaga INTRODUCTION nadaje klientowi dane id nastepnie ktore mu przesyła,
      następnie sprawdza czy połączyło się dwóch klientów jeżeli tak serwer generuje im plansze które są przesyłąne do klientów
      z flagą BOARD do ich wyrysowania. Następnie wysyłąna jest komęda do pierwszego klinanta GIVESHOT.*/
    case INTRODUCTION:
      int currentCount = clientCount;
      // Log serwera
      printf("Klient: %s\n", data_from_client.username);
      memset(&(clients[clientCount].data), 0, sizeof(clients[clientCount].data));
      pthread_mutex_lock(&clientsMutex);
      clients[clientCount].socket = clientSocket;
      memcpy(&(clients[clientCount].data), &data_from_client, sizeof(data_from_client));
      clients[clientCount].data.id = max_id + 1;
      clients[clientCount].data.comand = INVITE;
      send(clients[clientCount].socket, &(clients[clientCount].data), sizeof(clients[clientCount].data), 0);
      // Log serwera
      printf("send %d, k: %d\n", clientCount, clients[clientCount].data.comand);
      clientCount++;
      if (currentCount != clientCount && clientCount == 2) {
        for (int i = 0; i < clientCount; i++) {
          memset(&(clients[i].data.board), 0, sizeof(clients[i].data.board));
          set_board(clients[i].data.board);
          clients[i].data.comand = BOARD;
          send(clients[i].socket, &(clients[i].data), sizeof(clients[i].data), 0);
          printf("send %d, k: %d\n", i, clients[i].data.comand);
        }
      }
      if (clientCount == 2) {
        sleep(1);
        clients[0].data.comand = GIVESHOT;
        send(clients[0].socket, &(clients[0].data), sizeof(clients[0].data), 0);
        printf("send %d, k: %d\n", 0, clients[0].data.comand);
        gamestarted = 1;
        referencetime = time(NULL);
      }

      pthread_mutex_unlock(&clientsMutex);

      break;
      /*W przypadku flagi SHOT serwer sprawdza czy miejsce w którym oddano sprzał znajduje się statek jeżeli tak
      wysuje tam 'X' w przeciwnym wypadku 'o'. Następnie sprawdzany jest stan gry czy któryś z graczy zwyciężył
      jeżeli tak wysyłana jest struktura z flagą GAMEOVER kończąca grę, jeżeli gra nadal trwa ferwer odsyła strukturę
      z flagą BOARD do obu klientów oraz struktura z flagoa GIVESHOT do następnego gracza z kolei*/
    case SHOT:
      referencetime = time(NULL);
      // Log serwera
      printf("\n\nindex id: %d(%d), kto %d\n", clients[index_klienta].data.id, index_klienta, data_from_client.id);
      trafienie = 0;
      if (clients[index_klienta].data.id == data_from_client.id) {
        index_klienta = 1 - index_klienta;
        // Log serwera
        printf("Strzal Klient: %s.%d(%d)\n", data_from_client.username, data_from_client.id, index_klienta);
        char x = data_from_client.x;
        char y = data_from_client.y;
        if (clients[index_klienta].data.board[x][y] == 'A' ||
            clients[index_klienta].data.board[x][y] == 'B' ||
            clients[index_klienta].data.board[x][y] == 'C' ||
            clients[index_klienta].data.board[x][y] == 'D' ||
            clients[index_klienta].data.board[x][y] == 'X') {
          clients[index_klienta].data.board[x][y] = 'X';
          wygrana = 1;
          trafienie = 1;
        } else {
          clients[index_klienta].data.board[x][y] = 'o';
        }
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
            printf("send %d, %d\n", clients[index_klienta].data.comand, (int)send(clients[k].socket, &(clients[1 - index_klienta].data), sizeof(clients[1 - index_klienta].data), 0));
          }

        } else {
          int plansza = index_klienta;
          if (trafienie) {
            /*Po trafieniu przywarcamy klientowi kolejność*/
            index_klienta = 1 - index_klienta;
            clients[1 - index_klienta].data.comand = BOARD;
          } else {
            clients[index_klienta].data.comand = BOARD;
          }
          for (int i = 0; i < 2; i++) {
            clients[i].data.comand = BOARD;
            printf("send %d, %d\n", clients[plansza].data.comand, (int)send(clients[i].socket, &(clients[plansza].data), sizeof(clients[plansza].data), 0));
          }
        }
      }
      // Log serwera
      clients[index_klienta].data.comand = GIVESHOT;
      printf("send %d, %d\n", clients[index_klienta].data.comand, (int)send(clients[index_klienta].socket, &(clients[index_klienta].data), sizeof(clients[index_klienta].data), 0));
      printf("nowy index id: %d(%d)\n", clients[index_klienta].data.id, index_klienta);

      break;
    default:
      // Log serwera
      printf("Nieznane polecenie: %d\n", data_from_client.comand);
      break;
    }
  }

  /*Usuniecie klienta z vektora, oraz w przypadku przedwczesnego zakończenia gry zostaje wysłana wiadomość
  GAMEOVER do wygranego gracza*/
  pthread_mutex_lock(&clientsMutex);
  for (int i = 0; i < clientCount; i++) {
    if (clients[i].socket == clientSocket) {
      clients[i] = clients[clientCount - 1];
      clientCount--;
      for (int i = 0; i < clientCount; i++) {
        clients[i].data.comand = GAMEOVER;
        send(clients[i].socket, &(clients[i].data), sizeof(clients[i].data), 0);
        printf("send %d, k: %d\n", i, clients[i].data.comand);
      }
      break;
    }
  }
  pthread_mutex_unlock(&clientsMutex);
  close(clientSocket);
  gamestarted = 0;
}

void handleSigpipe(int signal) { printf("Caught SIGPIPE signal!\n"); }

void hadleWaitforshot() {
  while (1) {
    if (gamestarted) {
      double wait = difftime(time(NULL), referencetime);
      if (wait > 10) {
        clients[index_klienta].data.comand = GIVESHOT;
        printf("send %d %d, %d\n", index_klienta, clients[index_klienta].data.comand, (int)send(clients[index_klienta].socket, &(clients[index_klienta].data), sizeof(clients[index_klienta].data), 0));
        referencetime = time(NULL);
      }
      printf("waiting %f\n", wait);

    } else {
      printf("game not started\n");
    }
    sleep(1);
  }
}
int main() {
  // Ustawienie henlera SIGPIPE
  signal(SIGPIPE, handleSigpipe);
  srand(time(0));

  // Tworzenie socketu serwera
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Dtruktura adresu serwera
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(PORT);
  serverAddress.sin_addr.s_addr = inet_addr("0.0.0.0"); // Nasuchiwanie na wszystkich interfejsach

  // Bindowanie adresu serwera
  if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
    perror("Bind failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  // Start nasłuchiwania
  if (listen(serverSocket, 5) < 0) {
    perror("Listen failed");
    close(serverSocket);
    exit(EXIT_FAILURE);
  }

  printf("Server is listening on port %d...\n", PORT);

  fd_set readfds;
  int max_sd, activity;
  int clientCount = 0;

  // Czyszczenie tablicy deskryptorów
  FD_ZERO(&readfds);
  FD_SET(serverSocket, &readfds);
  max_sd = serverSocket;
  while (1) {

    // Dodawanie socketu klienta
    for (int i = 0; i < clientCount; i++) {
      int sd = clients[i].socket;
      if (sd > 0)
        FD_SET(sd, &readfds);
      if (sd > max_sd)
        max_sd = sd;
    }
    // Czekanie na aktywność w sockecie
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    // Sprawdzanie nadchozących połączeń
    if (FD_ISSET(serverSocket, &readfds)) {
      int clientSocket = accept(serverSocket, NULL, NULL);
      if (clientSocket < 0) {
        perror("Accept failed");
        continue;
      }

      if (clientCount >= 2) {
        printf("Too many players connected\n");
        dummyClient.comand = TOOMANY; // TOOMANY 
        send(clientSocket, &dummyClient, sizeof(dummyClient), 0);
        close(clientSocket);
      }
      // Obsługa klientów w nowym wądku
      pthread_t thread;
      pthread_create(&thread, NULL, (void *)handleClient, (void *)(intptr_t)clientSocket);
      pthread_detach(thread);
    }
  }

  close(serverSocket);
  return 0;
}
