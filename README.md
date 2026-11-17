# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST3 (migrazione / test)

Repository: https://github.com/Lo-lo78/LSpectrum

Questa versione continua la migrazione dei file **LSpectrum 4.5dB Analyzer.jsfx** e **LSpectrum 4.5dB Analyzer.lua** a un VST3 stereo indipendente da REAPER Lua.

## Correzioni centrali rispetto alla TEST1

- **R avvia una nuova scansione reale del segnale in ingresso L/R**. Un thread FFT riceve gli effettivi campioni stereo passati dal callback audio. L'acquisizione e il riarmo del FIFO avvengono mediante handshake al confine del blocco audio per evitare l'azzeramento dei puntatori in conflitto con il callback.
- Conta 10 secondi **di audio realmente processato**, modificabili con D (1..600). Se REAPER non invia campioni, non inventa una misurazione: la finestra indica **Waiting for incoming audio from REAPER**. Avviare il transport in REAPER o controllare il routing. Il VST3 non avvia né ferma la DAW.
- Alla fine **si drenano tutti i campioni fino alla fine della scansione** e si calcolano tutte le finestre FFT, comprese eventuali code. In caso di sovraccarico del FIFO viene segnalato un errore anziché una misura incompleta.
- La finestra mostra 4 colonne: Banda / Frequenza / differenza dB sinistro / differenza dB destro. I valori sono differenze rispetto a 4,5 dB per ottava, pivot circa 1 kHz. NON dBFS assoluti.
- Esc tenta di **chiudere la finestra FX flottante di REAPER**, anche mentre analisi e selezione sono attive; non annulla la scansione né disinserisce il plugin. Se la finestra FX è integrata/dockata, la chiusura dipende dal gestore finestre del host: il VST3 rimanda Esc al contenitore senza inviare pericolosi WM_CLOSE alla finestra principale di REAPER. Tasto e pulsante Close (Esc).
- Restano la selezione di 1..32 bande, l'ascolto passabanda stereo (S), la sinusoide (Invio), il noise stereo (N, + e -), la copia del rapporto e il salvataggio nello stato del progetto.
- Include entrambe le correzioni necessarie alla TEST1: `juce_generate_juce_header(LSpectrum)` in CMake e `juce::StringArray::fromLines` nel parser. GitHub usa `windows-2022` e Visual Studio 2022.

## Comandi con NVDA

| Tasto | Funzione |
|---|---|
| R | Nuova analisi stereo, per la durata memorizzata |
| D | Imposta la durata (1..600 secondi) |
| Frecce su/giù, sinistra/destra | Naviga righe e quattro colonne |
| Home/End, Pagina su/giù | Prima/ultima riga, sposta di otto bande |
| 1 / 2 / 3 / 4 | Legge le colonne; doppio tocco copia il valore |
| Tab | Cella, riga e copia riga con pressioni rapide |
| B | Modalità selezione bande / ritorno risultati |
| Spazio | Seleziona/deseleziona banda (solo in selezione) |
| X | Seleziona soltanto la banda corrente |
| Canc | Ripristina tutte le 32 bande |
| S | Attiva/disattiva l'ascolto passabanda stereo |
| Invio | Attiva/disattiva la sinusoide stereo (livelli basati su analisi L/R) |
| N, +, - | Noise on/off e livello a passi di 5 dB |
| Ctrl+C | Copia report completo |
| O | Apre report salvato |
| T | Legge curva e impostazioni |
| F1, ? | Legge aiuto sintetico |
| Esc | Esce dalla finestra del plugin, senza cancellare l'istanza |

I controlli prevedono annunci Windows UI Automation per NVDA, sulla base dell'impostazione già utilizzata nei VST3 accessibili. È necessaria una prova con NVDA e REAPER effettivi.

## Dove salva i dati

`%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\settings.xml`

`%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\spectrum_LastResult.txt`

Inoltre lo stato di ogni singola istanza viene incluso nel progetto DAW (parametri e risultati).

## Compilazione su GitHub

Estrarre **tutti i file, inclusa `.github`**, direttamente nella radice del clone locale `LSpectrum`, mantenendo `.git`. Da CMD aperto in quella cartella:

```cmd
git add . && git commit -m "Continue LSpectrum VST3 migration: stereo scan and Esc" && git push
```

Il workflow compila su `windows-2022`, generatore `Visual Studio 17 2022`, JUCE 8.0.15. Scaricare il VST3 negli Artifacts del run completato.

## Limiti della TEST3

- **Questi sorgenti non sono stati compilati con Visual Studio Windows né collaudati in REAPER / NVDA** nell'ambiente di creazione; la verifica finale avviene su GitHub Actions e nel tuo REAPER.
- La chiusura forzata di un editor dockato non è garantita: il contenitore grafico appartiene a REAPER, non al VST3.
- R non può riprodurre il progetto REAPER: misura solo i campioni effettivamente forniti al plugin mentre gira l'audio.
- I controlli di misura/rotazione della **fase ±180°** non sono inclusi: saranno un lavoro DSP separato dopo aver verificato la misura stereo e il passabanda.
- Nessuna logica Lua per Track/Master/Kick/ASDFGH: il VST3 analizza il proprio ingresso stereo, indipendentemente dalla posizione nella catena.
