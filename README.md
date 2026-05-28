# Guida Rapida: Build e Comandi

**Compilazione:** Apri il terminale nella cartella radice del progetto e lancia:
`cmake -B build -D CMAKE_BUILD_TYPE=Release` seguito da `cmake --build build`.

**Esecuzione:** Lanciare i binari sempre dalla cartella radice per far caricare correttamente texture e font (es. `./build/Tappa12.bin`).

**Comandi Interfaccia Utente (Versione Finale):**
* **Mouse:** Tieni premuto con il Click Sinistro per ruotare la visuale; usa la *Rotellina* per lo zoom in/out sul target.
* **Navigazione:** Usa la freccia `SU` per selezionare il Padre (es. verso il Sole), `GIÙ` per il Figlio (es. verso la Luna), `DESTRA/SINISTRA` per scorrere i Fratelli (es. tra le lune di Giove).
* **Modificatore tempo:** Premi `SHIFT` per accelerare le orbite, `CTRL` per rallentarle, e `BARRA SPAZIATRICE` per resettare il tempo alla velocità normale.