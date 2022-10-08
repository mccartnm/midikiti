#pragma once

#include "mk_common.h"

#include "lutil.h"
#include "lu_storage/vector.h"
#include "lu_state/state.h"

namespace mk {

class MidiCommander;

class MidiConnection
{
public:
    MidiConnection(uint16_t uuid, uint8_t address)
        : _uuid(uuid)
        , _address(address)
    {}
    
    void send(uint8_t *bytes, uint8_t size);

    bool poll(lutil::Vec<RawEvent> &events);

private:
    uint16_t _uuid;   // UUID of instance
    uint8_t _address; // I2C address of device
};

/* Primary Controller (ideally a teensy for the clock speed) */
class MidiController : public lutil::StateDriver<MidiController>
{
public:
    struct Config
    {
        // The midi channel we work on
        int midi_channel;

        // change this to match your SD shield or module;
        // Arduino Ethernet shield: pin 4
        // Adafruit SD shields and modules: pin 10
        // Sparkfun SD shield: pin 8
        // Teensy audio board: pin 10
        // Teensy 3.5 & 3.6 & 4.1 on-board: BUILTIN_SDCARD
        // Wiz820+SD board: pin 4
        // Teensy 2.0: pin 0
        // Teensy++ 2.0: pin 20
        // -1 means no-SD card (and no saved settings)
        int sdPin = -1;
    };

    MidiController(const Config &config, int ready_out = -1);

    // -- Transitions

    bool boot();
    bool connected();
    bool enter_runtime();

    // -- Runtimes

    void discover();
    void runtime();

    // --

    void add_local(MidiCommander *command);

    void scan_input();
    void process_command(Command &command);

private:
    void _process_event(RawEvent &event);
    int8_t _get_octave(uint8_t address);

    /**
     * Events can come in "packs" where multiple events occur at the
     * sametime (e.g. user presses a full piano chord). We want to
     * handle this gracefully.
     */

    bool _event;
    uint16_t _event_count;

    uint16_t _ready_out;
    ElapsedMicros _last_connection;

    uint8_t _last_address;
    lutil::Vec<MidiConnection*> _connections;
    lutil::Vec<uint8_t> _octaves;

    // -- Local interfaces (non i2c)
    lutil::Vec<MidiCommander*> _local;

    Command *_receiving = nullptr;
    uint8_t _payload_index;

    Config _config;
};

} // namespace mk
