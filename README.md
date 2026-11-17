# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST8

**Repository:** https://github.com/Lo-lo78/LSpectrum

VST3 stereo per REAPER, migrato da JSFX/Lua. Acquisizione e FFT su thread separato, 32 bande logaritmiche, riferimento 4,5 dB per ottava, analisi separata L/R, generatore di rumore e ascolto della banda in solo. L'audio in ingresso attraversa il plugin senza alterazioni quando noise e solo sono disattivati.

## Modifiche TEST8

- **Guadagno dell'ascolto passabanda:** Ctrl+Freccia su/giù modifica di 1 dB il livello del solo fra **-120 e +24 dB** (default 0 dB); rampa di 20 ms, nessun effetto sull'audio normale o sui dati FFT. Valore salvato in `settings.xml` e nello stato di ogni istanza REAPER, compatibile con i progetti TEST7 precedenti.
- **Primo ingresso NVDA:** il componente editor espone come nome e titolo accessibile la **cella corrente** invece del titolo ripetitivo. L'annuncio avviene quando finestra, focus e provider UI Automation sono realmente pronti, con ritentativi silenziosi se il provider non è ancora disponibile. Nessuna frase introduttiva. Il nome del prodotto VST3 rimane invariato per REAPER. REAPER può comunque annunciare il proprio titolo FX Chain: il plugin controlla soltanto il nome accessibile del proprio editor.
- **Visivo:** al posto del titolo statico viene mostrata la cella corrente, e sotto il guadagno del passabanda.
- **DSP confermato:** passabanda di sesto ordine su ciascun lato (36 dB/ottava), stereo, crossfade tra bande di 25 ms, noise, FFT su thread separato, comandi e tabella invariati.

**Attenzione ai picchi:** +24 dB equivale a un guadagno di circa 15,85 volte in ampiezza. Senza limiter si può arrivare al clipping nelle tracce seguenti.

## Comandi

| Tasto | Funzione |
| --- | --- |
| Frecce, Home, End, Pagina su/giù | Naviga la tabella a quattro colonne |
| Ctrl+Freccia su / Ctrl+Freccia giù | Guadagno ascolto passabanda +1 / -1 dB (da -120 a +24 dB) |
| Spazio | REAPER Play/Stop |
| Ctrl+Spazio | REAPER Play/Pausa |
| R | Nuova analisi stereo |
| C | Focus diretto al campo `countdown` (1–600 s), senza finestra modale |
| Invio (tabella) | Attiva/disattiva il passabanda della banda corrente |
| Invio (campo countdown) | Salva la durata, torna alla tabella |
| B | Selezione delle bande |
| Maiusc+Spazio (in B) | Seleziona/deseleziona banda |
| X (in B) / Canc | Solo banda selezionata / ripristina tutte le 32 bande |
| N, +, - | Rumore di riferimento e livello |
| 1, 2, 3, 4 | Legge la colonna; doppia pressione copia il dato |
| Ctrl+C | Copia il rapporto completo |
| O / P | Apre il report salvato |
| F1 | Guida ai comandi (solo su richiesta) |
| Esc | Esce dalla finestra del VST (secondo il contenitore REAPER) |

**Archiviazione:** `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\settings.xml` e `spectrum_LastResult.txt`. Posizione della tabella, impostazioni e risultati di ciascuna istanza sono anche nel progetto REAPER. Nessuna cartella Documenti.

## Compilazione

Copiare tutti i file **direttamente nella radice del repository locale**, sovrascrivendo quelli precedenti ma senza eliminare `.git`. Include `.github/workflows/build-windows.yml` per Windows 2022 e JUCE 8.0.15.

```cmd
git add . && git commit -m "LSpectrum TEST8 audition gain and first-focus NVDA" && git push
```

**Stato:** sorgenti controllati staticamente, con verifica numerica indipendente delle pendenze. La compilazione Windows e il collaudo in REAPER/NVDA restano da effettuare su GitHub Actions.
