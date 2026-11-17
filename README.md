# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST11

**Repository:** https://github.com/Lo-lo78/LSpectrum

VST3 stereo per REAPER, migrato da JSFX/Lua. Acquisizione e FFT su thread separato, 32 bande logaritmiche, riferimento 4,5 dB per ottava, analisi separata L/R, generatore di rumore e ascolto della banda in solo. L'audio in ingresso attraversa il plugin senza alterazioni quando noise e solo sono disattivati.

## Modifiche TEST11 — countdown ciclico e focus

- **C** apre il countdown nella stessa finestra e porta direttamente al campo di testo **countdown** (1–600 secondi), senza annunci personalizzati.
- Il gruppo ha esattamente **tre controlli**: `countdown` → `OK` → `Cancel` → `countdown`, con **Tab ciclico**; **Maiusc+Tab** percorre il ciclo al contrario. I pulsanti compaiono solo quando si modifica la durata.
- **Invio nel campo o su OK** conferma e salva i secondi; **Invio su Cancel** oppure **Esc** annulla senza salvare. Entrambe le azioni tornano alla tabella, senza chiudere il plugin.
- Le frecce nel campo spostano il cursore di testo; quando il focus è su OK o Cancel vengono intercettate e **non raggiungono la FX Chain**.
- Il ritorno alla tabella ristabilisce banda e colonna correnti, senza annunciare una banda diversa e senza chiamare una notifica NVDA aggiuntiva.
- **Esc nella tabella** continua invece a chiudere la finestra FX.
- **Nessuna modifica al DSP**: filtro, guadagno TEST10, FFT, rumore e trasporto invariati.

## Modifiche TEST10

- **Ripristinato il volume del filtro come in TEST8:** eliminato completamente il makeup interno di +3 dB introdotto nella TEST9. Ora **0 dB visualizzati = 0 dB effettivi** sul segnale filtrato (nessun amplificatore fisso aggiuntivo). Rimangono Ctrl+Freccia su/giù, intervallo da -120 a +24 dB, rampa di 20 ms e salvataggio delle impostazioni. Non è stato cambiato alcun coefficiente del filtro a 36 dB/ottava.
- **Mantenuta la correzione NVDA della TEST9:** la notifica esplicita della cella viene tentata solo la prima volta per istanza; ai successivi ingressi rimane la normale notifica di focus, per evitare il doppio annuncio.
- Nessun cambiamento ad analisi, countdown, trasporto, rumore, tabella, percorso dati o formato dei preset.

## Informazioni TEST8 (conservate)

- **Guadagno dell'ascolto passabanda:** Ctrl+Freccia su/giù modifica di 1 dB il livello del solo fra **-120 e +24 dB** (default 0 dB); rampa di 20 ms, nessun effetto sull'audio normale o sui dati FFT. Valore salvato in `settings.xml` e nello stato di ogni istanza REAPER, compatibile con i progetti TEST7 precedenti.
- **Primo ingresso NVDA:** il componente editor espone come nome e titolo accessibile la **cella corrente** invece del titolo ripetitivo. L'annuncio avviene quando finestra, focus e provider UI Automation sono realmente pronti, con ritentativi silenziosi se il provider non è ancora disponibile. Nessuna frase introduttiva. Il nome del prodotto VST3 rimane invariato per REAPER. REAPER può comunque annunciare il proprio titolo FX Chain: il plugin controlla soltanto il nome accessibile del proprio editor.
- **Visivo:** al posto del titolo statico viene mostrata la cella corrente, e sotto il guadagno del passabanda.
- **DSP confermato:** passabanda di sesto ordine su ciascun lato (36 dB/ottava), stereo, crossfade tra bande di 25 ms, noise, FFT su thread separato, comandi e tabella invariati.

**Attenzione ai picchi:** con guadagno utente positivo fino a +24 dB, il segnale filtrato può superare 0 dBFS. Non è presente un limiter.

## Comandi

| Tasto | Funzione |
| --- | --- |
| Frecce, Home, End, Pagina su/giù | Naviga la tabella a quattro colonne |
| Ctrl+Freccia su / Ctrl+Freccia giù | Guadagno ascolto passabanda +1 / -1 dB (da -120 a +24 dB) |
| Spazio | REAPER Play/Stop |
| Ctrl+Spazio | REAPER Play/Pausa |
| R | Nuova analisi stereo |
| C | Entra nel countdown, focus nel campo `countdown` (1–600 s) |
| Tab / Maiusc+Tab nel countdown | Ciclo tra campo `countdown`, `OK`, `Cancel` (avanti/indietro) |
| Esc nel countdown | Annulla e torna alla tabella, senza chiudere il VST |
| Invio su OK o nel campo | Salva e torna alla tabella |
| Invio su Cancel | Annulla e torna alla tabella |
| Invio (tabella) | Attiva/disattiva il passabanda della banda corrente |

| B | Selezione delle bande |
| Maiusc+Spazio (in B) | Seleziona/deseleziona banda |
| X (in B) / Canc | Solo banda selezionata / ripristina tutte le 32 bande |
| N, +, - | Rumore di riferimento e livello |
| 1, 2, 3, 4 | Legge la colonna; doppia pressione copia il dato |
| Ctrl+C | Copia il rapporto completo |
| O / P | Apre il report salvato |
| F1 | Guida ai comandi (solo su richiesta) |
| Esc (tabella) | Esce dalla finestra del VST (secondo il contenitore REAPER) |

**Archiviazione:** `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\settings.xml` e `spectrum_LastResult.txt`. Posizione della tabella, impostazioni e risultati di ciascuna istanza sono anche nel progetto REAPER. Nessuna cartella Documenti.

## Compilazione

Copiare tutti i file **direttamente nella radice del repository locale**, sovrascrivendo quelli precedenti ma senza eliminare `.git`. Include `.github/workflows/build-windows.yml` per Windows 2022 e JUCE 8.0.15.

```cmd
git add . && git commit -m "LSpectrum TEST11 countdown focus cycle" && git push
```

**Stato:** verificati staticamente gli handler dei tre controlli e l’assenza di modifiche al DSP rispetto alla TEST10. La compilazione Windows e il collaudo in REAPER/NVDA restano da effettuare su GitHub Actions.
