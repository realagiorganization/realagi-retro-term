FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    git \
    libegl1 \
    libgl1-mesa-dri \
    libqt6sql6-sqlite \
    libxcb-cursor0 \
    libxkbcommon-x11-0 \
    libxtst6 \
    mc \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    qml6-module-qtmultimedia \
    qml6-module-qt5compat-graphicaleffects \
    qml6-module-qtqml \
    qml6-module-qtqml-models \
    qml6-module-qtqml-workerscript \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-dialogs \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-localstorage \
    qml6-module-qtquick-templates \
    qml6-module-qtquick-window \
    qmake6 \
    qt6-5compat-dev \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-declarative-dev \
    qt6-multimedia-dev \
    qt6-shadertools-dev \
    qt6-svg-dev \
    scrot \
    tmux \
    x11-utils \
    xvfb \
    && rm -rf /var/lib/apt/lists/*
