
enum
{
    PRZEDSTAWIENIE=1,
    ZAPROSZENIE,
    STRZAL,
    PLANSZA,
    DAJSTRZAL,
    KONIECGRY
}KOMNEDY;

 typedef struct 
 {
    unsigned char komenda;
    char username[50];
    unsigned char id;
    char plansza[10][10];
    char x;
    char y;
    char trafienie;

 }ClientData;

typedef struct {
    int socket;
    ClientData data;
} Client;
 