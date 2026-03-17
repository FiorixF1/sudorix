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

* **31509** enigmojn el **31512** el `Just17.txt`

* **32099** enigmojn el **50000** el `top50000.txt`

## Etendado de teknikoj

Aldonu novajn teknikojn en `solver.cpp` per realigo de funkcio kun la sekva signaturo:

- `typedef void (*TechniqueFn)(SudokuBoard &, EventQueue &);`

Ĉiu funkcio povas:

- atribui valoron al ĉeloj
- forigi kandidatojn el ĉeloj

Eventoj estas priskribataj per `uint32_t out[1024]`:

- `out[0]` = tipo (`EventType::SetValue` aŭ `EventType::RemoveCandidate`)
- `out[1]` = reasonId (ekzemple `ReasonId::NakedSingle`)
- `out[2]` = 1 se la evento estis produktita en antaŭa iteracio
- `out[3]` = nombro da operacioj
- `out[4]` = nombro da fontoj
- listo de fontoj esprimitaj kiel duopo **(ĉelaro, cifermasko)**
- listo de operacioj esprimitaj kiel duopo **(indekso, cifermasko)**

La **cifermasko** estas masko por prezenti unu aŭ plurajn ciferojn per naŭ bitoj. Ekzemploj:

- `000000001` = cifero 1
- `000000010` = cifero 2
- `001000111` = ciferoj {1, 2, 3, 7}

La **indekso** estas nombro inter 0 kaj 80 por indiki unu ĉelon el la krado, ekzemple 0 por r1c1, 80 por r9c9, 9 por r2c1, 50 por r6c6 ktp

La **ĉelaro** estas kunmetita dateno por prezenti unu aŭ plurajn ĉelojn el unuo. La lastaj kvin bitoj kodas nombron, kiu indikas unuon laŭ tiu ĉi dispartigo:

- `0..8` = Vico
- `9..17` = Kolumno
- `18..26` = Bloko

La sekvantaj naŭ bitoj estas masko por indiki la ĉelojn de tiu unuo. Ekzemplo:

```
0000000000000000000 11000000 01101
                    |        ^ 01101 = 13 = kvina kolumno
                    ^ 11000000 = oka kaj naŭa ĉelo de la unuo
```

Tiu ĉi nombro prezentas la okan kaj naŭan ĉelon de la kvina kolumno, aŭ **r89c5** en eŭreka simbolaro.
La masko por ĉeloj povas esti malplena kaj tiukaze ĝi prezentas malplenan aron. Tiu speciala valoro estas ofte uzata por disigi logike malsamajn grupojn de fontoj.

Pluraj informoj pri la interfacoj, funkcioj kaj datenspecoj estas en dediĉita [dokumento](API.md).
