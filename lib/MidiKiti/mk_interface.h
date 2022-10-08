#pragma once

#include <Wire.h>

#include "mk_common.h"

#include "lutil.h"
#include "lu_state/state.h"
#include "lu_storage/vector.h"

namespace mk {

class MidiCommander;

/**
 * The abstract interface itself. The whole structure looks something
 * like the following:
 * 
 * MidiController
 *  `- [MidiCommander, ]  // local instances
 *  `- [MidiConnection, ] // i2c instances
 * 
 * MidiCommander
 *    `- _address == (i2c address || MidiController:local_index))
 *    `- [_AbstractMidiInterface, ]
 * 
 * So, from the controller, we can ask for specific interface data
 * through the commanders address and interface index.
 * 
 */
class _AbstractMidiInterface
{
public:
    virtual uint8_t interface_id() const = 0;
    virtual Parameters *parameters(size_t &size) const = 0;
    virtual void setParameters(Parameters *parameters, size_t size) = 0;
};

/**
 * Abstract interface for the devices in the midi deck
 */
class _MidiInterface
    : public lutil::StateDriver<_MidiInterface>
    , public _AbstractMidiInterface
{
public:
    explicit _MidiInterface(uint8_t id, MidiCommander *command);
    uint8_t interface_id() const override;

    // -- Transitions

    bool initialize();

    // Check to see if we are on deck to be added to the
    // network.
    bool ready_to_connect();
    bool connect_complete();
    bool runtime_engage();

    // -- Runtimes

    void connect();

    // Each device will have it's own runtime, but needs to
    // call this to clear any pooled events
    virtual void runtime();

protected:
    void queue_event(const RawEvent &event);

    template<typename T>
    void queue_event(const T &event)
    {
        RawEvent *re = rawevent_cast<T>(const_cast<T*>(&event));
        queue_event(*re);
    }

    // Protected access for ease of use
    MidiCommander *_command = nullptr;

private:
    void flush();

    int8_t _interface_id;
};

} // namespace mk