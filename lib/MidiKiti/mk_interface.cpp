#include "mk_interface.h"
#include "mk_command.h"

namespace mk {

_MidiInterface::_MidiInterface(uint8_t id, MidiCommander *command)
    : lutil::StateDriver<_MidiInterface>()
    , _command(command)
    , _interface_id(id)
{
    // Local controller Off -> Runtime
    add_transition(Off, Runtime, &_MidiInterface::initialize);

    // The virtual runtime interface
    add_runtime(Runtime, &_MidiInterface::runtime);

    _command->add(this);
}

uint8_t _MidiInterface::interface_id() const
{
    return _interface_id;
}

// -- Transitions

bool _MidiInterface::initialize()
{
    return _command->is_connected();
}

void _MidiInterface::runtime()
{
}

void _MidiInterface::queue_event(const RawEvent &event)
{
    _command->queue_event(event);
}

}
