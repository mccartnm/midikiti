# Command attributes
import struct

OCTAVE_ID = 0xF1
POT_ID = 0xF2
BUTTON_ID = 0xF3

KEY_EVENT_ID = 1
POT_EVENT_ID = 2
BUTTON_EVENT_ID = 2

StartFlag = 0xFF
GetLayout = 0x01
GetPreferences = 0x02
SetPreferences = 0x03
Message = 0x04

TypeNames = {
    OCTAVE_ID: 'Octave',
    POT_ID: 'Pot',
    BUTTON_ID: 'Button'
}

def request(serial, command, payload):
    """
    Submit a request to the given serial
    """
    if isinstance(payload, int):
        payload = struct.pack('B', payload)

    request = struct.pack(
        ('B' * 3) + f'{len(payload)}s',
        StartFlag,
        command,
        len(payload),
        payload
    )

    serial.write(request)


class _unit:
    key = None

    def __init__(self, verbose=None, description=None):
        self.verbose = verbose
        self.description = description or ''

class uint8_t(_unit):
    key = 'B'

class uint16_t(_unit):
    key = 'H'

class bool_(_unit):
    key = '?'

class float_(_unit):
    key = 'f'


class _Struct:
    """
    Abstract structure (un)packing device
    """
    def __init__(self, payload):
        """
        Unpack data into the variables
        """
        # hint: < means little-endian with no alignment
        data = struct.unpack('<' + self.format(), payload)

        index = 0
        mapping = {}
        for name, attr in vars(self.__class__).items():
            if isinstance(attr, _unit):
                mapping[name] = data[index]
                index += 1
        
        for var in mapping:
            setattr(self, var, mapping[var])
        
    
    @classmethod
    def format(cls):
        """
        :return: ``str`` - struct.(un)pack format
        """
        fmt = ''
        
        for name, attr in vars(cls).items():
            if isinstance(attr, _unit):
                fmt += attr.key

        return fmt


class PotParameters(_Struct):
    """
    Parameters for a pot
    """
    control = uint8_t()

    high = uint16_t()
    low = uint16_t()

    midi_high = uint8_t()
    midi_low = uint8_t()

    threshold = uint16_t()
    invert = bool_()


ParameterTypes = {
    POT_ID: PotParameters
}