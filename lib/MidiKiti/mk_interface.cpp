#include "mk_interface.h"

static mk::_MidiInterface *__handler_inst = nullptr;

/* Handler that pushed the request to the global instance */
void __handle_request()
{
    if (!__handler_inst)
        return;

    __handler_inst->events_requested();
}

namespace mk {

void _MidiInterface::SetInterfaceInstance(_MidiInterface *instance)
{
    __handler_inst = instance;
    Wire.onRequest(__handle_request);
}

_MidiInterface::_MidiInterface(uint8_t id, int ready_in, int ready_out)
    : _MidiDriver()
    , _ready_in(ready_in)
    , _ready_out(ready_out)
    , _uuid(id << 8)
    , _requested(false)
    , _connected(false)
    , _address(0xFE)
{
    // Off -> Boot
    add_transition(Off, Boot, &_MidiInterface::initialize);

    // Boot -> Connect
    add_transition(Boot, Connect, &_MidiInterface::ready_to_connect);

    // Connect -> Wait
    add_transition(Connect, Wait, &_MidiInterface::connect_complete);

    // Connect -> Runtime once initalized
    add_transition(Wait, Runtime, &_MidiInterface::runtime_engage);

    // -- Runtimes

    // Connection Handler
    add_runtime(Connect, &_MidiInterface::connect);

    // The virtual runtime interface
    add_runtime(Runtime, &_MidiInterface::runtime);
}

void _MidiInterface::events_requested()
{
    Wire.write((uint8_t)_pooled_events.count());
    _requested = true;
}

uint8_t _MidiInterface::address() const
{
    return _address;
}

// -- Transitions

bool _MidiInterface::initialize()
{
    // We need to make sure our READY_RUN pin is low
    // to avoid other devices working on the network
    digitalWrite(_ready_in, LOW);
    return true; // Move into the Boot stage
}

// Check to see if we are on deck to be added to the
// network.
bool _MidiInterface::ready_to_connect()
{
    if (digitalRead(_ready_in) == HIGH)
    {
        // Clear the buffer (if any)
        while (Wire.available() > 0)
            Wire.read();

        // --------------------------------------
        // Startup the I2C interface so, while we
        // handle the connection bits
        Wire.begin(0xFE);

        // Send the address
        Wire.beginTransmission(0);
        Wire.write((byte *)(&_uuid), sizeof(_uuid));
        Wire.endTransmission();
        // --------------------------------------

        return true;
    }

    return false;
}

bool _MidiInterface::connect_complete()
{
    return _connected;
}

bool _MidiInterface::runtime_engage()
{
    if (Wire.available() > 2)
    {
        byte read[3];
        Wire.readBytes(read, 3);
        return strcmp((const char *)read, ENGAGE_COMMAND) == 0;
    }

    return false;
}

// -- Runtimes

void _MidiInterface::connect()
{
    // Check on the available wire and swap to the new
    // address, then let the ready pin out know
    while (Wire.available() > 0)
    {
        _address = Wire.read();
        Wire.end();
        delay(1);
        Wire.begin(_address);

        // The next device (if any) can now take
        // its turn to regsiter for an address
        digitalWrite(_ready_out, HIGH);
        _connected = true;
    }
}

void _MidiInterface::queue_event(const RawEvent &event)
{
    _pooled_events.push(event);
}

uint8_t size_of_event(const RawEvent &event)
{
    switch (event.type)
    {
    case KEY_EVENT_ID: return sizeof(KeyEvent);
    }
    return 0;
}

void _MidiInterface::flush()
{
    Wire.beginTransmission(0);
    
    while (_pooled_events.count() > 0)
    {
        RawEvent event = _pooled_events.pop(0);
        Wire.send((uint8_t *)(&event), size_of_event(event));
        Wire.endTransmission(false);
    }

    // Reset the queue
    _pooled_events = {};
    Wire.endTransmission();
}

}
