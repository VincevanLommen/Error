#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "MQTTClient.h"

#define ADDRESS     "tcp://192.168.0.108:1883" // !! IP-address:Port-ID
#define QOS         1
#define CLIENTID    "FLANDRIEN"
#define SUB_TOPIC   "vizo/ERROR_REC"
#define PUB_TOPIC   "vizo/ERROR_SEND"
#define TOPIC_LEN   120
#define TIMEOUT     500L

#define ERR_OUT_LEN 1024







struct tbl {
    char ErrCode[ERR_CODE_LEN + 1]; // +1 voor null terminator
    char Err_Text[ERR_TEXT_LEN + 1]; // +1 voor null terminator
    struct tbl *next;
};

struct tbl *head = NULL;





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










// this mqtt token is set as global var to ease up this program
volatile MQTTClient_deliveryToken deliveredtoken;

// This function is called upon when a message is successfully delivered through mqtt
void delivered(void *context, MQTTClient_deliveryToken dt) {
    
    printf("Message with token value %d delivery confirmed\n", dt);
    printf( "-----------------------------------------------\n" );    
    deliveredtoken = dt;
}

// This function is called upon when an incoming message from mqtt is arrived
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *error_in = message->payload;
    char  error_out[ ERR_OUT_LEN ] = "";
    
    // print incoming message
    printf( "msgarrvd: error_in: <%s>\n", error_in );   
    
    // format error out msg
    sprintf( error_out, "%s + Some additional text here", error_in );
    printf( "msgarrvd: error_out: <%s>\n", error_out );   

    // Create a new client to publish the error_out message
    MQTTClient client = (MQTTClient)context;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = error_out;
    pubmsg.payloadlen = strlen( error_out );
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    //Publish the error_out message on PUB TOPIC 
    MQTTClient_publishMessage(client, PUB_TOPIC, &pubmsg, &token);
    printf("Publishing to topic %s\n", PUB_TOPIC);
    
    // Validate that message has been successfully delivered
    int rc = MQTTClient_waitForCompletion(client, token, TIMEOUT );
    printf("Message with delivery token %d delivered, rc=%d\n", token, rc);
    printf( "Msg out:\t<%s>\n", error_out );

    // Close the outgoing message queue
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    
    return 1;
}

// This function is called upon when the connection to the mqtt-broker is lost
void connlost(void *context, char *cause) {
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}


int main() {
   // Open MQTT client for listening
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Define the correct call back functions when messages arrive
    MQTTClient_setCallbacks(client, client, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    printf("Subscribing to topic %s for client %s using QoS%d\n\n", SUB_TOPIC, CLIENTID, QOS);
    MQTTClient_subscribe(client, SUB_TOPIC, QOS);

    // Keep the program running to continue receiving and publishing messages
    for(;;) {
        ;
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;


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
}