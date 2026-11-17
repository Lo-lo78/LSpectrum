# LSpectrum 4.5dB Analyzer — Guida italiana

**Versione:** 0.99.1  
**Data di pubblicazione:** 18 novembre 2026  
**Autore:** Lo-lo  
**Repository:** https://github.com/Lo-lo78/LSpectrum

## 1. Che cos'è

LSpectrum è un VST3 stereo per REAPER che misura la distribuzione spettrale su
**32 bande logaritmiche** tra circa 20 Hz e 20 kHz. Confronta il livello di
ciascuna banda con una **curva di riferimento decrescente di 4,5 dB/ottava**,
ancorata alla banda più vicina a 1 kHz. Non mostra dBFS assoluti: i risultati
sono le **differenze dalla curva**, calcolate separatamente per sinistra e destra.

L'analizzatore sostituisce la precedente combinazione JSFX e Lua. Non serve
selezionare Track/Master: elabora il segnale che riceve nella posizione della
catena FX dove è inserito. Non inserisce né rimuove automaticamente effetti.

Quando rumore e passabanda sono disattivati, il segnale stereo passa inalterato.
Quando il passabanda è attivo, viene fatto ascoltare il **segnale reale** nella
banda selezionata. Il rumore, quando abilitato, viene sommato ai due canali e
rientra anche nell'analisi: disattivarlo se si vuole misurare soltanto l'ingresso.

## 2. Misurare lo spettro

1. Inserire il VST3 nella catena FX della traccia o del master da misurare.
2. Avviare la riproduzione con **Spazio**; **Ctrl+Spazio** comanda Play/Pausa.
3. Premere **R** per avviare una nuova analisi.
4. Il countdown predefinito è **10 secondi di audio ricevuto**, non di attesa
   a REAPER fermo. Se il motore audio non invia campioni, l'analizzatore attende.
5. A fine scansione, leggere la tabella con le frecce o con **1–4**.

La FFT utilizza **32.768 campioni**, su un thread separato dal callback audio.
I risultati sono mediati fra i blocchi **non silenziosi**, indipendentemente su
L e R. Una scansione completamente silenziosa, o un canale senza segnale, viene
identificata come **No signal**: non come 0 dB. La soglia interna di silenzio è
molto bassa; segnali quasi impercettibili possono ancora essere misurati.

**Importante:** due canali con la stessa forma spettrale ma volumi diversi
possono mostrare valori di differenza quasi identici. Per confrontare i loro
livelli assoluti, usare un meter L/R in dBFS. Questo strumento misura la
*forma dello spettro*, non il bilanciamento complessivo tra canali.

## 3. La tabella e NVDA

Le quattro colonne sono **Banda**, **Frequenza**, **Left dB** e **Right dB**.
Le frecce su/giù cambiano riga, sinistra/destra cambiano colonna. Home/End
vanno alla prima/ultima riga; Pagina su/giù avanzano di otto righe.

**1, 2, 3, 4** leggono direttamente le colonne; una doppia pressione ravvicinata
copia il dato. **Tab** una volta ripete la cella, due volte legge la riga,
tre volte copia la riga. **Ctrl+C** copia il rapporto completo.

Il plugin ricorda la banda, la colonna e la modalità di selezione nella singola
istanza e nel progetto REAPER. All'ingresso NVDA deve leggere direttamente la
cella, senza un'introduzione lunga. **Esc dalla tabella** chiude la finestra FX
nei limiti consentiti da REAPER, senza rimuovere il plugin dal progetto.

## 4. Countdown e selezione delle bande

**C** apre il campo di editazione **countdown**, da 1 a 600 secondi.
All'interno il focus resta ciclico: **countdown → OK → Cancel → countdown**.
**Maiusc+Tab** fa il giro inverso. **Invio** sul campo o su OK conferma;
**Invio** su Cancel oppure **Esc** annulla. Tutte queste operazioni ritornano
alla tabella e non chiudono il plugin. Le frecce su OK e Cancel non devono
passare alla FX Chain.

**B** abilita la selezione delle bande. In questa modalità:
- **Maiusc+Spazio** include/esclude la banda corrente;
- **X** lascia selezionata soltanto la banda corrente;
- **Canc** ripristina tutte le 32 bande;
- **B** torna ai risultati filtrati secondo la selezione.

Rimane sempre attiva almeno una banda.

## 5. Passabanda e rumore

**Invio nella tabella** attiva/disattiva l'ascolto isolato della banda corrente.
Il filtro stereo usa un passa-alto e un passa-basso **Butterworth del sesto
ordine**: circa **36 dB/ottava per ciascun fianco**. Non è un taglio infinito.
Il cambiamento di banda ha una transizione di circa 25 ms.

**Ctrl+Freccia su/giù** regola il solo di 1 dB, da **−120 a +24 dB**.
Il valore predefinito è **0 dB**, senza guadagno nascosto. Il gain agisce
soltanto sull'ascolto passabanda, non sui dati analizzati né sul segnale dry.
Attenzione ai valori positivi: senza limiter è possibile superare 0 dBFS.

**N** accende/spegne il rumore di riferimento, **+ / −** ne regolano il
livello in passi di 5 dB. La sinusoide del vecchio JSFX è stata rimossa.

## 6. Altri comandi

| Comando | Effetto |
|---|---|
| Spazio / Ctrl+Spazio | REAPER Play/Stop / Play/Pausa (Windows/REAPER) |
| R | Nuova analisi |
| C | Countdown, con OK e Cancel |
| B / Maiusc+Spazio / X / Canc | Selezione delle bande |
| Invio (tabella) | Ascolto passabanda |
| Ctrl+Su/Giù | Guadagno passabanda |
| N / + / − | Rumore e relativo livello |
| F1 | Riepilogo vocale dei comandi |
| Alt+A | About, finestra interna con testo leggibile |
| O oppure P | Apre il rapporto salvato |
| Ctrl+C | Copia rapporto |
| Esc (tabella) | Chiude la finestra FX, non elimina il VST |

In **About**, Tab alterna testo e OK; **Esc** oppure Invio chiudono About e
tornano alla stessa cella della tabella.

## 7. Memoria e installazione

La singola istanza conserva nel progetto REAPER risultati e impostazioni.
Le preferenze generali e l'**ultimo rapporto esportato**, condiviso dalle
istanze, sono in:

`%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\`

con `settings.xml` e `spectrum_LastResult.txt`. Due istanze conservano i propri
risultati nello stato del progetto, ma l'ultimo file TXT su disco è globale e
viene aggiornato dall'ultima scansione conclusa.

Il pacchetto Windows contiene una directory `.vst3`: copiarla nella cartella
VST3 utilizzata da REAPER (di norma `C:\Program Files\Common Files\VST3`),
poi eseguire una nuova scansione dei plugin se necessaria. Il VST3 non richiede
il vecchio JSFX, Lua, OSARA o SWS per misurare lo spettro. La lettura accessibile
è progettata per NVDA tramite Windows UI Automation.

## 8. Limiti e licenza

L'analizzatore è pensato soprattutto per musica e segnali a spettro ampio.
I toni singoli possono mostrare dispersione verso bande adiacenti perché la
FFT mantiene il metodo originale senza finestratura Hann applicata. Nei pressi
di Nyquist, le bande più alte possono essere parzialmente non rappresentabili
alle frequenze di campionamento basse.

Il software e il sorgente sono distribuiti sotto **GNU AGPLv3 (versione 3.0
soltanto)**. Il testo completo è in `LICENSE`. JUCE 8.0.15 viene usato
secondo il percorso AGPLv3. Nessuna garanzia.
