#include "standalone_runtime.h"

void setup()
{
  standaloneRuntimeSetup({
      ScreenId::Launcher,
      true,
      true,
      false,
      true,
      250,
      90,
  });
}

void loop()
{
  standaloneRuntimeLoop();
}
