#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int checkIfDirectory(char* path); //sprawdza czy plik jest katalogiem
int copySmallFile(char* sourceFilePath, char* destinationPath, unsigned int bufferSize); //kopiuje maly plik normalnie
int copyBigFile(char* sourceFilePath, char* destinationPath, unsigned int bufferSize); //jak wielkosc pliku przekracza threshold to kopiujemy za pomocą mapowania
int removeFile(char* filePath); //usuwa plik
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);
int handleSIGUSR1();

int main(int argc, char** argv)
{
    if(parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive) != 0)
    {
        //zle
    }
    else
    {
        //dobrze
    }
    //zrob tak jak Adam, czyli sprawdzanie czy masz dostep do pliku
    //czyli przechodzisz whilem przez pliki zrodlowego i patrzysz czy w docelowym masz access(path, F_OK)
    //i jak nie masz to znaczy ze trzeba skopiowac

    //jezeli jest access do pliku w zrodlowym i docelowym to sprawdzamy czy ich daty sa takie same i jak sa to skip
    //a jak nie, to usuwamy plik w docelowym i kopiujemy go ze zrodlowego

    //trzeba przed tym zrobic sprawdzanie czy plik jest katalogiem czy plikiem, a jak jest katalogiem
    //to pomijamy go, a jak jest wlaczone -R to wtedy do katalogu wchodzimy i synchronizujemy go rekurencyjnie

    //trzeba tez ogarnac co zrobic, zeby stal sie demonem - najpierw sprawdzic czy isDirectory docelową i zrodłową
    //i jak nie ma bledow to trzeba zrobic z niego demona
}