#include <lcom/lcf.h>

#include "../include/mouse.h"


static int hook_id_mouse;
struct packet pack;
int byte_no = 0;


//subscribes mouse interruptions
int subscribe_mouse(uint8_t *bit_no) {
    hook_id_mouse = MOUSE_IRQ;
    *bit_no = hook_id_mouse;

    if (sys_irqsetpolicy(MOUSE_IRQ,IRQ_REENABLE|IRQ_EXCLUSIVE,&hook_id_mouse) != 0) {
        printf("Unable to subscribe mouse interruptions");
        return 1;
    }

    //printf("SUB\n");
    return 0;
}


//unubscribes mouse interruptions
int unsubscribe_mouse() {
    uint8_t stat;

    mouse_disable();
    if (sys_irqrmpolicy(&hook_id_mouse) != 0) {
        printf("Unable to unsubscribe mouse interruptions");
        return 1;
    }
 
    kbc_read_status(&stat);

    
     if (stat & OBF) {
         clear_obf();
    }
    
    //printf("UNSUB\n");
    return 0;
}

int mouse_disable() {
    if (sys_irqdisable(&hook_id_mouse) != 0) {
        return 1;
    }

    return 0;
}

int mouse_enable() {
    if (sys_irqenable(&hook_id_mouse) != 0) {
        return 1;
    }

    return 0;
}


int kbc_read_status(uint8_t * status) {
    uint32_t temp;

    if(sys_inb(STAT_REG,&temp) != 0) {
        printf("Error reading configuration");
        return 1;
    } 

    *status = (uint8_t) temp;

    return 0;
}

bool empty_obf() {
    uint8_t status;
    kbc_read_status(&status);

    if (status & OBF) {
        return false;
    }
    else {
        return true;
    }
}

bool empty_ibf() {
    uint8_t status;
    kbc_read_status(&status);

    if (status & IBF) {
        //puts("ojfveivjiej\n");
        return false;
    }
    else {
        //puts("OBF EMPTY\n");
        return true;
    }
}


int write_mouse_command(uint8_t command) {
    int error_count = 0;

    while (error_count < 5) {
        if (!empty_ibf()) {
            //printf("IBF not empty1\n");
            tickdelay(micros_to_ticks(DELAY_US));
            error_count++;
            continue;
        } 

        else {
            if(sys_outb(KBC_CMD_REG,WRITE_MOUSE) != 0) { //tells kbc to write to mouse
                printf("Failed to write to mouse\n");
                return 1;
            }
            
            if (send_arg(command) != 0) {
                return 1;
            }

            return 0;
        }
    }

    return 1;
}


int send_arg(uint8_t command) {

    int error_count = 0;
    uint32_t ret;

    while(error_count < 5) {
        if (!empty_ibf()) {
            //printf("IBF not empty2\n");
            tickdelay(micros_to_ticks(DELAY_US));
            error_count++;
            continue;
        } 

        else {
            if(sys_outb(KBC_ARGS_REG,command) != 0) { //tells kbc to write to mouse
                printf("Failed to write command\n");
                return 1;
            }

            tickdelay(micros_to_ticks(DELAY_US)); //wait for the acknowledgement

            if (sys_inb(KBC_RET_REG,&ret) != 0) { //reads acknowledgement
                printf("Failed to read return\n");
                return 1;
            }

            if (ret == ACK ) { 
                return 0;
            }
            else if (ret == NACK) {
                continue;
            }
            else {
                error_count++;
                continue;
            }
        }
    }

    return 1;

}

int write_kbc_command(uint8_t command) {
    int error_count = 0;

    while (error_count < 5) {
        if (!empty_ibf()) {
            //printf("IBF not empty1\n");
            tickdelay(micros_to_ticks(DELAY_US));
            error_count++;
            continue;
        } 

        else {
            if(sys_outb(KBC_CMD_REG,WRITE_CMD) != 0) { //tells kbc to write to mouse
                printf("Failed to write to mouse\n");
                return 1;
            }
            
            tickdelay(micros_to_ticks(DELAY_US)); //wait for the acknowledgement

            if (sys_outb(KBC_ARGS_REG,command) != 0) {
                return 1;
            }

            return 0;
        }
    }

    return 1;
}


int mouse_set_stream_mode() {
    if (write_mouse_command(SET_STREAM_MODE) != 0) {
        printf("Failed to set mouse to stream mode\n");
        return 1;
    }
    else {
        return 0;
    }
}

int mouse_set_remote_mode() {
    if(write_mouse_command(SET_REMOTE_MODE) != 0) {
        printf("Failed to set mouse to remote mode\n");
        return 1;
    }
    else {
        return 0;
    }
}

int mouse_enable_data_rep() {
    mouse_disable();
    //printf("ENABLING DATA REP...\n");

    if(write_mouse_command(ENABLE_REP) != 0) {
        printf("Failed enable data reporting\n"); 
        return 1;
    }
    
    mouse_enable();
    // printf("Enabeled data reporting\n");
    return 0;
}

int mouse_disable_data_rep() {
    mouse_disable();

    if(write_mouse_command(DISABLE_REP) != 0) {
        return 1;
    }

    mouse_enable();
    // printf("Disabeled data reporting\n");
    return 0;
}

int mouse_read_data() {

    if (write_mouse_command(READ_DATA) != 0) {
        return 1;
    }

    return 0;
}

int clear_obf() {
    uint32_t temp;

    if (sys_inb(OUT_BUF, &temp) != 0) {
        return 1;
    }
    return 0;
}

void (mouse_ih) (void) {
    uint32_t temp;
    uint8_t curr_byte;

    sys_inb(OUT_BUF, &temp);
    curr_byte = (uint8_t) temp;

    pack.bytes[byte_no  % 3] = curr_byte;

    byte_no++;

}


uint16_t revert_2_complement_x(uint8_t num) {
    uint16_t num_cpy = num;
    if (BIT(4) & pack.bytes[0]) { //test if number is negative
        num_cpy -= 256;
        return num_cpy;
    }
    else { //number is positive -> return it 
        return num;
    }
}

uint16_t revert_2_complement_y(uint8_t num) {
    uint16_t num_cpy = num;
    if (BIT(5) & pack.bytes[0]) { //test if number is negative
        num_cpy -= 256;
        return num_cpy;
    }
    else { //number is positive -> return it 
        return num;
    }
}

void build_packet(struct packet * pac) {
    pac->rb = BIT(1) & pac->bytes[0];
    pac->lb = BIT(0) & pac->bytes[0];
    pac->mb = BIT(2) & pac->bytes[0];
    pac->delta_x = revert_2_complement_x(pac->bytes[1]);
    pac->delta_y = revert_2_complement_y(pac->bytes[2]);
    pac->x_ov = BIT(6) & pac->bytes[0];
    pac->y_ov = BIT(7) & pac->bytes[0];
}
 





