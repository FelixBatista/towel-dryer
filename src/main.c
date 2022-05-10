#include "mgos.h"
#include "mgos_dht.h"
#include "mgos_rpc.h"
#include "mgos_dash.h"

float temp, humi;
bool resistance_is_off = true;
bool is_drying = false;

static void set_resistance (bool desired_state) {
  mgos_gpio_write (mgos_sys_config_get_app_resistancepin(), desired_state);
  resistance_is_off = desired_state;
}

static void regulate_temperature (float temp, float humi) {
  //If it is already drying a towel, keep it below 70 degrees Celsius
  if (is_drying == true){
    if (temp > mgos_sys_config_get_app_maxtemp()) {    //Histeresis on the resistance
      set_resistance (true);
    } else {
      set_resistance (false);
    }
    if (humi < mgos_sys_config_get_app_minhumi()) {   //if it is drying but the humidity is already low, then drying process is over 
      set_resistance (true);
      is_drying = false;
    }
  } else {  //If it is not drying, have to check if there is a wet towel and start drying
    if (humi > mgos_sys_config_get_app_maxhumi()) {
      is_drying = true;
      set_resistance (false);
    }
  }
}

//When timer reaches the time, it reads the sensor temperature and humidity, writes on the log and 
static void timer_cb(void *dht) {
  temp = mgos_dht_get_temp(dht);
  humi =  mgos_dht_get_humidity(dht);
  LOG(LL_INFO, ("Temperature: %lf, Humidity: %lf", temp, humi));
  regulate_temperature (temp, humi);
}
 

//Receives instructions fromt he internet via RPC
static void rpc_cb(struct mg_rpc_request_info *ri, void *cb_arg,
                   struct mg_rpc_frame_info *fi, struct mg_str args) {
  mg_rpc_send_responsef(ri, "{Temperature: %lf, Humidity: %lf}", mgos_dht_get_temp(cb_arg), mgos_dht_get_humidity(cb_arg));
  (void) fi;
  (void) args;
}

//Main function that runs once
enum mgos_app_init_result mgos_app_init(void) {
  mgos_gpio_setup_output(mgos_sys_config_get_app_resistancepin(), true);           //set resistance pin as output
  struct mgos_dht *dht = mgos_dht_create(mgos_sys_config_get_app_dhtpin(), DHT22);  //creates dht
  mgos_set_timer(5000, true, timer_cb, dht);                                        //sets timer
  mg_rpc_add_handler(mgos_rpc_get_global(), "DHT.read", "", rpc_cb, dht);           //sets rpc
  return MGOS_APP_INIT_SUCCESS;
}