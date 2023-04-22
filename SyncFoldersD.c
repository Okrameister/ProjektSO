#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>

//zrobione
int isDirectoryValid(const char *path); //sprawdza czy plik jest katalogiem

//do dokonczenia, obsługa bledow, wpisywanie do logow
int copySmallFile(char *sourceFilePath, char *destinationPath, unsigned int bufferSize); //kopiuje maly plik normalnie

//do dokonczenia, obsługa bledow, wpisywanie do logow
int copyBigFile(char *sourceFilePath, char *destinationPath, unsigned int bufferSize); //jak wielkosc pliku przekracza threshold to kopiujemy za pomocą mapowania

//zrobione
int removeFile(const char *filePath); //usuwa plik

//do zrobienia
int parseParameters(int argc, char **argv, char *source, char *destination, unsigned int *interval, char *recursive); //sprawdza poprawnosc parametrow i je dobrze ustawia

//do zrobienia
int handleSIGUSR1(); //zajmuje se sygnalem SIGUSR1

//poniższe narazie bez parametrów:

//do zrobienia
int normalSync(); //Normalna synchronizacja (same pliki, katalogi pomijamy)

//do zrobienia
int setDate(); //Ustawianie daty modyfikacji pliku

//do zrobienia
int recursiveSync(); //-R jako rekurencyjna synchronizaja (synchronizujemy pliki i katalogi)

//do zrobienia
int recursiveCopyDirectory(); //kopiuj katalog w przypadku -R

//do zrobienia
int recursiveRemoveDirectory(); //usuń katalog z docelowego jak nie ma w źródłowym


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

int isDirectoryValid(const char* path)
{
    DIR *dir = opendir(path); //otwieramy katalog
    if(dir == NULL) //jezeli katalog nie istnieje zwracamy blad
    {
        return 1;
    }
    if(closedir(dir) != 0)
    {
        return 2; //jak zamykanie nie zwraca 0, to znaczy, ze jest blad
    }

    return 0;
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

int removeFile(const char* path)
{
  //remove zwraca 0, gdy nie ma bledow
  return remove(path);
}

int copyBigFile(char *sourceFilePath, char *destinationPath,unsigned int bufferSize)
{
    int sourceFile = open(sourceFilePath, O_RDONLY);

    if (sourceFile != 0)
    {
        //blad otwierania pliku zrodlowego
        return 1;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700); // plik do odczytu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile != 0)
    {
        close(sourceFile);
        //blad otwierania pliku docelowego
        return 2;
    }

    // Pobierz rozmiar pliku źródłowego
    struct stat source;
    if (fstat(sourceFile, &source) != 0)
    {
        close(sourceFile);
        close(destinationFile);
        return 3;
    }
    // Zmapuj plik źródłowy w całości w pamięci
	//[1]wartość NULL, to jądro może umieścić mapowanie w dowolnym miejscu, które uzna za stosowne.
	//[2]liczba bajtów do zmapowania
	//[3]rodzaj dostępu
	//[4]mapowanie nie będzie widoczne dla żadnego innego procesu, a wprowadzone zmiany nie zostaną zapisane w pliku.
	//[5]jaki plik
	//[6]przesunięcie od miejsca, w którym rozpoczęło się mapowanie pliku.
	//Po sukcesie mmmap() zwraca 0; w przypadku niepowodzenia funkcja zwraca MAP_FAILED.

    char *sourceFileMemory = mmap(NULL, source.st_size, PROT_READ, MAP_PRIVATE, sourceFile, 0);

    if (sourceFileMemory == MAP_FAILED) 
    {
        close(sourceFile);
        close(destinationFile);
        return 4;
    }

    if (write(destinationFile, sourceFileMemory, source.st_size) != 0) // Zapisanie zmapowanego pliku źródłowego do pliku docelowego
    {
        munmap(sourceFileMemory, source.st_size); //usuwanie zmapowanego regionu
        close(sourceFile);
        close(destinationFile);
        return 5;
    }

    // Zwolnienie zasobów
    munmap(sourceFileMemory, source.st_size);
    close(sourceFile);
    close(destinationFile);

    return 0;
}
