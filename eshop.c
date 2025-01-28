// common.h - Κοινές σταθερές και δομές δεδομένων
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define MAX_PRODUCTS 20
#define MAX_CLIENTS 5
#define MAX_ORDERS_PER_CLIENT 10 
#define PORT 8080

typedef struct {
    char description[50]; // Περιγραφή του προϊόντος, π.χ. "Laptop" ή "Smartphone"
    float price;          // Τιμή του προϊόντος σε ευρώ
    int item_count;       // Διαθέσιμη ποσότητα του προϊόντος στο κατάστημα
} Product;

typedef struct {
    int product_id;       // ID του προϊόντος που ζητήθηκε
    int quantity;         // Ποσότητα που ζητήθηκε για το συγκεκριμένο προϊόν
} Order;

typedef struct {
    int client_id;        // Αναγνωριστικό του πελάτη που κάνει την παραγγελία
    Order orders[MAX_ORDERS_PER_CLIENT]; // Λίστα με τις παραγγελίες του πελάτη
} ClientRequest;

typedef struct {
    int success;          // Σημειώνει αν η παραγγελία ήταν επιτυχής (1) ή αποτυχημένη (0)
    char message[100];    // Μήνυμα προς τον πελάτη σχετικά με την κατάσταση της παραγγελίας
    float total_price;    // Συνολικό κόστος της επιτυχημένης παραγγελίας
} ServerResponse;
// Τα δεδομένα που αποστέλλονται μέσω του socket είναι  δομές τύπου `ClientRequest` (από τον client)
// και `ServerResponse` (από τον server). Αυτές περιλαμβάνουν πληροφορίες όπως τα προϊόντα, ποσότητες, μηνύματα και τιμές.

#include "common.h"

Product catalog[MAX_PRODUCTS];
int total_orders = 0, successful_orders = 0, failed_orders = 0;
float total_revenue = 0;
pthread_mutex_t mutex;

void initialize_catalog() {
 
    // Αρχικοποίηση του καταλόγου προϊόντων.
    // Χρησιμοποιούμε 2 τεμάχια ανά προϊόν 
  // Οι τιμές των προϊόντων είναι τυχαίες
    for (int i = 0; i < MAX_PRODUCTS; i++) {
        snprintf(catalog[i].description, 50, "Product %d", i + 1);
        catalog[i].price = (rand() % 1000) / 10.0; // Τυχαία τιμή μεταξύ 0 και 99.9
        catalog[i].item_count = 2; // Αρχικό πλήθος τεμαχίων
    }
}

void process_order(int client_socket, ClientRequest *request) {
    ServerResponse response;
    memset(&response, 0, sizeof(response)); // Χρησιμοποιούμε `memset` για να καθαρίσουμε τις προηγούμενες τιμές της μνήμης
                                           
    response.total_price = 0;

    for (int i = 0; i < MAX_ORDERS_PER_CLIENT; i++) {
        int product_id = request->orders[i].product_id;
        int quantity = request->orders[i].quantity;

        if (product_id < 0 || product_id >= MAX_PRODUCTS || quantity <= 0) {
            continue;
        }
       
        if (catalog[product_id].item_count >= quantity) {
            catalog[product_id].item_count -= quantity;
            response.total_price += catalog[product_id].price * quantity;
            response.success = 1;
        } else {
            snprintf(response.message, 100, "Product %d is out of stock.\n", product_id + 1);
            response.success = 0;
            failed_orders++;
        }
        

        if (response.success) {
            successful_orders++;
        }
        total_orders++;
    }

    // Εδώ αποστέλλουμε τη δομή `response` στον πελάτη.
    // Η δομή περιλαμβάνει πληροφορίες όπως την επιτυχία της παραγγελίας, το μήνυμα για την κατάσταση
    // της παραγγελίας, καθώς και το συνολικό κόστος της παραγγελίας.
    write(client_socket, &response, sizeof(response));
    sleep(1); // Αναμονή 1 δευτερολέπτου για τη διεκπεραίωση (εξομοίωση επεξεργασίας παραγγελίας σε πραγματικό σενάριο)
  // Το `sleep(1)` προσθέτει μια καθυστέρηση ενός δευτερολέπτου για να εξομοιώσει την καθυστέρηση
// επεξεργασίας μιας παραγγελίας 

}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    ClientRequest request;
    while (read(client_socket, &request, sizeof(request)) > 0) {
        process_order(client_socket, &request);
    }

    // Εδώ κλείνουμε το socket του πελάτη για να εξασφαλίσουμε ότι η σύνδεση απελευθερώνεται και
    // οι πόροι διαχειρίζονται σωστά. 
    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    srand(time(NULL));
    initialize_catalog();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_socket;
      
    }
  
    close(server_socket);
    return 0;
}

// client.c - Υλοποίηση του client
#include "common.h"

void send_orders(int client_socket, int client_id) {
    ClientRequest request;
    memset(&request, 0, sizeof(request));
    request.client_id = client_id;

    for (int i = 0; i < MAX_ORDERS_PER_CLIENT; i++) {
        request.orders[i].product_id = rand() % MAX_PRODUCTS;
        request.orders[i].quantity = 1 + (rand() % 2);

        // Εδώ αποστέλλουμε τη δομή `request` στον server μέσω του socket.
        // Η δομή περιέχει το ID του πελάτη και τις παραγγελίες που θέλει να υποβάλει.
        write(client_socket, &request, sizeof(request));

        ServerResponse response;
        // Διαβάζουμε τη δομή `response` από τον server μέσω του socket.
        // Περιέχει πληροφορίες για την επιτυχία ή αποτυχία της παραγγελίας και το συνολικό κόστος.
        read(client_socket, &response, sizeof(response));

        printf("Client %d - %sTotal Price: %.2f\n", client_id, response.message, response.total_price);
        sleep(1);
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    int client_id = getpid();
    send_orders
