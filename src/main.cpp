#include <mbed.h>
#include <EthernetInterface.h>
#include <rtos.h>
#include <mbed_events.h>

#include <C12832.h>

Thread eventhandler;
EventQueue eventqueue;

C12832 lcd(D11, D13, D12, D7, D10);

/* YOU will have to hardwire the IP address in here */
SocketAddress server("192.168.1.13",65200);
SocketAddress dash("192.168.1.13",65201);

EthernetInterface eth;
UDPSocket udp;
char buffer[512];
char line[80];
int send(char *m, size_t s) {
    nsapi_size_or_error_t r = udp.sendto( server, m, s);
    return r;
}
int receive(char *m, size_t s) {
    SocketAddress reply;
    nsapi_size_or_error_t r = udp.recvfrom(&reply, m,s );
    return r;
}
/* Input from Potentiometers */
AnalogIn  left(A0);
AnalogIn right(A1);

float constrain(float value, float lower, float upper) {
    if( value<lower ) value=lower;
    if( value>upper ) value=upper;
    return value;
}
float throttle = 0.0;
float last1 = 0;
float last2 = 0;
float integral = 0;
void controller(void) {
    const char text[] = "speed:?\n";
    char buffer[512];
    float mph;
    float demand = (int)(left.read()*100);
    strcpy(buffer, text);
    send(buffer, strlen(buffer));
    size_t len = receive(buffer, sizeof(buffer));
    buffer[len]='\0';
    sscanf(buffer,"speed:%f", &mph);
    lcd.locate(0,0);
    lcd.printf("Set %5.2f Speed : %5.2f",demand,mph);

    float error = demand-mph;

    float P = (error-last1);
    float I = (error);
    float D = (error-2*last1+last2);
    float Kp =5, Ki = 3, Kd = 3;
//    float Kp =10, Ki = 5, Kd = 0.1;
    throttle += ( Kp*P + Ki*I  + Kd*D)/1000;
    throttle = constrain(throttle,0,1);
    sprintf(buffer,"throttle:%f\n",throttle);
    send(buffer,strlen(buffer));
    last2 = last1;
    last1 = error;

    sprintf(buffer,"setting:%f\nspeed:%f\nthrottle:%f\n", demand, mph, throttle);
    udp.sendto( dash, buffer, strlen(buffer));
}

int main() {

    printf("conecting \n");
    eth.connect();
    const char *ip = eth.get_ip_address();
    printf("IP address is: %s\n", ip ? ip : "No IP");

    udp.open( &eth);
    SocketAddress source;
        printf("sending messages to %s/%d\n",
                server.get_ip_address(),  server.get_port() );

    eventhandler.start(callback(&eventqueue, &EventQueue::dispatch_forever));

    eventqueue.call_every(50,controller);

    while(1){}
}
