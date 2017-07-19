SOURCES += \
    base64.cpp \
    sample.cpp

HEADERS += \
    base64.h

OTHER_FILES += \
    curl/lib/libcurl.a \
    json-cpp/lib/libjson.a

unix:!macx: LIBS += -L$$PWD/curl/lib/ -lcurl

INCLUDEPATH += $$PWD/curl/include/curl
DEPENDPATH += $$PWD/curl/include/curl

unix:!macx: PRE_TARGETDEPS += $$PWD/curl/lib/libcurl.a

unix:!macx: LIBS += -L$$PWD/json-cpp/lib/ -ljson

INCLUDEPATH += $$PWD/json-cpp/include
DEPENDPATH += $$PWD/json-cpp/include

unix:!macx: PRE_TARGETDEPS += $$PWD/json-cpp/lib/libjson.a
