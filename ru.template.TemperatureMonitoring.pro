TARGET = ru.template.TemperatureMonitoring

CONFIG += \
    auroraapp

QT += sql network

SOURCES += \
    src/main.cpp \
    src/temperaturestorage.cpp \
    src/bridgeclient.cpp

HEADERS += \
    src/temperaturestorage.h \
    src/bridgeclient.h

DISTFILES += \
    rpm/ru.template.TemperatureMonitoring.spec

AURORAAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += auroraapp_i18n

TRANSLATIONS += \
    translations/ru.template.TemperatureMonitoring.ts \
    translations/ru.template.TemperatureMonitoring-ru.ts
