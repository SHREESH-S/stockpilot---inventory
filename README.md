# StockPilot — Inventory Management System (C + Linked Lists + Queue)

## Features
- Professional responsive dashboard
- Product create/edit/delete, search and sort
- Stock-in and stock-out with validation
- Automatic low-stock alerts
- FIFO order queue (linked-list implementation)
- Dashboard KPIs and inventory value
- CSV persistence for products
- Built with C backend + HTML/CSS/JavaScript frontend

## Requirements
- A C compiler (GCC/MinGW-w64)
- Windows: install MSYS2 and install GCC (`pacman -S --needed mingw-w64-ucrt-x86_64-gcc`) then use the UCRT64 terminal.
- macOS/Linux: GCC or Clang.

## Run on Windows (MSYS2 UCRT64 terminal)
1. Extract this ZIP to a folder, for example `C:\inventory_capstone`.
2. Open the MSYS2 UCRT64 terminal and `cd` into the extracted folder.
3. Compile:
   `gcc server.c -o inventory_server.exe -lws2_32`
4. Run:
   `./inventory_server.exe`
5. Open `http://localhost:8088` in your browser.

If you use PowerShell, you can compile with a MinGW-w64 GCC installation:
`gcc server.c -o inventory_server.exe -lws2_32`
Then run `.\inventory_server.exe`.

## Run on Linux
From this folder:
`gcc server.c -o inventory_server`
`./inventory_server`
Open `http://localhost:8088`.

## Run on macOS
`clang server.c -o inventory_server`
`./inventory_server`
Open `http://localhost:8088`.

## Important
- Keep `server.c`, `index.html`, `style.css`, and `app.js` in the same folder.
- Keep the server terminal open while using the website.
- Product data is saved in `products.csv` beside the server. Back up this file before deleting/resetting data.
- This is an educational capstone starter, not production-secure software. Do not expose it to the public internet without authentication, security hardening, and proper database design.
- The backend uses a singly linked list for products and a linked-list FIFO queue for orders. Search is linear; sorting is bubble sort.
