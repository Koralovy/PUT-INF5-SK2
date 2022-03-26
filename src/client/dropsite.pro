QT += widgets
LIBS += -lws2_32
requires(qtConfig(tablewidget))

HEADERS = droparea.h \
          dropsitewindow.h
SOURCES = droparea.cpp \
          dropsitewindow.cpp \
          main.cpp

# install
target.path = .
INSTALLS += target
