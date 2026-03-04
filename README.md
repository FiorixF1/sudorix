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

### Sencimigo

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

Nuntempe Sudorix povas solvi:

* **30742** enigmojn el **31512** el `Just17.txt`

* **168** enigmojn el **50000** el `top50000.txt`

## Etendado de teknikoj

Aldonu novajn teknikojn en `solver.cpp` per realigo de funkcio kun la sekva signaturo:

- `typedef void (*TechniqueFn)(SudokuBoard &);`

Ĉiu funkcio povas:

- atribui valoron al ĉeloj
- forigi kandidatojn el ĉeloj

Eventoj estas priskribataj per `uint32_t out[1024]`:

- `out[0]` = tipo (`EventType::SetValue` aŭ `EventType::RemoveCandidate`)
- `out[1]` = reasonId (ekzemple `ReasonId::NakedSingle`)
- `out[2]` = 1 se la evento estis produktita en antaŭa iteracio
- `out[3]` = nombro da operacioj
- `out[4]` = nombro da fontoj
- listo de fontoj esprimitaj kiel duopo indekso (0..80) / cifermasko (1..9)
- listo de operacioj esprimitaj kiel duopo indekso (0..80) / cifermasko (1..9)

### API

- `int sudorix_solver_count_solutions(const char *in81)`
  - ricevas Sudokuon kiel ĉenon kaj redonas la nombron da solvoj; malplenaj ĉeloj estas markitaj per `0` aŭ `.`
- `int sudorix_solver_full(const char *in81, char *out81)`
  - ricevas Sudokuon kiel ĉenon kaj redonas la solvon kiel ĉenon; malplenaj ĉeloj estas markitaj per `0` aŭ `.`
- `int sudorix_solver_init_board(const char *in81)`
  - ricevas Sudokuon kiel ĉenon kaj konservas ĝin en la interna memoro de la solvilo
- `int sudorix_solver_next_step(uint32_t *out, uint32_t out_words)`
  - redonas unu paŝon por solvi la Sudokuon ŝargitan per `sudorix_solver_init_board` kaj ĝisdatigas la internan staton; la eligo estas skribita en `out` kaj `out_words` estas la longeco utiligita el la bufro:
  - `out[0]=type`, `out[1]=reasonId`, `out[2]=fromPrev`, `out[3]=opCount`, `out[4]=srcCount`, `out[..srcCount..]=cells/mask`, `out[..opCount..]=idx/mask`
- `int sudorix_solver_hint(const uint8_t *values, const uint16_t *cands, uint32_t *out, uint32_t out_words)`
  - ricevas Sudokuon kiel tabelojn enhavantajn kaj la jam solvitajn ĉelojn kaj la kandidatojn por ĉiu ĉelo, kaj redonas unu paŝon por daŭrigi la solvon; la eligo estas skribita en `out` kaj `out_words` estas la longeco utiligita el la bufro:
  - `out[0]=type`, `out[1]=reasonId`, `out[2]=fromPrev`, `out[3]=opCount`, `out[4]=srcCount`, `out[..srcCount..]=cells/mask`, `out[..opCount..]=idx/mask`
  - **neniu interna stato estas ĝisdatigata**
