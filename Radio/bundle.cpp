#include "standalone_runtime.h"

void setup()
{
  standaloneRuntimeSetup({
      ScreenId::Radio,
      true,
      true,
      false,
      false,
      250,
      90,
  });
}

void loop()
{
  standaloneRuntimeLoop();
}
