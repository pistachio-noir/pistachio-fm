# TODO — testy ręczne Pistachio FM

Przygotowane 12 listopada 2026. Zaznaczaj `[x]` po sprawdzeniu na swoim pulpicie.
Wszystkie poniższe testy ręczne pozostają nieodhaczone, także te pokryte automatami.

## Start

Gotowy zestaw plików testowych: `/tmp/pfm-manual-017pmwrv`.
Zawiera różne pliki `raport.txt` w katalogu głównym, A, B i ukrytym podfolderze,
folder `wynik/wynik.txt`, pusty `cel`, archiwum ZIP, pętlę dowiązania i uszkodzone dowiązanie.
To pliki robocze do kopiowania, zmiany nazw i usuwania. `/tmp` może zostać wyczyszczony po restarcie.

Uruchom z katalogu projektu:

```sh
build/pistachio-fm /tmp/pfm-manual-017pmwrv
```

Nie trzeba instalować aplikacji. Testy operacji wykonuj na tym zestawie; usunięte pliki
trafiają do zwykłego kosza użytkownika. Przy teście opróżniania kosza sprawdź również jego pozostałą zawartość.

## Nowe: ustawienia domyślnych aplikacji

- [ ] Przycisk ustawień (suwaki) oraz Ctrl+, otwierają panel; Escape/Close zamykają go i przywracają obsługę plików.
- [ ] Popularne typy są widoczne od razu; wyszukaj PDF, `image/png` i własny pełny MIME. Enter wybiera pierwszy wynik.
- [ ] Dla wybranego typu bieżący program zgadza się z `xdg-mime query default application/pdf` (dla PDF).
- [ ] Zaznacz inną aplikację: ustawienie pozostaje bez zmian, dopóki nie klikniesz Apply. Aplikacja nie jest uruchamiana.
- [ ] Zanotuj poprzedni program, użyj Apply i sprawdź odczyt nowego defaultu; przywróć poprzedni po teście. Zmiana dotyczy całego pulpitu.
- [ ] Show all apps pozwala wybrać inny zainstalowany program; klawiatura, długie nazwy i brak defaultu są obsłużone czytelnie.
- [ ] Szybka zmiana typów nie przywraca starych wyników; zamknięcie panelu podczas zapisu nie przerywa Apply.

## Nowe: podgląd pod Spacją

