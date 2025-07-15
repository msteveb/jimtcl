ARG JIMTCL_DIR=/root/jimtcl
ARG CONFIGURE_ARGS=""


FROM alpine AS build
ARG JIMTCL_DIR
ARG CONFIGURE_ARGS

RUN apk add --update build-base

WORKDIR "$JIMTCL_DIR"
COPY . .
RUN ./configure $CONFIGURE_ARGS
RUN make


FROM alpine
ARG JIMTCL_DIR

RUN apk add --update make

COPY --from=build "$JIMTCL_DIR" "$JIMTCL_DIR"
RUN cd $JIMTCL_DIR && make install
RUN rm -rf "$JIMTCL_DIR"

ENTRYPOINT ["jimsh"]
