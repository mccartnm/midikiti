#include "mk_controller.h"

#include <Wire.h>
#include <MIDI.h>

constexpr char Boot[] = "Boot";
constexpr char Connect[] = "Connect";
constexpr char Engage[] = "Engage";
constexpr char Runtime[] = "Runtime";

// The controller listens on the extra hardware serial from our teensy
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, s_midi)

namespace mk {

using namespace lutil;

void MidiConnection::send(uint8_t *bytes, uint8_t size)
{
    Wire.beginTransmission(_address);
    Wire.send(bytes, size);
    Wire.endTransmission();
}

bool MidiConnection::poll(Vec<RawEvent> &events)
{
    // 1. Request an OK byte from the device as a handshake
    // 2. Then sit on the line waiting for the events
    //    to come in and pass them back.
    
    delayMicroseconds(25);
    uint8_t size = Wire.requestFrom(_address, sizeof(byte));
    if (size > 0)
    {
        uint8_t eventCount = Wire.read();
        if (eventCount < 0)
            return false;

        // With that done, we know there are events to process
        // so we need to collect them.
        delayMicroseconds(25);

        // Up to 127 simult events from a controller (yikes)
        while (eventCount > 0)
        {
            if (Wire.available() > 0)
            {
                eventCount--;

                // We have an event incoming...

                while (true)
                {
                    RawEvent event = {0};
                    if (Wire.available())
                    {
                        // Asserted
                        uint8_t type = Wire.read();
                        uint8_t address = Wire.read();

                        event.type = type;
                        event.payload |= address << 48;

                        uint16_t consumeBits = 0;
                        switch (type)
                        {
                        case KEY_EVENT_ID:
                        {
                            // key
                            event.payload |= Wire.read() << 40;

                            // velocity
                            event.payload |= Wire.read() << 32;

                            // pressed
                            event.payload |= Wire.read() << 24;
                            break;
                        }
                        default:
                            break;
                        }

                        events.push(event);
                        break;
                    }
                }
            }
        }
    }
    
    return size > 0;
}

MidiController::MidiController(int ready_out)
    : lutil::StateDriver<MidiController>()
    , _event(false)
    , _event_count(0)
    , _ready_out(ready_out)
    , _last_address(0)
    , _midi_channel(1)
{
    // -- Transitions
    
    add_transition(Boot, Connect, &MidiController::boot);
    add_transition(Connect, Engage, &MidiController::connected);
    add_transition(Engage, Runtime, &MidiController::enter_runtime);
    
    // -- Runtimes
    
    add_runtime(Connect, &MidiController::discover);
    add_runtime(Runtime, &MidiController::runtime);
}

bool MidiController::boot()
{
    //
    // Three primary components to the boot process
    // 1. Initialize the I2C
    // 2. MIDI Interface Setup and Pins
    // 3. Move the READY_RUN_OUTPUT to high to initialize
    //    the connection process.
    //

    Wire.begin();

    // We also hook up the MIDI interface here :D
    s_midi.begin(_midi_channel);

    digitalWrite(_ready_out, HIGH);
    return true;
}

bool MidiController::connected()
{
    size_t time = millis();
    return _last_connection + 500 < time;
}

void MidiController::discover()
{
    if (Wire.available() > 0)
    {
        // We have a new connection now!
        _last_connection = millis();

        uint16_t uuid = Wire.read();

        // The first 8 bits tell use if this is an Octave (for
        // which we support multiple)
        bool isOctave = uuid == OCTAVE_ID;
        uuid = uuid << 8;
        uuid |= Wire.read();

        delayMicroseconds(50);
        Wire.beginTransmission(0xFE);
        Wire.write(++_last_address);
        Wire.endTransmission();

        MidiConnection *conn = new MidiConnection(uuid, _last_address);
        _connections.push(conn);

        if (isOctave)
            _octaves.push(uuid);
    }
}

void MidiController::runtime()
{
    Vec<RawEvent> events;

    auto it = _connections.begin();
    for (; it != _connections.end(); it++)
    {
        MidiConnection *conn = (*it);
        conn->poll(events);
    }

    if (events.count() > 0)
    {
        // Process all events at the same time.
        auto eit = events.begin();
        for (; eit != events.end(); eit++)
            _process_event(*eit);
    }
}

void MidiController::_process_event(RawEvent &event)
{
    switch (event.type)
    {
    case KEY_EVENT_ID:
    {
        KeyEvent *key = event_cast<KeyEvent>(&event);

        // Based on the index of the KeyEvents address in our
        // known octave instances, we adjust the key accordingly
        int8_t octave = _get_octave(key->address);
        byte realkey = (int8_t(key->key) + 60) * octave; // 60 is the MIDI middle C

        if (key->pressed)
            s_midi.sendNoteOn(realkey, key->velocity, _midi_channel);
        else
            s_midi.sendNoteOff(realkey, key->velocity, _midi_channel);

        break;
    }
    }
}

int8_t MidiController::_get_octave(uint8_t address)
{
    int index = 0;
    auto it = _octaves.begin();
    for (; it != _octaves.end(); it++, index++)
    {
        if (address == (*it))
            break;
    }

    if (it == _octaves.end())
        return 0; // ??

    return index - (_octaves.count() / 2);
}

bool MidiController::enter_runtime()
{
    auto it = _connections.begin();
    while (it != _connections.end())
    {
        MidiConnection *conn = (*it);
        conn->send((uint8_t *)ENGAGE_COMMAND, 3);
        it++;
    }

    // Now that we've let the devices know, we can start
    // getting notes!
    return true;
}

}
