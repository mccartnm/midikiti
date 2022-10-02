#pragma once

#include "mk_common.h"

#include "lutil.h"
#include "lu_storage/vector.h"
#include "lu_state/state.h"

namespace mk {

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
    MidiController(int ready_out);

    // -- Transitions

    bool boot();
    bool connected();
    bool enter_runtime();

    // -- Runtimes

    void discover();
    void runtime();

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
    size_t _last_connection;

    uint8_t _last_address;
    lutil::Vec<MidiConnection*> _connections;
    lutil::Vec<uint8_t> _octaves;

    uint8_t _midi_channel;
};

} // namespace mk
