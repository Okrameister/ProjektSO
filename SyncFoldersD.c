#include "library.h"

//TODO: WSZYSTKIE PRINTF ZAMIENIC NA WPISYWANIE DO LOGOW

//TODO2: POROWNYWANIE DAT ZROBIC DOBRZE

//TODO3: RecursiveSync

//TODO4: SPRAWIĆ, ZEBY BYL FAKTYCZNYM DEMONEM

int main(int argc, char **argv)
{
    if (parseParameters(argc, argv) != 0)
    {
        printf("Niepoprawne argumenty - poprawna składnia: ./SyncFoldersD [-R] [-i <czas_spania>] [-t <prog_kopiowania_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa\n");
        return -1;
    }

    //sygnalem SIGUSR1 zajmuje sie funkcja handleSIGUSR1
    signal(SIGUSR1, handleSIGUSR1);

    //to zostaje printf'em! to jest printf dla uzytkownika, zeby znal PID
    printf("PID procesu: %d\n", getpid());

    runDaemon();

    return 0;
}

void runDaemon()
{
    while(1)
    {
        if(forcedSync == false)
        {
            //jezeli Demon nie jest wybudzony sygnalem to piszemy, ze zostal wybudzony normalnie
            printCurrentDateAndTime();
            printf("Demon budzi się\n");
        }

        if(recursive == false)
        {
            if(normalSync() != 0) return;
        }
        else
        {
            if(recursiveSync() != 0) return;
        }

        printCurrentDateAndTime();
        printf("Demon zasypia\n");

        forcedSync = false;
        sleep(interval);
    }
}

void clearBuffer(char* buffer)
{
    //pobranie dlugosci tablicy
    size_t len = strlen(buffer);
    //wyczyszczenie tablicy
    memset(buffer, '\0', len);
}

