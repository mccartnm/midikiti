#include "MidiKiti.h"
#include "mk_octave.h"
#include "mk_pot.h"

#include "mk_controller.h"
#include "mk_command.h"

/**
 * The piano module. This is a pretty killer little ditty
 * for working with the 
 */

#define kLoad 14
#define kClockEnable 4
#define kData 16
#define kClock 15

mk::MidiOctave *octave = nullptr;
mk::MidiPot *pot = nullptr;
mk::MidiCommander *command = nullptr;

mk::MidiController *controller = nullptr;

mk::MidiKey firstKey(0, 1, 2);

void setup()
{
    Serial.begin(9600);
    lutil::Processor::get().init();

    // Queues all interface events for the controller
    command = new mk::MidiCommander();

    mk::MidiOctave::Config octave_config;
    octave_config.loadPin = kLoad;
    octave_config.clockEnablePin = kClockEnable;
    octave_config.dataPin = kData;
    octave_config.clickPin = kClock;
    octave = new mk::MidiOctave(octave_config, command);
    octave->add_key(&firstKey);

    mk::MidiPot::Config pot_config;
    pot_config.pin = 19;
    pot_config.control = 1;
    pot_config.low = 15;
    pot = new mk::MidiPot(pot_config, command);

    // Skip i2c comms for now
    mk::MidiController::Config con_config;
    con_config.midi_channel = 1;
    controller = new mk::MidiController(con_config);
    controller->add_local(command);
}

void loop()
{
    lutil::Processor::get().process();
}