- [ ] Zaznacz `notatki.md` i naciśnij Spację: otwiera się podgląd treści bez uruchamiania edytora.
- [ ] Spacja ponownie, Escape, Close i kliknięcie poza oknem zamykają podgląd; strzałki znów sterują plikami.
- [ ] Sprawdź polskie znaki, pusty plik, długi tekst i tekst zawierający HTML: HTML pozostaje zwykłym tekstem, a przy przekroczeniu 128 KiB jest informacja o skróceniu.
- [ ] Zaznacz obraz PNG/JPEG (także ze spacją, # lub % w nazwie): widać cały obraz, bez okrągłego przycięcia miniatury.
- [ ] Przewijaj tekst i zmieniaj rozmiar okna; zaznacz i skopiuj fragment. Plik nie jest modyfikowany.
- [ ] Spacja w wyszukiwaniu i pasku ścieżki nadal wpisuje znak; nie otwiera podglądu.
- [ ] Bez zaznaczenia, z kilkoma zaznaczonymi elementami lub z folderem: Spacja nie wybiera przypadkowego pliku.
- [ ] Wyniki wyszukiwania i podzielona karta: otwiera się właściwy plik z aktywnego panelu.
- [ ] PDF: Spacja pokazuje pierwszą stronę i licznik; przyciski poprzedniej/następnej oraz strzałki lewo/prawo zmieniają strony.
- [ ] PNG/JPEG/PDF: przyciski −/+, klawisze +/− i Ctrl+kółko powiększają od dopasowania do 4×. Przeciągnięcie lub przewijanie przesuwa powiększoną zawartość.
- [ ] Fit albo 0 przywraca dopasowanie i zeruje przesunięcie; nowy plik zaczyna dopasowany, zmiana strony PDF zachowuje zoom.
- [ ] Na pierwszej i ostatniej stronie odpowiedni przycisk jest nieaktywny; obrazy stron nie są ucięte. Sprawdź też zeskanowany lub obrócony PDF.
- [ ] Zamknij PDF w trakcie ładowania: okno znika i nie wraca stara strona; pliki tymczasowe z podglądu są usuwane.
- [ ] Uszkodzony PDF i PDF na hasło pokazują komunikat; oryginalny plik pozostaje bez zmian.
- [ ] Multimedia i dokumenty Office nadal pokazują komunikat o braku podglądu.

## 1. Nowe wyszukiwanie — zacznij tutaj

- [+] Ctrl+F, wpisz `raport`: domyślny filtr pokazuje tylko `raport.txt` z katalogu głównego.
- [+] Kliknij lupę: pojawiają się również `A/raport.txt`, `B/raport.txt` i `A/raport-link.txt`.
- [+] Sprawdź te wyniki w siatce (Ctrl+1) i liście (Ctrl+2): ścieżki pozwalają rozróżnić pliki.
- [+] Otwórz `A/raport.txt`: treść mówi o podfolderze A, a nie o katalogu głównym.
- [+] Ctrl+H: pojawia się `.ukryty/raport.txt`; ponowne Ctrl+H ukrywa ten wynik.
- [+] Dowiązanie `A/petla` nie powoduje nieskończonego skanowania ani wyników z `petla/petla/...`.
- [+] Kliknij lupę ponownie: wraca filtr bieżącego katalogu; tekst zapytania zostaje.
- [+] Escape oraz przycisk × czyszczą zapytanie i przywracają zawartość katalogu głównego.
- [?] Tab do przycisku lupy, następnie Spacja lub Enter: zmienia zakres wyszukiwania.
- [+] Otwórz inny folder: zapytanie znika i wraca domyślny zakres lokalny.
- [+] Wyszukuj w dużym drzewie katalogów; zmieniaj zapytanie, wyczyść je lub przejdź gdzie indziej podczas skanowania: stare wyniki nie wracają, okno pozostaje używalne.
- [+] Podczas wyszukiwania utwórz nowe trafienie w podfolderze bez wcześniejszych wyników: po F5 zostaje znalezione.

Znane granice: wyniki pojawiają się po zakończeniu skanowania; nieczytelne podfoldery są pomijane;
nowe trafienia poza katalogami zawierającymi wcześniejsze wyniki mogą wymagać F5.
To wyszukiwanie nazw, bez przeszukiwania treści. W koszu zakres rekurencyjny jest wyłączony.

## 2. Operacje na wynikach z podfolderów

Po każdym teście przywróć pliki przez Ctrl+Z albo odtwórz je przed następnym.

- [ ] W wynikach `raport` zaznacz `A/raport.txt`, F2: zmień nazwę na `notatki.md`. Plik główny `notatki.md` nie blokuje zmiany, bo to inny katalog. Wynik ze starą nazwą znika.
- [ ] Cofnij zmianę nazwy przez Ctrl+Z: właściwy plik wraca do A i do wyników wyszukiwania.
- [ ] Zaznacz `A/raport.txt`, Compress: ZIP powstaje w katalogu przeszukiwania. Rozpakuj go: zawiera `A/raport.txt` z treścią „Plik z podfolderu A”, a nie plik główny.
- [ ] Skopiuj wynik z A do `cel`: kopiuje się właściwy plik i treść; oryginał zostaje.
- [ ] Wytnij wynik z B i wklej do `cel`: przenosi się właściwy plik, a wynik ze starej lokalizacji znika. Ctrl+Z przywraca go.
- [ ] Wyszukaj `wynik`, zaznacz folder `wynik` i jego plik `wynik/wynik.txt`, skopiuj do `cel`: dziecko znajduje się tylko wewnątrz folderu; nie powstaje dodatkowy plik obok niego.
- [ ] Dla takiego samego zaznaczenia sprawdź wycinanie, kosz i cofanie: bez błędu o brakującym dziecku.
- [ ] Dla roboczej kopii takiego zaznaczenia sprawdź Shift+Delete: usuwa ją po potwierdzeniu, bez podwójnej operacji.

## 3. Nawigacja i widoki

- [ ] Sidebar, okruszki ścieżki, Ctrl+L, Backspace, Alt+Left/Right oraz Alt+Up prowadzą do właściwych katalogów.
- [ ] Ctrl+L przyjmuje ścieżkę, `~` i ścieżkę względną; literówka pokazuje komunikat.
- [ ] Kliknięcie, Ctrl+klik, Shift+klik, Ctrl+A, strzałki i zaznaczanie prostokątem działają w siatce i liście.
- [ ] Sortowanie po nazwie, dacie, rozmiarze i typie działa; foldery pozostają pierwsze.
- [ ] Zmiana rozmiaru okna nie ucina przycisków; długie nazwy mają wielokropek.
- [ ] Ctrl+T, przełączanie kart, środkowy klik na folderze i Ctrl+W działają; ostatnia karta zamyka okno.
- [ ] F3 dzieli kartę: pasek ścieżki i akcje dotyczą aktywnego panelu; ponowne F3 zachowuje aktywny panel.
- [ ] Menu karty i przeciągnięcie karty na bok zawartości działają; każdy panel zachowuje swój filtr.

## 4. Pliki, schowek i archiwa

- [ ] Ctrl+Shift+N tworzy folder; Ctrl+Alt+N tworzy pusty plik; zajęta nazwa jest odrzucana.
- [ ] F2 zaznacza nazwę bez rozszerzenia; Enter zatwierdza, Escape anuluje.
- [ ] Ctrl+C/X/V działa między panelami; wycięte pliki są przygaszone; kolizja nazwy zachowuje oba pliki.
- [ ] Kopiowanie i wycinanie działają w obie strony z Nautilusem lub Dolphinem.
- [ ] Kopiowanie tekstu w polu wyszukiwania i ścieżki nie kopiuje zaznaczonych plików.
- [ ] Przeciąganie na folder i miejsce w sidebarze działa; Ctrl wymusza kopiowanie, Shift przenoszenie.
- [ ] Duże kopiowanie pokazuje postęp; Stop przerywa operację i usuwa niedokończony element, zachowując ukończone.
- [ ] ZIP: Compress, Extract here i Ctrl+Z działają; drugie rozpakowanie tworzy osobny folder.
- [ ] Właściwości (Alt+Enter) pokazują właściwą ścieżkę, rozmiar i uprawnienia; rozmiar folderu zostaje policzony.
- [ ] Open with uruchamia wskazaną aplikację; „Always use” zmienia domyślną aplikację danego typu w całym pulpicie.
- [ ] Open in terminal otwiera terminal w bieżącym katalogu; Copy path wkleja poprawną ścieżkę.

## 5. Kosz, zakładki i wygląd

- [ ] Delete pyta o przeniesienie do kosza; Cancel zachowuje plik; Restore i Ctrl+Z przywracają go.
- [ ] Shift+Delete pyta o trwałe usunięcie; domyślnym wyborem jest Cancel.
- [ ] W koszu właściwości pokazują miejsce pochodzenia i datę usunięcia; rekurencyjna lupa jest niedostępna.
- [ ] Empty trash pyta o potwierdzenie; sprawdź Cancel bez opróżniania kosza.
- [ ] Ctrl+D dodaje zakładkę, sidebar otwiera ją, a menu usuwa; zmiana jest widoczna w Nautilusie.
- [ ] Obrazy mają miniatury, menu mieści się przy krawędziach, F1 pokazuje skróty, Escape zamyka okna dialogowe.

## 6. Opcjonalnie: prawdziwy pendrive

- [ ] Podłączony nośnik pojawia się w Devices; kliknięcie montuje go i otwiera.
- [ ] Przetestuj kopiowanie wyłącznie roboczych plików, postęp i zatrzymanie.
- [ ] Eject odmontowuje nośnik; aplikacja opuszcza jego katalog, jeśli był otwarty.

## Weryfikacja automatyczna tej sesji

- [x] `cmake --build build` — zakończony kodem 0.
- [x] Backend — 72 zaliczone, 0 błędów, 0 pominiętych:
  `LC_ALL=en_US.UTF-8 QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/tst_backend`.
- [x] UI i zrzuty — wszystkie sprawdzenia zaliczone:
  `QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 build/pfm-shots /tmp/pfm-settings-shots`.
- [x] `cmake --build build --target all_qmllint` — bez ostrzeżeń.
- [x] `git diff --check` — bez błędów.

Logi: `/tmp/pfm-settings-backend.log`, `/tmp/pfm-settings-shots.log`.
Backend wymaga tutaj `en_US.UTF-8`, bo istniejący test sortowania liczbowego pada z `C.UTF-8`.
Runner UI wypisuje ostrzeżenia o niepoprawnych obrazach testowych oraz `DelegateModel::cancel`;
kończy się kodem 0. Te wyniki nie zastępują ręcznych testów Waylanda, prawdziwych aplikacji i urządzeń.

## Znalezione problemy

Dla każdego problemu dopisz: kroki, oczekiwany wynik, rzeczywisty wynik i ewentualny zrzut ekranu.

- [ ] …

Pełna wcześniejsza lista testów: [TESTING.md](TESTING.md).
