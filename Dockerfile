FROM fedora:43

RUN dnf install -y \
    clang \
    clang-tools-extra \
    gcc \
    gcc-c++ \
    cmake \
    make \
    bear \
    curl \
    wget \
    unzip \
    pkgconf-pkg-config \
    git \
    zsh \
    neovim \
    apr-devel \
    libuv-devel \
    valgrind \
    gdb \
    ripgrep \
    fd-find \
    python3 \
    python3-pip \
    SDL3-devel \
    SDL3_image-devel \
    SDL3_ttf-devel \
    ffmpeg-free-devel \
    wayland-devel \
    wayland-protocols-devel \
    mesa-libEGL-devel \
    mesa-libGL-devel \
    libxkbcommon-devel \
    && dnf clean all

RUN pip3 install --user pynvim

COPY deps/cglm.sh /opt/cglm.sh
RUN sh -c "/opt/cglm.sh"

# ---------- Create a non-root dev user ----------
RUN useradd -m -s /usr/bin/zsh dev

USER dev
WORKDIR /home/dev

# ---------- Install oh-my-zsh ----------
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# ---------- Set default theme to frontcube ----------
RUN sed -i 's/^ZSH_THEME=".*"/ZSH_THEME="frontcube"/' ~/.zshrc

# ---------- Set default shell ----------
CMD ["/usr/bin/zsh", "-c"]

# ---------- Auto install AstroNvim using template ----------
RUN git clone --depth 1 https://github.com/AstroNvim/template ~/.config/nvim && \
    nvim --headless \
        "+MasonInstall clangd" \
        "+LspInstall clangd" \
        "+quitall"

# ---------- Optional: minimal .zshrc tweaks ----------
RUN echo 'export PATH=$HOME/bin:$PATH' >> /home/dev/.zshrc
RUN echo 'export PREFIX=/work/root' >> /home/dev/.zshrc
RUN echo 'export PATH="$PREFIX/bin:$PREFIX/sbin:$PATH"' >> /home/dev/.zshrc
RUN echo 'export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib64:$LD_LIBRARY_PATH"' >> /home/dev/.zshrc
RUN echo 'export CPATH="$PREFIX/include:$CPATH"' >> /home/dev/.zshrc
RUN echo 'export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig:$PREFIX/share/pkgconfig:$PKG_CONFIG_PATH"' >> /home/dev/.zshrc
RUN echo 'export CMAKE_PREFIX_PATH="$PREFIX:$CMAKE_PREFIX_PATH"' >> /home/dev/.zshrc
RUN echo 'export ACLOCAL_PATH="$PREFIX/share/aclocal:$ACLOCAL_PATH"' >> /home/dev/.zshrc
RUN echo 'export LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib64:$LIBRARY_PATH"' >> /home/dev/.zshrc
# -------------------------------------

# ---------- Default CMD ----------
CMD ["/usr/bin/zsh"]
