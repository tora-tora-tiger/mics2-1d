FROM ubuntu:22.04

# 環境変数設定
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Tokyo

# ユーザー設定
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# 基本的なパッケージのインストール
RUN apt-get update && apt-get install -y \
    sudo \
    git \
    wget \
    curl \
    vim \
    htop \
    tree \
    && rm -rf /var/lib/apt/lists/*

# C++ 開発環境のインストール
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    gdb \
    cmake \
    make \
    ninja-build \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Python 開発環境のインストール
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    && rm -rf /var/lib/apt/lists/*

# pip のアップグレードと基本的なPythonパッケージのインストール
RUN python3 -m pip install --upgrade pip
#     && pip3 install --no-cache-dir \
#     numpy \
#     matplotlib \
#     pandas \
#     jupyter \
#     ipython \
#     pylint \
#     autopep8

# ユーザーの作成
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo "$USERNAME ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# 作業ディレクトリの設定
WORKDIR /workspaces

# ユーザーの切り替え
USER $USERNAME

# Python のエイリアス設定
RUN echo "alias python='python3'" >> ~/.bashrc \
    && echo "alias pip='pip3'" >> ~/.bashrc

# Claude Code をユーザーとしてインストール
RUN curl -fsSL https://claude.ai/install.sh | bash

# PATH の設定
ENV PATH="/home/${USERNAME}/.local/bin:${PATH}"

# コンテナ起動時のコマンド
CMD ["/bin/bash"]