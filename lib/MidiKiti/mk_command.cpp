#include "mk_command.h"
#include "mk_interface.h"

#include "mk_controller.h"

static mk::MidiCommander *__handler_inst = nullptr;

/* Handler that pushed the request to the global instance */
void __handle_request()
{
    if (!__handler_inst)
        return;

    __handler_inst->events_requested();
}

namespace mk
{

uint8_t size_of_event(const RawEvent &event)
{
    switch (event.type)
    {
    case KEY_EVENT_ID: return sizeof(KeyEvent);
    }
    return 0;
}

void MidiCommander::SetInterfaceInstance(MidiCommander *instance)
{
    __handler_inst = instance;
    Wire.onRequest(__handle_request);
}

void MidiCommander::add(_AbstractMidiInterface *interface)
{
    _interfaces.push(interface);
}


bool MidiCommander::is_local() const
{
    return _ready_in < 0;
}

bool MidiCommander::is_connected() const
{
    return is_local() || _connected;
}

bool MidiCommander::initialize()
{
    // We need to make sure our READY_RUN pin is low
    // to avoid other devices working on the network
    if (_ready_out >= 0)
        digitalWrite(_ready_out, LOW);
    return true;
}

// Check to see if we are on deck to be added to the
// network.
bool MidiCommander::ready_to_connect()
{
    if (_ready_in < 0) // local only
    {
        _connected = true;
        return true;
    }
        
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

        // TODO: SEND DESCRIPTOR THAT OUTLINES INTERFACE
        // LAYOUT AND PREFERENCES...
        // Wire.write((byte *)(&_uuid), sizeof(_uuid));

        Wire.endTransmission();
        // --------------------------------------

        return true;
    }

    return false;
}

bool MidiCommander::runtime_engage()
{
    if (_ready_in < 0) // local only
    {
        return true;
    }

    if (Wire.available() > 2)
    {
        byte read[3];
        Wire.readBytes(read, 3);
        return strcmp((const char *)read, ENGAGE_COMMAND) == 0;
    }

    return false;
}

void MidiCommander::connect()
{
    if (_ready_in < 0)
    {
        _connected = true;
        return;
    }
    
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
        if (_ready_out >= 0)
            digitalWrite(_ready_out, HIGH);
        _connected = true;
    }
}

void MidiCommander::queue_event(const RawEvent &event)
{
    _pooled_events.push(event);
}

void MidiCommander::flush()
{
    Wire.beginTransmission(0);
    
    while (_pooled_events.count() > 0)
    {
        RawEvent event = _pooled_events.pop(0);
        Wire.write((uint8_t *)(&event), size_of_event(event));
        Wire.endTransmission(false);
    }

    // Reset the queue
    _pooled_events = {};
    Wire.endTransmission();
}

void MidiCommander::set_address(uint8_t address)
{
    _address = address;
}

uint8_t MidiCommander::address() const
{
    return _address;
}

void MidiCommander::events_requested()
{
    Wire.write((uint8_t)_pooled_events.count());
    _requested = true;
}

void MidiCommander::take_events(lutil::Vec<RawEvent> &events)
{
    events.swap(_pooled_events);
}

void MidiCommander::push_layout(Stream *stream)
{
    // The first byte is the number of interfaces
    stream->write(_interfaces.count());

    auto it = _interfaces.begin();
    for (; it != _interfaces.end(); it++)
    {
        // For each interface, we communicate the type
        // of object
        stream->write((*it)->interface_id());
    }
}

void MidiCommander::query_preferences(uint8_t index)
{
    if (index >= _interfaces.count())
        return;
    
    _AbstractMidiInterface *interf = _interfaces[index];
    
    size_t size;
    Parameters *parms = interf->parameters(size);

    if (!parms)
        return;

    Serial.write(Command_StartFlag);
    Serial.write(Command_GetPreferences);
    Serial.write(size + sizeof(PreferencesHeader));

    // We need to include the 2 byte address of our interface
    PreferencesHeader header{ address(), index };
    Serial.write(
        reinterpret_cast<uint8_t*>(&header),
        sizeof(PreferencesHeader)
    );

    // We can now write out the parameters byte by byte (in case
    // it's too large for the buffer, this way it should feed into
    // the computer interface over time)
    for (size_t i = 0; i < size; i++)
        Serial.write(reinterpret_cast<uint8_t*>(parms)[i]);
}

void MidiCommander::set_preferences(
    uint8_t index,
    uint8_t size,
    uint8_t *buffer)
{
    if (index >= _interfaces.count())
    {
        Message("Invalid Interface Index");
        return;
    }

    _AbstractMidiInterface *interf = _interfaces[index];

    interf->setParameters(
        reinterpret_cast<Parameters*>(buffer),
        size
    );

    Message("Params Set");
}

lutil::Vec<_AbstractMidiInterface*> &MidiCommander::components()
{
    return _interfaces;
}

}
