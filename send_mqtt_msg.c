#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include <time.h>

#define ADDRESS     "tcp://192.168.0.108:1883"  // Vervang dit door het adres van je broker
#define CLIENTID    "Flandrien"
#define TOPIC       "vizo/err"
#define QOS         1
#define TIMEOUT     10000L

#define ERR_CODE_LEN    8
#define ERR_TEXT_LEN    200

char Naar_Broker[256]; // Bericht dat wordt doorgestuurd

struct tbl {
    char ErrCode[ERR_CODE_LEN + 1]; // +1 voor null terminator
    char Err_Text[ERR_TEXT_LEN + 1]; // +1 voor null terminator
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
    new_node->ErrCode[ERR_CODE_LEN] = '\0'; // Zorg ervoor dat de string null-terminated is
    strncpy(new_node->Err_Text, err_text, ERR_TEXT_LEN);
    new_node->Err_Text[ERR_TEXT_LEN] = '\0'; // Zorg ervoor dat de string null-terminated is
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
    new_node->ErrCode[ERR_CODE_LEN] = '\0'; // Zorg ervoor dat de string null-terminated is
    strncpy(new_node->Err_Text, err_text, ERR_TEXT_LEN);
    new_node->Err_Text[ERR_TEXT_LEN] = '\0'; // Zorg ervoor dat de string null-terminated is
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

// Functie om de huidige tijd als string te krijgen
void get_current_time_str(char* buffer, size_t buffer_size) {
    time_t raw_time;
    struct tm* time_info;

    time(&raw_time);
    time_info = localtime(&raw_time);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

// Callback voor ontvangen berichten
int messageArrivedHandler(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    char* payload_buffer = (char*)malloc(message->payloadlen + 1);
    if (payload_buffer) {
        memcpy(payload_buffer, message->payload, message->payloadlen);
        payload_buffer[message->payloadlen] = '\0'; 

        char *token = strtok(payload_buffer, ";"); 
        token = strtok(NULL, ";"); 
        char *err_code = strtok(NULL, ";"); 
        char *extra_text = strtok(NULL, ";");  

        if (err_code) {
            // Search for the error code in the list
            struct tbl *found = NULL;
            if (search_list(&found, err_code)) {
                char time_str[20];
                get_current_time_str(time_str, sizeof(time_str));

                if (extra_text && strstr(found->Err_Text, "%s")) {
                    // Replace %s met extra waarden
                    char formatted_err_text[ERR_TEXT_LEN];
                    snprintf(formatted_err_text, ERR_TEXT_LEN, found->Err_Text, extra_text);

                    // \n weghalen als deze er is
                    formatted_err_text[strcspn(formatted_err_text, "\n")] = '\0';

                    snprintf(Naar_Broker, sizeof(Naar_Broker), "%s;%s;%s;%s", payload_buffer, formatted_err_text, time_str);
                    printf("%s\n", Naar_Broker);
                } else {
                    // Idem
                    found->Err_Text[strcspn(found->Err_Text, "\n")] = '\0';

                    snprintf(Naar_Broker, sizeof(Naar_Broker), "%s;%s;%s", payload_buffer, found->Err_Text, time_str);
                    printf("%s\n", Naar_Broker);
                }
            } else {
                snprintf(Naar_Broker, sizeof(Naar_Broker), "Error Code: %s not found in the list.", err_code);
                printf("%s\n", Naar_Broker);
            }
        }

        free(payload_buffer);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
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
