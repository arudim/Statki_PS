
enum
{   
    CONECTIONLOST=0,
    INTRODUCTION,
    INVITE,
    SHOT,
    BOARD,
    GIVESHOT,
    GAMEOVER,
    TOOMANY
}COMANDS;

 typedef struct 
 {
    unsigned char comand;
    char username[50];
    unsigned char id;
    char board[10][10];
    char x;
    char y;
 }ClientData;

typedef struct {
    int socket;
    ClientData data;
} Client;
 