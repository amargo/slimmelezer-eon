# SlimmeLezer telepítés – ESPHome

### Készült Slimmelezer.E.ON - [4D4M](https://prohardver.hu/tag/4d4m.html)

https://drive.google.com/file/d/10OlD0Aoxti3LFLMXj2cScBJ9jfaaUWNJ/view

1. A "components" mappát és teljes tartalmát át kell másolni a HomeAssistant szerver "ESPHome" mappájába
   A célhelyen érvényes elérési útja a következő lesz: /config/esphome/components

2. Ki kell választani a nekünk megfelelő yaml konfigurációt vagy egyénileg szerkeszteni bármelyik mellékelt yaml fájlt
   
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
  
  Azon mérési adatokat tartalmazó sorok elé, amiket nem szeretnénk, hogy továbbítson a slimmelezer a HomeAssistant felé,
  célszerűen egy # jelet téve inaktiválhatjuk a fordító számára, így azok nem fognak megjelenni a HomeAssistantban entitásként.
  Értelemszerűen, ha valamit szeretnénk még megjeleníteni a meglévő példa yaml fájlokhoz képest, akkor azoknál ki kell
  törölni a # jelet a sorok elől, ügyelve a szóközökkel való megfelelő pozíciónálás megőrzésére.
  
  Magyarországi mérők sajátosságai miatt a következő paraméterek megadása kötelező a konfigurációs fájlban, különben nem lesz
  képes feldolgozni és továbbítani a mérők által küldött telegram üzenetet: (alapból tartalmazza mindegyik példa yaml fájl)
   
```yaml
dsmr:
  id: dsmr_instance
  crc_check: false
  max_telegram_length: 3000
```

#### megjegyzés: 
A csomag tartalmazza még slimmelezerre direktben(OTA, USB) feltölthető firmware-ek lefordított binárisait is, viszont ezek használata nem javasolt, mert frissítések alkalmával elveszhet a meglévő konfiguráció.
