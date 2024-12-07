#include "main.h"

void app_main(void)
{   
    init_nvs();
    // gpio_init();
    // start_gpio_check_task();
    // start_gpio_task();
//     ethernet_init_w5500();

    esp_task_wdt_init(10, true); 
    // init_nvs();
    // sdcard_mount();
    // init_rtc();
    // setClock();
    //  esp_task_wdt_reset(); 
    //Initializing the task watchdog subsystem with an interval of 2 seconds
     esp_task_wdt_reset();   
    // xTaskCreate(app_notifications,"App logic",2048*4, NULL, 5, &taskHandle);
    // Ket noi Wifi
    init_wifi();
    esp_task_wdt_reset(); 


    // MQTT Parameters
    char *host = "20.39.193.159";
    int port = 1883;
    int keepalive = 60;
    bool clean_session = true;
    struct mosquitto *mosq = NULL;

    // MQTT Setup
    srand(time(NULL));
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);
    if (!mosq) {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    fprintf(stdout, "Setting up callbacks\n");
    mosquitto_log_callback_set(mosq, my_log_callback);
    mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);
    mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);

    // xTaskCreate(modbus_read_task, "modbus_read_task", 2048, NULL, 5, NULL); // ham doc modbus va gui gia tri len sparkpluug

    //mosquitto_username_pw_set(mosq, "admin", "changeme");
    mosquitto_will_set(mosq, "spBv1.0/Sparkplug B Devices/NDEATH/C Edge Node 1", 0, NULL, 0, false);

    // Optional 'self-signed' SSL parameters for MQTT
    //mosquitto_tls_insecure_set(mosq, true);
    //mosquitto_tls_opts_set(mosq, 0, "tlsv1.2", NULL);               // 0 is DO NOT SSL_VERIFY_PEER

    // Optional 'real' SSL parameters for MQTT
    //mosquitto_tls_set(mosq, NULL, "/etc/ssl/certs/", NULL, NULL, NULL);   // Necessary if the CA or other certs need to be picked up elsewhere on the local filesystem
    //mosquitto_tls_insecure_set(mosq, false);
    //mosquitto_tls_opts_set(mosq, 1, "tlsv1.2", NULL);               // 1 is SSL_VERIFY_PEER

    // MQTT Connect
    fprintf(stdout, "Starting connection...\n");
    if (mosquitto_connect(mosq, host, port, keepalive)) {
        fprintf(stderr, "Unable to connect.\n");
        //return 1;
    }

    // Publish the NBIRTH and DBIRTH Sparkplug messages (Birth Certificates)
    publish_births(mosq);

    // Initialization of device peripheral and objects
    ESP_ERROR_CHECK(master_init());
    vTaskDelay(10);

    //

    // Loop and publish more DDATA messages every 5 seconds.  Note this should only be done in real/production
    // scenarios with change events on inputs.  Because Sparkplug ensures state there is no reason to send DDATA
    // messages unless the state of a I/O point has changed.
    int i;
    for (i = 0; i < 100; i++) {
        // publish_ddata_message(mosq); 
        printf("\ndata pushlish\n");
        int j;
        for (j = 0; j < 50; j++) { //50
            usleep(100000);
            mosquitto_loop(mosq, -1, 1);
        }
    }
// 	int rc;


//   int ii;
// 	struct mosquitto_message *msg;

// 	mosquitto_lib_init();

// 	rc = mosquitto_subscribe_simple(
// 			&msg, COUNT, true,
// 			"spBv1.0/#", 0,
// 			"20.11.21.225", 1883,
// 			NULL, 60, true,
// 			NULL, NULL,
// 			NULL, NULL);

// 	if(rc){
// 		printf("Error: %s\n", mosquitto_strerror(rc));
// 		mosquitto_lib_cleanup();
// 		return rc;
// 	}

// 	for(ii=0; ii<COUNT; ii++){
// 		printf("%s %s\n", msg[ii].topic, (char *)msg[ii].payload);
// 		mosquitto_message_free_contents(&msg[ii]);
// 	}
// 	free(msg);




//     mosquitto_loop_forever(mosq, -1, 1);





//      esp_task_wdt_reset(); 
//    while(1){
//     vTaskDelay(100);
//      esp_task_wdt_reset(); 
//    }

}