#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

//do zrobienia
int checkIfDirectory(char *path); //sprawdza czy plik jest katalogiem
//do dokonczenia
int copySmallFile(char *sourceFilePath, char *destinationPath, unsigned int bufferSize); //kopiuje maly plik normalnie
//do zrobienia
int copyBigFile(char *sourceFilePath, char *destinationPath, unsigned int bufferSize); //jak wielkosc pliku przekracza threshold to kopiujemy za pomocą mapowania
//do zrobienia
int removeFile(char *filePath); //usuwa plik
//do zrobienia
int parseParameters(int argc, char **argv, char *source, char *destination, unsigned int *interval, char *recursive); //sprawdza poprawnosc parametrow i je dobrze ustawia
//do zrobienia
int handleSIGUSR1(); //zajmuje se sygnalem SIGUSR1

int main(int argc, char **argv)
{
    char *source, destination, recursive;
    unsigned int interval;
    if (parseParameters(argc, argv, &source, &destination, interval, recursive) != 0)
    {
        // zle
    }
    else
    {
        // dobrze
    }
}

int copySmallFile(char *sourceFilePath, char *destinationPath, unsigned int bufferSize)
{
    if (bufferSize <= 0)
    {
        //za maly bufor, ale to chyba w parseParameters bedzie
    }

    int sourceFile = open(sourceFilePath, O_RDONLY);

    if (sourceFile == -1)
    {
        //blad otwierania pliku zrodlowego
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700); // plik do odczytu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile == -1)
    {
        close(sourceFile);
        //blad otwierania pliku docelowego
    }

    unsigned char *buffer = malloc(bufferSize);
    while (1)
    {
        unsigned int readFile = read(sourceFile, buffer, bufferSize); // czytamy z sourceFile
        write(destinationFile, buffer, readFile); //zapisujemy do destinationFile
        if (readFile < bufferSize)
            break; //jezeli pobierzemy mniej danych niz mozemy to znaczy, ze to ostatnia porcja i wychodzimy z petli
    }

    free(buffer);
    close(sourceFile);
    close(destinationFile);
    return 0;
}

void test123{
    printf("chuj");
}