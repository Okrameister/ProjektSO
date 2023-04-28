#include "library.h"

int main(int argc, char **argv)
{
    //inicjalizacja systemu logów
    openlog("SyncFoldersD", LOG_ODELAY | LOG_PID, LOG_DAEMON);   

    pid_t process_id = 0;
    pid_t sid = 0;

    //utworzenie procesu potomnego
    process_id = fork();
    if(process_id < 0){
        printf("\nBłąd! Nie uruchomiono daemona!\n");
        exit(1);
    }
    if(process_id > 0){
        //kill dla procesu pierwotnego
        exit(0);
    }

    if (parseParameters(argc, argv) != 0)
    {
        syslog(LOG_ERR, "Niepoprawne argumenty - poprawna składnia: ./SyncFoldersD [-R] [-i <czas_spania_w_sekundach>] [-t <prog_kopiowania_duzego_pliku_w_bajtach>] sciezka_zrodlowa sciezka_docelowa");
        printf("\nNiepoprawne argumenty - poprawna składnia: ./SyncFoldersD [-R] [-i <czas_spania>] [-t <prog_kopiowania_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa\n");
        return -1;
    }

    //sygnalem SIGUSR1 zajmuje sie funkcja handleSIGUSR1
    signal(SIGUSR1, handleSIGUSR1);

    //to zostaje printf'em! to jest printf dla uzytkownika, zeby znal PID
    printf("\nPID procesu: %d", getpid());

    //uruchomienie demona
    runDaemon();

    //zamknięcie logów
    closelog();

    return 0;
}

