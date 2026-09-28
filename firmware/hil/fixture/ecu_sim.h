#ifndef ECU_SIM_H
#define ECU_SIM_H

void ecu_sim_init(
    int rx_pin,
    int tx_pin
);
void ecu_sim_poll();

#endif /* ECU_SIM_H */
