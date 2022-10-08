#include "mk_controller.h"
#include "mk_interface.h"
#include "mk_command.h"

#include <SoftwareSerial.h>

#include <Wire.h>

// #include <MIDI.h>

// The controller listen on the extra hardware serial from our teensy
// MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, s_midi)

namespace mk {

using namespace lutil;

void MidiConnection::send(uint8_t *bytes, uint8_t size)
{
    Wire.beginTransmission(_address);
    Wire.write(bytes, size);
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
                        event.payload |= uint64_t(address) << 48;

                        switch (type)
                        {
                        case KEY_EVENT_ID:
                        {
                            // key
                            event.payload |= uint64_t(Wire.read()) << 40;

                            // velocity
                            event.payload |= uint64_t(Wire.read()) << 32;

                            // pressed
                            event.payload |= uint64_t(Wire.read()) << 24;
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
    // s_midi.begin();

    if (_ready_out >= 0)
        digitalWrite(_ready_out, HIGH);
    return true;
}

bool MidiController::connected()
{
    unsigned long time = millis();
    return (_ready_out < 0) || (_last_connection + 500 < time);
}

void MidiController::discover()
{
    if (Wire.available() > 0)
    {
        // We have a new connection now!
        _last_connection = ElapsedMicros();

        uint16_t uuid = Wire.read();

        // The first 8 bits tell use if this is an Octave (for
        // which we support multiple)
        bool isOctave = uuid == OCTAVE_ID;
        uuid = uuid << 8;
        uuid |= Wire.read();

        delayMicroseconds(50);
        Wire.beginTransmission(0xFE);
        Wire.write(_last_address++);
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

    auto lit = _local.begin();
    for (; lit != _local.end(); lit++)
    {
        MidiCommander *command = (*lit);

        // Consume the events
        lutil::Vec<RawEvent> events;
        command->take_events(events);

        auto eit = events.begin();
        for (; eit != events.end(); eit++)
        {
            _process_event(*eit);
        }
    }

    while(usbMIDI.read()){}

    // We'll also check for input commands from the
    // Serial lines. This way, we can have a computer
    // manage the preference settings, among other
    // future activities
    scan_input();
}

void MidiController::scan_input()
{
    while (Serial.available())
    {
        if (!_receiving)
        {
            uint8_t b = Serial.read();
            if (b == Command_StartFlag)
            {
                // Pulled High - time to start
                // reading a command.
                _receiving = new Command();
                _receiving->id = Command_StartFlag;
                _receiving->size = Command_StartFlag;
            }
        }
        else if (_receiving->id == Command_StartFlag)
        {
            _receiving->id = Serial.read();
        }
        else if (_receiving->size == Command_StartFlag)
        {
            _receiving->size = Serial.read();
            _receiving->payload = lutil::managed_data(
                _receiving->size
            );
        }
        else
        {
            _receiving->payload[_payload_index++] = Serial.read();
            if (_payload_index >= _receiving->size)
            {
                // We've got a command ready.
                process_command(*_receiving);

                delete _receiving;
                _receiving = nullptr;
                _payload_index = 0;
            }
        }
    }
}

void MidiController::process_command(Command &command)
{
    switch(command.id)
    {
    case Command_GetLayout:
    {
        // First, we write the number of commanders:
        uint8_t count = _connections.count();
        count += _local.count();

        Serial.write(Command_StartFlag);
        Serial.write(Command_GetLayout);
        Serial.write(count);

        // TODO
        // auto it = _connections.begin();
        // for (; it != _connections.end(); it++)
        // {
        //     _push_layout(*it);
        // }

        auto lit = _local.begin();
        for (; lit != _local.end(); lit++)
        {
            (*lit)->push_layout();
        }

        break;
    }
    case Command_GetPreferences:
    {
        PreferencesHeader *comm = reinterpret_cast<PreferencesHeader*>(
            command.payload.get()
        );

        uint8_t idx = comm->commander;
        if (idx >= _connections.count())
        {
            idx -= _connections.count();
            MidiCommander *commander = _local[idx];

            // Write to the Serial line
            commander->query_preferences(comm->index);
        }
        else
        {
            // TODO: MidiConnection::push_preferences()
        }

        Message("Prefs Requested");

        break;
    }
    case Command_SetPreferences:
    {
        uint8_t *data = command.payload.get();

        PreferencesHeader *comm = reinterpret_cast<PreferencesHeader*>(
            data
        );

        // Start of the actual Preferences bytes
        data = data + sizeof(PreferencesHeader);

        uint8_t idx = comm->commander;
        if (idx >= _connections.count())
        {
            idx -= _connections.count();
            MidiCommander *commander = _local[idx];

            Message("Set Preference");
            commander->set_preferences(
                comm->index,
                data[0],
                data + 1
            );

        }
        else
        {
            // TODO: MidiConnection::push_preferences()
        }

        break;
    }
    }
}

void MidiController::add_local(MidiCommander *command)
{
    _local.push(command);
    command->set_address(_last_address++);

    lutil::Vec<_AbstractMidiInterface*> &components = command->components();
    auto it = components.begin();
    for (; it != components.end(); it++)
    {
        _AbstractMidiInterface *component = (*it);
        if (component->interface_id() == OCTAVE_ID)
            _octaves.push(command->address());
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

        // 60 is the MIDI middle C, 12 keys to an octave
        byte realkey = (int8_t(key->key) + 60) + (12 * octave);

        if (key->pressed)
        {
            usbMIDI.sendNoteOn(realkey, key->velocity, _midi_channel);
        }
        else
        {
            usbMIDI.sendNoteOff(realkey, key->velocity, _midi_channel);
        }

        break;
    }
    case POT_EVENT_ID:
    {
        PotEvent *pot = event_cast<PotEvent>(&event);
        usbMIDI.sendControlChange(
            pot->control,
            pot->value,
            _midi_channel
        );
    }
    }
}

int8_t MidiController::_get_octave(uint8_t address)
{
    int index = 0;

    if (_octaves.count() == 1)
        return 0;
    
    auto it = _octaves.begin();
    for (; it != _octaves.end(); it++, index++)
    {
        if (address == (*it))
            break;
    }

    if (it == _octaves.end())
        return 0;

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
