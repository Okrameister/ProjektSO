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

int isDirectoryValid(const char *path); //sprawdza czy plik jest katalogiem

int copySmallFile(char *sourceFilePath, char *destinationPath); //kopiuje maly plik normalnie

int copyBigFile(char *sourceFilePath, char *destinationPath); //jak wielkosc pliku przekracza threshold to kopiujemy za pomocą mapowania

int removeFile(const char *filePath); //usuwa plik

int parseParameters(int argc, char **argv); //sprawdza poprawnosc parametrow i je dobrze ustawia

void handleSIGUSR1(); //zajmuje se sygnalem SIGUSR1

void printCurrentDateAndTime();

void runDaemon();

int normalSync(char* source, char* destination); //Normalna synchronizacja (same pliki, katalogi pomijamy)

int recursiveSync(char* recSource, char* recDestination); //-R jako rekurencyjna synchronizaja (synchronizujemy pliki i katalogi)

int recursiveCopyDirectory(char* sourceEntryPath, char* destinationEntryPath); //kopiuj katalog w przypadku -R

int recursiveRemoveDirectory(char* path); //usuń katalog z docelowego jak nie ma w źródłowym

void clearBuffer(char* buffer); //czyści bufor z pamięci
