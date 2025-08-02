Jednoduchý bezdrátový senzor s ATTINY84
=======================================

Tento projekt představuje nízkopříkonový bezdrátový senzor založený na mikrokontroléru ATTINY84, navržený pro snadné bateriové napájení,
měření teploty, vlhkosti, stavů vstupů a bezdrátový přenos dat přes 433 MHz. Kromě toho podporuje bezdrátové nahrávání firmware přes IR bootloader.

Funkce
------
 - Měření teploty a vlhkosti (DHT22 / AM2302)
 - Detekce pohybu (PIR čidlo) a dalších digitálních vstupů
 - Měření napětí baterie
 - Odesílání dat do centrály přes 433 MHz pomocí šifrovaného protokolu
 - Bezdrátová OTA aktualizace přes IR (infrared) bootloader

Co budete potřebovat
---

 * ATTINY84
 * Zdroj 3–5 V (např. Li-Ion 18650)
 * PIR senzor (např. AM312), RCWL-0516
 * DHT22 (AM2302)
 * 433 MHz vysílač (doporučeno: FS1000A)
 * IR přijímač (např. IRM-3638T)
 * IR LED pro nahrávání (např. přes Android telefon s IR vysílačem)
 * Programátor pro jednorazove nahrání bootloaderu (např. Arduino Nano, ESP32)

                          .------------------.
                          |     ATTINY84     |
                          |                  |
                      ----| 1  VCC    GND 14 |----
              FS1000A ----| 2  PB0    PA0 13 |----
                      ----| 3  PB1    PA1 12 |---- PIR
                RESET ----| 4  PB3    PA2 11 |---- RCWL
            IRM-3638T ----| 5  PB2    PA3 10 |----
                      ----| 6  PA7    PA4  9 |----
                      ----| 7  PA6    PA5  8 |---- AM2302
                          '------------------'


Bezdrátová aktualizace firmware (OTA)
---

 - Vlastní IR bootloader (do 512 B, psaný v assembleru)
 - Android aplikaci pro odeslání firmware ve formátu Intel HEX přes IR
 - Podporu přenosu i přes 433 MHz (s volitelným šifrováním pomocí SPECK)

Šifrování přenosu
---
Komunikace přes 433 MHz je šifrována algoritmem SPECK (bloková šifra). Šifra je optimalizována pro AVR – pouze několik stovek bajtů.
