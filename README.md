# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST7

**Repository:** https://github.com/Lo-lo78/LSpectrum

VST3 stereo per REAPER, migrato da JSFX/Lua. Acquisizione e FFT su thread separato, 32 bande logaritmiche, riferimento 4,5 dB per ottava, analisi separata L/R, generatore di rumore e ascolto della banda in solo. L'audio in ingresso attraversa il plugin senza alterazioni quando noise e solo sono disattivati.

## Modifiche TEST7

1. **Ingresso pulito con NVDA:** eliminate completamente le due frasi introduttive `Previous result available...` e `Arrows: table...`. La guida a video non viene più creata. L'etichetta di progresso è vuota all'apertura e viene esclusa dall'albero di accessibilità con `setAccessible(false)`; serve solo come indicatore visivo dopo l'avvio di un'analisi. NVDA annuncia soltanto la cella corrente dopo l'apertura, come nella TEST6. Restano i nomi nativi dei pulsanti, quando vengono raggiunti.
2. **Passabanda molto più selettivo:** il precedente filtro `StateVariableTPTFilter` con risonanza 4 è stato sostituito da due filtri Butterworth di **sesto ordine** in cascata: passa-alto 36 dB/ottava e passa-basso 36 dB/ottava, per ogni canale. Sono sei biquad per canale, tre passa-alto e tre passa-basso. Se il rumore di riferimento è attivo, anche questo viene filtrato durante il solo: nessun rumore non filtrato è sommato a valle. I tagli inferiore/superiore coincidono con i confini delle 32 bande dell'analizzatore, non con una larghezza Q arbitraria.
3. **Cambio di banda senza scatti:** due banchi di filtri già allocati e un crossfade di circa 25 ms tra bande. Attivazione/disattivazione del solo con transizione di circa 10 ms. Nessuna allocazione, mutex o generazione di coefficienti nel thread audio.
4. **Bande sopra Nyquist:** se il centro della banda scelta è fuori dall'intervallo riproducibile alla frequenza di campionamento corrente, il solo emette silenzio anziché far ascoltare una banda inferiore non richiesta.

Il nuovo passabanda non è un filtro ideale a pareti verticali: a 36 dB/ottava le frequenze esterne vengono attenuate gradualmente, non cancellate istantaneamente. L'ascolto isolato introduce fisiologicamente rotazioni di fase, ma non le applica al segnale normale quando il solo è spento.

## Comandi

| Tasto | Funzione |
| --- | --- |
| Frecce, Home, End, Pagina su/giù | Naviga la tabella a quattro colonne |
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
git add . && git commit -m "LSpectrum TEST7 clean NVDA entry and 36dB bandpass" && git push
```

**Stato:** sorgenti controllati staticamente, con verifica numerica indipendente delle pendenze. La compilazione Windows e il collaudo in REAPER/NVDA restano da effettuare su GitHub Actions.
