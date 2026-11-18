# LSpectrum 4.5dB Analyzer 0.99.1

Prima release pubblica del nuovo analizzatore VST3 in C++/JUCE per REAPER.

- Migrazione dell'analizzatore JSFX/Lua in un singolo plugin stereo.
- 32 bande logaritmiche e riferimento spettrale a 4,5 dB/ottava.
- Analisi Left/Right con FFT su thread dedicato e countdown da 1 a 600 secondi.
- Correzione della media: i blocchi silenziosi sono esclusi su ogni canale;
  se non c'è segnale viene mostrato **No signal**.
- Passabanda stereo a 36 dB/ottava per fianco, regolabile da −120 a +24 dB,
  senza guadagno nascosto a 0 dB.
- Generatore di rumore di riferimento, selezione delle bande e copia/esportazione.
- Interfaccia accessibile NVDA, countdown ciclico con OK/Cancel, Alt+A About.
- Persistenza nel progetto REAPER e preferenze in AppData/Roaming/Lo-lo.
- Bypass DSP in idle: senza scansione, noise a −120 dB e passabanda spento,
  l’audio stereo passa senza essere elaborato. Il thread FFT dorme finché non serve.
- Codice aperto sotto GNU AGPLv3, guide italiane e inglesi incluse.

Compatibilità: VST3 Windows x64; pensato per REAPER e NVDA.
I valori rappresentano differenze spettrali rispetto alla curva, non dBFS assoluti.
