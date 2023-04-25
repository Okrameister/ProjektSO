#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <utime.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <syslog.h>

//flaga mowiaca, czy demon byl wybudzony sygnalem SIGUSR1
bool forcedSync;

//flaga mowiaca, czy jest wlaczona opcja -R - rekurencyjna synchronizacja katalogow
bool recursive;

//katalog zrodlowy
char* source;

//katalog docelowy
char* destination;

//prog kopiowania duzych plikow
unsigned long int threshold;

//bufor do kopiowania plikow
#define bufferSize 65536

//czas spania demona
int interval;

//do dokonczenia - logi zamiast printf
int isDirectoryValid(const char *path); //sprawdza czy plik jest katalogiem

//do dokonczenia - logi zamiast printf
int copySmallFile(char *sourceFilePath, char *destinationPath); //kopiuje maly plik normalnie

//do dokonczenia - logi zamiast printf
int copyBigFile(char *sourceFilePath, char *destinationPath); //jak wielkosc pliku przekracza threshold to kopiujemy za pomocą mapowania

//zrobione
int removeFile(const char *filePath); //usuwa plik

//do dokonczenia, obsługa bledow, wpisywanie do logow
int parseParameters(int argc, char **argv); //sprawdza poprawnosc parametrow i je dobrze ustawia

//zrobione
void handleSIGUSR1(); //zajmuje se sygnalem SIGUSR1

//do dokonczenia - logi zamiast printf
void printCurrentDateAndTime();

//do dokonczenia - logi zamiast printf oraz MA STAWAC SIE DEMONEM
void runDaemon();

//do dokonczenia - logi zamiast printf
int normalSync(); //Normalna synchronizacja (same pliki, katalogi pomijamy)

//do zrobienia
int recursiveSync(); //-R jako rekurencyjna synchronizaja (synchronizujemy pliki i katalogi)

//do zrobienia
int recursiveCopyDirectory(); //kopiuj katalog w przypadku -R

//do zrobienia
int recursiveRemoveDirectory(); //usuń katalog z docelowego jak nie ma w źródłowym