#include <windows.h>
#include <GameInput.h>

STDAPI GameInputInitialize(REFIID riid, void** result);

// The v0 header declares GameInputCreate. Route it through the NuGet loader so
// the plugin can return an initialization error even without a GameInput DLL.
STDAPI GameInputCreate(IGameInput** result) {
  return GameInputInitialize(__uuidof(IGameInput),
                             reinterpret_cast<void**>(result));
}
