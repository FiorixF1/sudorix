#ifndef DEBUG_HPP
#define DEBUG_HPP

#ifdef __EMSCRIPTEN__
  // WASM
  #include <emscripten/emscripten.h>
#else
  // native
  #include <cstdio>
  #define EMSCRIPTEN_KEEPALIVE
  #define emscripten_log(x,  fmt,  ...) printf(fmt,  ##__VA_ARGS__);
#endif

#ifdef DEBUG
  #define console_log(fmt,  ...) emscripten_log(EM_LOG_CONSOLE,  fmt "\n",  ##__VA_ARGS__)
#else
  #define console_log(fmt,  ...) do { } while (0);
#endif

#endif // DEBUG_H
