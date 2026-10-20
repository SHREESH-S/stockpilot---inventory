/*
 StockPilot Inventory Management System
 Educational C HTTP server: linked list for products, linked-list FIFO queue for orders.
 Build Windows (MinGW): gcc server.c -o inventory_server.exe -lws2_32
 Build Linux/macOS: gcc server.c -o inventory_server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSESOCKET closesocket
  #define SOCKERR SOCKET_ERROR
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  typedef int socket_t;
  #define CLOSESOCKET close
  #define SOCKERR -1
#endif

#define DEFAULT_PORT 8088
#define MAX_REQ 65536
#define MAX_PRODUCTS 10000
#define MAX_ORDERS 10000
#ifdef _WIN32
  #define strcasecmp _stricmp
#endif

typedef struct Product {
    int id;
    char name[100];
    char category[60];
    double price;
    int quantity;
    int threshold;
    struct Product *next;
} Product;

typedef struct Order {
    int orderId;
    int productId;
    int quantity;
    char productName[100];
    struct Order *next;
} Order;

static Product *products = NULL;
static Order *orderFront = NULL, *orderRear = NULL;
static int nextOrderId = 1001;

static void clean_text(char *s) {
    for (int i=0; s[i]; i++) {
        if (s[i] == ',' || s[i] == '\n' || s[i] == '\r' || s[i] == '"') s[i] = ' ';
    }
}
static Product *find_product(int id) {
    Product *p=products;
    while (p) { if (p->id==id) return p; p=p->next; }
    return NULL;
}
static int product_count(void) {
    int n=0; Product *p=products; while(p){n++;p=p->next;} return n;
}
static void save_products(void) {
    FILE *f=fopen("products.csv","w");
    if(!f){perror("Cannot save products.csv");return;}
    fprintf(f,"id,name,category,price,quantity,threshold\n");
    Product *p=products;
    while(p){fprintf(f,"%d,%s,%s,%.2f,%d,%d\n",p->id,p->name,p->category,p->price,p->quantity,p->threshold);p=p->next;}
    fclose(f);
}
static void load_products(void) {
    FILE *f=fopen("products.csv","r");
    if(!f) return;
    char line[512]; fgets(line,sizeof(line),f);
    while(fgets(line,sizeof(line),f)) {
        Product *p=(Product*)calloc(1,sizeof(Product));
        if(!p) break;
        char *tok=strtok(line,",");
        if(!tok){free(p);continue;} p->id=atoi(tok);
        tok=strtok(NULL,","); if(tok) snprintf(p->name,sizeof(p->name),"%s",tok);
        tok=strtok(NULL,","); if(tok) snprintf(p->category,sizeof(p->category),"%s",tok);
        tok=strtok(NULL,","); if(tok) p->price=atof(tok);
        tok=strtok(NULL,","); if(tok) p->quantity=atoi(tok);
        tok=strtok(NULL,","); if(tok) p->threshold=atoi(tok);
        p->next=products; products=p;
    }
    fclose(f);
}
static void json_escape(const char *src, char *dst, size_t cap) {
    size_t j=0;
    for(size_t i=0;src[i] && j+2<cap;i++){
        if(src[i]=='"' || src[i]=='\\') dst[j++]='\\';
        if((unsigned char)src[i]>=32) dst[j++]=src[i];
    }
    dst[j]='\0';
}
static void products_json(char *out, size_t cap, int sortMode) {
    Product *arr[MAX_PRODUCTS]; int n=0;
    for(Product *p=products;p && n<MAX_PRODUCTS;p=p->next) arr[n++]=p;
    /* Bubble sort: 0=ID, 1=name, 2=quantity */
    for(int i=0;i<n;i++) for(int j=0;j<n-i-1;j++){
        int swap=0;
        if(sortMode==1) swap=strcasecmp(arr[j]->name,arr[j+1]->name)>0;
        else if(sortMode==2) swap=arr[j]->quantity>arr[j+1]->quantity;
        else swap=arr[j]->id>arr[j+1]->id;
        if(swap){Product *t=arr[j];arr[j]=arr[j+1];arr[j+1]=t;}
    }
    size_t used=0;
    used+=(size_t)snprintf(out+used,cap-used,"[");
    for(int i=0;i<n && used<cap;i++){
        char name[220],cat[140]; json_escape(arr[i]->name,name,sizeof(name));json_escape(arr[i]->category,cat,sizeof(cat));
        used+=(size_t)snprintf(out+used,cap-used,
          "%s{\"id\":%d,\"name\":\"%s\",\"category\":\"%s\",\"price\":%.2f,\"quantity\":%d,\"threshold\":%d}",
          i?",":"",arr[i]->id,name,cat,arr[i]->price,arr[i]->quantity,arr[i]->threshold);
    }
    if(used<cap) snprintf(out+used,cap-used,"]");
}
static void orders_json(char *out,size_t cap) {
    size_t used=(size_t)snprintf(out,cap,"[");
    for(Order *o=orderFront;o && used<cap;o=o->next)
        used+=(size_t)snprintf(out+used,cap-used,"%s{\"orderId\":%d,\"productId\":%d,\"productName\":\"%s\",\"quantity\":%d}",
            o==orderFront?"":",",o->orderId,o->productId,o->productName,o->quantity);
    if(used<cap) snprintf(out+used,cap-used,"]");
}
static void json_string(const char *body,const char *key,char *out,size_t cap) {
    char needle[100]; snprintf(needle,sizeof(needle),"\"%s\"",key);
    const char *p=strstr(body,needle); if(!p){out[0]=0;return;}
    p=strchr(p,':'); if(!p){out[0]=0;return;} p++;
    while(*p && isspace((unsigned char)*p))p++;
    if(*p!='"'){out[0]=0;return;} p++;
    size_t j=0;
    while(*p && *p!='"' && j+1<cap){if(*p=='\\' && p[1])p++;out[j++]=*p++;}
    out[j]=0;
}
static double json_number(const char *body,const char *key,double fallback) {
    char needle[100];snprintf(needle,sizeof(needle),"\"%s\"",key);
    const char *p=strstr(body,needle);if(!p)return fallback;
    p=strchr(p,':');if(!p)return fallback;p++;
    while(*p && isspace((unsigned char)*p))p++;
    return atof(p);
}
static void send_response(socket_t client,const char *status,const char *type,const char *body) {
    char header[512];
    int len=(int)strlen(body);
    snprintf(header,sizeof(header),"HTTP/1.1 %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",status,type,len);
    send(client,header,(int)strlen(header),0); send(client,body,len,0);
}
static void send_json(socket_t c,const char *status,const char *body){send_response(c,status,"application/json",body);}
static void send_file(socket_t c,const char *path,const char *type) {
    FILE *f=fopen(path,"rb");
    if(!f){send_response(c,"404 Not Found","text/plain","File not found. Keep frontend files beside server.c.");return;}
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    char *buf=(char*)malloc((size_t)n+1); if(!buf){fclose(f);send_response(c,"500 Internal Server Error","text/plain","Out of memory");return;}
    size_t got=fread(buf,1,(size_t)n,f);buf[got]=0;fclose(f);
    send_response(c,"200 OK",type,buf);free(buf);
}
static void add_product(socket_t c,const char *body) {
    int id=(int)json_number(body,"id",-1), qty=(int)json_number(body,"quantity",-1), th=(int)json_number(body,"threshold",5);
    double price=json_number(body,"price",-1);
    char name[100],cat[60];json_string(body,"name",name,sizeof(name));json_string(body,"category",cat,sizeof(cat));
    if(id<=0 || !name[0] || qty<0 || th<0 || price<0){send_json(c,"400 Bad Request","{\"error\":\"Enter a positive product ID, name, non-negative quantity/threshold and valid price.\"}");return;}
    if(find_product(id)){send_json(c,"409 Conflict","{\"error\":\"That product ID already exists.\"}");return;}
    Product *p=(Product*)calloc(1,sizeof(Product));if(!p){send_json(c,"500 Internal Server Error","{\"error\":\"Out of memory.\"}");return;}
    p->id=id;snprintf(p->name,sizeof(p->name),"%s",name);snprintf(p->category,sizeof(p->category),"%s",cat[0]?cat:"General");clean_text(p->name);clean_text(p->category);
    p->price=price;p->quantity=qty;p->threshold=th;p->next=products;products=p;save_products();
    send_json(c,"201 Created","{\"message\":\"Product added successfully.\"}");
}
static void update_product(socket_t c,const char *body) {
    int id=(int)json_number(body,"id",-1), qty=(int)json_number(body,"quantity",-1), th=(int)json_number(body,"threshold",-1);
    double price=json_number(body,"price",-1);
    char name[100],cat[60];json_string(body,"name",name,sizeof(name));json_string(body,"category",cat,sizeof(cat));
    Product *p=find_product(id);
    if(!p){send_json(c,"404 Not Found","{\"error\":\"Product not found.\"}");return;}
    if(!name[0] || qty<0 || th<0 || price<0){send_json(c,"400 Bad Request","{\"error\":\"Check the name, price, quantity and threshold.\"}");return;}
    snprintf(p->name,sizeof(p->name),"%s",name);snprintf(p->category,sizeof(p->category),"%s",cat[0]?cat:"General");clean_text(p->name);clean_text(p->category);
    p->quantity=qty;p->threshold=th;p->price=price;save_products();
    send_json(c,"200 OK","{\"message\":\"Product updated successfully.\"}");
}
static void delete_product(socket_t c,const char *path) {
    int id=atoi(strchr(path,'=')?strchr(path,'=')+1:"0");
    Product **cur=&products;
    while(*cur && (*cur)->id!=id)cur=&(*cur)->next;
    if(!*cur){send_json(c,"404 Not Found","{\"error\":\"Product not found.\"}");return;}
    Product *dead=*cur;*cur=dead->next;free(dead);save_products();
    send_json(c,"200 OK","{\"message\":\"Product deleted.\"}");
}
static void change_stock(socket_t c,const char *body,int direction) {
    int id=(int)json_number(body,"id",-1), qty=(int)json_number(body,"quantity",-1);
    Product *p=find_product(id);
    if(!p){send_json(c,"404 Not Found","{\"error\":\"Product not found.\"}");return;}
    if(qty<=0){send_json(c,"400 Bad Request","{\"error\":\"Quantity must be greater than zero.\"}");return;}
    if(direction<0 && p->quantity<qty){send_json(c,"400 Bad Request","{\"error\":\"Not enough stock for this stock-out.\"}");return;}
    p->quantity+=direction*qty;save_products();
    send_json(c,"200 OK","{\"message\":\"Stock updated successfully.\"}");
}
static void enqueue_order(socket_t c,const char *body) {
    int id=(int)json_number(body,"productId",-1), qty=(int)json_number(body,"quantity",-1);
    Product *p=find_product(id);
    if(!p){send_json(c,"404 Not Found","{\"error\":\"Product not found.\"}");return;}
    if(qty<=0){send_json(c,"400 Bad Request","{\"error\":\"Order quantity must be greater than zero.\"}");return;}
    Order *o=(Order*)calloc(1,sizeof(Order));if(!o){send_json(c,"500 Internal Server Error","{\"error\":\"Out of memory.\"}");return;}
    o->orderId=nextOrderId++;o->productId=id;o->quantity=qty;snprintf(o->productName,sizeof(o->productName),"%s",p->name);
    if(!orderRear)orderFront=orderRear=o;else{orderRear->next=o;orderRear=o;}
    send_json(c,"201 Created","{\"message\":\"Order added to FIFO queue.\"}");
}
static void process_order(socket_t c) {
    if(!orderFront){send_json(c,"400 Bad Request","{\"error\":\"The order queue is empty.\"}");return;}
    Order *o=orderFront;Product *p=find_product(o->productId);
    if(!p){send_json(c,"400 Bad Request","{\"error\":\"The product for the first order no longer exists.\"}");return;}
    if(p->quantity<o->quantity){send_json(c,"400 Bad Request","{\"error\":\"Insufficient stock. Replenish stock before processing this order.\"}");return;}
    p->quantity-=o->quantity;save_products();orderFront=o->next;if(!orderFront)orderRear=NULL;free(o);
    send_json(c,"200 OK","{\"message\":\"Oldest order processed; stock reduced.\"}");
}
static void handle_client(socket_t client) {
    char req[MAX_REQ];int total=0,n;
    while(total<MAX_REQ-1 && (n=recv(client,req+total,MAX_REQ-1-total,0))>0) {
        total+=n;req[total]=0;
        char *body=strstr(req,"\r\n\r\n");
        if(body){body+=4;char *cl=strstr(req,"Content-Length:");int need=cl?atoi(cl+15):0;
            if(total-(int)(body-req)>=need)break;
        } else if(total>4) break;
    }
    req[total]=0;
    char method[8]="",path[256]="";sscanf(req,"%7s %255s",method,path);
    char *body=strstr(req,"\r\n\r\n");body=body?body+4:"";
    if(!strcmp(method,"GET") && (!strcmp(path,"/") || !strcmp(path,"/index.html")))send_file(client,"index.html","text/html");
    else if(!strcmp(method,"GET") && !strcmp(path,"/style.css"))send_file(client,"style.css","text/css");
    else if(!strcmp(method,"GET") && !strcmp(path,"/app.js"))send_file(client,"app.js","application/javascript");
    else if(!strcmp(method,"GET") && !strncmp(path,"/api/products",13)){
        int mode=0;char *s=strstr(path,"sort=");if(s){if(strncmp(s+5,"id",2)==0)mode=0;else if(strncmp(s+5,"name",4)==0)mode=1;else if(strncmp(s+5,"quantity",8)==0)mode=2;}
        char *out=(char*)malloc(2*1024*1024);if(out){products_json(out,2*1024*1024,mode);send_json(client,"200 OK",out);free(out);}else send_json(client,"500 Internal Server Error","{\"error\":\"Out of memory\"}");
    }
    else if(!strcmp(method,"POST") && !strcmp(path,"/api/products"))add_product(client,body);
    else if(!strcmp(method,"PUT") && !strcmp(path,"/api/products"))update_product(client,body);
    else if(!strcmp(method,"DELETE") && !strncmp(path,"/api/products?id=",17))delete_product(client,path);
    else if(!strcmp(method,"POST") && !strcmp(path,"/api/stock-in"))change_stock(client,body,1);
    else if(!strcmp(method,"POST") && !strcmp(path,"/api/stock-out"))change_stock(client,body,-1);
    else if(!strcmp(method,"GET") && !strcmp(path,"/api/orders")){char out[1024*1024];orders_json(out,sizeof(out));send_json(client,"200 OK",out);}
    else if(!strcmp(method,"POST") && !strcmp(path,"/api/orders"))enqueue_order(client,body);
    else if(!strcmp(method,"POST") && !strcmp(path,"/api/orders/process"))process_order(client);
    else if(!strcmp(method,"GET") && !strcmp(path,"/api/summary")){
        int count=0,low=0,units=0;double value=0;
        for(Product*p=products;p;p=p->next){count++;units+=p->quantity;value+=p->price*p->quantity;if(p->quantity<=p->threshold)low++;}
        char out[512];snprintf(out,sizeof(out),"{\"products\":%d,\"lowStock\":%d,\"units\":%d,\"value\":%.2f,\"pendingOrders\":%d}",count,low,units,value,0);
        /* pending count */
        int pending=0;for(Order*o=orderFront;o;o=o->next)pending++;
        snprintf(out,sizeof(out),"{\"products\":%d,\"lowStock\":%d,\"units\":%d,\"value\":%.2f,\"pendingOrders\":%d}",count,low,units,value,pending);
        send_json(client,"200 OK",out);
    }
    else if(!strcmp(method,"OPTIONS"))send_response(client,"204 No Content","text/plain","");
    else send_json(client,"404 Not Found","{\"error\":\"Route not found.\"}");
    CLOSESOCKET(client);
}
int main(void) {
    int port = DEFAULT_PORT;
    const char *port_env = getenv("PORT");
    if (port_env && *port_env) {
        int parsed = atoi(port_env);
        if (parsed > 0 && parsed <= 65535) port = parsed;
    }
#ifdef _WIN32
    WSADATA wsa; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){fprintf(stderr,"Winsock initialization failed\n");return 1;}
