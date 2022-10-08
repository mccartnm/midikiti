from PySide6 import QtWidgets, QtCore, QtGui
Qt = QtCore.Qt

import struct
import serial
import threading
import serial.tools.list_ports

import mk
from protocol import Command, MidiKitiProtocol
from interfaces import (
    MidiLayout,
    MidiInterface,
    MidiCommander
)

class PreferenceValue(QtWidgets.QWidget):
    def __init__(self, name, parm_type, interface):
        super().__init__()
        self._name = name
        self._parm_type = parm_type
        self._interface = interface

        layout = QtWidgets.QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)

        if isinstance(self._parm_type, mk.uint8_t):
            self._widget = QtWidgets.QSpinBox()
            self._widget.setRange(0, 127)
            self._widget.valueChanged.connect(
                self._push_value
            )
            
        elif isinstance(self._parm_type, mk.uint16_t):
            self._widget = QtWidgets.QSpinBox()
            self._widget.setRange(0, 65535)
            self._widget.valueChanged.connect(
                self._push_value
            )

        elif isinstance(self._parm_type, mk.bool_):
            self._widget = QtWidgets.QCheckBox()
            self._widget.stateChanged.connect(
                self._push_value
            )
        
        elif isinstance(self._parm_type, mk.float_):
            self._widget = QtWidgets.QDoubleSpinBox()
            self._widget.setRange(-65535, 65535)
            self._widget.valueChanged.connect(
                self._push_value
            )

        else:
            return # ??

        layout.addWidget(self._widget)
        self.setLayout(layout)

    @property
    def parameters(self):
        return self._interface.parameters

    def _push_value(self, *args, **kwargs):
        value = None

        spins = (mk.uint8_t, mk.uint16_t, mk.float_)
        if isinstance(self._parm_type, spins):
            value = self._widget.value()
            
        elif isinstance(self._parm_type, mk.bool_):
            value = self._widget.isChecked()

        else:
            return # ??
        
        setattr(
            self.parameters,
            self._name,
            value
        )

    def poll_value(self):
        self._widget.blockSignals(True)

        value = getattr(self.parameters, self._name)

        spins = (mk.uint8_t, mk.uint16_t, mk.float_)
        if isinstance(self._parm_type, spins):
            self._widget.setValue(value)

        elif isinstance(self._parm_type, mk.bool_):
            self._widget.setChecked(bool(value))

        self._widget.blockSignals(False)


class MidiPreferencesWidget(QtWidgets.QWidget):

    def __init__(self, interface, win, parent=None):
        super().__init__(parent)
        self._add_index = 0
        self._win = win
        self.setSizePolicy(
            QtWidgets.QSizePolicy.MinimumExpanding,
            QtWidgets.QSizePolicy.Maximum
        )
        
        self._widgets = {}
        self._loaded = False
        self._interface = interface
        self._interface.preferencesLoaded.connect(
            self._fill_in_preferences
        )

        layout = QtWidgets.QHBoxLayout()

        self._icon = QtWidgets.QLabel()
        self._icon.setPixmap(mk.icon(interface.type).pixmap(128, 128))

        self._label = QtWidgets.QLabel(
            f'{mk.TypeNames[interface.type]}\n{interface.command.index} | {interface.index}'
        )

        f = self._label.setStyleSheet("font-size: 18px;")

        self._label.setAlignment(Qt.AlignCenter)

        layout.addWidget(self._icon)
        layout.addWidget(self._label)
        
        layout.addStretch()

        # Holds the preference value widgets
        self._forms = []

        def _make_form():
            f = QtWidgets.QFormLayout()
            f.setLabelAlignment(
                Qt.AlignRight | Qt.AlignVCenter
            )
            self._forms.append(f)
            layout.addLayout(f)

        _make_form()
        layout.addSpacing(10)
        _make_form()

        buttons = QtWidgets.QVBoxLayout()
        buttons.addStretch()

        layout.addSpacing(50)

        self._save = QtWidgets.QPushButton("Save")
        self._save.clicked.connect(self._update_preferences)
        buttons.addWidget(self._save)

        self._reset = QtWidgets.QPushButton("Reset")
        self._reset.clicked.connect(self._refresh_preferences)
        buttons.addWidget(self._reset)
        
        buttons.addStretch()
        layout.addLayout(buttons)
        
        layout.addStretch()

        self.setLayout(layout)

    
    @property
    def interface(self):
        return self._interface


    def paintEvent(self, event):
        """
        Give ourselves a simple background
        """
        painter = QtGui.QPainter(self)

        col = QtGui.QColor(42, 42, 42)
        painter.setPen(col.lighter(160))
        painter.setBrush(col)
        painter.drawRoundedRect(
            self.rect().adjusted(2, 2, -2, -2),
            5, 5
        )


    def _fill_in_preferences(self):
        """
        The underlying interface should have the preferences
        and we can query the class for it's attributes,
        generating the widgets
        """
        for name, parm_type in self.interface.parameter_types:
            if name not in self._widgets:
                pref = PreferenceValue(
                    name,
                    parm_type,
                    self.interface
                )
                self._widgets[name] = pref

                self._forms[self._add_index % 2].addRow(
                    parm_type.verbose or name.replace('_', ' ').title(),
                    pref
                )
                self._add_index += 1

            self._widgets[name].poll_value()

        self._loaded = True


    def _update_preferences(self):
        """
        Request the preferences be send to the controller for
        this interface.
        """
        if not self._loaded:
            return # Nothing to send yet...
        
        payload = self.interface.parameter_payload()
        self._win.request.emit(
            mk.SetPreferences,
            payload
        )

    def _refresh_preferences(self):
        """
        Poll the preference from our device
        """
        if not self._loaded:
            return

        self._win.request.emit(
            mk.GetPreferences,
            self.interface.header()
        )


