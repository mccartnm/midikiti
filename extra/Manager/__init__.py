

if __name__ == '__main__':
    from PySide6 import QtWidgets
    from window import ManagerWindow
    from qt_material import apply_stylesheet

    app = QtWidgets.QApplication()
    app.setStyle("fusion")
    apply_stylesheet(app, theme='dark_amber.xml')

    w = ManagerWindow()
    w.show()

    app.exec()