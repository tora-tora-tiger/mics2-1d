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

# Node.js のインストール (NodeSourceリポジトリ使用)
RUN curl -fsSL https://deb.nodesource.com/setup_current.x | bash - \
    && apt-get install -y nodejs

# pnpm のインストール
RUN npm install -g pnpm

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

# Codex CLI インストール
RUN npm i -g @openai/codex

# --- ここから TeX Live のインストールを追加 ---
ARG TEXLIVE_MIRROR=https://tug.ctan.org/systems/texlive/tlnet
ENV PATH=/usr/local/texlive/bin:$PATH

# 作業ディレクトリを用意し、プロファイルをコピーして TeX Live をインストール
WORKDIR /texlive-install
COPY .devcontainer/texlive.profile ./texlive.profile

RUN wget ${TEXLIVE_MIRROR}/install-tl-unx.tar.gz \
    && tar -xf install-tl-unx.tar.gz --strip-components 1 \
    && ./install-tl -profile texlive.profile --location ${TEXLIVE_MIRROR} \
    && ln -sf /usr/local/texlive/*/bin/* /usr/local/texlive/bin \
    && rm -rf /texlive-install/install-tl* /texlive-install/*

# latexmk のインストール
RUN /usr/local/texlive/bin/tlmgr install latexmk

# Ghostscript と Gnuplot のインストール（非対話）
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends ghostscript gnuplot \
    && rm -rf /var/lib/apt/lists/*

# gnuplot-lua-tikz 用のスタイルを生成して ls を更新
RUN gnuplot -e 'set term tikz createstyle' || true \
&& mktexlsr || true

WORKDIR /
# --- TeX Live インストールここまで ---
        
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