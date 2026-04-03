TEMPLATE = subdirs

CONFIG += ordered

!ios:SUBDIRS += qmltermwidget
SUBDIRS += app

desktop.files += realagi-retro-term.desktop
desktop.path += /usr/share/applications

INSTALLS += desktop
