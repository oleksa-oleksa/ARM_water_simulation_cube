#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define RUN_SIM_PER_DISPLAY_REFRESHS          20    ///< Iterations of refreshing the displays until an other step in the simulation is done 
#define UPDATE_ACC_DATA_PER_DISPLAY_REFRESHS 250    ///< Iterations of refreshing the displays until a new measurement with the accelerometer is done and send to the Raspberry Pi 1B
#define NO_PARTICLES_LOCAL                    50    ///< Number of particles simulated when local simulation has been enabled
#define DT_SIM                                 0.05 ///< Timestep of the simulation in sec

#include <utils.h>
#include <timer.h>
#include <particle.h>
#include <led32x32.h>
#include <protocol.h>
#include <paint_tool.h>
#include <accelerometer.h>




static panel_t panels[CHAIN_LEN];                               ///< The 6 panels of the cube
static particle_list_element_t particles[NO_PARTICLES_LOCAL];   ///< All particles simulated


//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------


int _sim_on_dev(void)
{
    double force[3] = {0, 0, 0};
    int16_t x, y, z;
    char i2c_msg[16];
    uint32_t no_refreshed = 0;

    /*Initialise simulation*/
    particle_init_list(particles, NO_PARTICLES_LOCAL);
    particle_init_grid(panels[0], particles, NO_PARTICLES_LOCAL);
    particle_init_grid(panels[1], NULL, 0);
    particle_init_grid(panels[2], NULL, 0);
    particle_init_grid(panels[3], NULL, 0);
    particle_init_grid(panels[4], NULL, 0);
    particle_init_grid(panels[5], NULL, 0);
    

    while(1)
    {

        if(no_refreshed >= RUN_SIM_PER_DISPLAY_REFRESHS)
        {
            x = getX();
            y = getY();
            z = getZ();
            force[0] = ((x * 1.0) / 128.0) * 9.8;
            force[1] = ((y * 1.0) / 128.0) * 9.8;
            force[2] = ((z * 1.0) / 128.0) * 9.8;
            sprintf(i2c_msg, "%4.2f %4.2f %4.2f", force[0], force[1], force[2]);
            lcd_print_coordinates((unsigned char *)i2c_msg);

            acc_data_t acc_data = {.force_x = force[0],\
                                   .force_y = force[1],\
                                   .force_z = force[2]};
            protocol_send_acc_data(acc_data);
            
            particle_move_cube(/*top=*/panels[0], /*bottom=*/panels[1], /*front=*/panels[2], /*back=*/panels[3], /*left=*/panels[4], /*right=*/panels[5], DT_SIM, force);
            no_refreshed = 0;
        }
        lp32x32_refresh_chain_blue(panels);
        ++no_refreshed;
    }
    
    return 0;
}

int _sim_external(void)
{
    double force[3] = {0, 0, 0};
    int16_t x, y, z;
    char i2c_msg[16];
    uint32_t no_refreshed = 0;

    if(protocol_init(panels))
    {
        return 1;
    }

    /*Initialise simulation*/
    particle_init_list(particles, 50);
    particle_init_grid(panels[0], NULL, 0);
    particle_init_grid(panels[1], NULL, 0);
    particle_init_grid(panels[2], NULL, 0);
    particle_init_grid(panels[3], NULL, 0);
    particle_init_grid(panels[4], NULL, 0);
    particle_init_grid(panels[5], NULL, 0);
    

    while(1)
    {

        if(no_refreshed >= UPDATE_ACC_DATA_PER_DISPLAY_REFRESHS)
        {
            x = getX();
            y = getY();
            z = getZ();
            force[0] = ((x * 1.0) / 128.0) * 9.8;
            force[1] = ((y * 1.0) / 128.0) * 9.8;
            force[2] = ((z * 1.0) / 128.0) * 9.8;
            sprintf(i2c_msg, "%4.2f %4.2f %4.2f", force[0], force[1], force[2]);
            lcd_print_coordinates((unsigned char *)i2c_msg);

            acc_data_t acc_data = {.force_x = force[0],\
                                   .force_y = force[1],\
                                   .force_z = force[2]};
            protocol_send_acc_data(acc_data);
            
            no_refreshed = 0;
        }
        lp32x32_refresh_chain_24bit_rgb(panels);
        ++no_refreshed;
    }
    
    return 0;
}


//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------


int main()
{
    volatile uint8_t id;
    
    CCLKCFG = 0x03; // 0x3 => generates 72 MHz (Max.)
    
    /* Initialize hardware */
    led32x32_init();
    lcd_init();
    lcd_print_greeting();
    delay(2000);
    i2c_init(); // fixed, works properly
    lcd_print_message((unsigned char *)"I2C Init done...");
    delay(2000);
    if(accelerometer_init()) 
    {
        lcd_print_message((unsigned char *)"ADXL345 found");
        delay(2000);
    } 
    else 
    {
        lcd_print_message((unsigned char *)"ADXL345 error");
        delay(2000);
        return 1;
    }
    lcd_print_message((unsigned char *)"ADXL345 started!");
    delay(2000);


    if((FIO3PIN0 & 0x80) != 0)
    {
        lcd_print_message((unsigned char *)"Local Simulation");
        delay(2000);
        return _sim_on_dev();
    }
    else
    {
        lcd_print_message((unsigned char *)"Remote Simulation");
        delay(2000);
        return _sim_external();
    }
}
