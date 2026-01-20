# SlimmeLezer telepítés – ESPHome

[![ESPHome Compile](https://github.com/amargo/slimmelezer-eon/actions/workflows/esphome.yml/badge.svg)](https://github.com/amargo/slimmelezer-eon/actions/workflows/esphome.yml)

## Frissítések

### ESPHome 2025.5.x kompatibilitási frissítés
A komponens frissítve lett, hogy kompatibilis legyen az ESPHome 2025.5.x és újabb verzióival. A frissítés megoldja a `text_sensor.TEXT_SENSOR_SCHEMA` elavult séma használatával kapcsolatos figyelmeztetést, amely a 2025.11.0 verzióban teljesen el lesz távolítva. A komponens most már a `text_sensor.text_sensor_schema()` függvényt használja, miközben megőrzi a visszafelé kompatibilitást a korábbi ESPHome verziókkal is.

### Memory Management Javítások
A DSMR komponens átállítva lett modern C++ memory management használatára `std::unique_ptr` smart pointerekkel. Ez a javítás számos előnnyel jár:

**Előnyök:**
- **Automatikus memória felszabadítás**: A smart pointerek automatikusan felszabadítják a memóriát, amikor már nincs rá szükség
- **Memory leak védelem**: Elkerüli a memória szivárgást, ami különösen fontos az ESP8266/ESP32 korlátozott memóriájával
- **Exception safety**: Ha kivétel keletkezik, a memória automatikusan felszabadul
- **Compiler optimalizáció**: Modern C++ standardok szerinti kód, amely jobban optimalizálható
- **Biztonságosabb kód**: Csökkenti a null pointer hibák és buffer overflow lehetőségét

**Technikai változások:**
- `char*` és `uint8_t*` nyers pointerek helyett `std::unique_ptr<char[]>` és `std::unique_ptr<uint8_t[]>` használata
- Automatikus buffer inicializálás `std::make_unique` segítségével
- Proper buffer elérés `.get()` metódussal
- `delete[]` helyett automatikus destruktor hívás


### Készült Slimmelezer.E.ON - [4D4M](https://prohardver.hu/tag/4d4m.html)

1. ESPHome platform szerinti telepítés esetén a [mindig aktuális útmutató](https://esphome.io/guides/getting_started_hassio) alapján létre kell hozni a slimmelezer konfiguráció fogadására alkalmas "üres" eszközt.

2. Az így kapott yaml fájlunk tartalmát az igényeinknek megfelelő, itt felsorolt valamelyik yaml fájl szerint ki kell egészíteni:
   
#### slimmelezer_alap.yaml:
  Alapvető információkat továbbít a magyar mérők jelenlegi (2023.10.03) beállítása szerint, hálózatra termelő napelemes
  rendszer nélküli mérőkhöz ajánlott, ahol nincs hálózatba betáplálás és nem fontosak a meddő teljesítmények és energiák.
   
#### slimmelezer_napelemes_2023.yaml:
  2024 előtt telepített (felprogramozott) mérők által küldött adatok feldolgozására alkalmas alapbeállítás. Tartalmazza az 
  összes napelemes rendszer mellett releváns mérési adatot, a meddő teljesítmények és energiákat is beleértve.
  Ezen kívül tartalmaz egy sciptet is, ami egy egyszerű pillanatnyi egyenleget von a betáplált és vételezett teljesítményre
  vonatkozóan.
  
#### slimmelezer_napelemes_2024.yaml:
  2024 után telepített (felprogramozott) mérők által küldött adatok feldolgozására alkalmas alapbeállítás. Tartalmazza az 
  összes napelemes rendszer mellett releváns mérési adatot, a meddő teljesítmények és energiákat is beleértve.
  Ezen kívül tartalmaz egy sciptet is, ami egy egyszerű pillanatnyi egyenleget von a betáplált és vételezett teljesítményre
  vonatkozóan.

  <ins>Mérők az újabb konfigurációval:</ins>  
   • Wasion aMeter100  
   • Wasion aMeter300  
   • Sagemcom MA110M  
   • Sagemcom MA309M  
   • Sanxing SX601 S12U26  
   • Sanxing SX631 S34U28  
   • Holley DDSD285 (2024-től)  
   • Holley DTSD545 (2024-től)  
  
  Azon mérési adatokat tartalmazó sorok elé, amiket nem szeretnénk, hogy továbbítson a slimmelezer a HomeAssistant felé,
  célszerűen egy # jelet téve inaktiválhatjuk a fordító számára, így azok nem fognak megjelenni a HomeAssistantban entitásként.
  Értelemszerűen, ha valamit szeretnénk még megjeleníteni a meglévő példa yaml fájlokhoz képest, akkor azoknál ki kell
  törölni a # jelet a sorok elől, ügyelve a szóközökkel való megfelelő pozícionálás megőrzésére.
  
  Magyarországi mérők sajátosságai miatt a következő paraméterek megadása kötelező a konfigurációs fájlban, különben nem lesz
  képes feldolgozni és továbbítani a mérők által küldött telegram üzenetet: (alapból tartalmazza mindegyik példa yaml fájl)
   
```yaml
dsmr:
  id: dsmr_instance
  crc_check: false
  max_telegram_length: 3000
```

## Új szenzorok (nettó értékek)

Az alábbi YAML-okban elérhetőek az új, nettó értéket mutató szenzorok:

- `slimmelezer_napelemes.yaml`
- `slimmelezer_eth_napelemes.yaml`

### Nettó pillanatnyi teljesítmény fázisonként

Három új szenzor jelenik meg, amelyek a fogyasztást pozitív, a betáplálást negatív előjellel mutatják:

- `Pillanatnyi teljesítmény L1 - nettó` (`power_net_l1`)
- `Pillanatnyi teljesítmény L2 - nettó` (`power_net_l2`)
- `Pillanatnyi teljesítmény L3 - nettó` (`power_net_l3`)

Ezek a szenzorok a `power_delivered_l*` és `power_returned_l*` szenzorok különbségeként kerülnek kiszámításra, ezért azoknak
ID-t adtunk a YAML-okban. Egység: `kW`, pontosság: 3 tizedes.

### Nettó meddő energia

- `Meddő energia - nettó` (`reactive_energy_net`)

Ez a szenzor a `Meddő energia - vételezett (+R)` és a `Meddő energia - betáplált (-R)` különbsége (`kvarh`).
Pozitív érték: nettó meddő energia fogyasztás. Negatív érték: nettó meddő energia betáplálás.

Megjegyzés: a `fields.h`-ban javítva lett a reaktív energia integer egysége (4.8.0 → `varh`), így a `kvarh/varh` párosítás
következetes és helyes.

## Per-fázis teljesítmény adatok – visszafelé kompatibilitás és E.ON kérés

Az új YAML-ok a fázisonkénti nettó teljesítményt elsődlegesen a mérő által küldött per‑fázis hatásos teljesítményből számolják:

- `power_delivered_l1..l3` és `power_returned_l1..l3` → nettó = vételezett − betáplált (kW)

Ha a mérő nem küld ilyen per‑fázis teljesítmény adatot (sok mérőn alapból tiltva van), a számítások visszafelé kompatibilisen működnek:

- Fallback képlet: P_net ≈ U × I × PF / 1000
  - feszültség: `voltage_l1..l3`
  - áram: `current_l1..l3`
  - teljesítménytényező: `instantaneous_power_factor_l1..l3` (ha nem érhető el, PF=1.0 feltételezés)
- A nettó áram (A) elsődlegesen P_net és U alapján kerül számításra (I = P_net×1000/U), ellenkező esetben a mérő által szolgáltatott `current_l*` kerül publikálásra.

### Per‑fázis teljesítmény bekapcsoltatása az E.ON-nál

Több mérőnél a per‑fázis hatásos teljesítmény értékek (`power_delivered_l*`, `power_returned_l*`) csak külön kérésre válnak elérhetővé. Ezek bekapcsoltatását az E.ON-nál lehet kérni e‑mailben. Ajánlott cím és sablon:

- Címzett: aramhalozat@eon.hu

Sablon:

```
Kedves Ügyfélszolgálat!
Szeretnék kérni szoftverfrissítést a villanyórámhoz.

E.ON felhasználó azonosító:
MVM felhasználó azonosító:
Mérő gyártási száma:
Felhasználási hely:

Köszönöm szépen!
```

Megjegyzés: a kérelemhez indoklás is szükséges (pl. napelemes rendszer fázisonkénti egyensúlyozása, terhelésmenedzsment, fázisonkénti visszatáplálás monitorozása stb.).

## Toleráns parszolás (lenient mód)

A magyar mérők (Holley DTSD545/DDSD285, Wasion aMeter100/aMeter100/aMeter300, Sagemcom MA105/MA110M egyfázisú, MA304/MA309M háromfázisú, Sanxing SX601 P12S01/S12U16, SX631 P34S02/S34U18) néha eltérő formátumban küldik az adatokat a standard DSMR-hez képest. A `lenient: true` opció bekapcsolásával a parszer nem áll meg egy-egy hibás sor miatt, hanem kihagyja azt és folytatja a többi mező feldolgozását.

### Kezelt eltérések

| Probléma | Leírás | Megoldás |
|----------|--------|----------|
| **Előjeles számok** | Reaktív teljesítmény negatív előjellel jöhet (pl. `-0.123*kvar`) | A parszer elfogadja a `-` jelet |
| **Breaker status (96.3.10)** | Egyes mérők `(1)` / `(0)`-t küldenek `ON`/`OFF` helyett | 1-3 karakter hossz elfogadva |
| **Unit kapitalizáció** | Egyes mérők `HZ`-t küldenek `Hz` helyett | Case-insensitive összehasonlítás |
| **Ismeretlen mezők** | Mérő-specifikus OBIS kódok | Lenient módban kihagyva |

### Használat

```yaml
dsmr:
  id: dsmr_instance
  crc_check: false
  max_telegram_length: 3000
  lenient: true  # Toleráns parszolás bekapcsolása
```

**Megjegyzés**: A `lenient: true` ajánlott, ha parse hibákat tapasztalsz a logban.

## DSMR debug/telegram naplózás

A nyers DSMR telegram naplózása bekerült VV szinten. Engedélyezés a YAML-ban:

```yaml
logger:
  baud_rate: 0
  level: INFO
  logs:
    dsmr: VERY_VERBOSE
```

Titkosított P1 esetén a visszafejtett telegram is megjelenik VV szinten. Ha túl zajos, állítsd `VERBOSE`-ra.

#### megjegyzés: 
Az itt elérhető csomag tartalmazza még slimmelezerre direktben(OTA, USB) feltölthető firmware-ek lefordított binárisait is, viszont ezek használata nem javasolt, mert frissítések alkalmával elveszhet a meglévő konfiguráció:
[Slimmelezer.E.ON-4D4M.rar](https://drive.google.com/file/d/10OlD0Aoxti3LFLMXj2cScBJ9jfaaUWNJ/view)
