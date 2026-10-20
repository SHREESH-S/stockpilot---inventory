FROM gcc:14-bookworm
WORKDIR /app
COPY . .
RUN gcc server.c -o inventory_server
EXPOSE 10000
CMD ["./inventory_server"]
