# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST6

**Repository:** https://github.com/Lo-lo78/LSpectrum

Migrazione da JSFX + Lua a VST3 stereo accessibile per REAPER. Mantiene le 32 bande, la curva di riferimento di 4,5 dB/ottava, FFT su thread dedicato, acquisizione con conto alla rovescia e rumore di riferimento. I risultati si leggono in quattro colonne: banda, frequenza, differenza dB L, differenza dB R.

## Modifiche TEST6

- Eliminato completamente il generatore di sinusoide dal DSP, dai controlli, dallo stato del VST e dalla persistenza. Il vecchio file `spectrum_LastLevels.txt` non viene più utilizzato o modificato. Per sicurezza non viene eliminato dal disco.
- **Invio:** attiva/disattiva il *passabanda stereo in solo* relativo alla banda sotto il cursore; il segnale è quello in ingresso, non generato. Il vecchio tasto **S** è libero e non attiva più il solo.
- **C:** porta direttamente il focus su un campo di testo accessibile denominato soltanto **countdown**. Nessuna finestra modale AlertWindow, nessun annuncio vocale sintetico di apertura.
- Campo countdown: digitare da 1 a 600 secondi e premere **Invio** per salvare, oppure usare l'unico pulsante **Save**. Dopo il salvataggio il focus torna alla tabella. Le frecce, quando il focus è nel campo, modificano il cursore del testo e non navigano la tabella né chiudono la finestra.
- **Esc:** chiude la finestra del plugin (come nelle TEST precedenti; con FX dockati il comportamento resta controllato dal contenitore REAPER). Non viene usato come Cancel del countdown.
- **D:** non apre più il countdown.
- Ripulite le istruzioni NVDA e il workflow dell'artefatto TEST6. Il risultato precedente e la lettura L/R restano compatibili.

## Comandi

| Tasto | Funzione |
| --- | --- |
| Frecce, Home, End, Pagina su/giù | Naviga la tabella |
| Spazio | REAPER Play/Stop |
| Ctrl+Spazio | REAPER Play/Pausa |
| R | Nuova scansione dell'ingresso stereo |
| C | Focus diretto al campo `countdown` (1–600 s) |
| Invio (nella tabella) | Accende/spegne il passabanda stereo della banda corrente |
| Invio (nel countdown) | Salva la durata e ritorna alla tabella |
| B | Selezione delle bande |
| Maiusc+Spazio (in B) | Seleziona/deseleziona la banda |
| X / Canc (in B) | Solo banda corrente / tutte le bande |
| N, +, - | Rumore di riferimento e livello |
| 1, 2, 3, 4 | Legge la colonna corrente, doppio tocco copia |
| Ctrl+C | Copia il rapporto completo |
| O / P | Apre il report salvato |
| F1 | Aiuto dei comandi |
| Esc | Esce dall'interfaccia del plugin |

Le bande selezionate, la durata e il rumore sono salvati in `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\settings.xml`; il report in `spectrum_LastResult.txt`. Posizione della tabella, stato dell'istanza e risultati vengono salvati nel progetto REAPER.

## Build

Estrarre i file direttamente nella **radice del clone LSpectrum** senza eliminare `.git`, mantenendo `.github/workflows/build-windows.yml`. GitHub Actions usa `windows-2022`, Visual Studio 2022 e JUCE 8.0.15.

```cmd
git add . && git commit -m "LSpectrum TEST6 remove sine and simplify countdown" && git push
```

**Stato:** sorgenti verificati staticamente. Necessaria la compilazione su GitHub Actions e prova effettiva con REAPER/NVDA. La futura rotazione di fase non fa parte della TEST6.
