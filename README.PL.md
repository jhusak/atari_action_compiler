## Atari Action! Compiler

Atari Action Compiler to kompilator Atari Action! uruchamiany w piaskownicy (sandboxie) zbudowanej na bazie fake6502 (emulatora procesora MOS 6502 napisanego w języku C), również napisanego w C.

Emuluje oryginalny, działający w sandboxie kartridż Action! w środowisku „gołego” Atari 8-bit. Emulowany jest wyłącznie procesor CPU — brak obsługi NMI — jednak przy każdej wirtualnej ramce (oryginalnie 1/50 sekundy) zwiększany jest zegar systemowy. Jedna ramka trwa około 30 000 instrukcji (ponieważ cykle procesora nie są zliczane). Wykorzystywany jest ROM XL (przepraszam za to, być może lepszym wyborem byłby ROM Altirra).

Kartridż nie został w żaden sposób zmodyfikowany — dodano jedynie kilka przechwytów (hooków), na przykład rozpoznawanie pętli bezczynności (idle loop), wstawianie nazw plików do buforów nazw plików oraz poke(764,12) (wciśnięcie return)  :). Po zakończeniu kompilacji pobierany jest kod błędu i wyświetlana jest ostatnia kompilowana linia, tak jak w oryginalnym kartridżu.

Działa to w następujący sposób: ustawiana jest flaga trybu Monitor, uruchamiany jest kartridż, następnie wywoływana jest komenda C"nazwa_pliku", po czym program czeka na wejście w pętlę IDLE. Następnie wywoływana jest komenda W"nazwa_pliku", wyświetlane są komunikaty błędów (jeśli wystąpiły), a program kończy działanie.

Zgodność nie jest stuprocentowa. Użytkownik może używać instrukcji SET, która zapisuje dane do pamięci, a istnieje kilka zastosowań tego mechanizmu. Program może nie działać poprawnie, jeśli zapis następuje do rejestrów sprzętowych (które nie są zaimplementowane) lub w przypadku innych nietypowych zastosowań. Jednak jeśli korzystasz z tych instrukcji zgodnie z ich przeznaczeniem, nie powinieneś napotkać problemów.

Nie występuje narzut związany z systemem operacyjnym dysku (DOS), dzięki czemu można wykorzystać całą dostępną pamięć — od rozsądnego początku obszaru pamięci aż do adresu 0x93FF.

Kartridż Action! zachowuje pełnię swoich możliwości. Powinien działać dokładnie tak jak oryginał, łącznie ze wszystkimi dołączonymi bibliotekami w plikach *.ACT (należy pamiętać o rozróżnianiu wielkości liter w nazwach plików). Również adresy procedur Action! są identyczne z tymi z prawdziwego kartridża.
