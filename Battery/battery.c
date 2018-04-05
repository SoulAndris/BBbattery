#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <time.h>

#define WAIT_PRINT_IN_FILE      3
#define WAIT_BLINK_GPIO         10
#define SLEEP_BETWENN_CHARGE    60

#define NUMBER_CHARGE           500
#define STATUS_CHARGE_MAX       0.5
#define BATT_MINIMUM            3


/*--------------------------------------ADC------------------------------------------*/

#define CMD_ADC          "BB_ADC"
#define ADC              "/sys/devices/platform/bone_capemgr/slots"
#define IN_BATT          "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"
#define IN_VSYS          "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"
#define IN_STATUS_CHARGE "/sys/bus/iio/devices/iio:device0/in_voltage3_raw"

/*----------------------------------------------------------------------------------*/

/*----------------------------------------GPIO--------------------------------------*/
#define OUT_RELAY_CHARGE    01
#define OUT_RELAY_LOAD      02
#define OUT_GPIO_LED        03

#define SYS_GPIO_DIR "/sys/class/gpio"
/*---------------------------------------------------------------------------------*/


void check_new_file(char *fname)
{
    FILE *new_file;
    time_t rawtime;
    char time_str[255];
    struct tm * timeinfo;

    time (&rawtime);
    timeinfo = localtime( &rawtime );

    strftime(time_str, 255, "%c", timeinfo);
    new_file = fopen(fname, "r");
    if(new_file == NULL)
    {
        new_file = fopen(fname, "w");
        fprintf(new_file, "%s\n", time_str);
    }

    fclose(new_file);
}

void print_value_in_file(int file, int number, int time_from_start, char value1, char value2)
{
    FILE* out;
    char name_file[255];
    char index [10];
    sprintf(index,"%d",number);
    if(file == 1)
    {
        strcat(name_file, "Charge");
        strcat(name_file,index);
        strcat(name_file,".txt");
        check_new_file(name_file);
        out = fopen(name_file, "a+");
    }
    else
    {
        strcat(name_file, "D-Charge");
        strcat(name_file,index);
        strcat(name_file,".txt");
        check_new_file(name_file);
        out = fopen(name_file, "a+");
    }
    fprintf(out, "%d\t%s\t%s\n",time_from_start,&value1, &value2);
    fclose (out);
}

void print_value_in_screen(int i, int time_from_start, char value1, char value2)
{
    if(i == 1)
    {
        printf("Charge №%d\n", i);
    }
    else
    {
        printf("D-Charge №%d\n", i);
    }
    printf("%d\t%s\t%s\n",time_from_start,&value1, &value2);

}

int gpio_write(int gpio, char status)
{
    char buff[255];

    int file;

    snprintf(buff, sizeof(buff), SYS_GPIO_DIR"/gpio%d/value",gpio);
    buff[sizeof(buff)-1] = 0;

    file = open(buff,O_WRONLY);
    if( file < 0) return -1;

    pwrite(file, &status, sizeof(status), 0);

    close(file);
    return 0;
}

int gpio_read(int gpio)
{
    char buff[255];
    int file;
    char value;

    snprintf(buff, sizeof(buff), SYS_GPIO_DIR"/gpio%d/value",gpio);

    file = open(buff, O_RDONLY);
    if( file < 0) return -1;

    pread(file, &value,sizeof(value),0);

    close(file);
    return value;
}

void gpio_init( int gpio)
{
    int export_gpio;
    export_gpio = open(SYS_GPIO_DIR"/export/", O_WRONLY );
    if(export_gpio < 0) return;

    char buff[255];
    int len;

    len = snprintf(buff, sizeof(buff), "%d", gpio);
    write(export_gpio, buff, len);
    close(export_gpio);

    snprintf(buff, sizeof(buff), SYS_GPIO_DIR"/gpio%d/direction", gpio);
    buff[sizeof(buff)-1] = 0;

    int direction_gpio;
    direction_gpio = open(buff, O_WRONLY);
    if(direction_gpio < 0) return;

    write(direction_gpio, "out", 4);
    close(direction_gpio);
}

void ADC_init()
{
    int ADC_export;
    ADC_export = open(ADC, O_WRONLY);
    if(ADC_export < 0) return;

    write(ADC_export, CMD_ADC, (strlen(CMD_ADC)+1));
    close(ADC_export);
}

int ADC_read(char ADC_number)
{
    char value;
    int ADC_read;
    ADC_read = open(&ADC_number, O_RDONLY);

    if(ADC_read < 0) return -1;

    pread(ADC_read, &value, sizeof(value), 0 );
    close(ADC_read);

    return value;
}

void all_gpio_off()
{
    gpio_write(OUT_RELAY_CHARGE, 0);
    gpio_write(OUT_RELAY_LOAD, 0);
    gpio_write(OUT_GPIO_LED,0);
}

void state_charge(int number)
{
    gpio_write(OUT_RELAY_CHARGE, 1);
    gpio_write(OUT_RELAY_LOAD, 0);

    int stop = ADC_read(*IN_STATUS_CHARGE);
    int time_from_start = 0;
    int time_count = 0;

    while(stop != STATUS_CHARGE_MAX )
    {
        int value1, value2, count = 0;
        value1 = ADC_read(*IN_BATT);
        value2 = ADC_read(*IN_VSYS);
        print_value_in_file(1 , number, time_from_start,  value1,  value2);
        print_value_in_screen(number, time_from_start,  value1,  value2);
        sleep(WAIT_PRINT_IN_FILE);
        stop = ADC_read(*IN_STATUS_CHARGE);
        count++;
        time_count++;
        if(count == 3)
        {
            gpio_write(OUT_GPIO_LED,1);
            count = 0;
        }
        else
        {
            gpio_write(OUT_GPIO_LED,0);
        }
        time_from_start = (3*time_count);
    }
    all_gpio_off();
}

void state_d_charge(int number)
{
    gpio_write(OUT_RELAY_CHARGE, 0);
    gpio_write(OUT_RELAY_LOAD, 1);

    int stop = ADC_read(*IN_BATT);
    int time_from_start = 0;
    int time_count = 0;

    while(stop != BATT_MINIMUM)
    {
        int value1, value2, count = 0;
        value1 = ADC_read(*IN_BATT);
        value2 = ADC_read(*IN_VSYS);
        print_value_in_file(0, number, time_from_start,  value1,  value2);
        print_value_in_screen(number, time_from_start,  value1,  value2);
        sleep(WAIT_PRINT_IN_FILE);
        stop = ADC_read(*IN_STATUS_CHARGE);
        count++;
        time_count++;
        if(count == 3)
        {
            gpio_write(OUT_GPIO_LED,1);
            count = 0;
        }
        else
        {
            gpio_write(OUT_GPIO_LED,0);
        }
        time_from_start = (3*time_count);
    }
    all_gpio_off();
}

int main(void)
{

    ADC_init();
    gpio_init(OUT_GPIO_LED);
    gpio_init(OUT_RELAY_CHARGE);
    gpio_init(OUT_RELAY_LOAD);

   for(int i = 0; i < NUMBER_CHARGE; i++)
   {

       state_charge(i+1);

       sleep(SLEEP_BETWENN_CHARGE);

       state_d_charge(i+1);

   }

    return 0;
}