int normalSync()
{
    DIR* sourceDir = opendir(source);
    if(sourceDir == NULL)
    {
      printCurrentDateAndTime();
      printf("normalSync: Błąd: błąd otwarcia katalogu żródłowego %s\n", source);
      return -1;
    }

    DIR* destinationDir = opendir(destination);
    if(destinationDir == NULL)
    {
      closedir(sourceDir);
      printCurrentDateAndTime();
      printf("normalSync: Błąd: błąd otwarcia katalogu docelowego %s\n", destination);
      return -2;
    }

    //struktury do przechowywania danych pozycji katalogu
    struct dirent* sourceEntry;
    struct dirent* destinationEntry;

    //bufory do przechowywania czasu modyfikacji plikow
    char sourceModificationTime[30];
    char destinationModificationTime[30];

    //bufor do przechowania ścieżki danej pozycji w katalogu
    char sourceEntryPath[PATH_MAX];
    char destinationEntryPath[PATH_MAX];

    //struktury do przechowywania informacji o plikach
    struct stat sourceFileInfo;
	struct stat destinationFileInfo;

    //struktura do przechowywania czasu modyfikacji pliku
    struct utimbuf sourceTime;

    //przechodzimy po wszystkich pozycjach w katalogu źródłowym
    while((sourceEntry = readdir(sourceDir)) != NULL)
    {
        //jezeli pozycja jest "zwyklym plikiem" to wchodzimy do ifa
        if(sourceEntry->d_type == DT_REG)
        {
            //budowanie ściezki do pliku docelowego - przechowywana w sestinationEntryPath
            if((snprintf(destinationEntryPath, PATH_MAX, "%s/%s", destination, sourceEntry->d_name) >= PATH_MAX))
            {
                printCurrentDateAndTime();
                printf("normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego\n");
                return -3;
            }
            //jezeli istnieje (mamy dostep) plik w katalogu docelowym (sciezka przechowana w destinationEntryPath)
            if(access(destinationEntryPath, F_OK) == 0)
            {
                //Tworzymy sciezke do pliku w katalogu zrodolwymm, aby moc sprawdzic jego date i porownac z docelowym
                if((snprintf(sourceEntryPath, PATH_MAX, "%s/%s", source, sourceEntry->d_name) >= PATH_MAX))
                {
                    printCurrentDateAndTime();
                    printf("normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
                    return -4;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) pliku zrodlowego
               	if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("normalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s\n", sourceEntryPath);
					return -5;
				}
                //ustawienie pod sourceModificationTime czasu modyfikacji pliku zrodlowego
				strftime(sourceModificationTime, sizeof(sourceModificationTime), "%Y-%m-%d %H:%M:%S", localtime(&sourceFileInfo.st_mtime));

                //szukamy odpowiedniego "zwyklego pliku" w katalogu docelowym
                while((destinationEntry = readdir(destinationDir)) != NULL)
                {
                    //jezeli znajdziemy w katalogu docelowym plik o tym samym typie i nazwie to jestesmy pewni, ze to ten sam plik
                    if(destinationEntry->d_type == DT_REG && strcmp(destinationEntry->d_name, sourceEntry->d_name) == 0)
                    {
                        //pobieramy informacje o pliku docelowym
                        if (stat(destinationEntryPath, &sourceFileInfo) != 0)
				        {
                            printCurrentDateAndTime();
					        printf("normalSync: Błąd: Błąd przy pobieraniu informacji pliku docelowego %s\n", destinationEntryPath);
					        return -6;
				        }
                        //pobieramy date do destinationModificationTime
                        strftime(destinationModificationTime, sizeof(destinationModificationTime), "%Y-%m-%d %H:%M:%S", localtime(&destinationFileInfo.st_mtime));

                        //jezeli nie zgadzaja sie daty modyfikacji, to kopiujemy plik zrodlowy i ustawiamy odpowiednie daty
                        if (strcmp(sourceModificationTime, destinationModificationTime) != 0)
                        {
                            long int size = sourceFileInfo.st_size;
	                        if(size <= threshold)
                            {
                                if(copySmallFile(sourceEntryPath, destinationEntryPath) != 0) return -7;
                            }
                            else
                            {
                                if(copyBigFile(sourceEntryPath, destinationEntryPath) != 0) return -8;
                            }

                            //pobranie czasów do ustawienia
                            sourceTime.actime = sourceFileInfo.st_atime;
                            sourceTime.modtime = sourceFileInfo.st_mtime;

                            //ustawienie czasow dla pliku docelowego
                            if (utime(destinationEntryPath, &sourceTime) != 0)
							{	
                                printCurrentDateAndTime();
								printf("normalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s\n",destinationEntryPath);
								return -7;
							}
                        }
                    }
                }
            }
            //jezeli plik nie istnieje w katalogu docelowyn
            else
            {
                //Tworzymy sciezke do pliku w katalogu zrodolwym, aby moc sprawdzic jego date i przypisac do przyszlego docelowego
                if((snprintf(sourceEntryPath, PATH_MAX, "%s/%s", source, sourceEntry->d_name) >= PATH_MAX))
                {
                    printCurrentDateAndTime();
                    printf("normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
                    return -8;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) pliku zrodlowego
               	if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("normalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s\n", sourceEntryPath);
					return -9;
				}
                long int size = sourceFileInfo.st_size;
	            if(size <= threshold)
                {
                    if(copySmallFile(sourceEntryPath, destinationEntryPath) != 0) return -10;
                }
                else
                {
                    if(copyBigFile(sourceEntryPath, destinationEntryPath) != 0) return -11;
                }

                //pobranie czasów do ustawienia
                sourceTime.actime = sourceFileInfo.st_atime;
                sourceTime.modtime = sourceFileInfo.st_mtime;

                //ustawienie czasow dla pliku docelowego
                if (utime(destinationEntryPath, &sourceTime) != 0)
				{	
                    printCurrentDateAndTime();
					printf("normalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s\n", destinationEntryPath);
					return -12;
				}
            }
        }
        clearBuffer(sourceEntryPath);
        clearBuffer(destinationEntryPath);
        clearBuffer(sourceModificationTime);
        clearBuffer(destinationModificationTime);
        rewinddir(destinationDir);
    }
    //TERAZ PRZECHODZIMY PO KATALOGU DOCELOWYM I USUWAMY PLIKI, KTORYCH NIE MA W ZRODLOWYM
    while((destinationEntry = readdir(destinationDir)) != NULL)
    {
        //jezeli pozycja w docelowym jest zwyklym plikiem
        if ((destinationEntry->d_type) == DT_REG)
        {
            //budujemy sciezke do pliku w katalogu zrodlowym
 			if (snprintf(sourceEntryPath, PATH_MAX, "%s/%s", source, destinationEntry->d_name) >= PATH_MAX)
			{
                printCurrentDateAndTime();
				printf("normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
				return -13;
			}
            //jezeli nie istnieje taki plik w katalogu zrodlowym
            if(access(sourceEntryPath, F_OK) != 0)
            {
                //budujemy sciezke do pliku w katalogu docelowym
                if (snprintf(destinationEntryPath, PATH_MAX, "%s/%s", destination, destinationEntry->d_name) >= PATH_MAX)
			    {
                    printCurrentDateAndTime();
				    printf("normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego\n");
				    return -14;
			    }
                //usuwamy plik z katalogu docelowego
                if(removeFile(destinationEntryPath) != 0)
                {
                    return -15;
                }
            }           
        }
        clearBuffer(destinationEntryPath);
        clearBuffer(sourceEntryPath);
    }
    closedir(sourceDir);
    closedir(destinationDir);
    
    return 0;
}

int recursiveSync()
{

}

int isDirectoryValid(const char* path)
{
    DIR *dir = opendir(path); //otwieramy katalog
    if(dir == NULL) //jezeli katalog nie istnieje zwracamy blad
    {
        return -1;
    }
    if(closedir(dir) != 0)
    {
        return -2; //jak zamykanie nie zwraca 0, to znaczy, ze jest blad
    }

    return 0;
}

int copySmallFile(char *sourceFilePath, char *destinationPath)
{
    int sourceFile = open(sourceFilePath, O_RDONLY);

    if (sourceFile == -1)
    {
        printCurrentDateAndTime();
        printf("copySmallFile: Błąd: błąd otwarcia pliku źródłowego %s\n", sourceFilePath);
        return -1;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700); // plik do odczytu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile == -1)
    {
        close(sourceFile);
        printCurrentDateAndTime();
        printf("copySmallFile: Błąd: błąd otwarcia pliku docelowego %s\n", destinationPath);
        return -2;
    }

    unsigned char *buffer = malloc(bufferSize);
    
    while (1)
    {
        unsigned int readFile = read(sourceFile, buffer, bufferSize); // czytamy z sourceFile
        write(destinationFile, buffer, readFile); //zapisujemy do destinationFile
        if (readFile < bufferSize)
            break; //jezeli pobierzemy mniej danych niz mozemy to znaczy, ze to ostatnia porcja i wychodzimy z petli
    }
    printCurrentDateAndTime();
    printf("copySmallFile: skopiowano plik %s\n", sourceFilePath);

    free(buffer);
    close(sourceFile);
    close(destinationFile);
    return 0;
}

int removeFile(const char* path)
{
  if(remove(path) != 0)
  {
      printCurrentDateAndTime();
      printf("removeFile: Błąd: usuwanie pliku %s nie powiodło się\n", path);
      return -1;
  }
  printCurrentDateAndTime();
  printf("removeFile: usunięto plik %s\n", path);
  return 0;
}

int copyBigFile(char *sourceFilePath, char *destinationPath)
{
    int sourceFile = open(sourceFilePath, O_RDONLY);

    if (sourceFile == -1)
    {
        printCurrentDateAndTime();
        printf("copyBigFile: Błąd: błąd otwarcia pliku źródłowego %s\n", sourceFilePath);
        return -1;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700); // plik do odczytu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile == -1)
    {
        close(sourceFile);
        printCurrentDateAndTime();
        printf("copyBigFile: Błąd: błąd otwarcia pliku docelowego %s\n", destinationPath);
        return -2;
    }

    // Pobierz rozmiar pliku źródłowego
    struct stat source;
    if (fstat(sourceFile, &source) != 0)
    {
        close(sourceFile);
        close(destinationFile);
        printCurrentDateAndTime();
        printf("copyBigFile: Błąd: nie udało się pobrać rozmiaru pliku źródłowego %s\n", sourceFilePath);
        return -3;
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
        printCurrentDateAndTime();
        printf("copyBigFile: Błąd: mapowanie pliku %s nie powiodło się\n", sourceFilePath);
        return -4;
    }

    if (write(destinationFile, sourceFileMemory, source.st_size) != 0) // Zapisanie zmapowanego pliku źródłowego do pliku docelowego
    {
        munmap(sourceFileMemory, source.st_size); //usuwanie zmapowanego regionu
        close(sourceFile);
        close(destinationFile);
        printCurrentDateAndTime();
        printf("copyBigFile: Błąd: zapisanie zmapowanego pliku źródłowego %s do pliku docelowego %s nie powiodło się\n", sourceFilePath, destinationPath);
        return -5;
    }

    printCurrentDateAndTime();
    printf("copyBigFile: skopiowano plik %s\n", sourceFilePath);

    // Zwolnienie zasobów
    munmap(sourceFileMemory, source.st_size);
    close(sourceFile);
    close(destinationFile);

    return 0;
}

//sprawdza poprawnosc parametrow i je dobrze ustawia
//argc liczba całkowita określająca liczbę argumentów wiersza poleceń,argv wskaźnik do tablicy reprezentującej argumenty wiersza poleceń.
int parseParameters(int argc, char **argv)
{
    // Jeżeli nie podano żadnych parametrów
    if (argc <= 1)
    {
        printCurrentDateAndTime();
        printf("parseParameters: Błąd: nie podano wystarczająco argumentów\n");
        return -1;
    }
    // Zapisujemy domyślny czas spania w sekundach równy 5*60 s = 5 min.
    interval = 5 * 60;
    // Zapisujemy domyślny brak rekurencyjnej synchronizacji katalogów.
    recursive = false;
    // Zapisujemy domyślny próg dużego pliku równy maksymalnej wartości zmiennej typu unsigned long int
    threshold = ULONG_MAX;
    //bazowo wymuszona synchronizacja jest na false
    forcedSync = false;

    // Sprawdzamy argumenty
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-R") == 0)
        {
            recursive = true; // Ustawiamy rekurencyjną synchronizację katalogów.
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
          //Sprawdzamy, czy anstepny argument jest liczbą
            if(argv[i+1][0] >= 48 && argv[i+1][0] <= 57)
            {
              interval= strtoul(argv[i + 1], NULL, 10); // Ustawiamy próg na podany przez uzytkownika
              i++;
            }
            else
            {
              printCurrentDateAndTime();
              printf("parseParameters: Błąd: nie podano wartości czasu spania programu (-i)\n");
              return -2;
            }
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
          //Sprawdzamy, czy anstepny argument jest liczbą
            if(argv[i+1][0] >= 48 && argv[i+1][0] <= 57)
            {
              threshold = strtoul(argv[i + 1], NULL, 10); // Ustawiamy próg na podany przez uzytkownika
              i++;
            }
            else
            {
              printCurrentDateAndTime();
              printf("parseParameters: Błąd: nie podano wartości progu (-t)\n");
              return -3;
            }
        }
        // Jeśli nie jest tamtymi opcjami, to jest to ścieżka źródłowa lub docelowa
        else
        {
            // Sprawdzamy czy nie podano już ścieżki źródłowej
            if (source == NULL)
            {
              source = argv[i];
            }
            // Sprawdzamy czy nie podano już ścieżki docelowej
            else if (destination == NULL)
            {
              destination = argv[i];
            }
            else
            {
              printCurrentDateAndTime();
              printf("parseParameters: Błąd: podano za dużo argumentów\n");
            }
        }
    }
    // Sprawdzamy czy podano obie ścieżki
    if (source == NULL || destination == NULL)
    {
        printCurrentDateAndTime();
        printf("parseParameters: Błąd: brak jednej ze ścieżek\n");
        return -5;
    }
    if (isDirectoryValid(source) != 0)
    {
        printCurrentDateAndTime();
        printf("parseParameters: Błąd: nieprawidłowy katalog źródłowy %s\n", source);
        return -6; // nie jest
    }
    if (isDirectoryValid(destination) != 0)
    {
        printCurrentDateAndTime();
        printf("parseParameters: Błąd: nieprawidłowy katalog docelowy %s\n", destination);
        return -7; // nie jest
    }

    return 0;
}

void printCurrentDateAndTime()
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  printf("[%02d.%02d.%d %02d:%02d:%02d]: ", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void handleSIGUSR1()
{
    printCurrentDateAndTime();
    printf("Daemon wybudzony sygnałem SIGUSR1\n");
    forcedSync = true;
}

