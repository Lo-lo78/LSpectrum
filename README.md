# LSpectrum 4.5dB Analyzer — VST3 JUCE TEST5

**Repository:** https://github.com/Lo-lo78/LSpectrum

Migrazione del JSFX e Lua originali a un VST3 C++ JUCE stereo autonomo. Una sola curva di riferimento (4,5 dB/ottava), 32 bande, tabella a quattro colonne, scansione FFT su thread dedicato, ingresso/uscita stereo, generatore di rumore e sinusoide e ascolto passabanda. La correzione di fase non è ancora implementata.

## Novità TEST5 (sinusoide stereo)

- **Un oscillatore, due guadagni indipendenti:** la sinusoide ha la stessa fase sui canali L/R, senza sfasamento artificiale. I due livelli seguono le ampiezze assolute L/R della **banda selezionata nell’ultima scansione completa**. Non vengono usati i dB di differenza rispetto alla curva 4,5 dB, che non rappresentano il volume assoluto.
- **Panorama proporzionale:** il canale più forte riceve il livello di riferimento del tone; quello più debole è attenuato secondo il rapporto reale fra i livelli. Per esempio, con 6 dB di differenza L/R, il canale più debole riceve circa metà ampiezza. In silenzio misurato, nessuna delle due sinusoidi suona; prima della prima scansione entrambe suonano centrate.
- **Niente clic:** rampa di 12 ms all’accensione/spegnimento e al cambio dei guadagni L/R durante la navigazione. I cambi di frequenza rimangono quelli previsti dalla TEST4; non è ancora una sintesi con glide.
- **Volume di riferimento separato:** la navigazione tra bande non cambia più il volume generale della sinusoide in funzione della differenza dalla curva 4,5 dB. Il valore predefinito rimane -24 dB; aprendo un progetto TEST4 viene ripristinato il livello di riferimento -24 dB anziché conservare una vecchia attenuazione transitoria.
- **Ripristino del panorama:** i livelli assoluti sono salvati nello stato del progetto REAPER e, per l’ultima analisi globale, nel nuovo file `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\spectrum_LastLevels.txt`. I precedenti rapporti TEST4 rimangono leggibili: se non esistono livelli assoluti salvati, la sinusoide resta centrata fino alla nuova scansione.
- La misura viene aggiornata **a fine analisi R**, non in modo continuo durante l’ascolto: una nuova scansione aggiorna il panorama; cambiare banda usa i valori memorizzati di quella banda.

## Novità TEST4 (rispetto alla TEST3)

1. **Comandi trasporto REAPER** dalla finestra VST3: `Spazio` = azione **Transport: Play/stop** (ID 40044); `Ctrl+Spazio` = **Transport: Play/pause** (ID 40073). Su Windows, la finestra principale REAPER è individuata nello **stesso processo** e riceve `WM_COMMAND`, non eventi di tastiera sintetici. Nessuna chiamata al trasporto dal thread audio. Se REAPER è bridged in un altro processo, il comando può non essere disponibile.
2. **Banda, colonna e modalità della finestra memorizzate per istanza**, sia chiudendo/riaprendo l'editor, sia nel salvataggio del progetto REAPER. All'apertura, il focus torna sulla cella conservata e NVDA riceve direttamente la sua indicazione, senza introduzione lunga.
3. **Nessuna descrizione introduttiva persistente**: rimossa la frase “Accessible stereo spectrum analyzer...”, così come l'assegnazione della descrizione dell'editor durante ogni annuncio. Il titolo accessibile iniziale identifica la cella.
4. Lettura diretta **`Left, 2.27 dB` / `Right, 2.27 dB`**, eliminando “difference”.
5. Messaggi più brevi per avvio e completamento analisi; un unico annuncio per passare tra selezione bande e risultati. Durante l'analisi continua il conto alla rovescia sui campioni realmente acquisiti.
6. Recuperati i comandi originali `P` per aprire il report e `\\` per leggere le impostazioni, in aggiunta a `O` e `T`.

### Comandi principali NVDA

| Tasti | Azione |
|---|---|
| Spazio | Play/stop di REAPER, anche nella tabella e nella selezione bande |
| Ctrl+Spazio | Play/pausa di REAPER |
| R | Nuova analisi stereo reale (10 s predefiniti) |
| D | Cambia durata (1..600 s) |
| Frecce, Home/End, Pagina su/giù | Navigazione della tabella |
| 1, 2, 3, 4 | Leggi colonna; doppio tocco copia valore |
| Tab veloce 1/2/3 volte | Leggi cella / riga / copia riga |
| B | Entra/esci dalla selezione delle bande |
| **Maiusc+Spazio** | **Seleziona/deseleziona banda** nella modalità B (lo Spazio senza Maiusc ora controlla il trasporto) |
| X / Canc | Seleziona solo banda corrente / tutte le 32 bande |
| S | Solo passabanda stereo |
| Invio | Sinusoide stereo |
| N, +, - | Rumore e suo livello |
| Ctrl+C | Copia rapporto completo |
| O / P | Apre ultimo report |
| T / \\ | Dice durata, curva e numero bande |
| F1 | Aiuto parlato |
| Esc | Chiude la finestra FX (il comportamento dockato dipende dal contenitore REAPER) |

### Stato del lavoro

- La TEST3 era stata provata in REAPER dall'utente: tabella e passabanda funzionano.
- La TEST4 aggiorna solo navigazione/trasporto/accessibilità e recupera i due comandi Lua; non tocca i calcoli FFT né il filtraggio audio.
- **TEST4 non compilata né collaudata con Visual Studio/NVDA in questo ambiente**: convalidare GitHub Actions e il comportamento effettivo di REAPER. Le azioni di trasporto richiedono il REAPER Windows non-bridged.

## Dati e configurazione

- Globale: `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\settings.xml` (durata, rumore, selezione bande).
- Ultimo report: `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\spectrum_LastResult.txt`.
- Livelli L/R per la sinusoide: `%APPDATA%\Lo-lo\LSpectrum 4.5dB Analyzer\spectrum_LastLevels.txt`.
- Posizione editor, modalità, risultati e impostazioni dell'istanza: stato VST3 salvato dentro il progetto DAW.

## Compilazione GitHub

Estrarre i file dello ZIP **direttamente nella radice del clone**, senza creare cartelle aggiuntive e senza cancellare la cartella `.git` locale. Includere `.github/workflows/build-windows.yml`. Il workflow configura JUCE 8.0.15 con Visual Studio 2022 su `windows-2022` e produce l'artefatto VST3.

Da CMD nella cartella del repository:

```cmd
git add . && git commit -m "LSpectrum TEST5 stereo sine balance" && git push
```

Per la prova: aprire il VST3, raggiungere una banda alta in colonna destra, Esc, riaprire il VST3 e verificarne il ripristino. Premere Spazio e Ctrl+Spazio, poi R su una traccia audio in riproduzione; nella modalità B provare Maiusc+Spazio.
