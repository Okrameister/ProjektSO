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

//poniższe narazie bez parametrów

int setDate(); //Ustawianie daty docelowego na taką samą jak żródło
int recursiveSync(); //-R jako rekurencyjna synchronizaja
int copyDirectory(); //kopiuj katalog w przypadku -R
int removeDirectory(); //usuń katalog z docelowego jak nie ma w źródłowym


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


    // zrob tak jak Adam, czyli sprawdzanie czy masz dostep do pliku
    // czyli przechodzisz whilem przez pliki zrodlowego i patrzysz czy w docelowym masz access(path, F_OK)
    // i jak nie masz to znaczy ze trzeba skopiowac

    // jezeli jest access do pliku w zrodlowym i docelowym to sprawdzamy czy ich daty sa takie same i jak sa to skip
    // a jak nie, to usuwamy plik w docelowym i kopiujemy go ze zrodlowego

    // trzeba przed tym zrobic sprawdzanie czy plik jest katalogiem czy plikiem, a jak jest katalogiem
    // to pomijamy go, a jak jest wlaczone -R to wtedy do katalogu wchodzimy i synchronizujemy go rekurencyjnie

    // trzeba tez ogarnac co zrobic, zeby stal sie demonem - najpierw sprawdzic czy isDirectory docelową i zrodłową
    // i jak nie ma bledow to trzeba zrobic z niego demona
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