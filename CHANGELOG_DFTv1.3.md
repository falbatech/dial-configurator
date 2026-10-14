# Lista zmian DFTv1.3

Wersja w `config.h`: `DFTv1.3`. Zawiera wszystko z DFTv1.2. Cel: wlasne profile na poziomie Stream Deck+ / Loupedeck.

## Wlasne profile
- **12 profili** (bylo 6), na dwoch pulpitach CUSTOM i CUSTOM 2.
- **Makra**: kazda akcja to do 6 krokow wykonywanych po kolei. Kroki: skrot klawiszowy, klawisz multimedialny,
  kolko pionowo / poziomo, pauza (ms), wpisz tekst (2 teksty na profil, do 32 znakow ASCII), nastepny / poprzedni tryb.
- **4 akcje w kazdym trybie**: obrot w lewo, obrot w prawo, tapniecie, klik pokretla.
  Pusty klik = nastepny tryb (jak dotad); przytrzymanie zawsze wraca na HOME.
- Migracja: profile z DFTv1.2 przenosza sie same (tap profilu trafia do kazdego trybu).

## Konfigurator
- Edytor makr: lista krokow z "+ krok" dla kazdej z 4 akcji.
- **Szablony** ("Zacznij od szablonu"): Browser, DaVinci edit, Premiere, Photoshop, Lightroom, Figma, VS Code, Signature
  (wpisywanie podpisu) - do skopiowania i edycji, np. jako edytowalna wersja wbudowanego profilu.
- **Eksport / import profili** do pliku JSON (udostepnianie, kopia zapasowa).
- Zapis profili po jednym (makra sa dlugie). Zgodnosc: z firmware DFTv1.2 dziala jak dotad (jeden krok, bez kliku),
  z komunikatem o aktualizacji.

## Protokol
- `CFG GET`: `cfgv: 2`, `maxSteps`, akcje jako listy krokow `[[t,m,c],...]`, `texts`.
- `CFG SET {"slot": i, "profile": {...}}` zapisuje jeden profil; stary format (tablica `custom`) nadal dziala.

## Do przetestowania na urzadzeniu
1. Aktualizacja z DFTv1.2: wlasne profile i ustawienia zostaja.
2. Makro z pauza i wpisywaniem tekstu (szablon Signature) w edytorze tekstu.
3. Klik z makrem oraz krok "nastepny / poprzedni tryb".
4. Szablony, eksport i import pliku.
5. 7+ profili: pulpit CUSTOM 2.