class ManagerWindow(QtWidgets.QMainWindow):
    """
    The primary interface for augmenting a mk::MidiController and any
    interfaces connected to it.
    """
    request = QtCore.Signal(int, bytes)

    def __init__(self):
        super().__init__()
        self.setWindowIcon(mk.icon(mk.OCTAVE_ID))
        self.setWindowTitle("MidiKit Control Surface")

        # -- Variables

        self._controller = None      # type: serial.Serial
        self._protocol = None        # type: MidiKitiProtocol
        self._protocol_thread = None # type: QtCore.QThread
        self._protocol_lock = threading.Lock()

        self._interface_widgets = []

        # -- Interface Setup

        w = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout()
        header = QtWidgets.QHBoxLayout()

        self._com_select = QtWidgets.QComboBox()
        header.addWidget(self._com_select)
        header.addStretch()

        self._com_select.setMinimumWidth(250)

        layout.addLayout(header)

        self._scroll = QtWidgets.QScrollArea()
        self._scroll.setSizePolicy(
            QtWidgets.QSizePolicy.MinimumExpanding,
            QtWidgets.QSizePolicy.MinimumExpanding
        )
        self._scroll.setWidgetResizable(True)

        self._internal_widget = QtWidgets.QWidget()
        self._internal_widget.setProperty("darkbg", "yes")
        self._internal_widget.setStyleSheet("""
            QWidget[darkbg="yes"] {
                background-color: rgb(15, 15, 15)
            }
        """)
        
        self._layout_area = QtWidgets.QVBoxLayout()
        self._layout_area.addStretch()
        self._internal_widget.setLayout(self._layout_area)

        self._scroll.setWidget(self._internal_widget)

        layout.addWidget(self._scroll)
        w.setLayout(layout)

        self.setCentralWidget(w)

        # Fetch the list of ports
        ports = serial.tools.list_ports.comports()
        for port_info in ports:
            self._com_select.addItem(
                port_info.name,
                port_info
            )

        self._com_select.setCurrentIndex(-1)
        self._com_select.currentIndexChanged[int].connect(
            self._com_selected
        )

        QtWidgets.QApplication.instance().aboutToQuit.connect(
            self._cleanup
        )
        

    def sizeHint(self):
        return QtCore.QSize(
            1280, 720
        )

    @QtCore.Slot(int)
    def _com_selected(self, index):
        port_info = self._com_select.itemData(index)

        if not port_info:
            return

        if self._controller:
            with self._protocol_lock:
                self._controller.close()
            self._controller = None

            # Cleanup any existing objects
            self._protocol_thread.quit()
            self._protocol.deleteLater()
            self._protocol_thread.deleteLater()

            self._protocol_thread = None
            self._protocol = None

        try:
            self._controller = serial.Serial(
                port=port_info.name,
                baudrate=9600,
                timeout=.1
            )
        except Exception as err:
            print (str(err))
            return

        while self._controller.in_waiting:
            # Flush out anything in the queue
            self._controller.read()

        # synchronously query the layout of the controller
        self._midi_layout = MidiLayout.initialize(
            self._controller
        )

        for commander in self._midi_layout.commanders:
            for interface in commander.interfaces:
                w = MidiPreferencesWidget(interface, self)
                self._layout_area.insertWidget(0, w)
                self._interface_widgets.append(w)

        # Spin up the Protocol and thread. Start your listening
        # engines!
        self._protocol = MidiKitiProtocol(
            self._controller,
            self._protocol_lock
        )
        self._protocol_thread = QtCore.QThread()
        self._protocol.moveToThread(self._protocol_thread)
        self._protocol_thread.started.connect(
            self._protocol.execute,
        )

        # Controller -> This
        self._protocol.received.connect(
            self._midi_layout.process_command,
            Qt.QueuedConnection
        )

        # This -> Controller
        self.request.connect(
            self._protocol.request,
            Qt.QueuedConnection
        )

        self._protocol_thread.start()

        # Request each interface's parameters
        for widget in self._interface_widgets:
            interface = widget.interface

            payload = interface.header()
            self.request.emit(
                mk.GetPreferences,
                payload
            )


    @QtCore.Slot()
    def _cleanup(self):
        """
        Make sure we shut things down nicely
        """
        with self._protocol_lock:
            if self._controller and self._controller.is_open:
                self._controller.close()
        
        if self._protocol_thread:
            self._protocol_thread.quit()

            # Wait up to 1s
            self._protocol_thread.wait(1000)
