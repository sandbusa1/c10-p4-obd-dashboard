#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum { OBD_OEM_NONE=0, OBD_OEM_FORD, OBD_OEM_GM, OBD_OEM_CHRYSLER_JEEP, OBD_OEM_BMW, OBD_OEM_LAND_ROVER } obd_oem_profile_t;
enum { OBD_CAP_STANDARD=1u<<0, OBD_CAP_MS_CAN=1u<<1, OBD_CAP_SW_CAN=1u<<2, OBD_CAP_USB=1u<<3, OBD_CAP_BT=1u<<4, OBD_CAP_VPW=1u<<5, OBD_CAP_DVI=1u<<6, OBD_CAP_HS_CAN=1u<<7 };
typedef struct {
 uint32_t seq; char state[24],protocol[24],vin[18]; bool uds_detected; int rpm; float mph,tps,load,ect_f,iat_f,map_kpa,maf_gps,volts,timing_deg,fuel_pct,afr; float stft1,ltft1,stft2,ltft2; uint32_t runtime_s,pid_support[6],pid_valid[6];
 float fuel_pressure_kpa,fuel_rail_kpa,fuel_rail_abs_kpa,baro_kpa,ambient_f,relative_tps,abs_load_pct,throttle_b_pct,throttle_c_pct,pedal_d_pct,pedal_e_pct,pedal_f_pct,rel_accel_pct,cmd_throttle_pct,control_module_v,evap_purge_pct,egr_cmd_pct,egr_error_pct,ethanol_pct,hybrid_battery_pct,catalyst_b1s1_f,catalyst_b2s1_f,catalyst_b1s2_f,catalyst_b2s2_f,evap_vapor_pa,actual_afr,oil_temp_f,injection_timing_deg,fuel_rate_lph,demanded_torque_pct,actual_torque_pct;
 uint32_t reference_torque_nm; uint8_t obd_standard,fuel_type; uint32_t mil_time_min,clear_time_min,distance_mil_km,distance_clear_km,warmups_since_clear; int gear; bool gear_valid; int shifter_range; bool shifter_range_valid; float oil_pressure_psi; bool oil_pressure_valid,mil_on; int dtc_count; char dtc_codes[320]; uint32_t good,nodata,errors; char adapter_name[40]; uint32_t adapter_caps; char oem_profile[24],oem_status[40],oem_result[256];
} obd_data_t;
esp_err_t obd_auto_start(void); void obd_auto_snapshot(obd_data_t *out); void obd_auto_request_dtc_refresh(void); void obd_auto_request_clear_dtcs(void); void obd_auto_set_oil_pressure(float psi,bool valid); void obd_auto_set_oem_profile(obd_oem_profile_t profile); void obd_auto_request_oem_scan(void);
