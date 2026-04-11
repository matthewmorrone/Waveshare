#include "standalone_runtime.h"

void setup()
{
  standaloneRuntimeSetup({
      ScreenId::Settings,
      true,
      true,
      true,
      false,
      250,
      90,
  });
}

void loop()
{
  standaloneRuntimeLoop();
}
