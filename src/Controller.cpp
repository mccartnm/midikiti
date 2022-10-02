#include <Arduino.h>

#include "MidiKiti.h"
#include "mk_controller.h"

#define READY_OUT 10

mk::MidiController *controller = nullptr;

void setup()
{
  controller = new mk::MidiController(READY_OUT);
  lutil::Processor::get().init();
}

void loop()
{
  lutil::Processor::get().process();
}