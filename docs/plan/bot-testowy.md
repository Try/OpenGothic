# Bot testowy — automatyczne sprawdzanie gry (plan na później, 2026-10-15)

Cel: zamiast ręcznego przechodzenia całej gry (faza 10) — automat, który chodzi po świecie, przechodzi fabułę i oddaje raport z miejscami do poprawy.
Decyzje użytkownika (2026-10-15): ręczny test jest zbyt czasochłonny; fabułę skryptować na podstawie gotowych solucji „krok po kroku”; „dodaj do planu bota z opisem, jak podejść do tematu, zrobimy to później”.

## Zasady
- Działa na komputerze użytkownika: potrzebuje karty graficznej (Vulkan) i plików Gothic II, których nie wolno wrzucać na GitHuba ani do CI. Uruchamiany na noc (plik .bat obok launchera), wyniki rano w raporcie
- Jeden tryb gry dla wszystkich testów: parametr `-autotest <scenariusz>`; wykonawca kroków w MainWindow::tick (co klatkę jeden krok albo czekanie), bez okien i pytań
- Wyniki w `gra\dane\autotest\<data i godzina>\`: raport.html, zrzuty ekranu (PNG), log kroków, zapisy gry z chwili błędu (do odtworzenia ręcznie)
- Raport HTML: miniatury, najbardziej podejrzane na górze, filtry (świat, rodzaj błędu), porównanie z poprzednim przebiegiem (co się poprawiło, co się zepsuło)

## Część 1 — objazd kamery (zacząć od tego)
- Dla każdego świata (Khorinis, Górnicza Dolina, Jarkendar, wyspa Irdorath): każdy punkt drogi (waynet) → przeniesienie bohatera, 1–2 s czekania (doczytanie, kamera się ustawia)
- Pomiary: ile ekranu jest czarne (cięcie, kamera w ścianie), czy bohater jest widoczny (promień kamera → bohater, prześwit), kamera w skale (DynamicWorld::rayBothSides), FPS, różnica wysokości względem sąsiednich punktów (mapy pionowe)
- Zrzut ekranu każdego punktu, ocena „podejrzane” z pomiarów; w raporcie najpierw najgorsze
- Najwięcej zysku dla fazy 10 (mapy pionowe: Górnicza Dolina, Jarkendar, wieże, jaskinie wielopoziomowe)

## Część 2 — ruch na klik
- Dla par sąsiednich punktów drogi: klik ARPG z jednego do drugiego, limit czasu
- Raport: miejsca, gdzie bohater nie doszedł (zacięcie, pętla, upadek, zła ścieżka)

## Część 3 — test dymny rozdziałów
- Zmiana rozdziału skryptem (B_Kapitelwechsel), przedmioty i zmienne jak u Pomocnika (CH), przywołani wrogowie, minuta walki automatem (klik w najbliższego wroga, atak ładowany, fikołek, czary)
- Sprawdzenie: brak wysypania, błędy w log.txt, czy drzewka i perki działają (napisy, obrażenia)

## Część 4 — bot fabuły
### Źródła kolejności (solucje)
- gry-online.pl — poradnik Gothic II Noc Kruka (po polsku, rozdziały, zadania ponumerowane do 186): https://www.gry-online.pl/poradniki/gothic-ii-noc-kruka/rozdzial-vi-zadania-184-186/z92af0
- colony-guide.com — pełna solucja i lista zadań (po angielsku): https://colony-guide.com/gothic-2-notr/walkthrough/ , https://colony-guide.com/gothic-2-notr/quests/
- zapasowo: StrategyWiki (https://strategywiki.org/wiki/Gothic_II:_Night_of_the_Raven), the-spoiler.com
- Z solucji bierzemy tylko kolejność kroków; tekstu nie kopiujemy do repozytorium

### Scenariusz = dane, nie kod
- Plik scenariusza w repozytorium (np. `autotest/fabula/rozdzial1.txt` albo JSON), jeden krok w linii, nazwy ze skryptów gry:
  - `idz <NPC | punkt drogi>`, `rozmowa <DIA_...>`, `wybor <funkcja wyboru>`, `daj <przedmiot> <NPC>`, `zabij <NPC>`, `uzyj <przedmiot>`, `zabierz <przedmiot>`
  - `czekaj na <wpis w dzienniku | status zadania | zmienna skryptu | godzina gry>`, `zapisz <nazwa>`
- Każdy krok ma sprawdzenie efektu (wpis w dzienniku, zmienna, przedmiot w plecaku) — bez tego krok uznany za niezaliczony
- Walidator w piaskownicy (bez gry, na GOTHIC.DAT, jak skany nauczycieli i map): czy nazwy istnieją (dialogi C_Info, wybory, przedmioty, postacie, tematy dziennika) i czy warunek dialogu (funkcja condition) da się spełnić w tym miejscu scenariusza

### Wykonawca w grze
- Poruszanie: tryb szybki = przeniesienie do postaci / punktu; tryb uczciwy = chodzenie po waynecie naszym ruchem na klik
- Rozmowa: wybór informacji po nazwie instancji przez DialogMenu (to samo, co klik w odpowiedź), wybory Info_AddChoice po funkcji
- Walka: tryb szybki = zabicie komendą; tryb uczciwy = sterowanie ARPG (atak, atak ładowany, mikstury)
- Przerywniki i rozmowy kinowe: czekanie do końca
- Plan dnia postaci: w razie potrzeby przewinięcie czasu do godziny, o której postać jest na miejscu
- Błąd kroku: zrzut, zapis gry, wpis w raporcie; dalej od następnego kroku albo stop (ustawienie scenariusza)

### Kolejność prac
1. Pilotaż: rozdział 1, wątek główny (tryb szybki)
2. Rozdziały 2–6, wątek główny
3. Ścieżki gildii osobno (najemnik / paladyn / mag ognia) — osobne scenariusze od miejsca wyboru gildii
4. Zadania poboczne (Noc Kruka: Jarkendar, piraci, bandyci)
5. Tryb uczciwy dla wybranych odcinków (sterowanie, balans walki)

### Ryzyka
- Losowość: walki, ruchy postaci, plany dnia — kroki krótkie, z czekaniem na stan zamiast na czas
- Zadania z kilkoma rozwiązaniami — w scenariuszu jedno, wybrane świadomie
- Zmiany świata (przejścia do Górniczej Doliny, Jarkendaru, Irdorath) i przerywniki
- Wielkość: sam wątek główny to około 150–250 kroków, wszystkie zadania kilka razy więcej

## Kolejność całości
Objazd kamery → ruch na klik → test dymny rozdziałów → bot fabuły (pilotaż rozdział 1).
