# Self-Hosted IBM Z Github Actions Runner.

FROM    almalinux:10

RUN     dnf update -y -q && \
        dnf install -y -q --enablerepo=crb wget git which sudo jq sed \
            cmake make automake autoconf m4 libtool ninja-build \
            python3-pip python3-devel python3-lxml \
            gcc gcc-c++ clang llvm-toolset glibc-all-langpacks langpacks-en \
            glibc-static libstdc++-static libstdc++-devel libxslt-devel libxml2-devel

RUN     dnf install -y -q dotnet-sdk-8.0 && \
        echo "Using SDK - `dotnet --version`"

RUN     cd /tmp && \
        git clone -q https://github.com/actions/runner && \
        cd runner && \
        git checkout $(git tag --sort=-v:refname | grep '^v[0-9]' | head -n1) && \
        git log -n 1 && \
        wget https://github.com/ppc64le/gaplib/raw/refs/heads/main/patches/runner-main-sdk8-s390x.patch -O runner-sdk-8.patch && \
        git apply --whitespace=nowarn runner-sdk-8.patch && \
        sed -i'' -e /version/s/8......\"$/$8.0.100\"/ src/global.json

RUN     cd /tmp/runner/src && \
        ./dev.sh layout && \
        ./dev.sh package && \
        rm -rf /root/.dotnet /root/.nuget

RUN     useradd -c "Action Runner" -m actions-runner && \
        usermod -L actions-runner

RUN     tar -xf /tmp/runner/_package/*.tar.gz -C /home/actions-runner && \
        chown -R actions-runner:actions-runner /home/actions-runner

# Cleanup
RUN     rm -rf /tmp/runner /var/cache/dnf/* /tmp/runner.patch /tmp/global.json && \
        dnf clean all

USER    actions-runner

# Scripts.
COPY    --chmod=555 entrypoint /usr/bin/
COPY    --chmod=555 actions-runner /usr/bin/
WORKDIR /home/actions-runner
ENTRYPOINT ["/usr/bin/entrypoint"]
CMD     ["/usr/bin/actions-runner"]
