# MiniFAT

**MiniFAT** to narzędzie umożliwiające odczyt danych z obrazu systemu plików FAT12. Projekt został stworzony w celach edukacyjnych i demonstruje podstawy działania systemów plików oraz manipulacji binarnymi obrazami dysków.


## Funkcje

- Odczyt sektora rozruchowego (boot sector)
- Interpretacja struktury katalogu głównego (root directory)
- Przeglądanie i odczyt plików z obrazu FAT12
- Obsługa tablicy alokacji plików (FAT)


# Struktura

- `disk_open_from_file`, `disk_read`, `disk_close` – niskopoziomowy dostęp do danych z pliku-dysku
- `fat_open`, `fat_close` – interpretacja struktury systemu plików FAT12
- `file_open`, `file_read`, `file_seek`, `file_close` – operacje na plikach
- `dir_open`, `dir_read`, `dir_close` – operacje na katalogu głównym
- `get_chain_fat12` – interpretacja tablicy FAT12 i tworzenie łańcucha klastrów