#endif
    load_products();
    socket_t server=socket(AF_INET,SOCK_STREAM,0);
    if(server==SOCKERR){fprintf(stderr,"Could not create server socket\n");return 1;}
    int opt=1;
#ifdef _WIN32
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
#else
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
#endif
    struct sockaddr_in addr;memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons((unsigned short)port);
    if(bind(server,(struct sockaddr*)&addr,sizeof(addr))==SOCKERR){fprintf(stderr,"Port %d is busy. Close the other server or change PORT.\n",port);CLOSESOCKET(server);return 1;}
    if(listen(server,16)==SOCKERR){fprintf(stderr,"Could not listen on port %d\n",port);CLOSESOCKET(server);return 1;}
    printf("\nStockPilot Inventory Management System\n");
    printf("Server listening on port %d\n",port);
    printf("Press Ctrl+C to stop. Product data is saved in products.csv\n\n");
    while(1){struct sockaddr_in clientAddr;
#ifdef _WIN32
        int len=sizeof(clientAddr);
#else
        socklen_t len=sizeof(clientAddr);
#endif
        socket_t client=accept(server,(struct sockaddr*)&clientAddr,&len);
        if(client==SOCKERR)continue;
        handle_client(client);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    CLOSESOCKET(server);return 0;
}
