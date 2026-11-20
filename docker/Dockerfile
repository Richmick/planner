FROM gcc:15-trixie

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		ca-certificates cmake git libjsoncpp-dev libpq-dev libsodium-dev libsqlite3-dev \
		libssl-dev openssl pkg-config tzdata uuid-dev zlib1g-dev \
	&& rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch v1.9.14 --recurse-submodules --shallow-submodules \
		https://github.com/drogonframework/drogon.git /tmp/drogon \
	&& cmake -S /tmp/drogon -B /tmp/drogon-build -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CXX_STANDARD=20 -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
	&& cmake --build /tmp/drogon-build --target install --parallel 2 \
	&& rm -rf /tmp/drogon /tmp/drogon-build