void runDaemon()
{
    //podczas dziania Demona ignorujemy sygnal
    signal(SIGUSR1, SIG_IGN);
    while(1)
    {
        if(forcedSync == false)
        {
            //jezeli Demon nie jest wybudzony sygnalem to piszemy, ze zostal wybudzony normalnie
            printCurrentDateAndTime();
            printf("\nDemon budzi się");
            syslog(LOG_INFO,"Demon budzi się");
        }

        if(recursive == false)
        {
            if(normalSync(source, destination) != 0) return;
        }
        else
        {
            if(recursiveSync(source, destination) != 0) return;
        }

        printCurrentDateAndTime();
        printf("\nDemon zasypia");
        syslog(LOG_INFO,"Demon zasypia");

        //po uspieniu wznawiamy obsluge sygnalu
        signal(SIGUSR1, handleSIGUSR1);

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

int normalSync(char* source, char* destination)
{
    DIR* sourceDir = opendir(source);
    if(sourceDir == NULL)
    {
      printCurrentDateAndTime();
      printf("\nnormalSync: Błąd: błąd otwarcia katalogu żródłowego %s\n", source);
      syslog(LOG_ERR,"normalSync: Błąd: błąd otwarcia katalogu żródłowego %s", source);
      return -1;
    }

    DIR* destinationDir = opendir(destination);
    if(destinationDir == NULL)
    {
      closedir(sourceDir);
      printCurrentDateAndTime();
      printf("\nnormalSync: Błąd: błąd otwarcia katalogu docelowego %s\n", destination);
      syslog(LOG_ERR,"normalSync: Błąd: błąd otwarcia katalogu docelowego %s", destination);
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
            //budowanie ściezki do pliku docelowego - przechowywana w destinationEntryPath
            if((snprintf(destinationEntryPath, PATH_MAX, "%s/%s", destination, sourceEntry->d_name) >= PATH_MAX))
            {
                printCurrentDateAndTime();
                printf("\nnormalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego\n");
                syslog(LOG_ERR,"normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego");
                return -3;
            }
            //jezeli istnieje (mamy dostep) plik w katalogu docelowym (sciezka przechowana w destinationEntryPath)
            if(access(destinationEntryPath, F_OK) == 0)
            {
                //Tworzymy sciezke do pliku w katalogu zrodolwym, aby moc sprawdzic jego date i porownac z docelowym
                if((snprintf(sourceEntryPath, PATH_MAX, "%s/%s", source, sourceEntry->d_name) >= PATH_MAX))
                {
                    printCurrentDateAndTime();
                    printf("\nnormalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego");
                    return -4;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) pliku zrodlowego
               	if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("\nnormalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s\n", sourceEntryPath);
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s", sourceEntryPath);
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
                        if (stat(destinationEntryPath, &destinationFileInfo) != 0)
				        {
                            printCurrentDateAndTime();
					        printf("\nnormalSync: Błąd: Błąd przy pobieraniu informacji pliku docelowego %s\n", destinationEntryPath);
                            syslog(LOG_ERR, "normalSync: Błąd: Błąd przy pobieraniu informacji pliku docelowego %s", destinationEntryPath);
					        return -6;
				        }
                        //pobieramy date do destinationModificationTime
                        strftime(destinationModificationTime, sizeof(destinationModificationTime), "%Y-%m-%d %H:%M:%S", localtime(&destinationFileInfo.st_mtime));

                        //jezeli nie zgadzaja sie daty modyfikacji, to kopiujemy plik zrodlowy i ustawiamy odpowiednie daty
                        if (strcmp(sourceModificationTime, destinationModificationTime) != 0)
                        {
                            printCurrentDateAndTime();
                            printf("\nnormalSync: Inna data modyfikacji pliku %s w katalogu docelowym\n", sourceEntryPath);
                            syslog(LOG_INFO,"normalSync: Inna data modyfikacji pliku %s w katalogu docelowym", sourceEntryPath);

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
								printf("\nnormalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s\n",destinationEntryPath);
                                syslog(LOG_ERR,"normalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s",destinationEntryPath);
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
                    printf("\nnormalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego");
                    return -8;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) pliku zrodlowego
               	if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("\nnormalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s\n", sourceEntryPath);
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s", sourceEntryPath);
					return -9;
				}

                printCurrentDateAndTime();
                printf("\nnormalSync: Plik %s nie istnieje w katalogu docelowym\n", sourceEntryPath);
                syslog(LOG_INFO,"normalSync: Plik %s nie istnieje w katalogu docelowym", sourceEntryPath);

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
					printf("\nnormalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s\n", destinationEntryPath);
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy ustawieniu czasu modyfikacji pliku docelowego %s", destinationEntryPath);
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
				printf("\nnormalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego\n");
                syslog(LOG_ERR,"normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku źródłowego");
				return -13;
			}
            //jezeli nie istnieje taki plik w katalogu zrodlowym
            if(access(sourceEntryPath, F_OK) != 0)
            {
                //budujemy sciezke do pliku w katalogu docelowym
                if (snprintf(destinationEntryPath, PATH_MAX, "%s/%s", destination, destinationEntry->d_name) >= PATH_MAX)
			    {
                    printCurrentDateAndTime();
				    printf("\nnormalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego\n");
                    syslog(LOG_ERR,"normalSync: Błąd: Błąd przy konstruowaniu ścieżki do pliku docelowego");
				    return -14;
			    }

                printCurrentDateAndTime();
                printf("\nnormalSync: Plik %s nie istnieje w katalogu źródłowym\n", destinationEntryPath);
                syslog(LOG_INFO,"normalSync: Plik %s nie istnieje w katalogu źródłowym", destinationEntryPath);

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

int recursiveSync(char* recSource, char* recDestination)
{
    DIR* sourceDir = opendir(recSource);
    if(sourceDir == NULL)
    {
      printCurrentDateAndTime();
      printf("\nrecursiveSync: Błąd: błąd otwarcia katalogu żródłowego %s\n", recSource);
      syslog(LOG_ERR,"recursiveSync: Błąd: błąd otwarcia katalogu żródłowego %s", recSource);
      return -1;
    }

    DIR* destinationDir = opendir(recDestination);
    if(destinationDir == NULL)
    {
      closedir(sourceDir);
      printCurrentDateAndTime();
      printf("\nrecursiveSync: Błąd: błąd otwarcia katalogu docelowego %s\n", recDestination);
      syslog(LOG_ERR,"recursiveSync: Błąd: błąd otwarcia katalogu docelowego %s", recDestination);
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
        //jezeli pozycja jest katalogiem to wchodzimy do ifa
        if(sourceEntry->d_type == DT_DIR)
        {
            //pominięcie folderów /. i /..
            if(!strcmp(sourceEntry->d_name,".")||!strcmp(sourceEntry->d_name,"..")) continue;
        
            //budowanie ściezki do katalogu docelowego - przechowywana w destinationEntryPath
            if((snprintf(destinationEntryPath, PATH_MAX, "%s/%s", recDestination, sourceEntry->d_name) >= PATH_MAX))
            {
                printCurrentDateAndTime();
                printf("\nrecursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu docelowego\n");
                syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu docelowego");
                return -3;
            }
            //jezeli istnieje (mamy dostep) do katalogu docelowego (sciezka przechowana w destinationEntryPath)
            if(access(destinationEntryPath, F_OK) == 0)
            {
                //Tworzymy sciezke do katalogu w katalogu zrodolwym, aby moc sprawdzic jego date i porownac z docelowym
                if((snprintf(sourceEntryPath, PATH_MAX, "%s/%s", recSource, sourceEntry->d_name) >= PATH_MAX))
                {
                    printCurrentDateAndTime();
                    printf("\nrecursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego\n");
                    syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego");
                    return -4;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) katalogu
               	if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("\nrecursiveSync: Błąd: Błąd przy pobieraniu informacji katalogu źródłowego %s\n", sourceEntryPath);
					syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy pobieraniu informacji pliku źródłowego %s", sourceEntryPath);
                    return -5;
				}
                //ustawienie pod sourceModificationTime czasu modyfikacji katalogu zrodlowego
				strftime(sourceModificationTime, sizeof(sourceModificationTime), "%Y-%m-%d %H:%M:%S", localtime(&sourceFileInfo.st_mtime));

                //szukamy odpowiedniego katalogu w miejscu docelowym
                while((destinationEntry = readdir(destinationDir)) != NULL)
                {
                    //jezeli znajdziemy w katalogu docelowym folder o tym samym typie i nazwie to jestesmy pewni, ze to ten sam katalog
                    if(destinationEntry->d_type == DT_DIR && strcmp(destinationEntry->d_name, sourceEntry->d_name) == 0)
                    {
                        //pobieramy informacje o katalogu docelowym
                        if (stat(destinationEntryPath, &destinationFileInfo) != 0)
				        {
                            printCurrentDateAndTime();
					        printf("\nrecursiveSync: Błąd: Błąd przy pobieraniu informacji katalogu docelowego %s\n", destinationEntryPath);
					        syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy pobieraniu informacji katalogu docelowego %s", destinationEntryPath);
                            return -6;
				        }
                        //pobieramy date do destinationModificationTime
                        strftime(destinationModificationTime, sizeof(destinationModificationTime), "%Y-%m-%d %H:%M:%S", localtime(&destinationFileInfo.st_mtime));

                        //jezeli nie zgadzaja sie daty modyfikacji, to kopiujemy katalog zrodlowy i ustawiamy odpowiednie daty
                        if (strcmp(sourceModificationTime, destinationModificationTime) != 0)
                        {
                            printCurrentDateAndTime();
                            printf("\nrecursiveSync: Inna data modyfikacji folderu %s w katalogu docelowym\n", sourceEntryPath);
                            syslog(LOG_INFO,"recursiveSync: Inna data modyfikacji folderu %s w katalogu docelowym", sourceEntryPath);

                            //wywołanie rekursywnej synchronizacji dla obecnego katalogu
                            recursiveSync(sourceEntryPath,destinationEntryPath);

                            //pobranie czasów do ustawienia
                            sourceTime.actime = sourceFileInfo.st_atime;
                            sourceTime.modtime = sourceFileInfo.st_mtime;

                            //ustawienie czasow dla katalogu docelowego
                            if (utime(destinationEntryPath, &sourceTime) != 0)
							{	
                                printCurrentDateAndTime();
								printf("\nrecursiveSync: Błąd: Błąd przy ustawieniu czasu modyfikacji katalogu docelowego %s\n",destinationEntryPath);
								syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy ustawieniu czasu modyfikacji katalogu docelowego %s",destinationEntryPath);
                                return -7;
							}
                        }
                    }
                }
            } else {
                //Tworzymy sciezke do folderu w katalogu zrodolwym, aby moc sprawdzic jego date i przypisac do przyszlego docelowego
                if((snprintf(sourceEntryPath, PATH_MAX, "%s/%s", recSource, sourceEntry->d_name) >= PATH_MAX))
                {
                    printCurrentDateAndTime();
                    printf("\nrecursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego\n");
                    syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego");
                    return -8;
                }
                //Jezeli nie uda nam sie pobrac informacji (takich jak data) folderu zrodlowego
               	
                if (stat(sourceEntryPath, &sourceFileInfo) != 0)
				{
                    printCurrentDateAndTime();
					printf("\nrecursiveSync: Błąd: Błąd przy pobieraniu informacji katalogu źródłowego %s\n", sourceEntryPath);
					return -9;
				}

                printCurrentDateAndTime();
                printf("\nrecursiveSync: Folder %s nie istnieje w katalogu docelowym\n", sourceEntryPath);
                syslog(LOG_INFO,"recursiveSync: Folder %s nie istnieje w katalogu docelowym", sourceEntryPath);

                recursiveCopyDirectory(sourceEntryPath, destinationEntryPath);

                //pobranie czasów do ustawienia
                sourceTime.actime = sourceFileInfo.st_atime;
                sourceTime.modtime = sourceFileInfo.st_mtime;

                //ustawienie czasow dla folderu docelowego
                if (utime(destinationEntryPath, &sourceTime) != 0)
				{	
                    printCurrentDateAndTime();
					printf("\nrecursiveSync: Błąd: Błąd przy ustawieniu czasu modyfikacji katalogu docelowego %s\n", destinationEntryPath);
					syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy ustawieniu czasu modyfikacji katalogu docelowego %s", destinationEntryPath);
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

    //Przechodzimy po katalogu docelowym i usuwamy foldery nieistniejące w źródłowym
    while((destinationEntry = readdir(destinationDir)) != NULL)
    {
        //jezeli pozycja w docelowym jest katalogiem
        if ((destinationEntry->d_type) == DT_DIR)
        {
            //budujemy sciezke do folderu w katalogu zrodlowym
 			if (snprintf(sourceEntryPath, PATH_MAX, "%s/%s", recSource, destinationEntry->d_name) >= PATH_MAX)
			{
                printCurrentDateAndTime();
				printf("\nrecursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego\n");
                syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu źródłowego");
				return -13;
			}
            //jezeli nie istnieje taki folder w katalogu zrodlowym
            if(access(sourceEntryPath, F_OK) != 0)
            {
                //budujemy sciezke do folderu w katalogu docelowym
                if (snprintf(destinationEntryPath, PATH_MAX, "%s/%s", recDestination, destinationEntry->d_name) >= PATH_MAX)
			    {
                    printCurrentDateAndTime();
				    printf("\nrecursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu docelowego\n");
                    syslog(LOG_ERR,"recursiveSync: Błąd: Błąd przy konstruowaniu ścieżki do katalogu docelowego");
				    return -14;
			    }

                printCurrentDateAndTime();
                printf("\nrecursiveSync: Folder %s nie istnieje w katalogu źródłowym\n", destinationEntryPath);
                syslog(LOG_INFO,"recursiveSync: Folder %s nie istnieje w katalogu źródłowym", destinationEntryPath);

                //usuwamy folder z katalogu docelowego
                if(recursiveRemoveDirectory(destinationEntryPath) != 0)
                {
                    printCurrentDateAndTime();
                    printf("\nrecursiveSync: Błąd: folder nie został usunięty z katalogu docelowego\n");
                    syslog(LOG_INFO,"recursiveSync: Błąd: folder nie został usunięty z katalogu docelowego");
                    return -15;
                }
            }           
        }
        clearBuffer(destinationEntryPath);
        clearBuffer(sourceEntryPath);
    }
    closedir(sourceDir);
    closedir(destinationDir);
    
    //synchronizacja zwykłych plików po rozpatrzeniu każdego folderu w katalogu 
    normalSync(recSource, recDestination);

    return 0;
}

int recursiveCopyDirectory(char* recSource, char* recDestination)
{
    DIR* sourceDir = opendir(recSource);
    if(sourceDir == NULL)
    {
      printCurrentDateAndTime();
      printf("\nrecursiveCopyDirectory: Błąd: błąd otwarcia katalogu żródłowego %s\n", recSource);
      syslog(LOG_ERR,"recursiveCopyDirectory: Błąd: błąd otwarcia katalogu żródłowego %s", recSource);
      return -1;
    }

    //struktury do przechowywania danych pozycji katalogu
    struct dirent* sourceEntry;
    struct dirent* destinationEntry;
    
    //struktury do przechowywania informacji o pliku wejściowym
    struct stat sourceFileInfo;

    //tworzenie nowego katalogu
    if(mkdir(recDestination, 0777)!=0){
        printCurrentDateAndTime();
        printf("\nrecursiveCopyDirectory: Błąd: Nie stworzono katalogu %s\n", recSource);
        syslog(LOG_ERR,"recursiveCopyDirectory: Błąd: Nie stworzono katalogu %s", recSource);
    } else{
        printCurrentDateAndTime();
        printf("\nrecursiveCopyDirectory: Stworzono katalog %s\n", recSource);
        syslog(LOG_INFO,"recursiveCopyDirectory: Stworzono katalog %s", recSource);
    }

    //przechodzimy po wszystkich pozycjach w katalogu źródłowym
    while((sourceEntry = readdir(sourceDir)) != NULL)
    {

        //stworzenie pełnej ścieżki do pliku
            char filePathSource[100];
            strcpy(filePathSource, recSource);
            strcat(filePathSource,"/");
            strcat(filePathSource, sourceEntry->d_name);

            char filePathDest[100];
            strcpy(filePathDest, recDestination);
            strcat(filePathDest,"/");
            strcat(filePathDest, sourceEntry->d_name);

        //jezeli pozycja jest "katalogiem" to wchodzimy do ifa
        if(sourceEntry->d_type == DT_DIR)
        {
            //pominięcie folderów /. i /..
            if(!strcmp(sourceEntry->d_name,".")||!strcmp(sourceEntry->d_name,"..")) continue;
            
            //rekurencyjne uruchomienie funkcji dla katalogu podrzędnego
            recursiveCopyDirectory(filePathSource,filePathDest);

        }else{
            //kopiowanie zwykłego pliku do katalogu docelowego
            long int size = sourceFileInfo.st_size;
            if(size <= threshold)
            {
                if(copySmallFile(filePathSource,filePathDest) != 0) return -7;
            }
            else
            {
                if(copyBigFile(filePathSource,filePathDest) != 0) return -8;
            }
        }
    }
}

int recursiveRemoveDirectory(char* path)
{
    DIR* folderPath = opendir(path);
    if(folderPath == NULL)
    {
      printCurrentDateAndTime();
      printf("\nrecursiveRemoveDirectory: Błąd: błąd otwarcia katalogu żródłowego %s\n", path);
      syslog(LOG_ERR,"recursiveRemoveDirectory: Błąd: błąd otwarcia katalogu żródłowego %s\n", path);
      return -1;
    }

    //Struktura przechowuje dane pozycji w katalogu
    struct dirent* pathEntry;

    //przechodzimy po wszystkich pozycjach w katalogu źródłowym
    while((pathEntry = readdir(folderPath)) != NULL)
    {
        //jezeli pozycja jest "katalogiem" to wchodzimy do ifa
        if(pathEntry->d_type == DT_DIR)
        {
            //pominięcie folderów /. i /..
            if(!strcmp(pathEntry->d_name,".")||!strcmp(pathEntry->d_name,"..")) continue;

            //stworzenie pełnej ścieżki do pliku
            char filePath[100];
            strcpy(filePath, path);
            strcat(filePath,"/");
            strcat(filePath, pathEntry->d_name);

            //rekursywne wejście do zagnieżdżonego folderu
            recursiveRemoveDirectory(filePath);
        }else{
            //stworzenie pełnej ścieżki do pliku
            char filePath[100];
            strcpy(filePath, path);
            strcat(filePath,"/");
            strcat(filePath, pathEntry->d_name);
            
            //usunięcie pliku
            removeFile(filePath);
        }
    }
    removeFile(path);
    return 0;
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
    int sourceFile = open(sourceFilePath, O_RDONLY, (mode_t)0700);

    if (sourceFile == -1)
    {
        printCurrentDateAndTime();
        printf("\ncopySmallFile: Błąd: błąd otwarcia pliku źródłowego %s\n", sourceFilePath);
        syslog(LOG_ERR,"copySmallFile: Błąd: błąd otwarcia pliku źródłowego %s", sourceFilePath);
        return -1;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)0700); // plik do zapisu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile == -1)
    {
        close(sourceFile);
        printCurrentDateAndTime();
        printf("\ncopySmallFile: Błąd: błąd otwarcia pliku docelowego %s\n", destinationPath);
        syslog(LOG_ERR,"copySmallFile: Błąd: błąd otwarcia pliku docelowego %s", destinationPath);
        return -2;
    }

    unsigned char *buffer = malloc(bufferSize);
    
    while (1)
    {
        unsigned int readFile = read(sourceFile, buffer, bufferSize); // czytamy z sourceFile
        if (write(destinationFile, buffer, readFile) == -1) //zapisujemy do destinationFile
        {
            free(buffer);
            close(sourceFile);
            close(destinationFile);
            printCurrentDateAndTime();
            printf("\ncopySmallFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się\n", sourceFilePath, destinationPath);
            syslog(LOG_ERR,"copySmallFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się", sourceFilePath, destinationPath);
            return -3;
        }
        if (readFile < bufferSize) break; //jezeli pobierzemy mniej danych niz mozemy to znaczy, ze to ostatnia porcja i wychodzimy z petli
    }
    printCurrentDateAndTime();
    printf("\ncopySmallFile: skopiowano plik %s\n", sourceFilePath);
    syslog(LOG_INFO,"copySmallFile: skopiowano plik %s", sourceFilePath);

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
      printf("\nremoveFile: Błąd: usuwanie pliku %s nie powiodło się\n", path);
      syslog(LOG_ERR,"removeFile: Błąd: usuwanie pliku %s nie powiodło się", path);
      return -1;
  }
  printCurrentDateAndTime();
  printf("\nremoveFile: usunięto plik %s\n", path);
  syslog(LOG_INFO,"removeFile: usunięto plik %s", path);

  return 0;
}

int copyBigFile(char *sourceFilePath, char *destinationPath)
{
    int sourceFile = open(sourceFilePath, O_RDONLY);

    if (sourceFile == -1)
    {
        printCurrentDateAndTime();
        printf("\ncopyBigFile: Błąd: błąd otwarcia pliku źródłowego %s\n", sourceFilePath);
        syslog(LOG_INFO,"copyBigFile: Błąd: błąd otwarcia pliku źródłowego %s", sourceFilePath);
        return -1;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0700); // plik do odczytu, utworz jezeli nie istnieje,
    // jezeli istnieje to wyczysc, uprawnienia rwx dla wlasciciela

    if (destinationFile == -1)
    {
        close(sourceFile);
        printCurrentDateAndTime();
        printf("\ncopyBigFile: Błąd: błąd otwarcia pliku docelowego %s\n", destinationPath);
        syslog(LOG_ERR,"copyBigFile: Błąd: błąd otwarcia pliku docelowego %s", destinationPath);
        return -2;
    }

    // Pobierz informacje o pliku zrodlowym (bedzie nas interesowal rozmiar .st_size)
    struct stat sourceFileInfo;
    if (fstat(sourceFile, &sourceFileInfo) != 0)
    {
        close(sourceFile);
        close(destinationFile);
        printCurrentDateAndTime();
        printf("\ncopyBigFile: Błąd: nie udało się pobrać rozmiaru pliku źródłowego %s\n", sourceFilePath);
        syslog(LOG_ERR,"copyBigFile: Błąd: nie udało się pobrać rozmiaru pliku źródłowego %s", sourceFilePath);

        return -3;
    }

    //mapowanie pliku wejsciowego do pamieci
    char *sourceFileMap= mmap(0, sourceFileInfo.st_size, PROT_READ, MAP_SHARED | MAP_FILE, sourceFile, 0);

    if (sourceFileMap== MAP_FAILED) 
    {
        close(sourceFile);
        close(destinationFile);
        printCurrentDateAndTime();
        printf("\ncopyBigFile: Błąd: mapowanie pliku %s nie powiodło się\n", sourceFilePath);
        syslog(LOG_ERR,"copyBigFile: Błąd: mapowanie pliku %s nie powiodło się", sourceFilePath);
        return -4;
    }

    //bufor do kopiowania
    unsigned char *buffer = malloc(bufferSize);

    // Numer bajtu w pliku źródłowym. Sluzy do sledzenia przejscia po pliku zmapowanym w pamieci
    //zaczynamy od zera poniewaz kopiujemy od poczatku
    unsigned long long nextByteIndex = 0;

    while(1)
    {
        //jezeli zostala nam porcja danych < bufferSize to wiemy, że nastepne skopiowanie bedzie ostatnim
        if(nextByteIndex + bufferSize >= sourceFileInfo.st_size)
        {
            //przypisujemy do zmiennej ilosc pozostalych bajtow (normalnie kopiujac przypisujemy caly bufferSize,
		//ale w tym przypadku wiemy, ze mamy mniej danych niz bufforSize wiec obliczamy je jako remainingBytes)
            size_t remainingBytes = sourceFileInfo.st_size - nextByteIndex;

            //kopiujemy do bufora zmapowane dane od odpowiedniej pozycji (nextByteIndex) porcje danych o wielkosci remainingBytes
            memcpy(buffer, sourceFileMap + nextByteIndex, remainingBytes);

            //zapisujemy do pliku
            if (write(destinationFile, buffer, remainingBytes) == -1)
            {
                munmap(sourceFileMap, sourceFileInfo.st_size);
                close(sourceFile);
                close(destinationFile);
                printCurrentDateAndTime();
                printf("\ncopyBigFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się\n", sourceFilePath, destinationPath);
                syslog(LOG_ERR,"copyBigFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się", sourceFilePath, destinationPath);
                return -5;
            }

            //wychodzmy z petli kopiujacej
            break;
        }
        
        //kopiujemy do bufora zmapowane dane od odpowiedniej pozycji (nextByteIndex) porcje danych o wielkosci bufferSize
        memcpy(buffer, sourceFileMap + nextByteIndex, bufferSize);

            //zapisujemy do pliku
            if (write(destinationFile, buffer, bufferSize) == -1)
            {
                munmap(sourceFileMap, sourceFileInfo.st_size);
                close(sourceFile);
                close(destinationFile);
                printCurrentDateAndTime();
                printf("\ncopyBigFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się\n", sourceFilePath, destinationPath);
                syslog(LOG_ERR,"copyBigFile: Błąd: zapisanie pliku źródłowego %s do pliku docelowego %s nie powiodło się", sourceFilePath, destinationPath);
                return -5;
            }

        //zwiekszamy indeks nastepnego bajtu o bufforSize, zeby pozniej moc pobrac kolejn porcje
        nextByteIndex += bufferSize;
    }

    printCurrentDateAndTime();
    printf("\ncopyBigFile: skopiowano plik %s\n", sourceFilePath);
    syslog(LOG_INFO,"copyBigFile: skopiowano plik %s", sourceFilePath);

    // Zwolnienie zasobów
    free(buffer);
    munmap(sourceFileMap, sourceFileInfo.st_size);
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
        printf("\nparseParameters: Błąd: nie podano wystarczająco argumentów\n");
        syslog(LOG_ERR,"parseParameters: Błąd: nie podano wystarczająco argumentów");
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
              printf("\nparseParameters: Błąd: nie podano wartości czasu spania programu (-i)\n");    
              syslog(LOG_ERR,"parseParameters: Błąd: nie podano wartości czasu spania programu (-i)");
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
              printf("\nparseParameters: Błąd: nie podano wartości progu (-t)\n");
              syslog(LOG_ERR,"parseParameters: Błąd: nie podano wartości progu (-t)");
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
              printf("\nparseParameters: Błąd: podano za dużo argumentów\n");
              syslog(LOG_ERR,"parseParameters: Błąd: podano za dużo argumentów");
            }
        }
    }
    // Sprawdzamy czy podano obie ścieżki
    if (source == NULL || destination == NULL)
    {
        printCurrentDateAndTime();
        printf("\nparseParameters: Błąd: brak jednej ze ścieżek\n");
        syslog(LOG_ERR,"parseParameters: Błąd: brak jednej ze ścieżek");
        return -5;
    }
    if (isDirectoryValid(source) != 0)
    {
        printCurrentDateAndTime();
        printf("\nparseParameters: Błąd: nieprawidłowy katalog źródłowy %s\n", source);
        syslog(LOG_ERR,"parseParameters: Błąd: nieprawidłowy katalog źródłowy %s", source);
        return -6; // nie jest
    }
    if (isDirectoryValid(destination) != 0)
    {
        printCurrentDateAndTime();
        printf("\nparseParameters: Błąd: nieprawidłowy katalog docelowy %s\n", destination);
        syslog(LOG_ERR,"parseParameters: Błąd: nieprawidłowy katalog docelowy %s", destination);
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
    printf("\nDaemon wybudzony sygnałem SIGUSR1\n");
    syslog(LOG_INFO,"Daemon wybudzony sygnałem SIGUSR1");
    forcedSync = true;
}
