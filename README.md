# Sudorix

Sudorix estas logika solvilo de Sudokuo realigita en C++ kaj alirebla per eleganta reta interfaco pere de WASM.
Tamen Sudorix ankaŭ povas esti kompilata kaj rulata kiel memstara programo.

La plej nova versio de Sudorix estas ĉiam je dispono ĉe [GitHub Pages](https://fiorixf1.github.io/sudorix).

## Kompilado

### Instali Emscripten

Antaŭkondiĉoj:

```bash
sudo apt update
sudo apt install -y git cmake python3 nodejs npm
```

Klonu la deponejon:

```bash
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
```

Instalu la plej novan version:

```bash
./emsdk install latest
./emsdk activate latest
```

Aktivigu la medion ĉe la starto de la terminalo:

```bash
echo 'source ~/emsdk/emsdk_env.sh' >> ~/.bashrc
```

### Kompili

```bash
make wasm
```

### Ruli (grava: uzu lokan HTTP-servilon)

Retumiloj kutime blokas la ŝargon de `.wasm` el `file://`.

```bash
make serve
# poste malfermu:
# http://localhost:8000/sudorix.html
```

### Memstara kompilo

Tio produktas nur la objektan dosieron. Ligu ĝin al ekzistanta aplikaĵo.

```bash
make native
```

### Baza sencimigo

Por obteni detalan protokolon dum la plenumo de la solvilo, aldonu la opcion `DEBUG=1` al la kompila komandlinio.

```bash
make serve DEBUG=1
```

## Testado

Kiam Sudorix estas kompilita kiel memstara aplikaĵo, ĝi povas esti ligata al ekzistanta testa aro, kiu nutras la solvilon per teksta dosiero enhavanta liston de Sudokuoj.
La nunaj testaj vektoroj estas prenitaj el **SudokuWiki**.

### Kompili

```bash
make test
```

### Ruli

```bash
make run PUZZLES=/path/to/file.txt MODE=full|step
```

### Avana sencimigo

Por sencimigo kun gdb, kompilu la solvilon ligatan al la provizita testa aro.

```bash
make test DEBUG=1
gdb bin/sudorix_test
> run /path/to/file.txt --mode=full --threads=1
```

Nuntempe Sudorix povas solvi:

* **31512** enigmojn el **31512** el `Just17.txt`

* **49148** enigmojn el **50000** el `top50000.txt`

## Etendado de teknikoj

Aldonu novajn teknikojn en `solver.cpp` per realigo de funkcio kun la sekva signaturo:

- `typedef void (*TechniqueFn)(SudokuBoard &, EventQueue &);`

Ĉiu funkcio povas:

- atribui valoron al ĉeloj
- forigi kandidatojn el ĉeloj

La komunikado inter la fasado kaj la internaĵo okazas per JSON-objektoj. Vizitu la [dokumentaron](API.md) por pliaj detaloj.

## Interesaj fontoj

Ege interesaj retejoj kaj solviloj estas enlistigitaj ĉi tie. Sur tiuj ĉi laboraĵoj multe sin bazas la evoluo de la solvilo kaj miaj konoj pri Sudokuo:

* [SudokuWiki](https://www.sudokuwiki.org/) el Andrew Stuart
* [HoDoKu](https://hodoku.sourceforge.net/)
* [r/Sudoku Wiki](https://www.reddit.com/r/sudoku/wiki/) el strmckr
* [Sudoku.Coach](https://sudoku.coach/) kaj ĝia [Discord-kanalo](https://discord.gg/p2YKqXrktA)
