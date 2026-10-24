#include <windows.h>
#include "../gameinput_release.h"

struct SlowInput {
  HANDLE gate;
  HANDLE completed;
  LONG* calls;
  void Release() {
    WaitForSingleObject(gate, INFINITE);
    InterlockedIncrement(calls);
    SetEvent(completed);
    delete this;
  }
};

extern "C" __declspec(dllexport) void StartRelease(HANDLE gate,
                                                  HANDLE completed,
                                                  LONG* calls) {
  ReleaseGameInputOffThread(new SlowInput{gate, completed, calls});
}
