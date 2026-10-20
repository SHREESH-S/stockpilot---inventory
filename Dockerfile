FROM gcc:14-bookworm

WORKDIR /app

COPY server.c index.html style.css app.js ./

RUN gcc -Wall -Wextra server.c -o inventory_server

ENV PORT=10000

EXPOSE 10000

CMD ["./inventory_server"]
