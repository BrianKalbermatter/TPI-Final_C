#include <utilidades.h>

int main(int argc, char** argv) {
  activar_utf8();

  log_set_min_level(LOG_LEVEL_TRACE);

  LOG_TRACE("%s", "Hola");
  LOG_DEBUG("Depurando. argc=%i", argc);
  limpiar_consola();
  LOG_INFO("Info");
  LOG_WARNING("Advertencia...");
  LOG_ERROR("Falló al reservar memoria");

  return 0;
}