# Self-Hosted IBM Z Github Actions Runner.

FROM    almalinux:9

RUN     dnf update -y -q && \
        dnf install -y -q --enablerepo=crb wget git which sudo jq sed \
            cmake make automake autoconf m4 libtool ninja-build python3-pip \
            gcc gcc-c++ clang llvm-toolset glibc-all-langpacks langpacks-en \
            glibc-static libstdc++-static libstdc++-devel libxslt-devel libxml2-devel

RUN     dnf install -y -q dotnet-sdk-6.0 && \
        echo "Using SDK - `dotnet --version`"

COPY    runner-s390x.patch /tmp/runner.patch

RUN     cd /tmp && \
        git clone -q https://github.com/actions/runner && \
        cd runner && \
        git checkout v2.317.0 -b build && \
        git apply /tmp/runner.patch && \
        sed -i'' -e /version/s/6......\"$/${SDK}.0.100\"/ src/global.json

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
COPY    fs/ /
WORKDIR /home/actions-runner
ENTRYPOINT ["/usr/bin/entrypoint"]
CMD     ["/usr/bin/actions-runner"]
