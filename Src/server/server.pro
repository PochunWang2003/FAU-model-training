QT += core network gui
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = fau-training-server
SOURCES += main.cpp trainingserver.cpp \
    inference/ModelRuntimeRegistry.cpp \
    camera/SyntheticCameraService.cpp camera/BaslerCameraService.cpp \
    motion/SimulatedMotionFeedbackProvider.cpp motion/SimulatedMotionBackend.cpp \
    motion/UnavailableMotionBackend.cpp \
    motion/NecMotionBackend/NecMotionBackend.cpp \
    motion/GmrtMotionBackend/GmrtMotionBackend.cpp
HEADERS += trainingserver.h inference/ModelRuntimeRegistry.h \
    camera/ICameraAcquisitionService.h \
    camera/SyntheticCameraService.h camera/BaslerCameraService.h \
    motion/IMotionFeedbackProvider.h \
    motion/IMotionBackend.h motion/UnavailableMotionBackend.h \
    motion/SimulatedMotionFeedbackProvider.h motion/SimulatedMotionBackend.h \
    motion/NecMotionBackend/NecMotionBackend.h \
    motion/GmrtMotionBackend/GmrtMotionBackend.h
INCLUDEPATH += ../include
DESTDIR = $$OUT_PWD
OBJECTS_DIR = $$OUT_PWD/obj
MOC_DIR = $$OUT_PWD/moc
