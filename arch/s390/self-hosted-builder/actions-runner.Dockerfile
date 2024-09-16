# Self-Hosted IBM Z Github Actions Runner.

FROM    almalinux:9

RUN     dnf update -y -q && \
        dnf install -y -q --enablerepo=crb wget git which sudo jq sed \
            cmake make automake autoconf m4 libtool ninja-build python3-pip \
            gcc gcc-c++ clang llvm-toolset glibc-all-langpacks langpacks-en \
            glibc-static libstdc++-static libstdc++-devel libxslt-devel libxml2-devel

RUN     dnf install -y -q dotnet-sdk-8.0 && \
        echo "Using SDK - `dotnet --version`"

RUN     cd /tmp && \
        git clone -q https://github.com/actions/runner && \
        cd runner && \
        git checkout $(git describe --tags $(git rev-list --tags --max-count=1)) -b build && \
        wget https://github.com/anup-kodlekere/gaplib/raw/refs/heads/main/build-files/runner-sdk-8.patch && \
        git apply runner-sdk-8.patch && \
        sed -i'' -e /version/s/8......\"$/$8.0.100\"/ src/global.json

RUN     cd /tmp/runner/src && \
        ./dev.sh layout && \
        ./dev.sh package && \
        rm -rf /root/.dotnet /root/.nuget

RUN     useradd -c "Action Runner" -m actions-runner && \
        usermod -L actions-runner

RUN     tar -xf /tmp/runner/_package/*.tar.gz -C /home/actions-runner && \
        chown -R actions-runner:actions-runner /home/actions-runner

#VOLUME  /home/actions-runner

RUN     rm -rf /tmp/runner /var/cache/dnf/* /tmp/runner.patch /tmp/global.json && \
        dnf clean all

USER    actions-runner

# Scripts.
COPY    entrypoint /usr/bin/
COPY    actions-runner /usr/bin/
WORKDIR /home/actions-runner
ENTRYPOINT ["/usr/bin/entrypoint"]
CMD     ["/usr/bin/actions-runner"]
