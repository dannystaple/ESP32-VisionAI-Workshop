# ESP-IDF v5.4.1 dev container
# Base: espressif/idf:v5.4.1 (Ubuntu 22.04 — Debian-based)
FROM espressif/idf:v5.4.1

# Install extra utilities used by the workshop tooling:
#   xxd        — model_data.cc generation (fetch_model.sh)
#   wget       — fetch_model.sh downloads
#   python3-venv, python3-pip — safety net if venv needs rebuilding
#   usbutils   — lsusb for board enumeration
#   git        — repo operations inside container
RUN apt-get update && apt-get install -y --no-install-recommends \
        xxd \
        wget \
        curl \
        grep \
        ccache \
        usbutils \
        python3-venv \
        python3-pip \
        git \
    && rm -rf /var/lib/apt/lists/*

# Make idf.py available without sourcing export.sh manually in every shell.
# The official image sets up /opt/esp/idf; we add the export shim here so
# non-interactive shells (e.g. VS Code tasks) also pick up the toolchain.
SHELL ["/bin/bash", "-c"]
RUN echo 'source /opt/esp/idf/export.sh > /dev/null 2>&1' >> /etc/bash.bashrc

WORKDIR /workspaces
