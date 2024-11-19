#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define ADDRESS     "tcp://192.168.0.108:1883"  // Vervang dit door het adres van je broker
#define CLIENTID    "Flandrien"
#define TOPIC       "vizo/err"
#define QOS         1
#define TIMEOUT     10000L

#define ERR_CODE_LEN    8
#define ERR_TEXT_LEN    200

struct tbl {
    char ErrCode[ERR_CODE_LEN];
    char Err_Text[ERR_TEXT_LEN];
    struct tbl *next;
};

struct tbl *head = NULL;

volatile MQTTClient_deliveryToken delivered_token;

// Functie om de eerste rij in te voegen
int insert_first(const char *err_code, const char *err_text) {
    struct tbl *new_node = (struct tbl*)malloc(sizeof(struct tbl));
    if (new_node == NULL) {
        return -1;
    }
    strncpy(new_node->ErrCode, err_code, ERR_CODE_LEN);
    strncpy(new_node->Err_Text, err_text, ERR_TEXT_LEN);
    new_node->next = head;
    head = new_node;
    return 0;
}

// Functie om een rij na een gegeven rij in te voegen
int insert_next(struct tbl *list, const char *err_code, const char *err_text) {
    struct tbl *new_node = (struct tbl*)malloc(sizeof(struct tbl));
    if (new_node == NULL) {
        return -1;
    }
    strncpy(new_node->ErrCode, err_code, ERR_CODE_LEN);
    strncpy(new_node->Err_Text, err_text, ERR_TEXT_LEN);
    new_node->next = list->next;
    list->next = new_node;
    return 0;
}

// Functie om de lijst af te drukken
void print_list() {
    struct tbl *p = head;
    printf("+------------+----------------------------------------------------+\n");
    printf("| %-10s | %-50s |\n", "ErrCode", "Err_Text");
    printf("+------------+----------------------------------------------------+\n");
    while (p != NULL) {
        printf("| %-10s | %-50s\n", p->ErrCode, p->Err_Text);
        p = p->next;
    }
    printf("+------------+----------------------------------------------------+\n");
    printf("End of error list.\n");
}

// Functie om te zoeken naar een foutcode in de lijst
int search_list(struct tbl **list, const char *zoekterm) {
    struct tbl *temp = head;
    while (temp != NULL) {
        if (strcasecmp(temp->ErrCode, zoekterm) == 0) {
            *list = temp;
            return 1;
        }
        temp = temp->next;
    }
    return 0;
}

// Callback voor ontvangen berichten
int messageArrivedHandler(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    printf("Message arrived on topic: %s\n", topicName);

    // Zorg ervoor dat de payload correct wordt gekopieerd en geprint
    char* payload_buffer = (char*)malloc(message->payloadlen + 1);
    if (payload_buffer) {
        memcpy(payload_buffer, message->payload, message->payloadlen);
        payload_buffer[message->payloadlen] = '\0';  // Voeg de null-terminator toe
        printf("Message: %s\n", payload_buffer);

        // Extract the error code (3rd value) from the payload
        char *token = strtok(payload_buffer, ";");  // First value
        token = strtok(NULL, ";");  // Second value
        token = strtok(NULL, ";");  // Third value (error code)
        
        if (token) {
            // Search for the error code in the list
            struct tbl *found = NULL;
            if (search_list(&found, token)) {
                printf("Error Code: %s, Error Text: %s\n", token, found->Err_Text);
            } else {
                printf("Error Code: %s not found in the list.\n", token);
            }
        } else {
            printf("Error: Could not extract error code from message.\n");
        }

        free(payload_buffer);
    } else {
        printf("Memory allocation error\n");
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;  // Zorg ervoor dat de juiste waarde wordt geretourneerd
}

int main(int argc, char *argv[]) {
    // MQTT Client setup
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Set callback to handle incoming messages
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrivedHandler, NULL);

    // Connect to the broker
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }
    printf("Connected to the broker.\n");

    // Subscribe to the topic
    MQTTClient_subscribe(client, TOPIC, QOS);

    // Read error messages from file and insert into list
    const char *filename = (argc == 2) ? argv[1] : "Error_msg_EN.txt";
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Kan het bestand niet openen: %s\n", filename);
        return 1;
    }

    char line[256];
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        // Als een regel niet begint met een #, scan de velden gescheiden door tabs
        if (line[0] != '#') {
            char *err_code = strtok(line, "\t");
            char *err_text = strtok(NULL, "\t");

            if (head == NULL) {
                if (insert_first(err_code, err_text) == -1) {
                    fclose(file);
                    return -1;
                }
            } else {
                struct tbl *current = head;
                while (current->next != NULL) {
                    current = current->next;
                }
                if (insert_next(current, err_code, err_text) == -1) {
                    fclose(file);
                    return -1;
                }
            }
        }
    }

    fclose(file);

    // Print the list
    print_list();

    // Keep the main function running to listen for messages
    while (1) {
        // Infinite loop to keep receiving messages
        // Could add some other conditions to exit if needed
    }

    // Disconnect and clean up
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return rc;
}
