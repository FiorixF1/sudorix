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
