#include "standalone_runtime.h"

void setup()
{
  standaloneRuntimeSetup({
      ScreenId::System,
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
