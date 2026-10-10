FROM alpine:latest

RUN apk add --no-cache git make g++ mosquitto-clients

WORKDIR /app
RUN git clone https://github.com/bxparks/EpoxyDuino.git

WORKDIR /app/libraries
RUN git clone https://github.com/bblanchon/ArduinoJson.git

WORKDIR /app/MyProject
COPY . /app/MyProject

RUN find . -maxdepth 1 -name "*.ino" ! -name "simulation.ino" -delete

RUN echo "APP_NAME = simulation" > Makefile && \
    echo "ARDUINO_LIBS =" >> Makefile && \
    echo "EXTRA_CXXFLAGS = -I../libraries/ArduinoJson/src" >> Makefile && \
    echo "include ../EpoxyDuino/EpoxyDuino.mk" >> Makefile && \
    make

CMD ["./simulation.out"]
